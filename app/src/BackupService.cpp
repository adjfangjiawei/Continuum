#include "BackupService.h"

#include "FileScanner.h"
#include "SecurityService.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <thread>
#include <utility>

#include <sqlite3.h>

namespace continuum
{
namespace
{

std::string Trim(const std::string& value)
{
    const auto first = std::find_if_not(
        value.begin(),
        value.end(),
        [](unsigned char character) {
            return std::isspace(character) != 0;
        }
    );

    if (first == value.end())
    {
        return std::string();
    }

    const auto last = std::find_if_not(
        value.rbegin(),
        value.rend(),
        [](unsigned char character) {
            return std::isspace(character) != 0;
        }
    ).base();

    return std::string(first, last);
}

std::string EscapeManifestValue(
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
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '=':
            result += "\\=";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
}

std::string UnescapeManifestValue(
    const std::string& value
)
{
    std::string result;
    bool escaped = false;

    for (const char character : value)
    {
        if (!escaped)
        {
            if (character == '\\')
            {
                escaped = true;
            }
            else
            {
                result += character;
            }

            continue;
        }

        switch (character)
        {
        case 'n':
            result += '\n';
            break;
        case 'r':
            result += '\r';
            break;
        case '=':
            result += '=';
            break;
        case '\\':
            result += '\\';
            break;
        default:
            result += character;
            break;
        }

        escaped = false;
    }

    if (escaped)
    {
        result += '\\';
    }

    return result;
}

std::size_t FindManifestSeparator(
    const std::string& line
)
{
    bool escaped = false;

    for (std::size_t index = 0;
         index < line.size();
         ++index)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (line[index] == '\\')
        {
            escaped = true;
            continue;
        }

        if (line[index] == '=')
        {
            return index;
        }
    }

    return std::string::npos;
}

StorageStatus FileError(
    int code,
    const std::string& context,
    const std::error_code& error
)
{
    return StorageStatus::Error(
        code,
        context + ": " + error.message()
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

StorageStatus SqliteMessage(
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

StorageStatus ApplyDatabaseKey(
    sqlite3* database,
    const std::string& key
)
{
    if (database == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "备份数据库句柄为空"
        );
    }

    if (key.empty())
    {
        return StorageStatus::Ok();
    }

    char* quotedKey = sqlite3_mprintf(
        "%Q",
        key.c_str()
    );

    if (quotedKey == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_NOMEM,
            "无法构造数据库密钥"
        );
    }

    const std::string sql =
        "PRAGMA key = " +
        std::string(quotedKey) +
        ";";

    sqlite3_free(quotedKey);

    char* error = nullptr;

    const int result = sqlite3_exec(
        database,
        sql.c_str(),
        nullptr,
        nullptr,
        &error
    );

    if (result != SQLITE_OK)
    {
        std::string message =
            "无法为备份数据库应用密钥";

        if (error != nullptr)
        {
            message += ": ";
            message += error;
            sqlite3_free(error);
        }

        return StorageStatus::Error(
            result,
            message
        );
    }

    if (error != nullptr)
    {
        sqlite3_free(error);
    }

    return StorageStatus::Ok();
}

StorageStatus LoadExistingWorkspaceKey(
    const WorkspaceService& workspace,
    std::string& key
)
{
    key.clear();

    if (!DatabaseSecurity::IsSqlCipherAvailable())
    {
        return StorageStatus::Ok();
    }

    SecureKeyStore keyStore(
        workspace.WorkspaceDirectory()
    );

    if (!keyStore.Exists())
    {
        return StorageStatus::Ok();
    }

    const auto status =
        keyStore.LoadDatabaseKey(key);

    if (!status.success)
    {
        return StorageStatus::Error(
            status.code,
            "无法加载现有工作区数据库密钥: " +
                status.message
        );
    }

    if (key.empty())
    {
        return StorageStatus::Error(
            SQLITE_AUTH,
            "工作区数据库密钥为空"
        );
    }

    return StorageStatus::Ok();
}

void RemoveDatabaseSidecars(
    const std::filesystem::path& databasePath
)
{
    std::error_code error;

    std::filesystem::remove(
        databasePath.u8string() + "-wal",
        error
    );

    error.clear();

    std::filesystem::remove(
        databasePath.u8string() + "-shm",
        error
    );

}

/*
 * RestoreBackup 会在替换数据库前停止后台线程。
 *
 * 如果恢复流程提前失败，但工作区已经成功重新打开，则函数退出
 * 时应恢复原本正在运行的后台服务。否则应用表面上仍可查询数据，
 * 但扫描、解析和全文索引会永久停止，直到应用重启。
 */
class RestoreWorkerGuard final
{
public:
    RestoreWorkerGuard(
        WorkspaceService& workspace,
        bool restartRequired
    )
        : workspace_(workspace),
          restartRequired_(restartRequired)
    {
    }

    ~RestoreWorkerGuard()
    {
        if (!restartRequired_ ||
            !workspace_.IsInitialized() ||
            BackgroundWorker::Instance().IsRunning())
        {
            return;
        }

        /*
         * 析构函数不能返回错误。显式成功路径仍会检查 Start()
         * 的返回值；这里负责恢复失败出口的尽力重启。
         */
        BackgroundWorker::Instance().Start(
            workspace_
        );
    }

    RestoreWorkerGuard(
        const RestoreWorkerGuard&
    ) = delete;

    RestoreWorkerGuard& operator=(
        const RestoreWorkerGuard&
    ) = delete;

private:
    WorkspaceService& workspace_;
    bool restartRequired_;
};

}

BackupService::BackupService(
    WorkspaceService& workspace
)
    : workspace_(workspace)
{
}

std::string BackupService::CurrentTimestamp()
{
    const auto now =
        std::chrono::system_clock::now();

    const auto value =
        std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &value);
#else
    localtime_r(&value, &localTime);
#endif

