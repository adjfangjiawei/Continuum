#pragma once

#include "Storage.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace continuum
{

struct DataSourceRecord
{
    std::string id;
    std::string name;
    std::string sourceType;
    std::string rootPath;
    bool enabled = true;
    std::string lastScanAt;
    std::string createdAt;
    std::string updatedAt;
};

struct FileRecord
{
    std::string id;
    std::string sourceId;
    std::string relativePath;
    std::string displayName;
    std::string mediaType;
    std::int64_t sizeBytes = 0;
    std::string modifiedAt;
    std::string fingerprint;
    std::string parseState;
    bool deleted = false;
    std::string createdAt;
    std::string updatedAt;
};

class DataSourceRepository final
{
public:
    explicit DataSourceRepository(Database& database);

    StorageStatus Save(
        const DataSourceRecord& record,
        const std::string& actor = "system"
    );

    std::optional<DataSourceRecord> FindById(
        const std::string& id
    ) const;

    std::vector<DataSourceRecord> List(
        bool includeDisabled = true,
        int limit = 200
    ) const;

    StorageStatus SetEnabled(
        const std::string& id,
        bool enabled,
        const std::string& actor = "system"
    );

    StorageStatus UpdateLastScan(
        const std::string& id,
        const std::string& scannedAt,
        const std::string& actor = "system"
    );

private:
    Database& database_;
};

class FileRepository final
{
public:
    explicit FileRepository(Database& database);

    StorageStatus Save(
        const FileRecord& record,
        const std::string& actor = "system"
    );

    std::optional<FileRecord> FindById(
        const std::string& id,
        bool includeDeleted = false
    ) const;

    std::optional<FileRecord> FindByPath(
        const std::string& sourceId,
        const std::string& relativePath,
        bool includeDeleted = false
    ) const;

    std::vector<FileRecord> ListBySource(
        const std::string& sourceId,
        bool includeDeleted = false,
        int limit = 1000
    ) const;

    std::vector<FileRecord> ListPending(
        int limit = 200
    ) const;

    StorageStatus SetParseState(
        const std::string& id,
        const std::string& parseState,
        const std::string& actor = "system"
    );

    StorageStatus MarkMissing(
        const std::string& id,
        const std::string& actor = "system"
    );

private:
    Database& database_;
};

class WorkspaceService final
{
public:
    static WorkspaceService& Instance();

    WorkspaceService(const WorkspaceService&) = delete;
    WorkspaceService& operator=(const WorkspaceService&) = delete;

    /*
     * 兼容内部恢复流程。路径不存在时允许创建数据库。
     * 新的用户入口不得调用本接口代替 CreateWorkspace/OpenWorkspace。
     */
    StorageStatus Initialize(
        const std::string& workspaceDirectory,
        const std::string& key = std::string()
    );

    StorageStatus CreateWorkspace(
        const std::string& workspaceDirectory,
        bool encrypted
    );

    StorageStatus OpenWorkspace(
        const std::string& workspaceDirectory
    );

    StorageStatus InitializeDefault();

    void Shutdown();

    bool IsInitialized() const;

    const std::string& WorkspaceDirectory() const;
    const std::string& DatabasePath() const;

    Database& GetDatabase();
    DataSourceRepository& DataSources();
    FileRepository& Files();
    ObjectRepository& Objects();
    EvidenceRepository& Evidence();
    AuditRepository& Audit();

private:
    WorkspaceService();
    ~WorkspaceService();

    StorageStatus InitializeInternal(
        const std::string& workspaceDirectory,
        const std::string& key,
        bool createIfMissing,
        bool createDirectories
    );

    std::string ResolveDefaultDirectory() const;

    mutable std::recursive_mutex mutex_;
    Database database_;
    DataSourceRepository dataSources_;
    FileRepository files_;
    ObjectRepository objects_;
    EvidenceRepository evidence_;
    AuditRepository audit_;
    std::string workspaceDirectory_;
    std::string databasePath_;
    bool initialized_;
};

}
