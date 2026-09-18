#include "BackgroundWorker.h"

#include <algorithm>
#include <exception>
#include <regex>
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

    int Status() const
    {
        return status_;
    }

    sqlite3_stmt* Get()
    {
        return statement_;
    }

private:
    sqlite3_stmt* statement_;
    int status_;
};

void BindText(
    sqlite3_stmt* statement,
    int parameter,
    const std::string& value
)
{
    sqlite3_bind_text(
        statement,
        parameter,
        value.c_str(),
        -1,
        SQLITE_TRANSIENT
    );
}

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

StorageStatus SqliteError(
    sqlite3* database,
    int code,
    const std::string& context
)
{
    std::string message = context;

    if (database != nullptr)
    {
        message += ": ";
        message += sqlite3_errmsg(database);
    }

    return StorageStatus::Error(code, message);
}

std::string ReplaceAll(
    std::string value,
    const std::string& from,
    const std::string& to
)
{
    if (from.empty())
    {
        return value;
    }

    std::size_t position = 0;

    while ((position = value.find(from, position)) !=
           std::string::npos)
    {
        value.replace(position, from.size(), to);
        position += to.size();
    }

    return value;
}

bool ContainsExternalParserError(
    const std::string& message
)
{
    return message.find("外部文档解析器") !=
            std::string::npos ||
        message.find("external parser") !=
            std::string::npos;
}

}

BackgroundWorker& BackgroundWorker::Instance()
{
    static BackgroundWorker instance;
    return instance;
}

BackgroundWorker::BackgroundWorker()
    : workspace_(nullptr),
      database_(nullptr),
      stopRequested_(false),
      wakeRequested_(false)
{
}

BackgroundWorker::~BackgroundWorker()
{
    Stop();
}

StorageStatus BackgroundWorker::Start(
    WorkspaceService& workspace,
    const BackgroundWorkerOptions& options
)
{
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (status_.running)
    {
        return StorageStatus::Ok();
    }

    if (!workspace.IsInitialized())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区尚未初始化，无法启动后台工作线程"
        );
    }

    workspace_ = &workspace;
    database_ = &workspace.GetDatabase();
    options_ = options;

    options_.maximumAttempts =
        std::max(1, options_.maximumAttempts);
    options_.indexBatchSize =
        std::max(1, options_.indexBatchSize);

    if (options_.idleInterval.count() < 10)
    {
        options_.idleInterval =
            std::chrono::milliseconds(10);
    }

    const auto schemaStatus = EnsureSchema();

    if (!schemaStatus.success)
    {
        workspace_ = nullptr;
        database_ = nullptr;
        return schemaStatus;
    }

    /*
     * 当前后台执行器是进程内单例。启动新线程前仍处于
     * running 的任务只能来自上一次异常退出，必须恢复，
     * 否则它们永远不会再次被领取。
     */
    const auto recoveryStatus =
        database_->Transaction([&]() {
            sqlite3* handle = database_->Handle();

            Statement recover(
                handle,
                "UPDATE jobs SET "
                "state='retry', "
                "progress=0, "
                "error_message='上次运行被中断，已自动重试', "
                "completed_at=NULL "
                "WHERE state='running';"
            );

            if (!recover.IsValid())
            {
                return SqliteError(
                    handle,
                    recover.Status(),
                    "无法准备中断任务恢复"
                );
            }

            if (sqlite3_step(recover.Get()) !=
                SQLITE_DONE)
            {
                return SqliteError(
                    handle,
                    sqlite3_errcode(handle),
                    "无法恢复中断任务"
                );
            }

            const int recovered =
                sqlite3_changes(handle);

            if (recovered == 0)
            {
                return StorageStatus::Ok();
            }

            return AuditRepository(*database_).Append(
                "background-worker",
                "job",
                "recover_interrupted",
                "job_queue",
                "default",
                "{\"count\":" +
                    std::to_string(recovered) +
                    "}"
            );
        });

    if (!recoveryStatus.success)
    {
        workspace_ = nullptr;
        database_ = nullptr;
        return recoveryStatus;
    }

    stopRequested_.store(false);
    wakeRequested_.store(true);

    status_ = BackgroundWorkerStatus{};
    status_.running = true;

    try
    {
        workerThread_ = std::thread(
            &BackgroundWorker::WorkerLoop,
            this
        );
    }
    catch (const std::exception& exception)
    {
        status_.running = false;
        workspace_ = nullptr;
        database_ = nullptr;

        return StorageStatus::Error(
            SQLITE_ERROR,
            std::string("无法启动后台工作线程: ") +
                exception.what()
        );
    }

    return StorageStatus::Ok();
}

