#pragma once

#include "ContentParser.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <string>
#include <vector>

namespace continuum
{

struct SearchRequest
{
    std::string query;
    std::string sourceId;
    std::string mediaType;
    bool advancedSyntax = false;
    int limit = 50;
    int offset = 0;
};

struct SearchHit
{
    std::string fileId;
    std::string sourceId;
    std::string relativePath;
    std::string displayName;
    std::string mediaType;
    std::string title;
    std::string author;
    std::string subject;
    std::string snippet;
    double score = 0.0;
};

struct SearchResponse
{
    StorageStatus status;
    std::string normalizedQuery;
    int total = 0;
    std::vector<SearchHit> hits;
};

struct IndexSyncSummary
{
    int visited = 0;
    int indexed = 0;
    int removed = 0;
    int failed = 0;
    std::vector<std::string> errors;
};

class SearchService final
{
public:
    explicit SearchService(Database& database);

    StorageStatus EnsureSchema();
    StorageStatus CheckAvailability();

    StorageStatus QueueFile(
        const std::string& fileId,
        const std::string& operation = "upsert"
    );

    StorageStatus IndexFile(
        const std::string& fileId,
        const std::string& actor = "indexer"
    );

    StorageStatus RemoveFile(
        const std::string& fileId,
        const std::string& actor = "indexer"
    );

    IndexSyncSummary SyncPending(
        int limit = 500,
        const std::string& actor = "indexer"
    );

    StorageStatus Rebuild(
        const std::string& actor = "indexer"
    );

    SearchResponse Search(
        const SearchRequest& request
    );

    int IndexedDocumentCount() const;
    int PendingCount() const;

private:
    static std::string BuildSafeQuery(
        const std::string& query
    );

    static std::string Trim(
        const std::string& value
    );

    Database& database_;
};

}