    std::ostringstream stream;

    stream << std::put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S"
    );

    return stream.str();
}

std::string BackupService::FilesystemTimestamp()
{
    const auto now =
        std::chrono::system_clock::now();

    const auto value =
        std::chrono::system_clock::to_time_t(now);

    const auto milliseconds =
        std::chrono::duration_cast<
            std::chrono::milliseconds
        >(
            now.time_since_epoch()
        ).count() % 1000;

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &value);
#else
    localtime_r(&value, &localTime);
#endif

    std::ostringstream stream;

    stream
        << std::put_time(
            &localTime,
            "%Y%m%d-%H%M%S"
        )
        << "-"
        << std::setw(3)
        << std::setfill('0')
        << milliseconds;

    return stream.str();
}

std::string BackupService::SanitizeLabel(
    const std::string& value
)
{
    std::string result;

    for (const unsigned char character : value)
    {
        if (std::isalnum(character) != 0 ||
            character == '-' ||
            character == '_')
        {
            result += static_cast<char>(character);
        }
        else if (std::isspace(character) != 0)
        {
            result += '-';
        }
    }

    while (!result.empty() &&
           result.front() == '-')
    {
        result.erase(result.begin());
    }

    while (!result.empty() &&
           result.back() == '-')
    {
        result.pop_back();
    }

    if (result.size() > 48)
    {
        result.resize(48);
    }

    return result;
}

std::string BackupService::BackupRoot() const
{
    if (workspace_.WorkspaceDirectory().empty())
    {
        return std::string();
    }

    return (
        std::filesystem::u8path(
            workspace_.WorkspaceDirectory()
        ) /
        "backups"
    ).u8string();
}

