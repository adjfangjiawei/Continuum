#include "WorkspaceService.h"
#include "SecurityService.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <string>
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
        return status_ == SQLITE_OK && statement_ != nullptr;
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
    const auto* value = sqlite3_column_text(statement, column);

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

bool TableExists(
    sqlite3* database,
    const std::string& tableName
)
{
    Statement statement(
        database,
        "SELECT 1 FROM sqlite_master "
        "WHERE type IN ('table', 'view') "
        "AND name=? LIMIT 1;"
    );

    if (!statement.IsValid())
    {
        return false;
    }

    BindText(statement.Get(), 1, tableName);
    return sqlite3_step(statement.Get()) == SQLITE_ROW;
}

StorageStatus InvalidateFileDerivedData(
    sqlite3* database,
    const std::string& fileId
)
{
    if (database == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    /*
     * 解析正文由解析服务按需创建。
     * 删除正文会将对应索引删除操作加入队列。
     */
    if (TableExists(database, "parsed_documents"))
    {
        Statement removeDocument(
            database,
            "DELETE FROM parsed_documents "
            "WHERE file_id=?;"
        );

        if (!removeDocument.IsValid())
        {
            return SqliteError(
                database,
                removeDocument.Status(),
                "无法准备旧解析正文失效"
            );
        }

        BindText(removeDocument.Get(), 1, fileId);

        const int result =
            sqlite3_step(removeDocument.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                database,
                result,
                "无法使旧解析正文失效"
            );
        }
    }

    /*
     * 队列同步存在时间差，因此立即清除 FTS 条目，
     * 避免变化或缺失文件的旧正文继续出现在搜索结果中。
     */
    if (TableExists(database, "search_fts"))
    {
        Statement removeIndex(
            database,
            "DELETE FROM search_fts "
            "WHERE file_id=?;"
        );

        if (!removeIndex.IsValid())
        {
            return SqliteError(
                database,
                removeIndex.Status(),
                "无法准备旧全文索引失效"
            );
        }

        BindText(removeIndex.Get(), 1, fileId);

        const int result =
            sqlite3_step(removeIndex.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                database,
                result,
                "无法使旧全文索引失效"
            );
        }
    }

    /*
     * 来源文件改变或消失后，先前通过的证据必须重新进入人工审查。
     * rejected 保持拒绝状态，避免把已经明确拒绝的证据重新激活。
     */
    Statement changeEvidence(
        database,
        "UPDATE evidence "
        "SET review_state='changed', "
        "updated_at=CURRENT_TIMESTAMP "
        "WHERE file_id=? "
        "AND deleted=0 "
        "AND review_state NOT IN ('changed', 'rejected');"
    );

    if (!changeEvidence.IsValid())
    {
        return SqliteError(
            database,
            changeEvidence.Status(),
            "无法准备关联证据失效"
        );
    }

    BindText(changeEvidence.Get(), 1, fileId);

    const int evidenceResult =
        sqlite3_step(changeEvidence.Get());

    if (evidenceResult != SQLITE_DONE)
    {
        return SqliteError(
            database,
            evidenceResult,
            "无法使关联证据失效"
        );
    }

    return StorageStatus::Ok();
}