void BackgroundWorker::Stop()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        if (!status_.running &&
            !workerThread_.joinable())
        {
            return;
        }

        stopRequested_.store(true);
        wakeRequested_.store(true);
        status_.stopRequested = true;
    }

    condition_.notify_all();

    if (workerThread_.joinable() &&
        workerThread_.get_id() !=
            std::this_thread::get_id())
    {
        workerThread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        status_.running = false;
        status_.stopRequested = false;
        status_.activeJobId.clear();
        status_.activeJobType.clear();
        workspace_ = nullptr;
        database_ = nullptr;
    }

    idleCondition_.notify_all();
}

void BackgroundWorker::Wake()
{
    wakeRequested_.store(true);
    condition_.notify_all();
}

bool BackgroundWorker::IsRunning() const
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return status_.running;
}

BackgroundWorkerStatus BackgroundWorker::Status() const
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    return status_;
}

StorageStatus BackgroundWorker::EnsureSchema()
{
    if (database_ == nullptr ||
        !database_->IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "后台工作线程没有可用数据库"
        );
    }

    return database_->Execute(R"sql(
CREATE TABLE IF NOT EXISTS job_runtime (
    job_id TEXT PRIMARY KEY,
    attempts INTEGER NOT NULL DEFAULT 0,
    cancel_requested INTEGER NOT NULL DEFAULT 0,
    claimed_at TEXT,
    worker_id TEXT NOT NULL DEFAULT '',
    last_error TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(job_id) REFERENCES jobs(id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_job_runtime_cancel
ON job_runtime(cancel_requested, updated_at);

CREATE INDEX IF NOT EXISTS idx_jobs_worker_queue
ON jobs(state, created_at, id);
)sql");
}

std::optional<BackgroundWorker::ClaimedJob>
BackgroundWorker::ClaimNextJob()
{
    if (database_ == nullptr)
    {
        return std::nullopt;
    }

    ClaimedJob claimed;
    bool found = false;

    const auto status = database_->Transaction([&]() {
        sqlite3* handle = database_->Handle();

        Statement select(
            handle,
            "SELECT "
            "j.id, j.job_type, j.state, j.progress, "
            "j.payload, j.error_message, j.created_at, "
            "j.started_at, j.completed_at, "
            "COALESCE(r.attempts, 0), "
            "COALESCE(r.cancel_requested, 0) "
            "FROM jobs j "
            "LEFT JOIN job_runtime r ON r.job_id=j.id "
            "WHERE j.state IN ('queued', 'retry') "
            "ORDER BY j.created_at, j.id "
            "LIMIT 1;"
        );

        if (!select.IsValid())
        {
            return SqliteError(
                handle,
                select.Status(),
                "无法准备后台任务领取查询"
            );
        }

        const int selectResult =
            sqlite3_step(select.Get());

        if (selectResult == SQLITE_DONE)
        {
            return StorageStatus::Ok();
        }

        if (selectResult != SQLITE_ROW)
        {
            return SqliteError(
                handle,
                selectResult,
                "无法读取待执行任务"
            );
        }

        claimed.job.id = ColumnText(select.Get(), 0);
        claimed.job.jobType = ColumnText(select.Get(), 1);
        claimed.job.state = ColumnText(select.Get(), 2);
        claimed.job.progress =
            sqlite3_column_int(select.Get(), 3);
        claimed.job.payload = ColumnText(select.Get(), 4);
        claimed.job.errorMessage =
            ColumnText(select.Get(), 5);
        claimed.job.createdAt =
            ColumnText(select.Get(), 6);
        claimed.job.startedAt =
            ColumnText(select.Get(), 7);
        claimed.job.completedAt =
            ColumnText(select.Get(), 8);
        claimed.attempts =
            sqlite3_column_int(select.Get(), 9);

        const bool cancelRequested =
            sqlite3_column_int(select.Get(), 10) != 0;

        if (cancelRequested)
        {
            Statement cancel(
                handle,
                "UPDATE jobs SET "
                "state='cancelled', "
                "progress=100, "
                "error_message='用户取消', "
                "completed_at=CURRENT_TIMESTAMP "
                "WHERE id=? "
                "AND state IN ('queued', 'retry');"
            );

            if (!cancel.IsValid())
            {
                return SqliteError(
                    handle,
                    cancel.Status(),
                    "无法准备已取消任务更新"
                );
            }

            BindText(
                cancel.Get(),
                1,
                claimed.job.id
            );

            if (sqlite3_step(cancel.Get()) !=
                SQLITE_DONE)
            {
                return SqliteError(
                    handle,
                    sqlite3_errcode(handle),
                    "无法取消待执行任务"
                );
            }

            {
                std::lock_guard<std::mutex> lock(
                    stateMutex_
                );
                ++status_.cancelledJobs;
            }

            return StorageStatus::Ok();
        }

        Statement claim(
            handle,
            "UPDATE jobs SET "
            "state='running', "
            "progress=1, "
            "error_message='', "
            "started_at=COALESCE("
            "started_at, CURRENT_TIMESTAMP"
            "), "
            "completed_at=NULL "
            "WHERE id=? "
            "AND state IN ('queued', 'retry');"
        );

        if (!claim.IsValid())
        {
            return SqliteError(
                handle,
                claim.Status(),
                "无法准备任务领取更新"
            );
        }

        BindText(claim.Get(), 1, claimed.job.id);

        if (sqlite3_step(claim.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法领取后台任务"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Ok();
        }

        Statement runtime(
            handle,
            "INSERT INTO job_runtime("
            "job_id, attempts, cancel_requested, "
            "claimed_at, worker_id, last_error, updated_at"
            ") VALUES("
            "?, 1, 0, CURRENT_TIMESTAMP, "
            "'continuum-background-worker', '', "
            "CURRENT_TIMESTAMP"
            ") ON CONFLICT(job_id) DO UPDATE SET "
            "attempts=job_runtime.attempts+1, "
            "claimed_at=CURRENT_TIMESTAMP, "
            "worker_id='continuum-background-worker', "
            "last_error='', "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!runtime.IsValid())
        {
            return SqliteError(
                handle,
                runtime.Status(),
                "无法准备任务运行记录"
            );
        }

        BindText(
            runtime.Get(),
            1,
            claimed.job.id
        );

        if (sqlite3_step(runtime.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法写入任务运行记录"
            );
        }

        ++claimed.attempts;
        claimed.job.state = "running";
        found = true;

        return StorageStatus::Ok();
    });

    if (!status.success)
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        status_.lastError = status.message;
        return std::nullopt;
    }

    if (!found)
    {
        return std::nullopt;
    }

    return claimed;
}

StorageStatus BackgroundWorker::RequestCancel(
    const std::string& jobId,
    const std::string& actor
)
{
    if (jobId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "取消任务时必须提供任务编号"
        );
    }

    if (database_ == nullptr ||
        !database_->IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "后台服务尚未启动"
        );
    }

    auto schemaStatus = EnsureSchema();

    if (!schemaStatus.success)
    {
        return schemaStatus;
    }

    auto status = database_->Transaction([&]() {
        sqlite3* handle = database_->Handle();

        Statement exists(
            handle,
            "SELECT state FROM jobs WHERE id=? LIMIT 1;"
        );

        if (!exists.IsValid())
        {
            return SqliteError(
                handle,
                exists.Status(),
                "无法准备任务查询"
            );
        }

        BindText(exists.Get(), 1, jobId);

        if (sqlite3_step(exists.Get()) != SQLITE_ROW)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "任务不存在"
            );
        }

        const std::string state =
            ColumnText(exists.Get(), 0);

        if (state == "completed" ||
            state == "failed" ||
            state == "cancelled" ||
            state == "blocked")
        {
            return StorageStatus::Error(
                SQLITE_CONSTRAINT,
                "任务已经结束，不能取消"
            );
        }

        Statement runtime(
            handle,
            "INSERT INTO job_runtime("
            "job_id, attempts, cancel_requested, "
            "updated_at"
            ") VALUES(?, 0, 1, CURRENT_TIMESTAMP) "
            "ON CONFLICT(job_id) DO UPDATE SET "
            "cancel_requested=1, "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!runtime.IsValid())
        {
            return SqliteError(
                handle,
                runtime.Status(),
                "无法准备任务取消请求"
            );
        }

        BindText(runtime.Get(), 1, jobId);

        if (sqlite3_step(runtime.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法记录任务取消请求"
            );
        }

        if (state == "queued" ||
            state == "retry")
        {
            Statement cancel(
                handle,
                "UPDATE jobs SET "
                "state='cancelled', "
                "progress=100, "
                "error_message='用户取消', "
                "completed_at=CURRENT_TIMESTAMP "
                "WHERE id=?;"
            );

            if (!cancel.IsValid())
            {
                return SqliteError(
                    handle,
                    cancel.Status(),
                    "无法准备排队任务取消"
                );
            }

            BindText(cancel.Get(), 1, jobId);

            if (sqlite3_step(cancel.Get()) !=
                SQLITE_DONE)
            {
                return SqliteError(
                    handle,
                    sqlite3_errcode(handle),
                    "无法取消排队任务"
                );
            }
        }

        return AuditRepository(*database_).Append(
            actor,
            "job",
            "cancel_requested",
            "job",
            jobId,
            "{}"
        );
    });

    Wake();
    return status;
}