StorageStatus BackupService::CopyDatabaseSnapshot(
    Database& source,
    const std::string& destination,
    const BackupCreateOptions& options,
    const std::string& key
)
{
    if (!source.IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区数据库尚未打开"
        );
    }

    sqlite3* destinationHandle = nullptr;

    const int openResult = sqlite3_open_v2(
        destination.c_str(),
        &destinationHandle,
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_CREATE |
        SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (openResult != SQLITE_OK)
    {
        const auto status = SqliteMessage(
            destinationHandle,
            openResult,
            "无法创建备份数据库"
        );

        if (destinationHandle != nullptr)
        {
            sqlite3_close_v2(destinationHandle);
        }

        return status;
    }

    const auto keyStatus =
        ApplyDatabaseKey(
            destinationHandle,
            key
        );

    if (!keyStatus.success)
    {
        sqlite3_close_v2(destinationHandle);
        return keyStatus;
    }

    sqlite3_busy_timeout(
        destinationHandle,
        5000
    );

    sqlite3_backup* backup = nullptr;

    {
        std::lock_guard<std::recursive_mutex> lock(
            source.Mutex()
        );

        backup = sqlite3_backup_init(
            destinationHandle,
            "main",
            source.Handle(),
            "main"
        );
    }

    if (backup == nullptr)
    {
        const auto status = SqliteMessage(
            destinationHandle,
            sqlite3_errcode(destinationHandle),
            "无法初始化 SQLite 在线备份"
        );

        sqlite3_close_v2(destinationHandle);
        return status;
    }

    int result = SQLITE_OK;
    int busyRetries = 0;

    const int pagesPerStep =
        std::max(1, options.pagesPerStep);

    const int retryLimit =
        std::max(0, options.busyRetryCount);

    const int sleepMilliseconds =
        std::max(1, options.busySleepMilliseconds);

    do
    {
        {
            std::lock_guard<std::recursive_mutex> lock(
                source.Mutex()
            );

            result = sqlite3_backup_step(
                backup,
                pagesPerStep
            );
        }

        if (result == SQLITE_BUSY ||
            result == SQLITE_LOCKED)
        {
            if (++busyRetries > retryLimit)
            {
                break;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(
                    sleepMilliseconds
                )
            );
        }
        else
        {
            busyRetries = 0;
        }
    }
    while (result == SQLITE_OK ||
           result == SQLITE_BUSY ||
           result == SQLITE_LOCKED);

    const int finishResult =
        sqlite3_backup_finish(backup);

    if (result == SQLITE_DONE)
    {
        result = finishResult;
    }

    if (result == SQLITE_OK)
    {
        char* error = nullptr;

        const int checkpointResult = sqlite3_exec(
            destinationHandle,
            "PRAGMA wal_checkpoint(TRUNCATE);",
            nullptr,
            nullptr,
            &error
        );

        if (error != nullptr)
        {
            sqlite3_free(error);
        }

        if (checkpointResult != SQLITE_OK)
        {
            result = checkpointResult;
        }
    }

    StorageStatus status = StorageStatus::Ok();

    if (result != SQLITE_OK)
    {
        status = SqliteMessage(
            destinationHandle,
            result,
            "SQLite 在线备份失败"
        );
    }

    const int closeResult =
        sqlite3_close_v2(destinationHandle);

    if (status.success &&
        closeResult != SQLITE_OK)
    {
        status = StorageStatus::Error(
            closeResult,
            "无法关闭备份数据库"
        );
    }

    return status;
}

StorageStatus BackupService::ValidateDatabaseFile(
    const std::string& databaseFile,
    const std::string& key
)
{
    sqlite3* handle = nullptr;

    const int openResult = sqlite3_open_v2(
        databaseFile.c_str(),
        &handle,
        SQLITE_OPEN_READONLY |
        SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (openResult != SQLITE_OK)
    {
        const auto status = SqliteMessage(
            handle,
            openResult,
            "无法打开备份数据库"
        );

        if (handle != nullptr)
        {
            sqlite3_close_v2(handle);
        }

        return status;
    }

    const auto keyStatus =
        ApplyDatabaseKey(
            handle,
            key
        );

    if (!keyStatus.success)
    {
        sqlite3_close_v2(handle);
        return keyStatus;
    }

    sqlite3_stmt* statement = nullptr;

    int result = sqlite3_prepare_v2(
        handle,
        "PRAGMA quick_check;",
        -1,
        &statement,
        nullptr
    );

    if (result != SQLITE_OK)
    {
        const auto status = SqliteMessage(
            handle,
            result,
            "无法准备备份完整性检查"
        );

        sqlite3_close_v2(handle);
        return status;
    }

    result = sqlite3_step(statement);

    if (result != SQLITE_ROW)
    {
        const auto status = SqliteMessage(
            handle,
            result,
            "无法执行备份完整性检查"
        );

        sqlite3_finalize(statement);
        sqlite3_close_v2(handle);
        return status;
    }

    const std::string check =
        ColumnText(statement, 0);

    sqlite3_finalize(statement);

    if (check != "ok")
    {
        sqlite3_close_v2(handle);

        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "备份数据库完整性检查返回: " +
                check
        );
    }

    sqlite3_close_v2(handle);

    return StorageStatus::Ok();
}