StorageStatus CaptureFileChange(
    sqlite3* database,
    const std::string& fileId,
    const std::string& nextFingerprint,
    const std::string& changeType
)
{
    if (database == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    Statement oldFile(
        database,
        "SELECT fingerprint, size_bytes, modified_at "
        "FROM files WHERE id=? LIMIT 1;"
    );

    if (!oldFile.IsValid())
    {
        return SqliteError(
            database,
            oldFile.Status(),
            "无法准备文件变化记录"
        );
    }

    BindText(oldFile.Get(), 1, fileId);

    const int oldFileResult =
        sqlite3_step(oldFile.Get());

    if (oldFileResult == SQLITE_DONE)
    {
        return StorageStatus::Ok();
    }

    if (oldFileResult != SQLITE_ROW)
    {
        return SqliteError(
            database,
            oldFileResult,
            "无法读取文件变化前状态"
        );
    }

    const std::string oldFingerprint =
        ColumnText(oldFile.Get(), 0);

    const std::int64_t oldSize =
        sqlite3_column_int64(oldFile.Get(), 1);

    const std::string oldModifiedAt =
        ColumnText(oldFile.Get(), 2);

    if (oldFingerprint.empty())
    {
        return StorageStatus::Ok();
    }

    std::string oldContent;

    if (TableExists(database, "parsed_documents"))
    {
        Statement parsed(
            database,
            "SELECT content FROM parsed_documents "
            "WHERE file_id=? LIMIT 1;"
        );

        if (!parsed.IsValid())
        {
            return SqliteError(
                database,
                parsed.Status(),
                "无法准备旧解析正文读取"
            );
        }

        BindText(parsed.Get(), 1, fileId);

        const int parsedResult =
            sqlite3_step(parsed.Get());

        if (parsedResult == SQLITE_ROW)
        {
            oldContent =
                ColumnText(parsed.Get(), 0);
        }
        else if (parsedResult != SQLITE_DONE)
        {
            return SqliteError(
                database,
                parsedResult,
                "无法读取旧解析正文"
            );
        }
    }

    const std::string versionId =
        "FV-" + fileId + "-" + oldFingerprint;

    Statement saveVersion(
        database,
        "INSERT INTO file_versions("
        "id, file_id, version_number, fingerprint, "
        "size_bytes, modified_at, content"
        ") VALUES("
        "?, ?, "
        "COALESCE(("
        "SELECT max(version_number)+1 "
        "FROM file_versions WHERE file_id=?"
        "), 1), "
        "?, ?, NULLIF(?,''), ?"
        ") ON CONFLICT(file_id, fingerprint) "
        "DO UPDATE SET "
        "content=CASE "
        "WHEN file_versions.content='' "
        "THEN excluded.content "
        "ELSE file_versions.content END;"
    );

    if (!saveVersion.IsValid())
    {
        return SqliteError(
            database,
            saveVersion.Status(),
            "无法准备文件版本保存"
        );
    }

    BindText(saveVersion.Get(), 1, versionId);
    BindText(saveVersion.Get(), 2, fileId);
    BindText(saveVersion.Get(), 3, fileId);
    BindText(saveVersion.Get(), 4, oldFingerprint);
    sqlite3_bind_int64(
        saveVersion.Get(),
        5,
        oldSize
    );
    BindText(
        saveVersion.Get(),
        6,
        oldModifiedAt
    );
    BindText(
        saveVersion.Get(),
        7,
        oldContent
    );

    const int versionResult =
        sqlite3_step(saveVersion.Get());

    if (versionResult != SQLITE_DONE)
    {
        return SqliteError(
            database,
            versionResult,
            "无法保存文件版本"
        );
    }

    const std::string reviewFingerprint =
        nextFingerprint.empty()
            ? "missing"
            : nextFingerprint;

    const std::string reviewId =
        "CR-" + fileId + "-" + reviewFingerprint;

    Statement saveReview(
        database,
        "INSERT INTO change_reviews("
        "id, file_id, previous_version_id, "
        "current_version_id, change_type, status"
        ") VALUES("
        "?, ?, ?, NULL, ?, 'pending'"
        ") ON CONFLICT(id) DO UPDATE SET "
        "previous_version_id=excluded.previous_version_id, "
        "change_type=excluded.change_type, "
        "status='pending', "
        "completed_at=NULL, "
        "updated_at=CURRENT_TIMESTAMP;"
    );

    if (!saveReview.IsValid())
    {
        return SqliteError(
            database,
            saveReview.Status(),
            "无法准备变化审查记录"
        );
    }

    BindText(saveReview.Get(), 1, reviewId);
    BindText(saveReview.Get(), 2, fileId);
    BindText(saveReview.Get(), 3, versionId);
    BindText(saveReview.Get(), 4, changeType);

    const int reviewResult =
        sqlite3_step(saveReview.Get());

    if (reviewResult != SQLITE_DONE)
    {
        return SqliteError(
            database,
            reviewResult,
            "无法保存变化审查记录"
        );
    }

    Statement saveImpacts(
        database,
        "INSERT INTO change_review_evidence("
        "review_id, evidence_id, impact_state, "
        "old_anchor, new_anchor, note"
        ") "
        "SELECT ?, id, 'needs_review', "
        "anchor, '', '' "
        "FROM evidence "
        "WHERE file_id=? AND deleted=0 "
        "ON CONFLICT(review_id, evidence_id) "
        "DO UPDATE SET "
        "impact_state='needs_review', "
        "old_anchor=excluded.old_anchor, "
        "new_anchor='', "
        "note='', "
        "updated_at=CURRENT_TIMESTAMP;"
    );

    if (!saveImpacts.IsValid())
    {
        return SqliteError(
            database,
            saveImpacts.Status(),
            "无法准备变化影响保存"
        );
    }

    BindText(saveImpacts.Get(), 1, reviewId);
    BindText(saveImpacts.Get(), 2, fileId);

    const int impactResult =
        sqlite3_step(saveImpacts.Get());

    if (impactResult != SQLITE_DONE)
    {
        return SqliteError(
            database,
            impactResult,
            "无法保存受影响证据"
        );
    }

    return StorageStatus::Ok();
}

DataSourceRecord ReadDataSource(sqlite3_stmt* statement)
{
    DataSourceRecord record;
    record.id = ColumnText(statement, 0);
    record.name = ColumnText(statement, 1);
    record.sourceType = ColumnText(statement, 2);
    record.rootPath = ColumnText(statement, 3);
    record.enabled = sqlite3_column_int(statement, 4) != 0;
    record.lastScanAt = ColumnText(statement, 5);
    record.createdAt = ColumnText(statement, 6);
    record.updatedAt = ColumnText(statement, 7);
    return record;
}

FileRecord ReadFile(sqlite3_stmt* statement)
{
    FileRecord record;
    record.id = ColumnText(statement, 0);
    record.sourceId = ColumnText(statement, 1);
    record.relativePath = ColumnText(statement, 2);
    record.displayName = ColumnText(statement, 3);
    record.mediaType = ColumnText(statement, 4);
    record.sizeBytes = sqlite3_column_int64(statement, 5);
    record.modifiedAt = ColumnText(statement, 6);
    record.fingerprint = ColumnText(statement, 7);
    record.parseState = ColumnText(statement, 8);
    record.deleted = sqlite3_column_int(statement, 9) != 0;
    record.createdAt = ColumnText(statement, 10);
    record.updatedAt = ColumnText(statement, 11);
    return record;
}

const char* DataSourceColumns()
{
    return
        "id, name, source_type, root_path, enabled, last_scan_at, "
        "created_at, updated_at";
}

const char* FileColumns()
{
    return
        "id, source_id, relative_path, display_name, media_type, "
        "size_bytes, modified_at, fingerprint, parse_state, deleted, "
        "created_at, updated_at";
}

std::string EnvironmentValue(const char* name)
{
    const char* value = std::getenv(name);

    return value == nullptr
        ? std::string()
        : std::string(value);
}

}

