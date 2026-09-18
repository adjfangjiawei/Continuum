#include "Storage.h"
#include "SecurityService.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <iomanip>
#include <sstream>
#include <utility>

#include <sqlite3.h>

namespace continuum
{
namespace
{

class Statement final
{
public:
    Statement(sqlite3* database, const std::string& sql)
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

std::string ColumnText(sqlite3_stmt* statement, int column)
{
    const auto* value = sqlite3_column_text(statement, column);

    return value == nullptr
        ? std::string()
        : reinterpret_cast<const char*>(value);
}

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

std::string CurrentTimestamp()
{
    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::system_clock::to_time_t(now);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &value);
#else
    localtime_r(&value, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
    return stream.str();
}

std::string MakeIdentifier(const std::string& prefix)
{
    static std::atomic<std::uint64_t> sequence{0};

    const auto now = std::chrono::system_clock::now();
    const auto value = std::chrono::duration_cast<
        std::chrono::microseconds
    >(now.time_since_epoch()).count();
    const auto suffix = sequence.fetch_add(
        1,
        std::memory_order_relaxed
    );

    std::ostringstream stream;
    stream << prefix << "-" << std::hex
           << value << "-" << suffix;
    return stream.str();
}

std::uint64_t Fnv1a(
    const std::string& value,
    std::uint64_t seed
)
{
    std::uint64_t result = seed;

    for (const unsigned char character : value)
    {
        result ^= static_cast<std::uint64_t>(character);
        result *= 1099511628211ULL;
    }

    return result;
}

std::string ChainChecksum(const std::string& value)
{
    const std::array<std::uint64_t, 4> seeds = {
        1469598103934665603ULL,
        1099511628211ULL,
        7809847782465536322ULL,
        9650029242287828579ULL
    };

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    for (const auto seed : seeds)
    {
        stream << std::setw(16) << Fnv1a(value, seed);
    }

    return stream.str();
}

std::string EscapeJson(const std::string& value)
{
    std::string result;
    result.reserve(value.size());

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
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

const char* ObjectColumns()
{
    return
        "id, object_type, title, description, status, priority, "
        "owner, occurred_at, valid_from, valid_to, known_at, "
        "time_precision, created_at, updated_at, deleted";
}

const char* EvidenceColumns()
{
    return
        "id, source_id, file_id, title, quote, anchor, fingerprint, "
        "review_state, created_at, updated_at, deleted";
}

bool IsValidEvidenceReviewState(const std::string& state)
{
    return state == "unverified" ||
        state == "needs_review" ||
        state == "changed" ||
        state == "verified" ||
        state == "rejected";
}

DomainObjectRecord ReadObject(sqlite3_stmt* statement)
{
    DomainObjectRecord record;
    record.id = ColumnText(statement, 0);
    record.objectType = ColumnText(statement, 1);
    record.title = ColumnText(statement, 2);
    record.description = ColumnText(statement, 3);
    record.status = ColumnText(statement, 4);
    record.priority = ColumnText(statement, 5);
    record.owner = ColumnText(statement, 6);
    record.occurredAt = ColumnText(statement, 7);
    record.validFrom = ColumnText(statement, 8);
    record.validTo = ColumnText(statement, 9);
    record.knownAt = ColumnText(statement, 10);
    record.timePrecision = ColumnText(statement, 11);
    record.createdAt = ColumnText(statement, 12);
    record.updatedAt = ColumnText(statement, 13);
    record.deleted = sqlite3_column_int(statement, 14) != 0;
    return record;
}

EvidenceRecord ReadEvidence(sqlite3_stmt* statement)
{
    EvidenceRecord record;
    record.id = ColumnText(statement, 0);
    record.sourceId = ColumnText(statement, 1);
    record.fileId = ColumnText(statement, 2);
    record.title = ColumnText(statement, 3);
    record.quote = ColumnText(statement, 4);
    record.anchor = ColumnText(statement, 5);
    record.fingerprint = ColumnText(statement, 6);
    record.reviewState = ColumnText(statement, 7);
    record.createdAt = ColumnText(statement, 8);
    record.updatedAt = ColumnText(statement, 9);
    record.deleted = sqlite3_column_int(statement, 10) != 0;
    return record;
}

}

StorageStatus StorageStatus::Ok()
{
    return {true, SQLITE_OK, std::string()};
}

StorageStatus StorageStatus::Error(
    int code,
    const std::string& message
)
{
    return {false, code, message};
}

Database::Database()
    : handle_(nullptr)
{
}

Database::~Database()
{
    Close();
}

StorageStatus Database::Open(
    const std::string& databasePath,
    const std::string& key,
    bool createIfMissing
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    Close();

    int flags =
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_FULLMUTEX;

    if (createIfMissing)
    {
        flags |= SQLITE_OPEN_CREATE;
    }

    const int result = sqlite3_open_v2(
        databasePath.c_str(),
        &handle_,
        flags,
        nullptr
    );

    if (result != SQLITE_OK)
    {
        const auto status = SqliteError(
            handle_,
            result,
            "无法打开工作区数据库"
        );
        Close();
        return status;
    }

    path_ = databasePath;

    auto status = Configure(key);

    if (!status.success)
    {
        Close();
        return status;
    }

    status = CreateSchema();

    if (!status.success)
    {
        Close();
    }

    return status;
}

void Database::Close()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (handle_ != nullptr)
    {
        sqlite3_close_v2(handle_);
        handle_ = nullptr;
    }