StorageStatus BackgroundWorker::RetryNow(
    const std::string& jobId,
    const std::string& actor
)
{
    if (jobId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "重试任务时必须提供任务编号"
        );
    }

    if (database_ == nullptr ||
        !database_->IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "后台服务尚未启动"
        );
    }

    const auto status = database_->Transaction([&]() {
        sqlite3* handle = database_->Handle();

        Statement statement(
            handle,
            "UPDATE jobs SET "
            "state='retry', "
            "progress=0, "
            "error_message='', "
            "completed_at=NULL "
            "WHERE id=? "
            "AND state IN ('failed', 'blocked', 'cancelled');"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备任务重试"
            );
        }

        BindText(statement.Get(), 1, jobId);

        if (sqlite3_step(statement.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法重试任务"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "任务不存在或当前状态不能重试"
            );
        }

        Statement runtime(
            handle,
            "INSERT INTO job_runtime("
            "job_id, attempts, cancel_requested, updated_at"
            ") VALUES(?, 0, 0, CURRENT_TIMESTAMP) "
            "ON CONFLICT(job_id) DO UPDATE SET "
            "attempts=0, "
            "cancel_requested=0, "
            "last_error='', "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!runtime.IsValid())
        {
            return SqliteError(
                handle,
                runtime.Status(),
                "无法准备重试运行记录"
            );
        }

        BindText(runtime.Get(), 1, jobId);

        if (sqlite3_step(runtime.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法重置任务运行记录"
            );
        }

        return AuditRepository(*database_).Append(
            actor,
            "job",
            "manual_retry",
            "job",
            jobId,
            "{}"
        );
    });

    Wake();
    return status;
}