DataSourceRepository::DataSourceRepository(Database& database)
    : database_(database)
{
}

StorageStatus DataSourceRepository::Save(
    const DataSourceRecord& record,
    const std::string& actor
)
{
    if (record.id.empty() ||
        record.name.empty() ||
        record.sourceType.empty() ||
        record.rootPath.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "数据源编号、名称、类型和路径不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT INTO data_sources("
            "id, name, source_type, root_path, enabled, last_scan_at, "
            "created_at, updated_at"
            ") VALUES("
            "?, ?, ?, ?, ?, NULLIF(?, ''), "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP), "
            "CURRENT_TIMESTAMP"
            ") ON CONFLICT(id) DO UPDATE SET "
            "name=excluded.name, "
            "source_type=excluded.source_type, "
            "root_path=excluded.root_path, "
            "enabled=excluded.enabled, "
            "last_scan_at=excluded.last_scan_at, "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备数据源保存"
            );
        }

        BindText(statement.Get(), 1, record.id);
        BindText(statement.Get(), 2, record.name);
        BindText(statement.Get(), 3, record.sourceType);
        BindText(statement.Get(), 4, record.rootPath);
        sqlite3_bind_int(
            statement.Get(),
            5,
            record.enabled ? 1 : 0
        );
        BindText(statement.Get(), 6, record.lastScanAt);
        BindText(statement.Get(), 7, record.createdAt);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法保存数据源"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "data_source",
            "save",
            "data_source",
            record.id,
            "{}"
        );
    });
}

