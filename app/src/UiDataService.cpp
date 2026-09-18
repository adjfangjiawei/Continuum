#include "UiDataService.h"

#include "FileScanner.h"
#include "SecurityService.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <utility>

#include <sqlite3.h>

namespace continuum
{
namespace
{

class Statement final
{
public:
    Statement(
        sqlite3* database,
        const std::string& sql
    )
        : statement_(nullptr),
          status_(sqlite3_prepare_v2(
              database,
              sql.c_str(),
              -1,
              &statement_,
              nullptr
          ))
    {
    }

    ~Statement()
    {
        if (statement_ != nullptr)
        {
            sqlite3_finalize(statement_);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    bool IsValid() const
    {
        return status_ == SQLITE_OK &&
            statement_ != nullptr;
    }

    sqlite3_stmt* Get()
    {
        return statement_;
    }

private:
    sqlite3_stmt* statement_;
    int status_;
};

std::string ColumnText(
    sqlite3_stmt* statement,
    int column
)
{
    const auto* value =
        sqlite3_column_text(statement, column);

    return value == nullptr
        ? std::string()
        : reinterpret_cast<const char*>(value);
}

std::int64_t ScalarInteger(
    Database& database,
    const std::string& sql
)
{
    std::lock_guard<std::recursive_mutex> lock(
        database.Mutex()
    );

    sqlite3* handle = database.Handle();

    if (handle == nullptr)
    {
        return 0;
    }

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return 0;
    }

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return 0;
    }

    return sqlite3_column_int64(
        statement.Get(),
        0
    );
}

std::string EscapeJson(
    const std::string& value
)
{
    std::string result;

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
}

}

UiOperationResult UiOperationResult::Ok(
    const std::string& message
)
{
    return {
        true,
        SQLITE_OK,
        message
    };
}

UiOperationResult UiOperationResult::FromStorage(
    const StorageStatus& status
)
{
    return {
        status.success,
        status.code,
        status.message
    };
}

UiDataService& UiDataService::Instance()
{
    static UiDataService instance;
    return instance;
}

UiDataService::UiDataService()
    : workspace_(WorkspaceService::Instance()),
      nextListenerToken_(1)
{
}

UiOperationResult UiDataService::Initialize()
{
    /*
     * 数据服务不能自行选择或创建默认工作区。
     *
     * 工作区必须由应用启动中心或显式的工作区切换流程先行打开。
     * 否则在“关闭旧工作区、打开新工作区”的间隙调用本方法时，
     * 会静默回落到默认数据库，导致后续操作写入错误工作区。
     */
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化，请先创建或打开工作区"
        };
    }

    auto& worker = BackgroundWorker::Instance();

    if (!worker.IsRunning())
    {
        const auto status =
            worker.Start(workspace_);

        if (!status.success)
        {
            return UiOperationResult::FromStorage(
                status
            );
        }
    }

    Notify({
        UiChangeType::Workspace,
        std::string(),
        "工作区数据服务已就绪"
    });

    return UiOperationResult::Ok(
        "工作区数据服务已就绪"
    );
}

bool UiDataService::IsReady() const
{
    return workspace_.IsInitialized();
}