StorageStatus BackupService::WriteManifest(
    const BackupRecord& record
)
{
    const auto manifestPath =
        std::filesystem::u8path(record.directory) /
        "manifest.txt";

    const auto temporaryPath =
        manifestPath.u8string() + ".tmp";

    std::ofstream stream(
        std::filesystem::u8path(temporaryPath),
        std::ios::binary |
        std::ios::trunc
    );

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法创建备份清单"
        );
    }

    stream
        << "format=continuum-backup-v1\n"
        << "id="
        << EscapeManifestValue(record.id)
        << "\n"
        << "created_at="
        << EscapeManifestValue(record.createdAt)
        << "\n"
        << "database_file=continuum.db\n"
        << "database_sha256="
        << record.databaseSha256
        << "\n"
        << "database_size="
        << record.databaseSize
        << "\n"
        << "schema_version="
        << record.schemaVersion
        << "\n";

    stream.flush();

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_IOERR_WRITE,
            "写入备份清单失败"
        );
    }

    stream.close();

    std::error_code error;

    std::filesystem::rename(
        std::filesystem::u8path(temporaryPath),
        manifestPath,
        error
    );

    if (error)
    {
        std::filesystem::remove(
            std::filesystem::u8path(temporaryPath),
            error
        );

        return StorageStatus::Error(
            SQLITE_IOERR,
            "提交备份清单失败: " +
                error.message()
        );
    }

    return StorageStatus::Ok();
}

StorageStatus BackupService::ReadManifest(
    const std::string& backupDirectory,
    BackupRecord& record
)
{
    const auto directory =
        std::filesystem::u8path(backupDirectory);

    const auto manifestPath =
        directory / "manifest.txt";

    std::ifstream stream(
        manifestPath,
        std::ios::binary
    );

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "备份清单不存在或无法读取"
        );
    }

    std::map<std::string, std::string> values;
    std::string line;

    while (std::getline(stream, line))
    {
        const auto separator =
            FindManifestSeparator(line);

        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string key =
            Trim(line.substr(0, separator));

        const std::string value =
            UnescapeManifestValue(
                line.substr(separator + 1)
            );

        values[key] = value;
    }

    if (values["format"] !=
        "continuum-backup-v1")
    {
        return StorageStatus::Error(
            SQLITE_FORMAT,
            "不支持的备份清单格式"
        );
    }

    if (values["id"].empty() ||
        values["database_file"].empty() ||
        values["database_sha256"].empty())
    {
        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "备份清单缺少必要字段"
        );
    }

    record = BackupRecord{};
    record.id = values["id"];
    record.directory = directory.u8string();
    record.databaseFile = (
        directory /
        std::filesystem::u8path(
            values["database_file"]
        )
    ).u8string();
    record.createdAt = values["created_at"];
    record.databaseSha256 =
        values["database_sha256"];
    record.schemaVersion =
        values["schema_version"].empty()
            ? 1
            : std::stoi(values["schema_version"]);

    try
    {
        record.databaseSize =
            static_cast<std::uintmax_t>(
                std::stoull(
                    values["database_size"]
                )
            );


    }
    catch (const std::exception&)
    {
        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "备份清单数值字段无效"
        );
    }

    return StorageStatus::Ok();
}