std::optional<DataSourceRecord>
DataSourceRepository::FindById(
    const std::string& id
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return std::nullopt;
    }

    const std::string sql =
        std::string("SELECT ") +
        DataSourceColumns() +
        " FROM data_sources WHERE id=? LIMIT 1;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return std::nullopt;
    }

    BindText(statement.Get(), 1, id);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    return ReadDataSource(statement.Get());
}

std::vector<DataSourceRecord> DataSourceRepository::List(
    bool includeDisabled,
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<DataSourceRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    std::string sql =
        std::string("SELECT ") +
        DataSourceColumns() +
        " FROM data_sources";

    if (!includeDisabled)
    {
        sql += " WHERE enabled=1";
    }

    sql += " ORDER BY name COLLATE NOCASE LIMIT ?;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(statement.Get(), 1, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        records.push_back(ReadDataSource(statement.Get()));
    }

    return records;
}

StorageStatus DataSourceRepository::SetEnabled(
    const std::string& id,
    bool enabled,
    const std::string& actor
)
{
    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE data_sources "
            "SET enabled=?, updated_at=CURRENT_TIMESTAMP "
            "WHERE id=?;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备数据源状态更新"
            );
        }

        sqlite3_bind_int(
            statement.Get(),
            1,
            enabled ? 1 : 0
        );
        BindText(statement.Get(), 2, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法更新数据源状态"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "数据源不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "data_source",
            enabled ? "enable" : "disable",
            "data_source",
            id,
            "{}"
        );
    });
}

StorageStatus DataSourceRepository::UpdateLastScan(
    const std::string& id,
    const std::string& scannedAt,
    const std::string& actor
)
{
    if (scannedAt.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "扫描时间不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE data_sources "
            "SET last_scan_at=?, updated_at=CURRENT_TIMESTAMP "
            "WHERE id=?;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备扫描时间更新"
            );
        }

        BindText(statement.Get(), 1, scannedAt);
        BindText(statement.Get(), 2, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法更新扫描时间"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "数据源不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "data_source",
            "scan_completed",
            "data_source",
            id,
            "{}"
        );
    });
}

FileRepository::FileRepository(Database& database)
    : database_(database)
{
}