    path_.clear();
}

bool Database::IsOpen() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return handle_ != nullptr;
}

StorageStatus Database::Configure(const std::string& key)
{
    if (handle_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    if (!key.empty())
    {
        // CONTINUUM_SQLCIPHER_KEY_VALIDATION
        std::string escapedKey;

        for (const char character : key)
        {
            escapedKey += character;

            if (character == '\'')
            {
                escapedKey += '\'';
            }
        }

        auto keyStatus = Execute(
            "PRAGMA key = '" + escapedKey + "';"
        );

        if (!keyStatus.success)
        {
            return keyStatus;
        }

        std::string cipherVersion;

        const auto cipherStatus =
            DatabaseSecurity::ValidateSqlCipherConnection(
                handle_,
                &cipherVersion
            );

        if (!cipherStatus.success)
        {
            return StorageStatus::Error(
                cipherStatus.code,
                "拒绝在不支持 SQLCipher 的 SQLite 上应用密钥: " +
                    cipherStatus.message
            );
        }

        /*
         * 读取 sqlite_master 会强制 SQLCipher 验证密钥。
         * 错误密钥通常在此处返回 SQLITE_NOTADB。
         */
        sqlite3_stmt* validation = nullptr;

        const int prepareResult = sqlite3_prepare_v2(
            handle_,
            "SELECT count(*) FROM sqlite_master;",
            -1,
            &validation,
            nullptr
        );

        if (prepareResult != SQLITE_OK)
        {
            return StorageStatus::Error(
                prepareResult,
                std::string("SQLCipher 数据库密钥验证失败: ") +
                    sqlite3_errmsg(handle_)
            );
        }

        const int stepResult =
            sqlite3_step(validation);

        sqlite3_finalize(validation);

        if (stepResult != SQLITE_ROW)
        {
            return StorageStatus::Error(
                stepResult,
                std::string("SQLCipher 数据库密钥无效: ") +
                    sqlite3_errmsg(handle_)
            );
        }
    }

    const std::array<const char*, 7> pragmas = {
        "PRAGMA foreign_keys = ON;",
        "PRAGMA journal_mode = WAL;",
        "PRAGMA synchronous = NORMAL;",
        "PRAGMA temp_store = MEMORY;",
        "PRAGMA busy_timeout = 5000;",
        "PRAGMA trusted_schema = OFF;",
        "PRAGMA recursive_triggers = ON;"
    };

    for (const auto* pragma : pragmas)
    {
        const auto status = Execute(pragma);

        if (!status.success)
        {
            return status;
        }
    }

    return StorageStatus::Ok();
}

StorageStatus Database::Execute(const std::string& sql)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (handle_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    char* error = nullptr;
    const int result = sqlite3_exec(
        handle_,
        sql.c_str(),
        nullptr,
        nullptr,
        &error
    );

    if (result != SQLITE_OK)
    {
        std::string message = "SQL 执行失败";

        if (error != nullptr)
        {
            message += ": ";
            message += error;
            sqlite3_free(error);
        }

        return StorageStatus::Error(result, message);
    }

    return StorageStatus::Ok();
}

StorageStatus Database::Transaction(
    const std::function<StorageStatus()>& operation
)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    auto status = Execute("BEGIN IMMEDIATE;");

    if (!status.success)
    {
        return status;
    }

    try
    {
        status = operation();

        if (!status.success)
        {
            Execute("ROLLBACK;");
            return status;
        }

        const auto commit = Execute("COMMIT;");

        if (!commit.success)
        {
            Execute("ROLLBACK;");
            return commit;
        }

        return status;
    }
    catch (const std::exception& exception)
    {
        Execute("ROLLBACK;");
        return StorageStatus::Error(
            SQLITE_ABORT,
            exception.what()
        );
    }
    catch (...)
    {
        Execute("ROLLBACK;");
        return StorageStatus::Error(
            SQLITE_ABORT,
            "事务发生未知异常"
        );
    }
}



StorageStatus Database::CreateSchema()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (handle_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    return Transaction([this]() {
        return Execute(R"sql(
CREATE TABLE IF NOT EXISTS workspace_meta (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS data_sources (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    source_type TEXT NOT NULL,
    root_path TEXT NOT NULL,
    enabled INTEGER NOT NULL DEFAULT 1,
    last_scan_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS files (
    id TEXT PRIMARY KEY,
    source_id TEXT NOT NULL,
    relative_path TEXT NOT NULL,
    display_name TEXT NOT NULL,
    media_type TEXT,
    size_bytes INTEGER NOT NULL DEFAULT 0,
    modified_at TEXT,
    fingerprint TEXT,
    parse_state TEXT NOT NULL DEFAULT 'pending',
    deleted INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(source_id) REFERENCES data_sources(id),
    UNIQUE(source_id, relative_path)
);

CREATE TABLE IF NOT EXISTS evidence (
    id TEXT PRIMARY KEY,
    source_id TEXT,
    file_id TEXT,
    title TEXT NOT NULL,
    quote TEXT NOT NULL DEFAULT '',
    anchor TEXT NOT NULL DEFAULT '',
    fingerprint TEXT NOT NULL DEFAULT '',
    review_state TEXT NOT NULL DEFAULT 'unverified',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted INTEGER NOT NULL DEFAULT 0,
    FOREIGN KEY(source_id) REFERENCES data_sources(id),
    FOREIGN KEY(file_id) REFERENCES files(id)
);

CREATE TABLE IF NOT EXISTS objects (
    id TEXT PRIMARY KEY,
    object_type TEXT NOT NULL,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'active',
    priority TEXT NOT NULL DEFAULT 'normal',
    owner TEXT NOT NULL DEFAULT '',
    occurred_at TEXT,
    valid_from TEXT,
    valid_to TEXT,
    known_at TEXT,
    time_precision TEXT NOT NULL DEFAULT 'unknown',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    deleted INTEGER NOT NULL DEFAULT 0
);

CREATE TABLE IF NOT EXISTS object_evidence (
    object_id TEXT NOT NULL,
    evidence_id TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'support',
    note TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(object_id, evidence_id, role),
    FOREIGN KEY(object_id) REFERENCES objects(id),
    FOREIGN KEY(evidence_id) REFERENCES evidence(id)
);

CREATE TABLE IF NOT EXISTS entities (
    id TEXT PRIMARY KEY,
    entity_type TEXT NOT NULL,
    canonical_name TEXT NOT NULL,
    aliases TEXT NOT NULL DEFAULT '',
    confirmed INTEGER NOT NULL DEFAULT 0,
    deleted INTEGER NOT NULL DEFAULT 0,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS tags (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    group_name TEXT NOT NULL DEFAULT '',
    color TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS object_tags (
    object_id TEXT NOT NULL,
    tag_id TEXT NOT NULL,
    PRIMARY KEY(object_id, tag_id),
    FOREIGN KEY(object_id) REFERENCES objects(id),
    FOREIGN KEY(tag_id) REFERENCES tags(id)
);

CREATE TABLE IF NOT EXISTS saved_queries (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    query_json TEXT NOT NULL,
    pinned INTEGER NOT NULL DEFAULT 0,
    schedule TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS rules (
    id TEXT PRIMARY KEY,
    name TEXT NOT NULL,
    rule_type TEXT NOT NULL,
    expression TEXT NOT NULL,
    severity TEXT NOT NULL DEFAULT 'normal',
    enabled INTEGER NOT NULL DEFAULT 0,
    revision INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS jobs (
    id TEXT PRIMARY KEY,
    job_type TEXT NOT NULL,
    state TEXT NOT NULL DEFAULT 'queued',
    progress INTEGER NOT NULL DEFAULT 0,
    payload TEXT NOT NULL DEFAULT '',
    error_message TEXT NOT NULL DEFAULT '',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    started_at TEXT,
    completed_at TEXT
);

CREATE TABLE IF NOT EXISTS conflicts (
    id TEXT PRIMARY KEY,
    conflict_type TEXT NOT NULL,
    severity TEXT NOT NULL DEFAULT 'medium'
        CHECK(severity IN ('low', 'medium', 'high')),
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    subject_object_id TEXT,
    property_name TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'open'
        CHECK(status IN (
            'open',
            'retained',
            'insufficient',
            'resolved'
        )),
    resolution TEXT NOT NULL DEFAULT '',
    resolution_note TEXT NOT NULL DEFAULT '',
    detected_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    resolved_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(subject_object_id) REFERENCES objects(id)
);

CREATE TABLE IF NOT EXISTS conflict_claims (
    id TEXT PRIMARY KEY,
    conflict_id TEXT NOT NULL,
    side_key TEXT NOT NULL,
    object_id TEXT,
    evidence_id TEXT,
    value_text TEXT NOT NULL,
    source_label TEXT NOT NULL DEFAULT '',
    valid_from TEXT,
    valid_to TEXT,
    known_at TEXT,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(conflict_id) REFERENCES conflicts(id)
        ON DELETE CASCADE,
    FOREIGN KEY(object_id) REFERENCES objects(id),
    FOREIGN KEY(evidence_id) REFERENCES evidence(id),
    UNIQUE(conflict_id, side_key)
);

CREATE TABLE IF NOT EXISTS object_relations (
    id TEXT PRIMARY KEY,
    source_object_id TEXT NOT NULL,
    target_object_id TEXT NOT NULL,
    relation_type TEXT NOT NULL
        CHECK(relation_type IN (
            'support',
            'oppose',
            'replace',
            'depend',
            'block',
            'impact'
        )),
    status TEXT NOT NULL DEFAULT 'confirmed'
        CHECK(status IN (
            'candidate',
            'confirmed',
            'invalid'
        )),
    note TEXT NOT NULL DEFAULT '',
    valid_from TEXT,
    valid_to TEXT,
    created_by TEXT NOT NULL DEFAULT 'system',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(source_object_id) REFERENCES objects(id),
    FOREIGN KEY(target_object_id) REFERENCES objects(id),
    CHECK(source_object_id <> target_object_id),
    UNIQUE(
        source_object_id,
        target_object_id,
        relation_type
    )
);

CREATE TABLE IF NOT EXISTS file_versions (
    id TEXT PRIMARY KEY,
    file_id TEXT NOT NULL,
    version_number INTEGER NOT NULL,
    fingerprint TEXT NOT NULL,
    size_bytes INTEGER NOT NULL DEFAULT 0,
    modified_at TEXT,
    content TEXT NOT NULL DEFAULT '',
    captured_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(file_id) REFERENCES files(id),
    UNIQUE(file_id, version_number),
    UNIQUE(file_id, fingerprint)
);

CREATE TABLE IF NOT EXISTS change_reviews (
    id TEXT PRIMARY KEY,
    file_id TEXT NOT NULL,
    previous_version_id TEXT,
    current_version_id TEXT,
    change_type TEXT NOT NULL
        CHECK(change_type IN (
            'created',
            'modified',
            'missing',
            'restored'
        )),
    status TEXT NOT NULL DEFAULT 'pending'
        CHECK(status IN (
            'pending',
            'reanchored',
            'invalidated',
            'no_impact',
            'completed'
        )),
    resolution_note TEXT NOT NULL DEFAULT '',
    detected_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    completed_at TEXT,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(file_id) REFERENCES files(id),
    FOREIGN KEY(previous_version_id) REFERENCES file_versions(id),
    FOREIGN KEY(current_version_id) REFERENCES file_versions(id)
);

CREATE TABLE IF NOT EXISTS change_review_evidence (
    review_id TEXT NOT NULL,
    evidence_id TEXT NOT NULL,
    impact_state TEXT NOT NULL DEFAULT 'needs_review'
        CHECK(impact_state IN (
            'needs_review',
            'reanchored',
            'invalidated',
            'no_impact'
        )),
    old_anchor TEXT NOT NULL DEFAULT '',
    new_anchor TEXT NOT NULL DEFAULT '',
    note TEXT NOT NULL DEFAULT '',
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(review_id, evidence_id),
    FOREIGN KEY(review_id) REFERENCES change_reviews(id)
        ON DELETE CASCADE,
    FOREIGN KEY(evidence_id) REFERENCES evidence(id)
);

CREATE TABLE IF NOT EXISTS handover_capsules (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    description TEXT NOT NULL DEFAULT '',
    status TEXT NOT NULL DEFAULT 'draft'
        CHECK(status IN (
            'draft',
            'ready',
            'exported',
            'archived'
        )),
    redaction_enabled INTEGER NOT NULL DEFAULT 0,
    created_by TEXT NOT NULL DEFAULT 'ui',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    exported_at TEXT
);

CREATE TABLE IF NOT EXISTS handover_sections (
    id TEXT PRIMARY KEY,
    capsule_id TEXT NOT NULL,
    ordinal INTEGER NOT NULL,
    title TEXT NOT NULL,
    content TEXT NOT NULL DEFAULT '',
    section_type TEXT NOT NULL DEFAULT 'custom',
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(capsule_id) REFERENCES handover_capsules(id)
        ON DELETE CASCADE,
    UNIQUE(capsule_id, ordinal)
);

CREATE TABLE IF NOT EXISTS handover_items (
    capsule_id TEXT NOT NULL,
    object_id TEXT NOT NULL,
    section_id TEXT,
    ordinal INTEGER NOT NULL DEFAULT 0,
    note TEXT NOT NULL DEFAULT '',
    included INTEGER NOT NULL DEFAULT 1,
    created_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY(capsule_id, object_id),
    FOREIGN KEY(capsule_id) REFERENCES handover_capsules(id)
        ON DELETE CASCADE,
    FOREIGN KEY(section_id) REFERENCES handover_sections(id)
        ON DELETE SET NULL,
    FOREIGN KEY(object_id) REFERENCES objects(id)
);

CREATE TABLE IF NOT EXISTS audit_events (
    sequence INTEGER PRIMARY KEY AUTOINCREMENT,
    event_id TEXT NOT NULL UNIQUE,
    occurred_at TEXT NOT NULL,
    actor TEXT NOT NULL,
    category TEXT NOT NULL,
    action TEXT NOT NULL,
    object_type TEXT NOT NULL DEFAULT '',
    object_id TEXT NOT NULL DEFAULT '',
    payload TEXT NOT NULL DEFAULT '',
    previous_hash TEXT NOT NULL DEFAULT '',
    event_hash TEXT NOT NULL
);

CREATE INDEX IF NOT EXISTS idx_objects_type_state
ON objects(object_type, deleted, status);

CREATE INDEX IF NOT EXISTS idx_objects_updated
ON objects(updated_at DESC);

CREATE INDEX IF NOT EXISTS idx_evidence_review
ON evidence(review_state, deleted);

CREATE INDEX IF NOT EXISTS idx_files_source_state
ON files(source_id, deleted, parse_state);

CREATE INDEX IF NOT EXISTS idx_audit_time
ON audit_events(occurred_at DESC);

CREATE INDEX IF NOT EXISTS idx_audit_object
ON audit_events(object_type, object_id, sequence DESC);

CREATE INDEX IF NOT EXISTS idx_jobs_state
ON jobs(state, created_at);

CREATE INDEX IF NOT EXISTS idx_conflicts_status
ON conflicts(status, severity, updated_at DESC);

CREATE INDEX IF NOT EXISTS idx_conflicts_subject
ON conflicts(subject_object_id, property_name);

CREATE INDEX IF NOT EXISTS idx_conflict_claims_conflict
ON conflict_claims(conflict_id, side_key);

CREATE INDEX IF NOT EXISTS idx_relations_source
ON object_relations(
    source_object_id,
    status,
    relation_type
);

CREATE INDEX IF NOT EXISTS idx_relations_target
ON object_relations(
    target_object_id,
    status,
    relation_type
);

CREATE INDEX IF NOT EXISTS idx_file_versions_file
ON file_versions(file_id, version_number DESC);

CREATE INDEX IF NOT EXISTS idx_change_reviews_status
ON change_reviews(status, detected_at DESC);

CREATE INDEX IF NOT EXISTS idx_change_reviews_file
ON change_reviews(file_id, detected_at DESC);

CREATE INDEX IF NOT EXISTS idx_change_evidence_evidence
ON change_review_evidence(evidence_id, impact_state);

CREATE INDEX IF NOT EXISTS idx_handover_status
ON handover_capsules(status, updated_at DESC);

CREATE INDEX IF NOT EXISTS idx_handover_sections
ON handover_sections(capsule_id, ordinal);
)sql");
    });
}

StorageStatus Database::CheckIntegrity()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (handle_ == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    Statement statement(handle_, "PRAGMA quick_check;");

    if (!statement.IsValid())
    {
        return SqliteError(
            handle_,
            statement.Status(),
            "无法运行数据库完整性检查"
        );
    }

    const int result = sqlite3_step(statement.Get());

    if (result != SQLITE_ROW)
    {
        return SqliteError(
            handle_,
            result,
            "数据库完整性检查失败"
        );
    }

    const std::string value = ColumnText(statement.Get(), 0);

    return value == "ok"
        ? StorageStatus::Ok()
        : StorageStatus::Error(
            SQLITE_CORRUPT,
            "数据库完整性检查返回: " + value
        );
}

const std::string& Database::Path() const
{
    return path_;
}

std::string Database::LastError() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    return handle_ == nullptr
        ? "数据库尚未打开"
        : sqlite3_errmsg(handle_);
}

sqlite3* Database::Handle()
{
    return handle_;
}

std::recursive_mutex& Database::Mutex()
{
    return mutex_;
}

AuditRepository::AuditRepository(Database& database)
    : database_(database)
{
}

StorageStatus AuditRepository::Append(
    const std::string& actor,
    const std::string& category,
    const std::string& action,
    const std::string& objectType,
    const std::string& objectId,
    const std::string& payload
)
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    std::string previousHash;

    Statement previous(
        handle,
        "SELECT event_hash FROM audit_events "
        "ORDER BY sequence DESC LIMIT 1;"
    );

    if (!previous.IsValid())
    {
        return SqliteError(
            handle,
            previous.Status(),
            "无法读取前序审计校验值"
        );
    }

    if (sqlite3_step(previous.Get()) == SQLITE_ROW)
    {
        previousHash = ColumnText(previous.Get(), 0);
    }

    const std::string eventId = MakeIdentifier("AUD");
    const std::string occurredAt = CurrentTimestamp();

    const std::string eventHash = ChainChecksum(
        previousHash + "|" +
        eventId + "|" +
        occurredAt + "|" +
        actor + "|" +
        category + "|" +
        action + "|" +
        objectType + "|" +
        objectId + "|" +
        payload
    );

    Statement statement(
        handle,
        "INSERT INTO audit_events("
        "event_id, occurred_at, actor, category, action, "
        "object_type, object_id, payload, previous_hash, event_hash"
        ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
    );

    if (!statement.IsValid())
    {
        return SqliteError(
            handle,
            statement.Status(),
            "无法准备审计事件写入"
        );
    }

    BindText(statement.Get(), 1, eventId);
    BindText(statement.Get(), 2, occurredAt);
    BindText(statement.Get(), 3, actor);
    BindText(statement.Get(), 4, category);
    BindText(statement.Get(), 5, action);
    BindText(statement.Get(), 6, objectType);
    BindText(statement.Get(), 7, objectId);
    BindText(statement.Get(), 8, payload);
    BindText(statement.Get(), 9, previousHash);
    BindText(statement.Get(), 10, eventHash);

    const int result = sqlite3_step(statement.Get());

    return result == SQLITE_DONE
        ? StorageStatus::Ok()
        : SqliteError(
            handle,
            result,
            "无法写入审计事件"
        );
}

std::vector<AuditEventRecord> AuditRepository::Recent(
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<AuditEventRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT sequence, event_id, occurred_at, actor, category, "
        "action, object_type, object_id, payload, previous_hash, "
        "event_hash FROM audit_events "
        "ORDER BY sequence DESC LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(statement.Get(), 1, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        AuditEventRecord record;
        record.sequence = sqlite3_column_int64(statement.Get(), 0);
        record.eventId = ColumnText(statement.Get(), 1);
        record.occurredAt = ColumnText(statement.Get(), 2);
        record.actor = ColumnText(statement.Get(), 3);
        record.category = ColumnText(statement.Get(), 4);
        record.action = ColumnText(statement.Get(), 5);
        record.objectType = ColumnText(statement.Get(), 6);
        record.objectId = ColumnText(statement.Get(), 7);
        record.payload = ColumnText(statement.Get(), 8);
        record.previousHash = ColumnText(statement.Get(), 9);
        record.eventHash = ColumnText(statement.Get(), 10);
        records.push_back(std::move(record));
    }

    return records;
}

ObjectRepository::ObjectRepository(Database& database)
    : database_(database)
{
}

StorageStatus ObjectRepository::Save(
    const DomainObjectRecord& record,
    const std::string& actor
)
{
    if (record.id.empty() ||
        record.objectType.empty() ||
        record.title.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "对象编号、类型和标题不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT INTO objects("
            "id, object_type, title, description, status, priority, "
            "owner, occurred_at, valid_from, valid_to, known_at, "
            "time_precision, created_at, updated_at, deleted"
            ") VALUES("
            "?, ?, ?, ?, ?, ?, ?, NULLIF(?, ''), NULLIF(?, ''), "
            "NULLIF(?, ''), NULLIF(?, ''), ?, "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP), "
            "CURRENT_TIMESTAMP, ?"
            ") ON CONFLICT(id) DO UPDATE SET "
            "object_type=excluded.object_type, "
            "title=excluded.title, "
            "description=excluded.description, "
            "status=excluded.status, "
            "priority=excluded.priority, "
            "owner=excluded.owner, "
            "occurred_at=excluded.occurred_at, "
            "valid_from=excluded.valid_from, "
            "valid_to=excluded.valid_to, "
            "known_at=excluded.known_at, "
            "time_precision=excluded.time_precision, "
            "updated_at=CURRENT_TIMESTAMP, "
            "deleted=excluded.deleted;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备业务对象保存"
            );
        }

        BindText(statement.Get(), 1, record.id);
        BindText(statement.Get(), 2, record.objectType);
        BindText(statement.Get(), 3, record.title);
        BindText(statement.Get(), 4, record.description);
        BindText(statement.Get(), 5, record.status);
        BindText(statement.Get(), 6, record.priority);
        BindText(statement.Get(), 7, record.owner);
        BindText(statement.Get(), 8, record.occurredAt);
        BindText(statement.Get(), 9, record.validFrom);
        BindText(statement.Get(), 10, record.validTo);
        BindText(statement.Get(), 11, record.knownAt);
        BindText(statement.Get(), 12, record.timePrecision);
        BindText(statement.Get(), 13, record.createdAt);
        sqlite3_bind_int(
            statement.Get(),
            14,
            record.deleted ? 1 : 0
        );

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法保存业务对象"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "object",
            "save",
            record.objectType,
            record.id,
            "{\"title\":\"" +
                EscapeJson(record.title) +
                "\"}"
        );
    });
}