BackupResult BackupService::CreateBackup(
    const BackupCreateOptions& options
)
{
    BackupResult result;

    if (!workspace_.IsInitialized())
    {
        result.status = StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区尚未初始化"
        );

        return result;
    }

    std::string backupKey;

    const auto keyStatus =
        LoadExistingWorkspaceKey(
            workspace_,
            backupKey
        );

    if (!keyStatus.success)
    {
        result.status = keyStatus;
        return result;
    }

    const auto integrity =
        workspace_.GetDatabase().CheckIntegrity();

    if (!integrity.success)
    {
        result.status = StorageStatus::Error(
            integrity.code,
            "创建备份前数据库检查失败: " +
                integrity.message
        );

        return result;
    }

    const std::filesystem::path root =
        std::filesystem::u8path(BackupRoot());

    std::error_code error;

    std::filesystem::create_directories(
        root,
        error
    );

    if (error)
    {
        result.status = FileError(
            SQLITE_CANTOPEN,
            "无法创建备份根目录",
            error
        );

        return result;
    }

    std::string identifier =
        "backup-" + FilesystemTimestamp();

    const std::string label =
        SanitizeLabel(options.label);

    if (!label.empty())
    {
        identifier += "-" + label;
    }

    std::filesystem::path finalDirectory =
        root / identifier;

    int collision = 1;

    while (std::filesystem::exists(
        finalDirectory,
        error
    ))
    {
        finalDirectory =
            root /
            (
                identifier +
                "-" +
                std::to_string(collision++)
            );
    }

    const std::filesystem::path temporaryDirectory =
        std::filesystem::u8path(
            finalDirectory.u8string() +
            ".partial"
        );

    std::filesystem::remove_all(
        temporaryDirectory,
        error
    );

    error.clear();

    std::filesystem::create_directories(
        temporaryDirectory,
        error
    );

    if (error)
    {
        result.status = FileError(
            SQLITE_CANTOPEN,
            "无法创建临时备份目录",
            error
        );

        return result;
    }

    const std::filesystem::path databaseFile =
        temporaryDirectory / "continuum.db";

    const auto copyStatus =
        CopyDatabaseSnapshot(
            workspace_.GetDatabase(),
            databaseFile.u8string(),
            options,
            backupKey
        );

    if (!copyStatus.success)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = copyStatus;
        return result;
    }

    BackupRecord record;
    record.id = finalDirectory.filename().u8string();
    record.directory = temporaryDirectory.u8string();
    record.databaseFile = databaseFile.u8string();
    record.createdAt = CurrentTimestamp();

    const auto validationStatus =
        ValidateDatabaseFile(
            record.databaseFile,
            backupKey
        );

    if (!validationStatus.success)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = validationStatus;
        return result;
    }

    const auto hashStatus =
        ContentHasher::Sha256File(
            record.databaseFile,
            record.databaseSha256
        );

    if (!hashStatus.success)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = hashStatus;
        return result;
    }

    record.databaseSize =
        std::filesystem::file_size(
            databaseFile,
            error
        );

    if (error)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = FileError(
            SQLITE_IOERR,
            "无法读取备份数据库大小",
            error
        );

        return result;
    }

    const auto manifestStatus =
        WriteManifest(record);

    if (!manifestStatus.success)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = manifestStatus;
        return result;
    }

    std::filesystem::rename(
        temporaryDirectory,
        finalDirectory,
        error
    );

    if (error)
    {
        std::filesystem::remove_all(
            temporaryDirectory,
            error
        );

        result.status = FileError(
            SQLITE_IOERR,
            "无法提交备份目录",
            error
        );

        return result;
    }

    record.directory = finalDirectory.u8string();
    record.databaseFile = (
        finalDirectory / "continuum.db"
    ).u8string();
    record.valid = true;
    record.validationMessage = "ok";

    AuditRepository(
        workspace_.GetDatabase()
    ).Append(
        "backup-service",
        "backup",
        "created",
        "workspace",
        "default",
        "{\"backup_id\":\"" +
            record.id +
            "\",\"sha256\":\"" +
            record.databaseSha256 +
            "\"}"
    );

    if (options.retainLatest > 0)
    {
        const auto pruneStatus =
            PruneBackups(options.retainLatest);

        if (!pruneStatus.success)
        {
            result.status = pruneStatus;
            result.backup = record;
            return result;
        }
    }

    result.status = StorageStatus::Ok();
    result.backup = record;
    return result;
}