DashboardSnapshot UiDataService::Dashboard() const
{
    DashboardSnapshot snapshot;

    snapshot.workspaceDirectory =
        workspace_.WorkspaceDirectory();

    snapshot.databasePath =
        workspace_.DatabasePath();

    if (!workspace_.IsInitialized())
    {
        snapshot.lastError = "工作区尚未初始化";
        return snapshot;
    }

    Database& database =
        workspace_.GetDatabase();

    snapshot.activeObjects = ScalarInteger(
        database,
        "SELECT count(*) FROM objects "
        "WHERE deleted=0;"
    );

    snapshot.deletedObjects = ScalarInteger(
        database,
        "SELECT count(*) FROM objects "
        "WHERE deleted=1;"
    );

    snapshot.evidenceItems = ScalarInteger(
        database,
        "SELECT count(*) FROM evidence "
        "WHERE deleted=0;"
    );

    snapshot.evidenceNeedsReview = ScalarInteger(
        database,
        "SELECT count(*) FROM evidence "
        "WHERE deleted=0 AND review_state IN "
        "('unverified','needs_review','changed');"
    );

    snapshot.enabledDataSources = ScalarInteger(
        database,
        "SELECT count(*) FROM data_sources "
        "WHERE enabled=1;"
    );

    snapshot.indexedFiles = ScalarInteger(
        database,
        "SELECT count(*) FROM files "
        "WHERE deleted=0 AND parse_state='parsed';"
    );

    snapshot.pendingFiles = ScalarInteger(
        database,
        "SELECT count(*) FROM files "
        "WHERE deleted=0 AND parse_state IN "
        "('pending','changed','failed','parsing');"
    );

    snapshot.queuedJobs = ScalarInteger(
        database,
        "SELECT count(*) FROM jobs "
        "WHERE state IN ('queued','retry','running');"
    );

    snapshot.failedJobs = ScalarInteger(
        database,
        "SELECT count(*) FROM jobs "
        "WHERE state IN ('failed','blocked');"
    );

    snapshot.auditEvents = ScalarInteger(
        database,
        "SELECT count(*) FROM audit_events;"
    );

    SearchService search(database);

    snapshot.searchDocuments =
        search.IndexedDocumentCount();

    snapshot.pendingSearchItems =
        search.PendingCount();

    const auto workerStatus =
        BackgroundWorker::Instance().Status();

    snapshot.backgroundWorkerRunning =
        workerStatus.running;

    snapshot.lastError =
        workerStatus.lastError;

    snapshot.sqlCipherAvailable =
        DatabaseSecurity::IsSqlCipherAvailable();

    return snapshot;
}

std::vector<DomainObjectRecord>
UiDataService::ListObjects(
    const std::string& objectType,
    bool includeDeleted,
    int limit
) const
{
    if (!workspace_.IsInitialized())
    {
        return {};
    }

    return workspace_.Objects().List(
        objectType,
        includeDeleted,
        limit
    );
}