std::optional<DomainObjectRecord> ObjectRepository::FindById(
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
        std::string("SELECT ") + ObjectColumns() +
        " FROM objects WHERE id=?";

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

    return ReadObject(statement.Get());
}

std::vector<DomainObjectRecord> ObjectRepository::List(
    const std::string& objectType,
    bool includeDeleted,
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<DomainObjectRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    std::string sql =
        std::string("SELECT ") + ObjectColumns() +
        " FROM objects WHERE (?='' OR object_type=?)";

    if (!includeDeleted)
    {
        sql += " AND deleted=0";
    }

    sql += " ORDER BY updated_at DESC LIMIT ?;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return records;
    }

    BindText(statement.Get(), 1, objectType);
    BindText(statement.Get(), 2, objectType);
    sqlite3_bind_int(statement.Get(), 3, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        records.push_back(ReadObject(statement.Get()));
    }

    return records;
}

StorageStatus ObjectRepository::SoftDelete(
    const std::string& id,
    const std::string& reason,
    const std::string& actor
)
{
    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE objects "
            "SET deleted=1, updated_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND deleted=0;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备对象软删除"
            );
        }

        BindText(statement.Get(), 1, id);
        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(handle, result, "无法删除对象");
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "对象不存在或已经删除"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "object",
            "soft_delete",
            "object",
            id,
            "{\"reason\":\"" +
                EscapeJson(reason) +
                "\"}"
        );
    });
}