StorageStatus BackupService::ValidateBackup(
    const std::string& backupDirectory,
    BackupRecord* output
) const
{
    BackupRecord record;
    std::string backupKey;

    const auto keyStatus =
        LoadExistingWorkspaceKey(
            workspace_,
            backupKey
        );

    if (!keyStatus.success)
    {
        if (output != nullptr)
        {
            record.directory = backupDirectory;
            record.valid = false;
            record.validationMessage =
                keyStatus.message;
            *output = record;
        }

        return keyStatus;
    }

    auto status = ReadManifest(
        backupDirectory,
        record
    );

    if (!status.success)
    {
        if (output != nullptr)
        {
            record.directory = backupDirectory;
            record.valid = false;
            record.validationMessage =
                status.message;
            *output = record;
        }

        return status;
    }

    std::error_code error;

    if (!std::filesystem::exists(
            std::filesystem::u8path(
                record.databaseFile
            ),
            error
        ) ||
        error)
    {
        status = StorageStatus::Error(
            SQLITE_CANTOPEN,
            "备份数据库文件不存在"
        );
    }
    else
    {
        const auto actualSize =
            std::filesystem::file_size(
                std::filesystem::u8path(
                    record.databaseFile
                ),
                error
            );

        if (error)
        {
            status = FileError(
                SQLITE_IOERR,
                "无法读取备份文件大小",
                error
            );
        }
        else if (actualSize !=
                 record.databaseSize)
        {
            status = StorageStatus::Error(
                SQLITE_CORRUPT,
                "备份数据库大小与清单不一致"
            );
        }
    }

    if (status.success)
    {
        std::string actualHash;

        status = ContentHasher::Sha256File(
            record.databaseFile,
            actualHash
        );

        if (status.success &&
            actualHash !=
                record.databaseSha256)
        {
            status = StorageStatus::Error(
                SQLITE_CORRUPT,
                "备份数据库 SHA-256 校验失败"
            );
        }
    }

    if (status.success)
    {
        status = ValidateDatabaseFile(
            record.databaseFile,
            backupKey
        );
    }

    record.valid = status.success;
    record.validationMessage =
        status.success ? "ok" : status.message;

    if (output != nullptr)
    {
        *output = record;
    }

    return status;
}

std::uint64_t BackupService::CountBackups() const
{
    const std::filesystem::path root =
        std::filesystem::u8path(BackupRoot());

    std::error_code error;

    if (!std::filesystem::exists(root, error) ||
        error)
    {
        return 0;
    }

    std::uint64_t count = 0;

    for (std::filesystem::directory_iterator iterator(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             error
         ), end;
         iterator != end;
         iterator.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }

        if (!iterator->is_directory(error) || error)
        {
            error.clear();
            continue;
        }

        const std::string name =
            iterator->path().filename().u8string();

        /*
         * 只统计 CreateBackup() 已正式提交的目录。
         * .partial 和 rollback 文件均不会满足该前缀。
         */
        if (name.rfind("backup-", 0) == 0 &&
            (
                name.size() < 8 ||
                name.substr(name.size() - 8) !=
                    ".partial"
            ))
        {
            ++count;
        }
    }

    return count;
}

std::vector<BackupRecord>
BackupService::ListBackups() const
{
    std::vector<BackupRecord> records;

    const std::filesystem::path root =
        std::filesystem::u8path(BackupRoot());

    std::error_code error;

    if (!std::filesystem::exists(root, error) ||
        error)
    {
        return records;
    }

    for (std::filesystem::directory_iterator iterator(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             error
         ), end;
         iterator != end;
         iterator.increment(error))
    {
        if (error)
        {
            error.clear();
            continue;
        }

        if (!iterator->is_directory(error))
        {
            continue;
        }

        const std::string name =
            iterator->path().filename().u8string();

        if (name.size() >= 8 &&
            name.substr(name.size() - 8) ==
                ".partial")
        {
            continue;
        }

        if (name.rfind(
                "rollback-",
                0
            ) == 0)
        {
            continue;
        }

        BackupRecord record;

        ValidateBackup(
            iterator->path().u8string(),
            &record
        );

        records.push_back(std::move(record));
    }

    std::sort(
        records.begin(),
        records.end(),
        [](const BackupRecord& first,
           const BackupRecord& second) {
            return first.id > second.id;
        }
    );

    return records;
}

