#pragma once

#include "Storage.h"
#include "WorkspaceService.h"

#include <cstdint>
#include <string>
#include <vector>

namespace continuum
{

struct JobRecord
{
    std::string id;
    std::string jobType;
    std::string state;
    int progress = 0;
    std::string payload;
    std::string errorMessage;
    std::string createdAt;
    std::string startedAt;
    std::string completedAt;
};

class JobRepository final
{
public:
    explicit JobRepository(Database& database);

    StorageStatus Enqueue(
        const JobRecord& record,
        const std::string& actor = "system"
    );

    StorageStatus EnqueueParse(
        const std::string& fileId,
        const std::string& fingerprint,
        const std::string& actor = "scanner"
    );

    std::vector<JobRecord> ListPending(
        int limit = 200
    ) const;

    StorageStatus SetState(
        const std::string& id,
        const std::string& state,
        int progress,
        const std::string& errorMessage = std::string(),
        const std::string& actor = "system"
    );

private:
    Database& database_;
};

struct ScanOptions
{
    bool recursive = true;
    bool includeHidden = false;
    bool enqueueParseJobs = true;
    bool markMissingFiles = true;
    std::uintmax_t maximumFileSize =
        static_cast<std::uintmax_t>(1024) * 1024 * 1024;
};

struct ScanIssue
{
    std::string path;
    std::string message;
};

struct ScanSummary
{
    std::string sourceId;
    std::string startedAt;
    std::string completedAt;
    std::uint64_t visited = 0;
    std::uint64_t supported = 0;
    std::uint64_t added = 0;
    std::uint64_t changed = 0;
    std::uint64_t unchanged = 0;
    std::uint64_t missing = 0;
    std::uint64_t skipped = 0;
    std::uint64_t failed = 0;
    std::uint64_t parseJobs = 0;
    std::vector<ScanIssue> issues;
};

class ContentHasher final
{
public:
    static StorageStatus Sha256File(
        const std::string& path,
        std::string& digest
    );

    static std::string Sha256Text(
        const std::string& value
    );
};

class FileScanner final
{
public:
    FileScanner(
        Database& database,
        DataSourceRepository& dataSources,
        FileRepository& files
    );

    ScanSummary ScanSource(
        const std::string& sourceId,
        const ScanOptions& options = ScanOptions()
    );

    std::vector<ScanSummary> ScanEnabledSources(
        const ScanOptions& options = ScanOptions()
    );

    static bool IsSupportedFile(
        const std::string& path
    );

    static std::string DetectMediaType(
        const std::string& path
    );

private:
    Database& database_;
    DataSourceRepository& dataSources_;
    FileRepository& files_;
    JobRepository jobs_;
};

}
