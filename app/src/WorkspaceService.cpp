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

StorageStatus WorkspaceService::Initialize(
    const std::string& workspaceDirectory,
    const std::string& key
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

    if (initialized_ &&
        workspaceDirectory_ == workspaceDirectory &&
        database_.IsOpen())
    {
        return StorageStatus::Ok();
    }

    Shutdown();

    try
    {
        const std::filesystem::path directory =
            std::filesystem::u8path(workspaceDirectory);

        std::filesystem::create_directories(directory);
        std::filesystem::create_directories(directory / "content");
        std::filesystem::create_directories(directory / "index");
        std::filesystem::create_directories(directory / "backups");
        std::filesystem::create_directories(directory / "logs");

        const std::filesystem::path databasePath =
            directory / "continuum.db";

        workspaceDirectory_ = directory.u8string();
        databasePath_ = databasePath.u8string();
    }
    catch (const std::exception& exception)
    {
        workspaceDirectory_.clear();
        databasePath_.clear();

        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            std::string("无法创建工作区目录: ") +
                exception.what()
        );
    }

    auto status = database_.Open(databasePath_, key);

    if (!status.success)
    {
        workspaceDirectory_.clear();
        databasePath_.clear();
        return status;
    }

    status = database_.CheckIntegrity();

    if (!status.success)
    {
        database_.Close();
        workspaceDirectory_.clear();
        databasePath_.clear();
        return status;
    }

    initialized_ = true;

    status = EnsureSeedData();

    if (!status.success)
    {
        Shutdown();
        return status;
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

    const auto keyResult =
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

    std::string temporaryKey =
        keyResult.key;

    std::fill(
        temporaryKey.begin(),
        temporaryKey.end(),
        '\0'
    );

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

StorageStatus WorkspaceService::EnsureSeedData()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (!initialized_ || !database_.IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区尚未初始化"
        );
    }

    const auto sources = dataSources_.List(true, 1);

    if (!sources.empty())
    {
        return StorageStatus::Ok();
    }

    DataSourceRecord source;
    source.id = "SRC-001";
    source.name = "示例项目文档";
    source.sourceType = "local_folder";
    source.rootPath = (
        std::filesystem::u8path(workspaceDirectory_) /
        "sample-documents"
    ).u8string();
    source.enabled = true;

    auto status = dataSources_.Save(source, "bootstrap");

    if (!status.success)
    {
        return status;
    }

    FileRecord file;
    file.id = "FILE-0001";
    file.sourceId = source.id;
    file.relativePath = "README.md";
    file.displayName = "README.md";
    file.mediaType = "text/markdown";
    file.sizeBytes = 0;
    file.parseState = "pending";

    status = files_.Save(file, "bootstrap");

    if (!status.success)
    {
        return status;
    }

    DomainObjectRecord object;
    object.id = "F-0001";
    object.objectType = "fact";
    object.title = "工作区初始化完成";
    object.description =
        "这是首次启动时创建的示例事实，可在接入真实数据后删除。";
    object.status = "active";
    object.priority = "normal";
    object.owner = "system";
    object.timePrecision = "exact";

    status = objects_.Save(object, "bootstrap");

    if (!status.success)
    {
        return status;
    }

    EvidenceRecord evidence;
    evidence.id = "E-0001";
    evidence.sourceId = source.id;
    evidence.fileId = file.id;
    evidence.title = "工作区初始化记录";
    evidence.quote = "Continuum workspace initialized.";
    evidence.anchor = "README.md";
    evidence.reviewState = "verified";

    status = evidence_.Save(evidence, "bootstrap");

    if (!status.success)
    {
        return status;
    }

    EvidenceLinkRecord link;
    link.objectId = object.id;
    link.evidenceId = evidence.id;
    link.role = "support";
    link.note = "初始化事实的系统证据";

    status = objects_.LinkEvidence(link, "bootstrap");

    if (!status.success)
    {
        return status;
    }

    return audit_.Append(
        "bootstrap",
        "workspace",
        "initialize",
        "workspace",
        "default",
        "{}"
    );
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