StorageStatus BackupService::PruneBackups(
    int retainLatest
)
{
    if (retainLatest < 0)
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "备份保留数量不能为负数"
        );
    }

    const std::filesystem::path root =
        std::filesystem::u8path(BackupRoot());

    std::error_code error;

    if (!std::filesystem::exists(root, error))
    {
        return error
            ? FileError(
                SQLITE_IOERR,
                "无法读取备份根目录",
                error
            )
            : StorageStatus::Ok();
    }

    std::vector<std::filesystem::path> directories;

    for (std::filesystem::directory_iterator iterator(
             root,
             std::filesystem::directory_options::
                 skip_permission_denied,
             error
         ), end;
         iterator != end;
         iterator.increment(error))
    {
        if (error)
        {
            return FileError(
                SQLITE_IOERR,
                "无法枚举备份目录",
                error
            );
        }

        if (!iterator->is_directory(error))
        {
            if (error)
            {
                return FileError(
                    SQLITE_IOERR,
                    "无法读取备份目录项目",
                    error
                );
            }

            continue;
        }

        const std::string name =
            iterator->path().filename().u8string();

        if (name.rfind("backup-", 0) != 0)
        {
            continue;
        }

        if (name.size() >= 8 &&
            name.substr(name.size() - 8) ==
                ".partial")
        {
            continue;
        }

        directories.push_back(iterator->path());
    }

    /*
     * 备份目录名以可排序时间戳开头，降序即由新到旧。
     */
    std::sort(
        directories.begin(),
        directories.end(),
        [](const std::filesystem::path& first,
           const std::filesystem::path& second) {
            return first.filename().u8string() >
                second.filename().u8string();
        }
    );

    const std::size_t firstToRemove =
        static_cast<std::size_t>(retainLatest);

    for (std::size_t index = firstToRemove;
         index < directories.size();
         ++index)
    {
        error.clear();

        std::filesystem::remove_all(
            directories[index],
            error
        );

        if (error)
        {
            return FileError(
                SQLITE_IOERR,
                "无法删除过期备份 " +
                    directories[index]
                        .filename()
                        .u8string(),
                error
            );
        }
    }

    return StorageStatus::Ok();
}