StorageStatus FileRepository::Save(
    const FileRecord& record,
    const std::string& actor
)
{
    if (record.id.empty() ||
        record.sourceId.empty() ||
        record.relativePath.empty() ||
        record.displayName.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "文件编号、数据源、相对路径和名称不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();
        bool invalidateDerivedData = false;
        std::string detectedChangeType;

        /*
         * 更新文件记录前读取旧指纹。新文件没有旧派生内容；
         * 指纹变化或缺失文件重新出现时必须使旧派生内容失效。
         */
        Statement existing(
            handle,
            "SELECT fingerprint, deleted, parse_state "
            "FROM files WHERE id=? LIMIT 1;"
        );

        if (!existing.IsValid())
        {
            return SqliteError(
                handle,
                existing.Status(),
                "无法准备旧文件状态查询"
            );
        }

        BindText(existing.Get(), 1, record.id);

        const int existingResult =
            sqlite3_step(existing.Get());

        if (existingResult == SQLITE_ROW)
        {
            const std::string oldFingerprint =
                ColumnText(existing.Get(), 0);
            const bool wasDeleted =
                sqlite3_column_int(existing.Get(), 1) != 0;
            const std::string oldParseState =
                ColumnText(existing.Get(), 2);

            invalidateDerivedData =
                oldFingerprint != record.fingerprint ||
                wasDeleted ||
                oldParseState == "missing";

            if (invalidateDerivedData)
            {
                detectedChangeType =
                    wasDeleted ||
                    oldParseState == "missing"
                        ? "restored"
                        : "modified";
            }
        }
        else if (existingResult != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                existingResult,
                "无法读取旧文件状态"
            );
        }

        if (invalidateDerivedData)
        {
            const auto captureStatus =
                CaptureFileChange(
                    handle,
                    record.id,
                    record.fingerprint,
                    detectedChangeType
                );

            if (!captureStatus.success)
            {
                return captureStatus;
            }
        }

        Statement statement(
            handle,
            "INSERT INTO files("
            "id, source_id, relative_path, display_name, media_type, "
            "size_bytes, modified_at, fingerprint, parse_state, "
            "deleted, created_at, updated_at"
            ") VALUES("
            "?, ?, ?, ?, ?, ?, NULLIF(?, ''), ?, ?, ?, "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP), "
            "CURRENT_TIMESTAMP"
            ") ON CONFLICT(id) DO UPDATE SET "
            "source_id=excluded.source_id, "
            "relative_path=excluded.relative_path, "
            "display_name=excluded.display_name, "
            "media_type=excluded.media_type, "
            "size_bytes=excluded.size_bytes, "
            "modified_at=excluded.modified_at, "
            "fingerprint=excluded.fingerprint, "
            "parse_state=excluded.parse_state, "
            "deleted=excluded.deleted, "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备文件保存"
            );
        }

        BindText(statement.Get(), 1, record.id);
        BindText(statement.Get(), 2, record.sourceId);
        BindText(statement.Get(), 3, record.relativePath);
        BindText(statement.Get(), 4, record.displayName);
        BindText(statement.Get(), 5, record.mediaType);
        sqlite3_bind_int64(
            statement.Get(),
            6,
            record.sizeBytes
        );
        BindText(statement.Get(), 7, record.modifiedAt);
        BindText(statement.Get(), 8, record.fingerprint);
        BindText(
            statement.Get(),
            9,
            record.parseState.empty()
                ? "pending"
                : record.parseState
        );
        sqlite3_bind_int(
            statement.Get(),
            10,
            record.deleted ? 1 : 0
        );
        BindText(statement.Get(), 11, record.createdAt);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法保存文件"
            );
        }

        if (invalidateDerivedData)
        {
            const auto invalidateStatus =
                InvalidateFileDerivedData(
                    handle,
                    record.id
                );

            if (!invalidateStatus.success)
            {
                return invalidateStatus;
            }
        }

        return AuditRepository(database_).Append(
            actor,
            "file",
            "save",
            "file",
            record.id,
            "{}"
        );
    });
}

std::optional<FileRecord> FileRepository::FindById(
    const std::string& id,
    bool includeDeleted
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return std::nullopt;
    }

    std::string sql =
        std::string("SELECT ") +
        FileColumns() +
        " FROM files WHERE id=?";

    if (!includeDeleted)
    {
        sql += " AND deleted=0";
    }

    sql += " LIMIT 1;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return std::nullopt;
    }

    BindText(statement.Get(), 1, id);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    return ReadFile(statement.Get());
}

std::optional<FileRecord> FileRepository::FindByPath(
    const std::string& sourceId,
    const std::string& relativePath,
    bool includeDeleted
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return std::nullopt;
    }

    std::string sql =
        std::string("SELECT ") +
        FileColumns() +
        " FROM files WHERE source_id=? AND relative_path=?";

    if (!includeDeleted)
    {
        sql += " AND deleted=0";
    }

    sql += " LIMIT 1;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return std::nullopt;
    }

    BindText(statement.Get(), 1, sourceId);
    BindText(statement.Get(), 2, relativePath);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::nullopt;
    }

    return ReadFile(statement.Get());
}

