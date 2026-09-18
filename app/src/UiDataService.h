#pragma once

#include "BackgroundWorker.h"
#include "BackupService.h"
#include "SearchService.h"
#include "SecurityService.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace continuum
{

enum class UiChangeType
{
    Workspace,
    Dashboard,
    Objects,
    Evidence,
    DataSources,
    Files,
    Search,
    Jobs,
    Backups,
    Security
};

struct UiChangeEvent
{
    UiChangeType type = UiChangeType::Workspace;
    std::string objectId;
    std::string message;
};

struct UiOperationResult
{
    bool success = false;
    int code = 0;
    std::string message;

    static UiOperationResult Ok(
        const std::string& message = std::string()
    );

    static UiOperationResult FromStorage(
        const StorageStatus& status
    );
};

struct DashboardSnapshot
{
    std::int64_t activeObjects = 0;
    std::int64_t deletedObjects = 0;
    std::int64_t evidenceItems = 0;
    std::int64_t evidenceNeedsReview = 0;
    std::int64_t enabledDataSources = 0;
    std::int64_t indexedFiles = 0;
    std::int64_t pendingFiles = 0;
    std::int64_t queuedJobs = 0;
    std::int64_t failedJobs = 0;
    std::int64_t auditEvents = 0;
    std::int64_t searchDocuments = 0;
    std::int64_t pendingSearchItems = 0;
    bool backgroundWorkerRunning = false;
    bool sqlCipherAvailable = false;
    std::string databasePath;
    std::string workspaceDirectory;
    std::string lastError;
};

struct UiJobRecord
{
    std::string id;
    std::string type;
    std::string state;
    int progress = 0;
    int attempts = 0;
    bool cancellationRequested = false;
    std::string errorMessage;
    std::string createdAt;
    std::string startedAt;
    std::string completedAt;
};

class UiDataService final
{
public:
    using Listener = std::function<void(
        const UiChangeEvent&
    )>;

    static UiDataService& Instance();

    UiDataService(const UiDataService&) = delete;
    UiDataService& operator=(const UiDataService&) = delete;

    UiOperationResult Initialize();
    bool IsReady() const;

    DashboardSnapshot Dashboard() const;

    std::vector<DomainObjectRecord> ListObjects(
        const std::string& objectType = std::string(),
        bool includeDeleted = false,
        int limit = 200
    ) const;

    UiOperationResult SaveObject(
        const DomainObjectRecord& record,
        const std::string& actor = "ui"
    );

    UiOperationResult DeleteObject(
        const std::string& id,
        const std::string& reason,
        const std::string& actor = "ui"
    );

    UiOperationResult RestoreObject(
        const std::string& id,
        const std::string& actor = "ui"
    );

    std::vector<EvidenceRecord> EvidenceForReview(
        int limit = 200
    ) const;

    UiOperationResult SetEvidenceReviewState(
        const std::string& id,
        const std::string& state,
        const std::string& actor = "ui"
    );

    std::vector<DataSourceRecord> ListDataSources(
        bool includeDisabled = true,
        int limit = 200
    ) const;

    UiOperationResult SaveDataSource(
        const DataSourceRecord& record,
        const std::string& actor = "ui"
    );

    UiOperationResult SetDataSourceEnabled(
        const std::string& id,
        bool enabled,
        const std::string& actor = "ui"
    );

    UiOperationResult QueueSourceScan(
        const std::string& sourceId,
        const std::string& actor = "ui"
    );

    SearchResponse Search(
        const SearchRequest& request
    );

    std::vector<UiJobRecord> ListJobs(
        int limit = 200
    ) const;

    UiOperationResult CancelJob(
        const std::string& jobId,
        const std::string& actor = "ui"
    );

    UiOperationResult RetryJob(
        const std::string& jobId,
        const std::string& actor = "ui"
    );

    BackgroundWorkerStatus WorkerStatus() const;

    BackupResult CreateBackup(
        const BackupCreateOptions& options =
            BackupCreateOptions()
    );

    std::vector<BackupRecord> ListBackups() const;

    UiOperationResult ValidateBackup(
        const std::string& backupDirectory,
        BackupRecord* record = nullptr
    ) const;

    UiOperationResult RestoreBackup(
        const std::string& backupDirectory,
        const BackupRestoreOptions& options =
            BackupRestoreOptions()
    );

    SecurityCapabilities Security() const;

    std::uint64_t Subscribe(Listener listener);
    void Unsubscribe(std::uint64_t token);

    void Refresh(
        UiChangeType type = UiChangeType::Dashboard,
        const std::string& message = std::string()
    );

private:
    UiDataService();

    static std::string MakeJobId(
        const std::string& prefix,
        const std::string& subject
    );

    void Notify(const UiChangeEvent& event);

    WorkspaceService& workspace_;
    mutable std::mutex listenerMutex_;
    std::map<std::uint64_t, Listener> listeners_;
    std::uint64_t nextListenerToken_;
};

}