bool BackgroundWorker::IsCancellationRequested(
    const std::string& jobId
)
{
    if (database_ == nullptr)
    {
        return true;
    }

    std::lock_guard<std::recursive_mutex> lock(
        database_->Mutex()
    );

    sqlite3* handle = database_->Handle();

    if (handle == nullptr)
    {
        return true;
    }

    Statement statement(
        handle,
        "SELECT cancel_requested "
        "FROM job_runtime "
        "WHERE job_id=? LIMIT 1;"
    );

    if (!statement.IsValid())
    {
        return false;
    }

    BindText(statement.Get(), 1, jobId);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return false;
    }

    return sqlite3_column_int(statement.Get(), 0) != 0;
}

StorageStatus BackgroundWorker::MarkCancelled(
    const std::string& jobId,
    const std::string& actor
)
{
    if (database_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库不可用"
        );
    }

    return database_->Transaction([&]() {
        sqlite3* handle = database_->Handle();

        Statement statement(
            handle,
            "UPDATE jobs SET "
            "state='cancelled', "
            "progress=100, "
            "error_message='用户取消', "
            "completed_at=CURRENT_TIMESTAMP "
            "WHERE id=?;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备任务取消状态"
            );
        }

        BindText(statement.Get(), 1, jobId);

        if (sqlite3_step(statement.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法更新任务取消状态"
            );
        }

        return AuditRepository(*database_).Append(
            actor,
            "job",
            "cancelled",
            "job",
            jobId,
            "{}"
        );
    });
}