StorageStatus ObjectRepository::Restore(
    const std::string& id,
    const std::string& actor
)
{
    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE objects "
            "SET deleted=0, updated_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND deleted=1;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备对象恢复"
            );
        }

        BindText(statement.Get(), 1, id);
        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(handle, result, "无法恢复对象");
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "回收站中不存在该对象"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "object",
            "restore",
            "object",
            id,
            "{}"
        );
    });
}

StorageStatus ObjectRepository::LinkEvidence(
    const EvidenceLinkRecord& link,
    const std::string& actor
)
{
    if (link.objectId.empty() || link.evidenceId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "对象编号和证据编号不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT INTO object_evidence("
            "object_id, evidence_id, role, note, created_at"
            ") VALUES(?, ?, ?, ?, CURRENT_TIMESTAMP) "
            "ON CONFLICT(object_id, evidence_id, role) "
            "DO UPDATE SET note=excluded.note;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备证据关联"
            );
        }

        BindText(statement.Get(), 1, link.objectId);
        BindText(statement.Get(), 2, link.evidenceId);
        BindText(
            statement.Get(),
            3,
            link.role.empty() ? "support" : link.role
        );
        BindText(statement.Get(), 4, link.note);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(handle, result, "无法关联证据");
        }

        return AuditRepository(database_).Append(
            actor,
            "evidence",
            "link",
            "object",
            link.objectId,
            "{\"evidence_id\":\"" +
                EscapeJson(link.evidenceId) +
                "\",\"role\":\"" +
                EscapeJson(link.role) +
                "\"}"
        );
    });
}

