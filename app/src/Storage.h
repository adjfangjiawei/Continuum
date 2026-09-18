#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

struct sqlite3;

namespace continuum
{

struct StorageStatus
{
    bool success = false;
    int code = 0;
    std::string message;

    static StorageStatus Ok();

    static StorageStatus Error(
        int code,
        const std::string& message
    );
};

struct DomainObjectRecord
{
    std::string id;
    std::string objectType;
    std::string title;
    std::string description;
    std::string status;
    std::string priority;
    std::string owner;
    std::string occurredAt;
    std::string validFrom;
    std::string validTo;
    std::string knownAt;
    std::string timePrecision;
    std::string createdAt;
    std::string updatedAt;
    bool deleted = false;
};

struct EvidenceRecord
{
    std::string id;
    std::string sourceId;
    std::string fileId;
    std::string title;
    std::string quote;
    std::string anchor;
    std::string fingerprint;
    std::string reviewState;
    std::string createdAt;
    std::string updatedAt;
    bool deleted = false;
};

struct EvidenceLinkRecord
{
    std::string objectId;
    std::string evidenceId;
    std::string role;
    std::string note;
    std::string createdAt;
};

struct AuditEventRecord
{
    std::int64_t sequence = 0;
    std::string eventId;
    std::string occurredAt;
    std::string actor;
    std::string category;
    std::string action;
    std::string objectType;
    std::string objectId;
    std::string payload;
    std::string previousHash;
    std::string eventHash;
};

class Database final
{
public:
    Database();
    ~Database();

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    StorageStatus Open(
        const std::string& databasePath,
        const std::string& key = std::string(),
        bool createIfMissing = true
    );

    void Close();
    bool IsOpen() const;

    StorageStatus Execute(const std::string& sql);
    StorageStatus CreateSchema();
    StorageStatus CheckIntegrity();

    StorageStatus Transaction(
        const std::function<StorageStatus()>& operation
    );
    const std::string& Path() const;
    std::string LastError() const;

    sqlite3* Handle();
    std::recursive_mutex& Mutex();

private:
    StorageStatus Configure(const std::string& key);

    sqlite3* handle_;
    std::string path_;
    mutable std::recursive_mutex mutex_;
};

class AuditRepository final
{
public:
    explicit AuditRepository(Database& database);

    StorageStatus Append(
        const std::string& actor,
        const std::string& category,
        const std::string& action,
        const std::string& objectType,
        const std::string& objectId,
        const std::string& payload
    );

    std::vector<AuditEventRecord> Recent(
        int limit = 100
    ) const;

private:
    Database& database_;
};

class ObjectRepository final
{
public:
    explicit ObjectRepository(Database& database);

    StorageStatus Save(
        const DomainObjectRecord& record,
        const std::string& actor = "system"
    );

    std::optional<DomainObjectRecord> FindById(
        const std::string& id,
        bool includeDeleted = false
    ) const;

    std::vector<DomainObjectRecord> List(
        const std::string& objectType = std::string(),
        bool includeDeleted = false,
        int limit = 200
    ) const;

    StorageStatus SoftDelete(
        const std::string& id,
        const std::string& reason,
        const std::string& actor = "system"
    );

    StorageStatus Restore(
        const std::string& id,
        const std::string& actor = "system"
    );

    StorageStatus LinkEvidence(
        const EvidenceLinkRecord& link,
        const std::string& actor = "system"
    );

    std::vector<EvidenceLinkRecord> EvidenceLinks(
        const std::string& objectId
    ) const;

private:
    Database& database_;
};

class EvidenceRepository final
{
public:
    explicit EvidenceRepository(Database& database);

    StorageStatus Save(
        const EvidenceRecord& record,
        const std::string& actor = "system"
    );

    std::optional<EvidenceRecord> FindById(
        const std::string& id,
        bool includeDeleted = false
    ) const;

    std::vector<EvidenceRecord> ListForReview(
        int limit = 200
    ) const;

    StorageStatus SetReviewState(
        const std::string& id,
        const std::string& state,
        const std::string& actor = "system"
    );

private:
    Database& database_;
};

}