std::vector<FileRecord> FileRepository::ListBySource(
    const std::string& sourceId,
    bool includeDeleted,
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<FileRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    std::string sql =
        std::string("SELECT ") +
        FileColumns() +
        " FROM files WHERE source_id=?";

    if (!includeDeleted)
    {
        sql += " AND deleted=0";
    }

    sql += " ORDER BY relative_path COLLATE NOCASE LIMIT ?;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return records;
    }

    BindText(statement.Get(), 1, sourceId);
    sqlite3_bind_int(statement.Get(), 2, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        records.push_back(ReadFile(statement.Get()));
    }

    return records;
}

std::vector<FileRecord> FileRepository::ListPending(
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<FileRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    const std::string sql =
        std::string("SELECT ") +
        FileColumns() +
        " FROM files "
        "WHERE deleted=0 "
        "AND parse_state IN ('pending', 'changed', 'failed') "
        "ORDER BY updated_at LIMIT ?;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(statement.Get(), 1, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        records.push_back(ReadFile(statement.Get()));
    }

    return records;
}

StorageStatus FileRepository::SetParseState(
    const std::string& id,
    const std::string& parseState,
    const std::string& actor
)
{
    if (parseState.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "解析状态不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE files SET parse_state=?, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND deleted=0;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备文件解析状态更新"
            );
        }

        BindText(statement.Get(), 1, parseState);
        BindText(statement.Get(), 2, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法更新文件解析状态"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "文件不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "file",
            "parse_state",
            "file",
            id,
            "{\"state\":\"" + parseState + "\"}"
        );
    });
}

StorageStatus FileRepository::MarkMissing(
    const std::string& id,
    const std::string& actor
)
{
    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        const auto captureStatus =
            CaptureFileChange(
                handle,
                id,
                std::string(),
                "missing"
            );

        if (!captureStatus.success)
        {
            return captureStatus;
        }

        Statement statement(
            handle,
            "UPDATE files SET parse_state='missing', "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND deleted=0;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备文件缺失标记"
            );
        }

        BindText(statement.Get(), 1, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法标记缺失文件"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "文件不存在"
            );
        }

        const auto invalidateStatus =
            InvalidateFileDerivedData(
                handle,
                id
            );

        if (!invalidateStatus.success)
        {
            return invalidateStatus;
        }

        return AuditRepository(database_).Append(
            actor,
            "file",
            "missing",
            "file",
            id,
            "{}"
        );
    });
}

WorkspaceService& WorkspaceService::Instance()
{
    static WorkspaceService instance;
    return instance;
}

WorkspaceService::WorkspaceService()
    : database_(),
      dataSources_(database_),
      files_(database_),
      objects_(database_),
      evidence_(database_),
      audit_(database_),
      initialized_(false)
{
}

WorkspaceService::~WorkspaceService()
{
    Shutdown();
}

StorageStatus WorkspaceService::InitializeInternal(
    const std::string& workspaceDirectory,
    const std::string& key,
    bool createIfMissing,
    bool createDirectories
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (workspaceDirectory.empty())
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "工作区目录不能为空"
        );
    }

    std::filesystem::path directory;
    std::filesystem::path databasePath;

    try
    {
        directory =
            std::filesystem::absolute(
                std::filesystem::u8path(workspaceDirectory)
            ).lexically_normal();

        databasePath = directory / "continuum.db";

        if (!createIfMissing)
        {
            std::error_code error;

            if (!std::filesystem::is_directory(directory, error) ||
                error)
            {
                return StorageStatus::Error(
                    SQLITE_CANTOPEN,
                    "工作区目录不存在或不可访问"
                );
            }

            error.clear();

            if (!std::filesystem::is_regular_file(
                    databasePath,
                    error
                ) ||
                error)
            {
                return StorageStatus::Error(
                    SQLITE_CANTOPEN,
                    "所选目录不是有效工作区：缺少 continuum.db"
                );
            }
        }

        if (createDirectories)
        {
            std::filesystem::create_directories(directory);
            std::filesystem::create_directories(directory / "content");
            std::filesystem::create_directories(directory / "index");
            std::filesystem::create_directories(directory / "backups");
            std::filesystem::create_directories(directory / "logs");
        }
    }
    catch (const std::exception& exception)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            std::string("无法访问工作区目录: ") +
                exception.what()
        );
    }

    if (initialized_ &&
        workspaceDirectory_ == directory.u8string() &&
        database_.IsOpen())
    {
        return StorageStatus::Ok();
    }

    Shutdown();

    const auto status = database_.Open(
        databasePath.u8string(),
        key,
        createIfMissing
    );

    if (!status.success)
    {
        workspaceDirectory_.clear();
        databasePath_.clear();
        initialized_ = false;
        return status;
    }

    workspaceDirectory_ = directory.u8string();
    databasePath_ = databasePath.u8string();
    initialized_ = true;
    return StorageStatus::Ok();
}