std::string BackgroundWorker::ExtractJsonString(
    const std::string& json,
    const std::string& key
)
{
    const std::regex expression(
        "\"" + key +
        "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\""
    );

    std::smatch match;

    if (!std::regex_search(
            json,
            match,
            expression
        ) ||
        match.size() < 2)
    {
        return std::string();
    }

    std::string value = match[1].str();
    value = ReplaceAll(value, "\\\\", "\\");
    value = ReplaceAll(value, "\\\"", "\"");
    value = ReplaceAll(value, "\\n", "\n");
    value = ReplaceAll(value, "\\r", "\r");
    value = ReplaceAll(value, "\\t", "\t");

    return value;
}

StorageStatus BackgroundWorker::ExecuteParseJob(
    const ClaimedJob& claimed
)
{
    if (workspace_ == nullptr ||
        database_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区服务不可用"
        );
    }

    const std::string fileId =
        ExtractJsonString(
            claimed.job.payload,
            "file_id"
        );

    const std::string expectedFingerprint =
        ExtractJsonString(
            claimed.job.payload,
            "fingerprint"
        );

    if (fileId.empty() ||
        expectedFingerprint.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "解析任务缺少 file_id 或 fingerprint"
        );
    }

    const auto file =
        workspace_->Files().FindById(
            fileId,
            false
        );

    if (!file)
    {
        /*
         * 文件已经删除或不再可用。该版本任务无需重试，
         * 否则它会在队列中反复失败。
         */
        return StorageStatus::Ok();
    }

    if (file->fingerprint != expectedFingerprint)
    {
        /*
         * 这是旧版本任务。当前版本在扫描时会生成自己的
         * 确定性解析任务，因此旧任务应安全结束，而不是
         * 解析与任务载荷不一致的版本。
         */
        return StorageStatus::Ok();
    }

    ParseService parser(
        *database_,
        workspace_->Files()
    );

    return parser.ExecuteJob(
        claimed.job,
        "background-worker",
        false
    );
}

StorageStatus BackgroundWorker::ExecuteScanJob(
    const ClaimedJob& claimed
)
{
    if (workspace_ == nullptr ||
        database_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区服务不可用"
        );
    }

    const std::string sourceId =
        ExtractJsonString(
            claimed.job.payload,
            "source_id"
        );

    if (sourceId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "扫描任务缺少 source_id"
        );
    }

    FileScanner scanner(
        *database_,
        workspace_->DataSources(),
        workspace_->Files()
    );

    ScanOptions options;
    options.recursive = true;
    options.enqueueParseJobs = true;
    options.markMissingFiles = true;

    const auto summary =
        scanner.ScanSource(sourceId, options);

    if (summary.failed > 0)
    {
        const std::string message =
            summary.issues.empty()
                ? "数据源扫描存在失败项目"
                : summary.issues.front().message;

        return StorageStatus::Error(
            SQLITE_IOERR,
            message
        );
    }

    return StorageStatus::Ok();
}