std::vector<EvidenceLinkRecord> ObjectRepository::EvidenceLinks(
    const std::string& objectId
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<EvidenceLinkRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT object_id, evidence_id, role, note, created_at "
        "FROM object_evidence WHERE object_id=? "
        "ORDER BY created_at DESC;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    BindText(statement.Get(), 1, objectId);

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        EvidenceLinkRecord record;
        record.objectId = ColumnText(statement.Get(), 0);
        record.evidenceId = ColumnText(statement.Get(), 1);
        record.role = ColumnText(statement.Get(), 2);
        record.note = ColumnText(statement.Get(), 3);
        record.createdAt = ColumnText(statement.Get(), 4);
        records.push_back(std::move(record));
    }

    return records;
}

EvidenceRepository::EvidenceRepository(Database& database)
    : database_(database)
{
}

StorageStatus EvidenceRepository::Save(
    const EvidenceRecord& record,
    const std::string& actor
)
{
    if (record.id.empty() || record.title.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "证据编号和标题不能为空"
        );
    }

    const std::string reviewState =
        record.reviewState.empty()
            ? "unverified"
            : record.reviewState;

    if (!IsValidEvidenceReviewState(reviewState))
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "无效的证据审查状态"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT INTO evidence("
            "id, source_id, file_id, title, quote, anchor, "
            "fingerprint, review_state, created_at, updated_at, deleted"
            ") VALUES("
            "?, NULLIF(?, ''), NULLIF(?, ''), ?, ?, ?, ?, ?, "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP), "
            "CURRENT_TIMESTAMP, ?"
            ") ON CONFLICT(id) DO UPDATE SET "
            "source_id=excluded.source_id, "
            "file_id=excluded.file_id, "
            "title=excluded.title, "
            "quote=excluded.quote, "
            "anchor=excluded.anchor, "
            "fingerprint=excluded.fingerprint, "
            "review_state=excluded.review_state, "
            "updated_at=CURRENT_TIMESTAMP, "
            "deleted=excluded.deleted;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备证据保存"
            );
        }

        BindText(statement.Get(), 1, record.id);
        BindText(statement.Get(), 2, record.sourceId);
        BindText(statement.Get(), 3, record.fileId);
        BindText(statement.Get(), 4, record.title);
        BindText(statement.Get(), 5, record.quote);
        BindText(statement.Get(), 6, record.anchor);
        BindText(statement.Get(), 7, record.fingerprint);
        BindText(
            statement.Get(),
            8,
            reviewState
        );
        BindText(statement.Get(), 9, record.createdAt);
        sqlite3_bind_int(
            statement.Get(),
            10,
            record.deleted ? 1 : 0
        );

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(handle, result, "无法保存证据");
        }

        return AuditRepository(database_).Append(
            actor,
            "evidence",
            "save",
            "evidence",
            record.id,
            "{\"title\":\"" +
                EscapeJson(record.title) +
                "\"}"
        );
    });
}