StorageStatus WorkspaceService::Initialize(
    const std::string& workspaceDirectory,
    const std::string& key
)
{
    return InitializeInternal(
        workspaceDirectory,
        key,
        true,
        true
    );
}

StorageStatus WorkspaceService::CreateWorkspace(
    const std::string& workspaceDirectory,
    bool encrypted
)
{
    if (workspaceDirectory.empty())
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "工作区目录不能为空"
        );
    }

    std::filesystem::path directory;

    try
    {
        directory =
            std::filesystem::absolute(
                std::filesystem::u8path(workspaceDirectory)
            ).lexically_normal();

        std::error_code error;
        const auto databasePath = directory / "continuum.db";

        if (std::filesystem::exists(databasePath, error) &&
            !error)
        {
            return StorageStatus::Error(
                SQLITE_CONSTRAINT,
                "目标目录已经包含工作区数据库"
            );
        }

        if (error)
        {
            return StorageStatus::Error(
                SQLITE_IOERR,
                "无法检查目标工作区目录: " +
                    error.message()
            );
        }

        std::filesystem::create_directories(directory);
    }
    catch (const std::exception& exception)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            std::string("无法创建工作区目录: ") +
                exception.what()
        );
    }

    std::string key;
    bool keyCreated = false;

    if (encrypted)
    {
        if (!DatabaseSecurity::IsSqlCipherAvailable())
        {
            return StorageStatus::Error(
                SQLITE_AUTH,
                "当前 SQLite 未启用 SQLCipher，不能创建加密工作区"
            );
        }

        SecureKeyStore keyStore(directory.u8string());

        auto keyResult =
            keyStore.LoadOrCreateDatabaseKey();

        if (!keyResult.status.success)
        {
            return keyResult.status;
        }

        key = std::move(keyResult.key);
        keyCreated = keyResult.created;
    }

    auto status = InitializeInternal(
        directory.u8string(),
        key,
        true,
        true
    );

    std::fill(key.begin(), key.end(), '\0');
    key.clear();

    if (!status.success && keyCreated)
    {
        SecureKeyStore(
            directory.u8string()
        ).RemoveDatabaseKey();
    }

    return status;
}

StorageStatus WorkspaceService::OpenWorkspace(
    const std::string& workspaceDirectory
)
{
    if (workspaceDirectory.empty())
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "工作区目录不能为空"
        );
    }

    std::filesystem::path directory;

    try
    {
        directory =
            std::filesystem::absolute(
                std::filesystem::u8path(workspaceDirectory)
            ).lexically_normal();
    }
    catch (const std::exception& exception)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            std::string("工作区路径无效: ") +
                exception.what()
        );
    }

    std::string key;
    SecureKeyStore keyStore(directory.u8string());

    if (keyStore.Exists())
    {
        if (!DatabaseSecurity::IsSqlCipherAvailable())
        {
            return StorageStatus::Error(
                SQLITE_AUTH,
                "该工作区包含数据库密钥，但当前 SQLite 不支持 SQLCipher"
            );
        }

        const auto keyStatus =
            keyStore.LoadDatabaseKey(key);

        if (!keyStatus.success)
        {
            return StorageStatus::Error(
                keyStatus.code,
                "无法解锁工作区: " +
                    keyStatus.message
            );
        }
    }

    auto status = InitializeInternal(
        directory.u8string(),
        key,
        false,
        false
    );

    std::fill(key.begin(), key.end(), '\0');
    key.clear();

    if (!status.success)
    {
        return status;
    }

    const auto integrity = database_.CheckIntegrity();

    if (!integrity.success)
    {
        Shutdown();

        return StorageStatus::Error(
            integrity.code,
            "工作区完整性检查失败: " +
                integrity.message
        );
    }

    return StorageStatus::Ok();
}

