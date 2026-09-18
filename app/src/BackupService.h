#pragma once

#include "BackgroundWorker.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace continuum
{

struct BackupRecord
{
    std::string id;
    std::string directory;
    std::string databaseFile;
    std::string createdAt;
    std::string databaseSha256;
    std::uintmax_t databaseSize = 0;
    int schemaVersion = 1;
    bool valid = false;
    std::string validationMessage;
};

struct BackupCreateOptions
{
    std::string label;
    int pagesPerStep = 128;
    int busyRetryCount = 50;
    int busySleepMilliseconds = 20;
    int retainLatest = 20;
};

struct BackupRestoreOptions
{
    bool createRollbackCopy = true;
    bool restartBackgroundWorker = true;
};

struct BackupResult
{
    StorageStatus status;
    BackupRecord backup;
};

class BackupService final
{
public:
    explicit BackupService(
        WorkspaceService& workspace
    );

    BackupResult CreateBackup(
        const BackupCreateOptions& options =
            BackupCreateOptions()
    );

    StorageStatus ValidateBackup(
        const std::string& backupDirectory,
        BackupRecord* record = nullptr
    ) const;

    StorageStatus RestoreBackup(
        const std::string& backupDirectory,
        const BackupRestoreOptions& options =
            BackupRestoreOptions()
    );

    std::vector<BackupRecord> ListBackups() const;

    // 仅统计已提交的备份目录，不执行 SHA-256 或 quick_check。
    std::uint64_t CountBackups() const;

    StorageStatus PruneBackups(
        int retainLatest
    );

    std::string BackupRoot() const;

private:
    static std::string CurrentTimestamp();
    static std::string FilesystemTimestamp();
    static std::string SanitizeLabel(
        const std::string& value
    );

    static StorageStatus ReadManifest(
        const std::string& backupDirectory,
        BackupRecord& record
    );

    static StorageStatus WriteManifest(
        const BackupRecord& record
    );

    static StorageStatus ValidateDatabaseFile(
        const std::string& databaseFile,
        const std::string& key = std::string()
    );

    static StorageStatus CopyDatabaseSnapshot(
        Database& source,
        const std::string& destination,
        const BackupCreateOptions& options,
        const std::string& key
    );

    WorkspaceService& workspace_;
};

}