StorageStatus BackgroundWorker::ExecuteClaimedJob(
    const ClaimedJob& claimed
)
{
    if (IsCancellationRequested(claimed.job.id))
    {
        return StorageStatus::Error(
            SQLITE_INTERRUPT,
            "任务已取消"
        );
    }

    if (claimed.job.jobType == "parse_file")
    {
        return ExecuteParseJob(claimed);
    }

    if (claimed.job.jobType == "scan_source")
    {
        return ExecuteScanJob(claimed);
    }

    return StorageStatus::Error(
        SQLITE_MISMATCH,
        "后台执行器不支持任务类型: " +
            claimed.job.jobType
    );
}

StorageStatus BackgroundWorker::FinishJob(
    const ClaimedJob& claimed,
    const StorageStatus& executionStatus
)
{
    if (database_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库不可用"
        );
    }

    if (IsCancellationRequested(claimed.job.id) ||
        executionStatus.code == SQLITE_INTERRUPT)
    {
        const auto status = MarkCancelled(
            claimed.job.id,
            "background-worker"
        );

        std::lock_guard<std::mutex> lock(stateMutex_);
        ++status_.cancelledJobs;
        return status;
    }

    if (executionStatus.success)
    {
        JobRepository jobs(*database_);

        const auto status = jobs.SetState(
            claimed.job.id,
            "completed",
            100,
            std::string(),
            "background-worker"
        );

        std::lock_guard<std::mutex> lock(stateMutex_);
        ++status_.completedJobs;
        status_.lastError.clear();

        return status;
    }

    const bool blocked =
        ContainsExternalParserError(
            executionStatus.message
        );

    const bool retry =
        !blocked &&
        claimed.attempts < options_.maximumAttempts &&
        !stopRequested_.load();

    const std::string state = blocked
        ? "blocked"
        : (retry ? "retry" : "failed");

    const int progress = retry ? 0 : 100;

    auto status = database_->Transaction([&]() {
        sqlite3* handle = database_->Handle();

        Statement jobStatement(
            handle,
            "UPDATE jobs SET "
            "state=?, "
            "progress=?, "
            "error_message=?, "
            "completed_at=CASE "
            "WHEN ? IN ('failed', 'blocked') "
            "THEN CURRENT_TIMESTAMP "
            "ELSE NULL END "
            "WHERE id=?;"
        );

        if (!jobStatement.IsValid())
        {
            return SqliteError(
                handle,
                jobStatement.Status(),
                "无法准备失败任务状态更新"
            );
        }

        BindText(jobStatement.Get(), 1, state);
        sqlite3_bind_int(
            jobStatement.Get(),
            2,
            progress
        );
        BindText(
            jobStatement.Get(),
            3,
            executionStatus.message
        );
        BindText(jobStatement.Get(), 4, state);
        BindText(
            jobStatement.Get(),
            5,
            claimed.job.id
        );

        if (sqlite3_step(jobStatement.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法更新失败任务"
            );
        }

        Statement runtimeStatement(
            handle,
            "UPDATE job_runtime SET "
            "last_error=?, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE job_id=?;"
        );

        if (!runtimeStatement.IsValid())
        {
            return SqliteError(
                handle,
                runtimeStatement.Status(),
                "无法准备任务错误记录"
            );
        }

        BindText(
            runtimeStatement.Get(),
            1,
            executionStatus.message
        );
        BindText(
            runtimeStatement.Get(),
            2,
            claimed.job.id
        );

        if (sqlite3_step(runtimeStatement.Get()) !=
            SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法保存任务错误"
            );
        }

        return AuditRepository(*database_).Append(
            "background-worker",
            "job",
            retry ? "retry" : state,
            "job",
            claimed.job.id,
            "{\"attempt\":" +
                std::to_string(claimed.attempts) +
                "}"
        );
    });

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        status_.lastError = executionStatus.message;

        if (retry)
        {
            ++status_.retriedJobs;
        }
        else
        {
            ++status_.failedJobs;
        }
    }

    if (retry)
    {
        Wake();
    }

    return status;
}