StorageStatus WorkspaceService::InitializeDefault()
{
    // CONTINUUM_SECURE_DEFAULT_WORKSPACE
    const std::string directory =
        ResolveDefaultDirectory();

    std::string cipherVersion;

    const bool cipherAvailable =
        DatabaseSecurity::IsSqlCipherAvailable(
            &cipherVersion
        );

    if (!cipherAvailable)
    {
        if (
            DatabaseSecurity::
                EncryptionRequiredByEnvironment()
        )
        {
            return StorageStatus::Error(
                SQLITE_AUTH,
                "CONTINUUM_REQUIRE_ENCRYPTION 已启用，"
                "但当前 SQLite 不支持 SQLCipher"
            );
        }

        /*
         * 普通 SQLite 模式下不生成、也不传入伪密钥，
         * 避免让用户误以为数据库已经加密。
         */
        return Initialize(directory);
    }

    SecureKeyStore keyStore(directory);

    auto keyResult =
        keyStore.LoadOrCreateDatabaseKey();

    if (!keyResult.status.success)
    {
        return StorageStatus::Error(
            keyResult.status.code,
            "无法加载工作区数据库密钥: " +
                keyResult.status.message
        );
    }

    auto status = Initialize(
        directory,
        keyResult.key
    );

    std::fill(
        keyResult.key.begin(),
        keyResult.key.end(),
        '\0'
    );
    keyResult.key.clear();

    return status;
}

std::string WorkspaceService::ResolveDefaultDirectory() const
{
    const std::string explicitDirectory =
        EnvironmentValue("CONTINUUM_WORKSPACE_DIR");

    if (!explicitDirectory.empty())
    {
        return explicitDirectory;
    }

#if defined(_WIN32)
    const std::string localAppData =
        EnvironmentValue("LOCALAPPDATA");

    if (!localAppData.empty())
    {
        return (
            std::filesystem::u8path(localAppData) /
            "Continuum" /
            "workspace"
        ).u8string();
    }
#else
    const std::string home = EnvironmentValue("HOME");

    if (!home.empty())
    {
        return (
            std::filesystem::u8path(home) /
            ".local" /
            "share" /
            "continuum" /
            "workspace"
        ).u8string();
    }
#endif

    return (
        std::filesystem::current_path() /
        "data" /
        "workspace"
    ).u8string();
}

void WorkspaceService::Shutdown()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    database_.Close();
    workspaceDirectory_.clear();
    databasePath_.clear();
    initialized_ = false;
}

bool WorkspaceService::IsInitialized() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return initialized_ && database_.IsOpen();
}

const std::string& WorkspaceService::WorkspaceDirectory() const
{
    return workspaceDirectory_;
}

const std::string& WorkspaceService::DatabasePath() const
{
    return databasePath_;
}

Database& WorkspaceService::GetDatabase()
{
    return database_;
}

DataSourceRepository& WorkspaceService::DataSources()
{
    return dataSources_;
}

FileRepository& WorkspaceService::Files()
{
    return files_;
}

ObjectRepository& WorkspaceService::Objects()
{
    return objects_;
}

EvidenceRepository& WorkspaceService::Evidence()
{
    return evidence_;
}

AuditRepository& WorkspaceService::Audit()
{
    return audit_;
}

}
