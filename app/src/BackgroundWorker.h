#pragma once

#include "ContentParser.h"
#include "FileScanner.h"
#include "SearchService.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace continuum
{

struct BackgroundWorkerOptions
{
    int maximumAttempts = 3;
    int indexBatchSize = 100;
    std::chrono::milliseconds idleInterval{
        std::chrono::milliseconds(1000)
    };
};

struct BackgroundWorkerStatus
{
    bool running = false;
    bool stopRequested = false;
    std::string activeJobId;
    std::string activeJobType;
    std::uint64_t completedJobs = 0;
    std::uint64_t failedJobs = 0;
    std::uint64_t retriedJobs = 0;
    std::uint64_t cancelledJobs = 0;
    std::uint64_t indexedDocuments = 0;
    std::string lastError;
};

class BackgroundWorker final
{
public:
    static BackgroundWorker& Instance();

    BackgroundWorker(const BackgroundWorker&) = delete;
    BackgroundWorker& operator=(const BackgroundWorker&) = delete;

    StorageStatus Start(
        WorkspaceService& workspace,
        const BackgroundWorkerOptions& options =
            BackgroundWorkerOptions()
    );

    void Stop();
    void Wake();

    bool IsRunning() const;
    BackgroundWorkerStatus Status() const;

    StorageStatus RequestCancel(
        const std::string& jobId,
        const std::string& actor = "user"
    );

    StorageStatus RetryNow(
        const std::string& jobId,
        const std::string& actor = "user"
    );

    bool WaitUntilIdle(
        std::chrono::milliseconds timeout
    );

private:
    BackgroundWorker();
    ~BackgroundWorker();

    struct ClaimedJob
    {
        JobRecord job;
        int attempts = 0;
    };

    StorageStatus EnsureSchema();
    std::optional<ClaimedJob> ClaimNextJob();

    StorageStatus ExecuteClaimedJob(
        const ClaimedJob& claimed
    );

    StorageStatus ExecuteScanJob(
        const ClaimedJob& claimed
    );

    StorageStatus ExecuteParseJob(
        const ClaimedJob& claimed
    );

    bool IsCancellationRequested(
        const std::string& jobId
    );

    StorageStatus FinishJob(
        const ClaimedJob& claimed,
        const StorageStatus& executionStatus
    );

    StorageStatus MarkCancelled(
        const std::string& jobId,
        const std::string& actor
    );

    bool SynchronizeSearchIndex();
    void WorkerLoop();

    static std::string ExtractJsonString(
        const std::string& json,
        const std::string& key
    );

    mutable std::mutex stateMutex_;
    std::condition_variable condition_;
    std::condition_variable idleCondition_;
    std::thread workerThread_;

    WorkspaceService* workspace_;
    Database* database_;

    BackgroundWorkerOptions options_;
    BackgroundWorkerStatus status_;

    std::atomic<bool> stopRequested_;
    std::atomic<bool> wakeRequested_;
};

}