bool BackgroundWorker::SynchronizeSearchIndex()
{
    if (database_ == nullptr ||
        stopRequested_.load())
    {
        return false;
    }

    SearchService search(*database_);

    const auto availability =
        search.CheckAvailability();

    if (!availability.success)
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        status_.lastError = availability.message;
        return false;
    }

    const auto summary = search.SyncPending(
        options_.indexBatchSize,
        "background-worker"
    );

    {
        std::lock_guard<std::mutex> lock(stateMutex_);

        status_.indexedDocuments +=
            static_cast<std::uint64_t>(
                std::max(0, summary.indexed)
            );

        if (summary.failed > 0 &&
            !summary.errors.empty())
        {
            status_.lastError =
                summary.errors.front();
        }
        else if (summary.visited > 0)
        {
            status_.lastError.clear();
        }
    }

    /*
     * 失败项会留在队列中。返回 false 使 WorkerLoop 进入
     * 有超时的等待，防止永久失败项导致无间隔满核循环。
     */
    return summary.visited > 0 &&
        summary.failed == 0;
}

void BackgroundWorker::WorkerLoop()
{
    while (!stopRequested_.load())
    {
        bool performedWork = false;

        try
        {
            const auto claimed = ClaimNextJob();

            if (claimed)
            {
                performedWork = true;

                {
                    std::lock_guard<std::mutex> lock(
                        stateMutex_
                    );
                    status_.activeJobId =
                        claimed->job.id;
                    status_.activeJobType =
                        claimed->job.jobType;
                }

                StorageStatus executionStatus;

                try
                {
                    executionStatus =
                        ExecuteClaimedJob(*claimed);
                }
                catch (const std::exception& exception)
                {
                    executionStatus =
                        StorageStatus::Error(
                            SQLITE_ABORT,
                            exception.what()
                        );
                }
                catch (...)
                {
                    executionStatus =
                        StorageStatus::Error(
                            SQLITE_ABORT,
                            "后台任务发生未知异常"
                        );
                }

                FinishJob(
                    *claimed,
                    executionStatus
                );

                {
                    std::lock_guard<std::mutex> lock(
                        stateMutex_
                    );
                    status_.activeJobId.clear();
                    status_.activeJobType.clear();
                }

                idleCondition_.notify_all();
            }

            if (!stopRequested_.load())
            {
                const int pendingBefore =
                    database_ == nullptr
                        ? 0
                        : SearchService(
                            *database_
                        ).PendingCount();

                if (pendingBefore > 0)
                {
                    const bool indexProgress =
                        SynchronizeSearchIndex();

                    performedWork =
                        performedWork || indexProgress;
                }
            }
        }
        catch (const std::exception& exception)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            status_.lastError = exception.what();
        }
        catch (...)
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            status_.lastError =
                "后台工作线程发生未知异常";
        }

        if (!performedWork &&
            !stopRequested_.load())
        {
            std::unique_lock<std::mutex> lock(
                stateMutex_
            );

            idleCondition_.notify_all();

            condition_.wait_for(
                lock,
                options_.idleInterval,
                [this]() {
                    return stopRequested_.load() ||
                        wakeRequested_.exchange(false);
                }
            );
        }
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        status_.running = false;
        status_.activeJobId.clear();
        status_.activeJobType.clear();
    }

    idleCondition_.notify_all();
}

bool BackgroundWorker::WaitUntilIdle(
    std::chrono::milliseconds timeout
)
{
    const auto deadline =
        std::chrono::steady_clock::now() + timeout;

    std::unique_lock<std::mutex> lock(stateMutex_);

    while (std::chrono::steady_clock::now() < deadline)
    {
        lock.unlock();

        bool hasJobs = false;
        bool hasIndexItems = false;

        if (database_ != nullptr &&
            database_->IsOpen())
        {
            JobRepository jobs(*database_);
            hasJobs = !jobs.ListPending(1).empty();

            SearchService search(*database_);
            hasIndexItems = search.PendingCount() > 0;
        }

        lock.lock();

        const bool active =
            !status_.activeJobId.empty();

        if (!active &&
            !hasJobs &&
            !hasIndexItems)
        {
            return true;
        }

        idleCondition_.wait_until(
            lock,
            deadline
        );
    }

    return false;
}

}