UiOperationResult UiDataService::SaveObject(
    const DomainObjectRecord& record,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    const auto status =
        workspace_.Objects().Save(
            record,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Objects,
            record.id,
            "业务对象已保存"
        });

        Notify({
            UiChangeType::Dashboard,
            record.id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

UiOperationResult UiDataService::DeleteObject(
    const std::string& id,
    const std::string& reason,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    const auto status =
        workspace_.Objects().SoftDelete(
            id,
            reason,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Objects,
            id,
            "业务对象已移入回收站"
        });

        Notify({
            UiChangeType::Dashboard,
            id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

UiOperationResult UiDataService::RestoreObject(
    const std::string& id,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    const auto status =
        workspace_.Objects().Restore(
            id,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Objects,
            id,
            "业务对象已恢复"
        });

        Notify({
            UiChangeType::Dashboard,
            id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

std::vector<EvidenceRecord>
UiDataService::EvidenceForReview(
    int limit
) const
{
    if (!workspace_.IsInitialized())
    {
        return {};
    }

    return workspace_.Evidence().ListForReview(
        limit
    );
}

std::optional<EvidenceRecord>
UiDataService::FindEvidence(
    const std::string& id,
    bool includeDeleted
) const
{
    if (!workspace_.IsInitialized() || id.empty())
    {
        return std::nullopt;
    }

    return workspace_.Evidence().FindById(
        id,
        includeDeleted
    );
}

ParsedFileContent UiDataService::LoadParsedFile(
    const std::string& fileId,
    int maximumContentCharacters,
    int maximumSections
) const
{
    ParsedFileContent result;

    if (!workspace_.IsInitialized())
    {
        result.status = {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
        return result;
    }

    if (fileId.empty())
    {
        result.status = {
            false,
            SQLITE_CONSTRAINT,
            "文件编号不能为空"
        };
        return result;
    }

    const auto file =
        workspace_.Files().FindById(fileId, false);

    if (!file)
    {
        result.status = {
            false,
            SQLITE_NOTFOUND,
            "文件不存在或已经删除"
        };
        return result;
    }

    result.file = *file;

    ParsedContentRepository repository(
        workspace_.GetDatabase()
    );

    const auto schemaStatus = repository.EnsureSchema();

    if (!schemaStatus.success)
    {
        result.status =
            UiOperationResult::FromStorage(schemaStatus);
        return result;
    }

    maximumContentCharacters = std::max(
        1,
        std::min(4 * 1024 * 1024, maximumContentCharacters)
    );
    maximumSections = std::max(
        1,
        std::min(2000, maximumSections)
    );

    Database& database = workspace_.GetDatabase();

    std::lock_guard<std::recursive_mutex> lock(
        database.Mutex()
    );

    sqlite3* handle = database.Handle();

    if (handle == nullptr)
    {
        result.status = {
            false,
            SQLITE_MISUSE,
            "数据库尚未打开"
        };
        return result;
    }

    Statement document(
        handle,
        "SELECT fingerprint, "
        "length(CAST(content AS BLOB)), "
        "substr(content, 1, ?) "
        "FROM parsed_documents "
        "WHERE file_id=? LIMIT 1;"
    );

    if (!document.IsValid())
    {
        result.status = {
            false,
            sqlite3_errcode(handle),
            std::string("无法读取解析正文: ") +
                sqlite3_errmsg(handle)
        };
        return result;
    }

    sqlite3_bind_int(
        document.Get(),
        1,
        maximumContentCharacters
    );
    sqlite3_bind_text(
        document.Get(),
        2,
        fileId.c_str(),
        -1,
        SQLITE_TRANSIENT
    );

    const int documentStep =
        sqlite3_step(document.Get());

    if (documentStep != SQLITE_ROW)
    {
        result.status = {
            false,
            documentStep == SQLITE_DONE
                ? SQLITE_NOTFOUND
                : documentStep,
            documentStep == SQLITE_DONE
                ? "该文件尚无解析正文"
                : (
                    std::string("读取解析正文失败: ") +
                    sqlite3_errmsg(handle)
                )
        };
        return result;
    }

    const std::string parsedFingerprint =
        ColumnText(document.Get(), 0);

    result.contentBytes =
        sqlite3_column_int64(document.Get(), 1);
    result.content =
        ColumnText(document.Get(), 2);
    /*
     * contentBytes 与 std::string 及 char:N 锚点采用相同的
     * UTF-8 字节单位。substr(TEXT) 的参数仍按 Unicode
     * 字符计算，因此以实际返回字节数判断是否截断。
     */
    result.contentTruncated =
        result.contentBytes >
            static_cast<std::int64_t>(
                result.content.size()
            );
    result.matchesCurrentFileFingerprint =
        !result.file.fingerprint.empty() &&
        parsedFingerprint == result.file.fingerprint;

    Statement sections(
        handle,
        "SELECT ordinal, heading, anchor, content "
        "FROM parsed_sections "
        "WHERE file_id=? "
        "ORDER BY ordinal LIMIT ?;"
    );

    if (!sections.IsValid())
    {
        result.status = {
            false,
            sqlite3_errcode(handle),
            std::string("无法读取解析分段: ") +
                sqlite3_errmsg(handle)
        };
        return result;
    }

    sqlite3_bind_text(
        sections.Get(),
        1,
        fileId.c_str(),
        -1,
        SQLITE_TRANSIENT
    );
    sqlite3_bind_int(
        sections.Get(),
        2,
        maximumSections + 1
    );

    int sectionStep = SQLITE_ROW;

    while ((sectionStep = sqlite3_step(sections.Get())) ==
           SQLITE_ROW)
    {
        if (static_cast<int>(result.sections.size()) >=
            maximumSections)
        {
            result.sectionsTruncated = true;
            break;
        }

        ParsedSection section;
        section.ordinal =
            sqlite3_column_int(sections.Get(), 0);
        section.heading =
            ColumnText(sections.Get(), 1);
        section.anchor =
            ColumnText(sections.Get(), 2);
        section.content =
            ColumnText(sections.Get(), 3);

        result.sections.push_back(
            std::move(section)
        );
    }

    if (sectionStep != SQLITE_DONE &&
        sectionStep != SQLITE_ROW)
    {
        result.status = {
            false,
            sectionStep,
            std::string("读取解析分段失败: ") +
                sqlite3_errmsg(handle)
        };
        return result;
    }

    result.status = UiOperationResult::Ok();
    return result;
}

UiOperationResult
UiDataService::SetEvidenceReviewState(
    const std::string& id,
    const std::string& state,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    if (id.empty())
    {
        return {
            false,
            SQLITE_MISUSE,
            "证据编号不能为空"
        };
    }

    const bool validState =
        state == "unverified" ||
        state == "needs_review" ||
        state == "changed" ||
        state == "verified" ||
        state == "rejected";

    if (!validState)
    {
        return {
            false,
            SQLITE_MISMATCH,
            "无效的证据审查状态"
        };
    }

    const auto status =
        workspace_.Evidence().SetReviewState(
            id,
            state,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Evidence,
            id,
            "证据审查状态已更新"
        });

        Notify({
            UiChangeType::Dashboard,
            id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

std::vector<DataSourceRecord>
UiDataService::ListDataSources(
    bool includeDisabled,
    int limit
) const
{
    if (!workspace_.IsInitialized())
    {
        return {};
    }

    return workspace_.DataSources().List(
        includeDisabled,
        limit
    );
}

UiOperationResult UiDataService::SaveDataSource(
    const DataSourceRecord& record,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    const auto status =
        workspace_.DataSources().Save(
            record,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::DataSources,
            record.id,
            "数据源已保存"
        });

        Notify({
            UiChangeType::Dashboard,
            record.id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

UiOperationResult
UiDataService::SetDataSourceEnabled(
    const std::string& id,
    bool enabled,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    const auto status =
        workspace_.DataSources().SetEnabled(
            id,
            enabled,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::DataSources,
            id,
            enabled
                ? "数据源已启用"
                : "数据源已停用"
        });

        Notify({
            UiChangeType::Dashboard,
            id,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

std::string UiDataService::MakeJobId(
    const std::string& prefix,
    const std::string& subject
)
{
    const auto now =
        std::chrono::system_clock::now();

    const auto ticks =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(
            now.time_since_epoch()
        ).count();

    const std::string digest =
        ContentHasher::Sha256Text(
            prefix +
            "\n" +
            subject +
            "\n" +
            std::to_string(ticks)
        );

    return prefix + "-" + digest.substr(0, 24);
}

UiOperationResult UiDataService::QueueSourceScan(
    const std::string& sourceId,
    const std::string& actor
)
{
    if (!workspace_.IsInitialized())
    {
        return {
            false,
            SQLITE_MISUSE,
            "工作区尚未初始化"
        };
    }

    if (!workspace_.DataSources().FindById(sourceId))
    {
        return {
            false,
            SQLITE_NOTFOUND,
            "数据源不存在"
        };
    }

    JobRecord job;
    job.id = MakeJobId("JOB-SCAN", sourceId);
    job.jobType = "scan_source";
    job.state = "queued";
    job.payload =
        "{\"source_id\":\"" +
        EscapeJson(sourceId) +
        "\"}";

    JobRepository jobs(
        workspace_.GetDatabase()
    );

    const auto status =
        jobs.Enqueue(job, actor);

    if (status.success)
    {
        BackgroundWorker::Instance().Wake();

        Notify({
            UiChangeType::Jobs,
            job.id,
            "数据源扫描任务已加入队列"
        });

        Notify({
            UiChangeType::DataSources,
            sourceId,
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

SearchResponse UiDataService::Search(
    const SearchRequest& request
)
{
    SearchResponse response;

    if (!workspace_.IsInitialized())
    {
        response.status = StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区尚未初始化"
        );

        return response;
    }

    SearchService search(
        workspace_.GetDatabase()
    );

    response = search.Search(request);

    Notify({
        UiChangeType::Search,
        std::string(),
        response.status.success
            ? "搜索完成"
            : response.status.message
    });

    return response;
}

std::vector<UiJobRecord>
UiDataService::ListJobs(
    int limit
) const
{
    std::vector<UiJobRecord> records;

    if (!workspace_.IsInitialized())
    {
        return records;
    }

    Database& database =
        workspace_.GetDatabase();

    std::lock_guard<std::recursive_mutex> lock(
        database.Mutex()
    );

    sqlite3* handle = database.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT "
        "j.id, j.job_type, j.state, j.progress, "
        "COALESCE(r.attempts,0), "
        "COALESCE(r.cancel_requested,0), "
        "j.error_message, j.created_at, "
        "j.started_at, j.completed_at "
        "FROM jobs j "
        "LEFT JOIN job_runtime r ON r.job_id=j.id "
        "ORDER BY j.created_at DESC, j.id DESC "
        "LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(
        statement.Get(),
        1,
        std::max(1, std::min(2000, limit))
    );

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        UiJobRecord record;
        record.id = ColumnText(statement.Get(), 0);
        record.type = ColumnText(statement.Get(), 1);
        record.state = ColumnText(statement.Get(), 2);
        record.progress =
            sqlite3_column_int(statement.Get(), 3);
        record.attempts =
            sqlite3_column_int(statement.Get(), 4);
        record.cancellationRequested =
            sqlite3_column_int(statement.Get(), 5) != 0;
        record.errorMessage =
            ColumnText(statement.Get(), 6);
        record.createdAt =
            ColumnText(statement.Get(), 7);
        record.startedAt =
            ColumnText(statement.Get(), 8);
        record.completedAt =
            ColumnText(statement.Get(), 9);

        records.push_back(std::move(record));
    }

    return records;
}

UiOperationResult UiDataService::CancelJob(
    const std::string& jobId,
    const std::string& actor
)
{
    const auto status =
        BackgroundWorker::Instance().RequestCancel(
            jobId,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Jobs,
            jobId,
            "任务取消请求已提交"
        });
    }

    return UiOperationResult::FromStorage(status);
}

UiOperationResult UiDataService::RetryJob(
    const std::string& jobId,
    const std::string& actor
)
{
    const auto status =
        BackgroundWorker::Instance().RetryNow(
            jobId,
            actor
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Jobs,
            jobId,
            "任务已重新加入队列"
        });
    }

    return UiOperationResult::FromStorage(status);
}

BackgroundWorkerStatus
UiDataService::WorkerStatus() const
{
    return BackgroundWorker::Instance().Status();
}

BackupResult UiDataService::CreateBackup(
    const BackupCreateOptions& options
)
{
    BackupService service(workspace_);

    auto result = service.CreateBackup(options);

    if (result.status.success)
    {
        Notify({
            UiChangeType::Backups,
            result.backup.id,
            "工作区备份已创建"
        });
    }

    return result;
}

std::vector<BackupRecord>
UiDataService::ListBackups() const
{
    BackupService service(workspace_);
    return service.ListBackups();
}

std::uint64_t UiDataService::BackupCount() const
{
    if (!workspace_.IsInitialized())
    {
        return 0;
    }

    BackupService service(workspace_);
    return service.CountBackups();
}

UiOperationResult UiDataService::ValidateBackup(
    const std::string& backupDirectory,
    BackupRecord* record
) const
{
    BackupService service(workspace_);

    return UiOperationResult::FromStorage(
        service.ValidateBackup(
            backupDirectory,
            record
        )
    );
}

UiOperationResult UiDataService::RestoreBackup(
    const std::string& backupDirectory,
    const BackupRestoreOptions& options
)
{
    BackupService service(workspace_);

    const auto status =
        service.RestoreBackup(
            backupDirectory,
            options
        );

    if (status.success)
    {
        Notify({
            UiChangeType::Workspace,
            std::string(),
            "工作区备份已恢复"
        });

        Notify({
            UiChangeType::Dashboard,
            std::string(),
            std::string()
        });

        Notify({
            UiChangeType::Objects,
            std::string(),
            std::string()
        });

        Notify({
            UiChangeType::Evidence,
            std::string(),
            std::string()
        });
    }

    return UiOperationResult::FromStorage(status);
}

SecurityCapabilities UiDataService::Security() const
{
    return DatabaseSecurity::Diagnose();
}

std::uint64_t UiDataService::Subscribe(
    Listener listener
)
{
    if (!listener)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(
        listenerMutex_
    );

    const std::uint64_t token =
        nextListenerToken_++;

    listeners_[token] =
        std::move(listener);

    return token;
}

void UiDataService::Unsubscribe(
    std::uint64_t token
)
{
    std::lock_guard<std::mutex> lock(
        listenerMutex_
    );

    listeners_.erase(token);
}

void UiDataService::Refresh(
    UiChangeType type,
    const std::string& message
)
{
    Notify({
        type,
        std::string(),
        message
    });
}

void UiDataService::Notify(
    const UiChangeEvent& event
)
{
    std::vector<Listener> listeners;

    {
        std::lock_guard<std::mutex> lock(
            listenerMutex_
        );

        listeners.reserve(listeners_.size());

        for (const auto& entry : listeners_)
        {
            listeners.push_back(entry.second);
        }
    }

    for (const auto& listener : listeners)
    {
        try
        {
            listener(event);
        }
        catch (...)
        {
            /*
             * UI 监听器异常不能破坏数据服务操作。
             */
        }
    }
}

}