StorageStatus BackupService::RestoreBackup(
    const std::string& backupDirectory,
    const BackupRestoreOptions& options
)
{
    if (!workspace_.IsInitialized())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "工作区尚未初始化"
        );
    }

    BackupRecord record;

    auto status = ValidateBackup(
        backupDirectory,
        &record
    );

    if (!status.success)
    {
        return StorageStatus::Error(
            status.code,
            "恢复前备份检查失败: " +
                status.message
        );
    }

    std::string databaseKey;

    const auto keyStatus =
        LoadExistingWorkspaceKey(
            workspace_,
            databaseKey
        );

    if (!keyStatus.success)
    {
        return keyStatus;
    }

    const std::string workspaceDirectory =
        workspace_.WorkspaceDirectory();

    const std::filesystem::path databasePath =
        std::filesystem::u8path(
            workspace_.DatabasePath()
        );

    const std::filesystem::path restoreSource =
        std::filesystem::u8path(
            record.databaseFile
        );

    const std::filesystem::path restoreTemporary =
        std::filesystem::u8path(
            databasePath.u8string() +
            ".restore.tmp"
        );

    const std::filesystem::path rollbackFile =
        std::filesystem::u8path(
            databasePath.u8string() +
            ".rollback-" +
            FilesystemTimestamp()
        );

    BackgroundWorkerOptions workerOptions;
    const bool workerWasRunning =
        BackgroundWorker::Instance().IsRunning();

    BackgroundWorker::Instance().Stop();

    RestoreWorkerGuard workerGuard(
        workspace_,
        workerWasRunning &&
            options.restartBackgroundWorker
    );

    AuditRepository(
        workspace_.GetDatabase()
    ).Append(
        "backup-service",
        "backup",
        "restore_started",
        "workspace",
        "default",
        "{\"backup_id\":\"" +
            record.id +
            "\"}"
    );

    workspace_.Shutdown();

    std::error_code error;

    std::filesystem::remove(
        restoreTemporary,
        error
    );

    error.clear();

    if (options.createRollbackCopy &&
        std::filesystem::exists(
            databasePath,
            error
        ))
    {
        error.clear();

        std::filesystem::copy_file(
            databasePath,
            rollbackFile,
            std::filesystem::copy_options::
                overwrite_existing,
            error
        );

        if (error)
        {
            const auto reopen =
                workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

            if (workerWasRunning &&
                options.restartBackgroundWorker &&
                reopen.success)
            {
                BackgroundWorker::Instance().Start(
                    workspace_,
                    workerOptions
                );
            }

            return FileError(
                SQLITE_IOERR,
                "无法创建恢复回滚副本",
                error
            );
        }
    }

    std::filesystem::copy_file(
        restoreSource,
        restoreTemporary,
        std::filesystem::copy_options::
            overwrite_existing,
        error
    );

    if (error)
    {
        workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

        return FileError(
            SQLITE_IOERR,
            "无法复制待恢复数据库",
            error
        );
    }

    status = ValidateDatabaseFile(
        restoreTemporary.u8string(),
        databaseKey
    );

    if (!status.success)
    {
        std::filesystem::remove(
            restoreTemporary,
            error
        );

        workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

        return StorageStatus::Error(
            status.code,
            "临时恢复数据库检查失败: " +
                status.message
        );
    }

    RemoveDatabaseSidecars(databasePath);

    std::filesystem::remove(
        databasePath,
        error
    );

    if (error)
    {
        std::filesystem::remove(
            restoreTemporary,
            error
        );

        workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

        return FileError(
            SQLITE_IOERR,
            "无法移除当前数据库",
            error
        );
    }

    error.clear();

    std::filesystem::rename(
        restoreTemporary,
        databasePath,
        error
    );

    if (error)
    {
        if (options.createRollbackCopy &&
            std::filesystem::exists(
                rollbackFile
            ))
        {
            std::error_code rollbackError;

            std::filesystem::copy_file(
                rollbackFile,
                databasePath,
                std::filesystem::copy_options::
                    overwrite_existing,
                rollbackError
            );
        }

        workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

        return FileError(
            SQLITE_IOERR,
            "无法提交恢复数据库",
            error
        );
    }

    status = workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

    if (!status.success)
    {
        workspace_.Shutdown();

        if (options.createRollbackCopy &&
            std::filesystem::exists(
                rollbackFile
            ))
        {
            std::error_code rollbackError;

            RemoveDatabaseSidecars(databasePath);

            std::filesystem::copy_file(
                rollbackFile,
                databasePath,
                std::filesystem::copy_options::
                    overwrite_existing,
                rollbackError
            );

            if (!rollbackError)
            {
                const auto rollbackOpen =
                    workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );

                if (!rollbackOpen.success)
                {
                    return StorageStatus::Error(
                        rollbackOpen.code,
                        "恢复失败，并且回滚数据库无法重新打开: " +
                            rollbackOpen.message
                    );
                }
            }
            else
            {
                return FileError(
                    SQLITE_IOERR,
                    "恢复失败，并且无法回滚原数据库",
                    rollbackError
                );
            }
        }

        return StorageStatus::Error(
            status.code,
            "恢复数据库无法打开: " +
                status.message
        );
    }

    const auto finalCheck =
        workspace_.GetDatabase().CheckIntegrity();

    if (!finalCheck.success)
    {
        workspace_.Shutdown();

        if (options.createRollbackCopy &&
            std::filesystem::exists(
                rollbackFile
            ))
        {
            std::error_code rollbackError;

            RemoveDatabaseSidecars(databasePath);

            std::filesystem::copy_file(
                rollbackFile,
                databasePath,
                std::filesystem::copy_options::
                    overwrite_existing,
                rollbackError
            );

            if (!rollbackError)
            {
                workspace_.Initialize(
                    workspaceDirectory,
                    databaseKey
                );
            }
        }

        return StorageStatus::Error(
            finalCheck.code,
            "恢复后的数据库完整性检查失败: " +
                finalCheck.message
        );
    }

    AuditRepository(
        workspace_.GetDatabase()
    ).Append(
        "backup-service",
        "backup",
        "restore_completed",
        "workspace",
        "default",
        "{\"backup_id\":\"" +
            record.id +
            "\"}"
    );

    if (workerWasRunning &&
        options.restartBackgroundWorker)
    {
        const auto workerStatus =
            BackgroundWorker::Instance().Start(
                workspace_,
                workerOptions
            );

        if (!workerStatus.success)
        {
            return StorageStatus::Error(
                workerStatus.code,
                "数据库恢复成功，但后台服务重启失败: " +
                    workerStatus.message
            );
        }
    }

    return StorageStatus::Ok();
}

}