std::optional<EvidenceRecord> EvidenceRepository::FindById(
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
        std::string("SELECT ") + EvidenceColumns() +
        " FROM evidence WHERE id=?";

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

    return ReadEvidence(statement.Get());
}

std::vector<EvidenceRecord> EvidenceRepository::ListForReview(
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<EvidenceRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    const std::string sql =
        std::string("SELECT ") + EvidenceColumns() +
        " FROM evidence WHERE deleted=0 "
        "AND review_state IN "
        "('unverified', 'needs_review', 'changed') "
        "ORDER BY updated_at DESC LIMIT ?;";

    Statement statement(handle, sql);

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(statement.Get(), 1, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        records.push_back(ReadEvidence(statement.Get()));
    }

    return records;
}

StorageStatus EvidenceRepository::SetReviewState(
    const std::string& id,
    const std::string& state,
    const std::string& actor
)
{
    if (id.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "证据编号不能为空"
        );
    }

    if (!IsValidEvidenceReviewState(state))
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "无效的证据审查状态"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE evidence SET review_state=?, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE id=? AND deleted=0;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备证据状态更新"
            );
        }

        BindText(statement.Get(), 1, state);
        BindText(statement.Get(), 2, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法更新证据状态"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "证据不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "evidence",
            "review_state",
            "evidence",
            id,
            "{\"state\":\"" +
                EscapeJson(state) +
                "\"}"
        );
    });
}

}
