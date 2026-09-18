#include "InvestigationService.h"

#include "FileScanner.h"
#include "SecurityService.h"

#include <algorithm>
#include <chrono>
#include <deque>
#include <iomanip>
#include <set>
#include <sstream>
#include <unordered_map>
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

void Bind(
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

std::string Text(sqlite3_stmt* statement, int column)
{
    const auto* value = sqlite3_column_text(statement, column);

    return value == nullptr
        ? std::string()
        : reinterpret_cast<const char*>(value);
}

StorageStatus Failure(
    sqlite3* database,
    int code,
    const std::string& context
)
{
    return StorageStatus::Error(
        code,
        context + ": " +
            (database == nullptr
                ? std::string("数据库尚未打开")
                : std::string(sqlite3_errmsg(database)))
    );
}

std::string EscapeJson(const std::string& value)
{
    std::string result;

    for (const char character : value)
    {
        switch (character)
        {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += character; break;
        }
    }

    return result;
}

std::string RelationId(
    const std::string& source,
    const std::string& target,
    const std::string& type
)
{
    return "REL-" +
        ContentHasher::Sha256Text(
            source + "\n" + target + "\n" + type
        ).substr(0, 24);
}

bool ValidRelationType(const std::string& value)
{
    return value == "support" ||
        value == "oppose" ||
        value == "replace" ||
        value == "depend" ||
        value == "block" ||
        value == "impact";
}

}

InvestigationService::InvestigationService(Database& database)
    : database_(database)
{
}

std::vector<ConflictViewRecord>
InvestigationService::ListConflicts(
    const std::string& status,
    const std::string& type,
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<ConflictViewRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT "
        "c.id, c.conflict_type, c.severity, c.title, "
        "c.description, COALESCE(c.subject_object_id,''), "
        "c.property_name, c.status, c.resolution, "
        "c.resolution_note, c.detected_at, "
        "COALESCE(a.value_text,''), "
        "COALESCE(a.source_label,''), "
        "COALESCE(b.value_text,''), "
        "COALESCE(b.source_label,'') "
        "FROM conflicts c "
        "LEFT JOIN conflict_claims a "
        "ON a.conflict_id=c.id AND a.side_key='A' "
        "LEFT JOIN conflict_claims b "
        "ON b.conflict_id=c.id AND b.side_key='B' "
        "WHERE (?='' OR c.status=?) "
        "AND (?='' OR c.conflict_type=?) "
        "ORDER BY "
        "CASE c.severity "
        "WHEN 'high' THEN 0 "
        "WHEN 'medium' THEN 1 ELSE 2 END, "
        "c.detected_at DESC "
        "LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    Bind(statement.Get(), 1, status);
    Bind(statement.Get(), 2, status);
    Bind(statement.Get(), 3, type);
    Bind(statement.Get(), 4, type);
    sqlite3_bind_int(
        statement.Get(),
        5,
        std::max(1, std::min(5000, limit))
    );

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        ConflictViewRecord record;
        record.id = Text(statement.Get(), 0);
        record.type = Text(statement.Get(), 1);
        record.severity = Text(statement.Get(), 2);
        record.title = Text(statement.Get(), 3);
        record.description = Text(statement.Get(), 4);
        record.objectId = Text(statement.Get(), 5);
        record.propertyName = Text(statement.Get(), 6);
        record.status = Text(statement.Get(), 7);
        record.resolution = Text(statement.Get(), 8);
        record.resolutionNote = Text(statement.Get(), 9);
        record.detectedAt = Text(statement.Get(), 10);
        record.claimA = Text(statement.Get(), 11);
        record.sourceA = Text(statement.Get(), 12);
        record.claimB = Text(statement.Get(), 13);
        record.sourceB = Text(statement.Get(), 14);
        records.push_back(std::move(record));
    }

    return records;
}

StorageStatus InvestigationService::DetectConflicts(
    const std::string& actor
)
{
    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        const char* statements[] = {
            R"sql(
INSERT INTO conflicts(
    id, conflict_type, severity, title, description,
    subject_object_id, property_name, status
)
SELECT
    'CF-MISSING-' || o.id,
    'missing_evidence',
    CASE
        WHEN o.priority IN ('critical','high') THEN 'high'
        ELSE 'medium'
    END,
    o.title || ' 缺少关联证据',
    '业务对象没有任何支持证据',
    o.id,
    'evidence',
    'open'
FROM objects o
WHERE o.deleted=0
AND NOT EXISTS(
    SELECT 1
    FROM object_evidence oe
    JOIN evidence e ON e.id=oe.evidence_id
    WHERE oe.object_id=o.id
    AND e.deleted=0
    AND oe.role='support'
)
ON CONFLICT(id) DO UPDATE SET
    severity=excluded.severity,
    title=excluded.title,
    description=excluded.description,
    updated_at=CURRENT_TIMESTAMP;
)sql",
            R"sql(
INSERT INTO conflicts(
    id, conflict_type, severity, title, description,
    subject_object_id, property_name, status
)
SELECT
    'CF-TIME-' || o.id,
    'temporal_logic',
    'high',
    o.title || ' 的生效时间范围无效',
    '有效开始时间晚于有效结束时间',
    o.id,
    'validity',
    'open'
FROM objects o
WHERE o.deleted=0
AND COALESCE(o.valid_from,'')<>''
AND COALESCE(o.valid_to,'')<>''
AND o.valid_from>o.valid_to
ON CONFLICT(id) DO UPDATE SET
    title=excluded.title,
    description=excluded.description,
    updated_at=CURRENT_TIMESTAMP;
)sql",
            R"sql(
INSERT INTO conflict_claims(
    id, conflict_id, side_key, object_id,
    value_text, source_label
)
SELECT
    'CC-TIME-A-' || o.id,
    'CF-TIME-' || o.id,
    'A',
    o.id,
    o.valid_from,
    '对象有效开始时间'
FROM objects o
WHERE o.deleted=0
AND COALESCE(o.valid_from,'')<>''
AND COALESCE(o.valid_to,'')<>''
AND o.valid_from>o.valid_to
ON CONFLICT(conflict_id, side_key) DO UPDATE SET
    value_text=excluded.value_text,
    source_label=excluded.source_label;
)sql",
            R"sql(
INSERT INTO conflict_claims(
    id, conflict_id, side_key, object_id,
    value_text, source_label
)
SELECT
    'CC-TIME-B-' || o.id,
    'CF-TIME-' || o.id,
    'B',
    o.id,
    o.valid_to,
    '对象有效结束时间'
FROM objects o
WHERE o.deleted=0
AND COALESCE(o.valid_from,'')<>''
AND COALESCE(o.valid_to,'')<>''
AND o.valid_from>o.valid_to
ON CONFLICT(conflict_id, side_key) DO UPDATE SET
    value_text=excluded.value_text,
    source_label=excluded.source_label;
)sql",
            R"sql(
INSERT INTO conflicts(
    id, conflict_type, severity, title, description,
    subject_object_id, property_name, status
)
SELECT
    'CF-REFERENCE-' || o.id || '-' || e.id,
    'invalid_reference',
    'high',
    o.title || ' 引用了失效证据',
    e.title,
    o.id,
    'evidence',
    'open'
FROM objects o
JOIN object_evidence oe ON oe.object_id=o.id
JOIN evidence e ON e.id=oe.evidence_id
WHERE o.deleted=0
AND e.deleted=1
ON CONFLICT(id) DO UPDATE SET
    title=excluded.title,
    description=excluded.description,
    updated_at=CURRENT_TIMESTAMP;
)sql",
            R"sql(
INSERT INTO conflicts(
    id, conflict_type, severity, title, description,
    subject_object_id, property_name, status
)
SELECT
    'CF-STATUS-' || a.id || '-' || b.id,
    'status_value',
    'medium',
    a.title || ' 存在不同当前状态',
    a.status || ' 与 ' || b.status,
    a.id,
    'status',
    'open'
FROM objects a
JOIN objects b
ON a.object_type=b.object_type
AND lower(trim(a.title))=lower(trim(b.title))
AND a.id<b.id
WHERE a.deleted=0
AND b.deleted=0
AND a.status<>b.status
ON CONFLICT(id) DO UPDATE SET
    title=excluded.title,
    description=excluded.description,
    updated_at=CURRENT_TIMESTAMP;
)sql",
            R"sql(
INSERT INTO conflict_claims(
    id, conflict_id, side_key, object_id,
    value_text, source_label
)
SELECT
    'CC-STATUS-A-' || a.id || '-' || b.id,
    'CF-STATUS-' || a.id || '-' || b.id,
    'A',
    a.id,
    a.status,
    a.id
FROM objects a
JOIN objects b
ON a.object_type=b.object_type
AND lower(trim(a.title))=lower(trim(b.title))
AND a.id<b.id
WHERE a.deleted=0
AND b.deleted=0
AND a.status<>b.status
ON CONFLICT(conflict_id, side_key) DO UPDATE SET
    value_text=excluded.value_text,
    source_label=excluded.source_label;
)sql",
            R"sql(
INSERT INTO conflict_claims(
    id, conflict_id, side_key, object_id,
    value_text, source_label
)
SELECT
    'CC-STATUS-B-' || a.id || '-' || b.id,
    'CF-STATUS-' || a.id || '-' || b.id,
    'B',
    b.id,
    b.status,
    b.id
FROM objects a
JOIN objects b
ON a.object_type=b.object_type
AND lower(trim(a.title))=lower(trim(b.title))
AND a.id<b.id
WHERE a.deleted=0
AND b.deleted=0
AND a.status<>b.status
ON CONFLICT(conflict_id, side_key) DO UPDATE SET
    value_text=excluded.value_text,
    source_label=excluded.source_label;
)sql"
        };

        for (const char* sql : statements)
        {
            char* error = nullptr;
            const int result = sqlite3_exec(
                handle,
                sql,
                nullptr,
                nullptr,
                &error
            );

            if (result != SQLITE_OK)
            {
                std::string message =
                    error == nullptr
                        ? sqlite3_errmsg(handle)
                        : error;

                if (error != nullptr)
                {
                    sqlite3_free(error);
                }

                return StorageStatus::Error(
                    result,
                    "冲突检测失败: " + message
                );
            }
        }

        return AuditRepository(database_).Append(
            actor,
            "conflict",
            "detect",
            "workspace",
            std::string(),
            "{}"
        );
    });
}

StorageStatus InvestigationService::ResolveConflict(
    const std::string& id,
    const std::string& resolution,
    const std::string& note,
    const std::string& actor
)
{
    if (id.empty() || note.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "冲突编号和处理依据不能为空"
        );
    }

    const std::string status =
        resolution == "retain"
            ? "retained"
            : resolution == "insufficient"
                ? "insufficient"
                : "resolved";

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE conflicts SET "
            "status=?, resolution=?, resolution_note=?, "
            "resolved_at=CASE WHEN ?='resolved' "
            "THEN CURRENT_TIMESTAMP ELSE NULL END, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE id=?;"
        );

        if (!statement.IsValid())
        {
            return Failure(
                handle,
                statement.Status(),
                "无法准备冲突处理"
            );
        }

        Bind(statement.Get(), 1, status);
        Bind(statement.Get(), 2, resolution);
        Bind(statement.Get(), 3, note);
        Bind(statement.Get(), 4, status);
        Bind(statement.Get(), 5, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return Failure(handle, result, "无法处理冲突");
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "冲突记录不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "conflict",
            "resolve",
            "conflict",
            id,
            "{\"resolution\":\"" +
                EscapeJson(resolution) +
                "\",\"note\":\"" +
                EscapeJson(note) +
                "\"}"
        );
    });
}

std::vector<ChangeReviewViewRecord>
InvestigationService::ListChanges(
    const std::string& status,
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<ChangeReviewViewRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT "
        "cr.id, cr.file_id, f.display_name, "
        "cr.change_type, cr.status, cr.detected_at, "
        "COALESCE(pv.content,''), "
        "COALESCE(pd.content,''), "
        "(SELECT count(*) FROM change_review_evidence ce "
        " WHERE ce.review_id=cr.id), "
        "(SELECT count(DISTINCT oe.object_id) "
        " FROM change_review_evidence ce "
        " JOIN object_evidence oe "
        " ON oe.evidence_id=ce.evidence_id "
        " WHERE ce.review_id=cr.id) "
        "FROM change_reviews cr "
        "JOIN files f ON f.id=cr.file_id "
        "LEFT JOIN file_versions pv "
        "ON pv.id=cr.previous_version_id "
        "LEFT JOIN parsed_documents pd "
        "ON pd.file_id=cr.file_id "
        "WHERE (?='' OR cr.status=?) "
        "ORDER BY cr.detected_at DESC "
        "LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    Bind(statement.Get(), 1, status);
    Bind(statement.Get(), 2, status);
    sqlite3_bind_int(
        statement.Get(),
        3,
        std::max(1, std::min(5000, limit))
    );

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        ChangeReviewViewRecord record;
        record.id = Text(statement.Get(), 0);
        record.fileId = Text(statement.Get(), 1);
        record.fileName = Text(statement.Get(), 2);
        record.changeType = Text(statement.Get(), 3);
        record.status = Text(statement.Get(), 4);
        record.detectedAt = Text(statement.Get(), 5);
        record.previousContent = Text(statement.Get(), 6);
        record.currentContent = Text(statement.Get(), 7);
        record.affectedEvidence =
            sqlite3_column_int(statement.Get(), 8);
        record.affectedObjects =
            sqlite3_column_int(statement.Get(), 9);
        records.push_back(std::move(record));
    }

    return records;
}

StorageStatus InvestigationService::ResolveChange(
    const std::string& id,
    const std::string& resolution,
    const std::string& note,
    const std::string& actor
)
{
    const bool valid =
        resolution == "reanchored" ||
        resolution == "invalidated" ||
        resolution == "no_impact" ||
        resolution == "completed";

    if (id.empty() || !valid)
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "变化记录或处理方式无效"
        );
    }

    const std::string impactState =
        resolution == "completed"
            ? "no_impact"
            : resolution;

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement review(
            handle,
            "UPDATE change_reviews SET "
            "status=?, resolution_note=?, "
            "completed_at=CURRENT_TIMESTAMP, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE id=?;"
        );

        if (!review.IsValid())
        {
            return Failure(
                handle,
                review.Status(),
                "无法准备变化审查处理"
            );
        }

        Bind(review.Get(), 1, resolution);
        Bind(review.Get(), 2, note);
        Bind(review.Get(), 3, id);

        const int reviewResult = sqlite3_step(review.Get());

        if (reviewResult != SQLITE_DONE)
        {
            return Failure(
                handle,
                reviewResult,
                "无法处理变化审查"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "变化审查记录不存在"
            );
        }

        Statement impacts(
            handle,
            "UPDATE change_review_evidence "
            "SET impact_state=?, note=?, "
            "updated_at=CURRENT_TIMESTAMP "
            "WHERE review_id=?;"
        );

        if (!impacts.IsValid())
        {
            return Failure(
                handle,
                impacts.Status(),
                "无法准备证据影响处理"
            );
        }

        Bind(impacts.Get(), 1, impactState);
        Bind(impacts.Get(), 2, note);
        Bind(impacts.Get(), 3, id);

        const int impactResult = sqlite3_step(impacts.Get());

        if (impactResult != SQLITE_DONE)
        {
            return Failure(
                handle,
                impactResult,
                "无法更新证据影响"
            );
        }

        if (resolution == "invalidated")
        {
            Statement evidence(
                handle,
                "UPDATE evidence SET "
                "review_state='rejected', "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id IN ("
                "SELECT evidence_id "
                "FROM change_review_evidence "
                "WHERE review_id=?"
                ");"
            );

            if (!evidence.IsValid())
            {
                return Failure(
                    handle,
                    evidence.Status(),
                    "无法准备证据失效"
                );
            }

            Bind(evidence.Get(), 1, id);

            const int evidenceResult =
                sqlite3_step(evidence.Get());

            if (evidenceResult != SQLITE_DONE)
            {
                return Failure(
                    handle,
                    evidenceResult,
                    "无法使旧证据失效"
                );
            }
        }
        else if (
            resolution == "reanchored" ||
            resolution == "no_impact" ||
            resolution == "completed"
        )
        {
            Statement evidence(
                handle,
                "UPDATE evidence SET "
                "review_state='verified', "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id IN ("
                "SELECT evidence_id "
                "FROM change_review_evidence "
                "WHERE review_id=?"
                ");"
            );

            if (!evidence.IsValid())
            {
                return Failure(
                    handle,
                    evidence.Status(),
                    "无法准备证据确认"
                );
            }

            Bind(evidence.Get(), 1, id);

            const int evidenceResult =
                sqlite3_step(evidence.Get());

            if (evidenceResult != SQLITE_DONE)
            {
                return Failure(
                    handle,
                    evidenceResult,
                    "无法确认受影响证据"
                );
            }
        }

        return AuditRepository(database_).Append(
            actor,
            "change_review",
            "resolve",
            "change_review",
            id,
            "{\"resolution\":\"" +
                EscapeJson(resolution) +
                "\",\"note\":\"" +
                EscapeJson(note) +
                "\"}"
        );
    });
}

std::vector<RelationViewRecord>
InvestigationService::QueryRelations(
    const std::string& startObjectId,
    const std::string& targetObjectId,
    const std::string& relationType,
    int maximumDepth,
    bool includeInvalid
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<RelationViewRecord> all;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return all;
    }

    Statement statement(
        handle,
        "SELECT "
        "r.id, r.source_object_id, s.title, "
        "r.target_object_id, t.title, "
        "r.relation_type, r.status, r.note "
        "FROM object_relations r "
        "JOIN objects s ON s.id=r.source_object_id "
        "JOIN objects t ON t.id=r.target_object_id "
        "WHERE s.deleted=0 AND t.deleted=0 "
        "AND (?='' OR r.relation_type=?) "
        "AND (?=1 OR r.status<>'invalid') "
        "ORDER BY r.updated_at DESC;"
    );

    if (!statement.IsValid())
    {
        return all;
    }

    Bind(statement.Get(), 1, relationType);
    Bind(statement.Get(), 2, relationType);
    sqlite3_bind_int(
        statement.Get(),
        3,
        includeInvalid ? 1 : 0
    );

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        RelationViewRecord record;
        record.id = Text(statement.Get(), 0);
        record.sourceId = Text(statement.Get(), 1);
        record.sourceTitle = Text(statement.Get(), 2);
        record.targetId = Text(statement.Get(), 3);
        record.targetTitle = Text(statement.Get(), 4);
        record.type = Text(statement.Get(), 5);
        record.status = Text(statement.Get(), 6);
        record.note = Text(statement.Get(), 7);
        all.push_back(std::move(record));
    }

    if (startObjectId.empty())
    {
        return all;
    }

    maximumDepth = std::max(1, std::min(8, maximumDepth));

    std::set<std::string> visited;
    std::set<std::string> selectedIds;
    std::deque<std::pair<std::string, int>> queue;

    visited.insert(startObjectId);
    queue.push_back({startObjectId, 0});

    while (!queue.empty())
    {
        const auto current = queue.front();
        queue.pop_front();

        if (current.second >= maximumDepth)
        {
            continue;
        }

        for (const auto& relation : all)
        {
            if (relation.sourceId != current.first)
            {
                continue;
            }

            selectedIds.insert(relation.id);

            if (visited.insert(relation.targetId).second)
            {
                queue.push_back({
                    relation.targetId,
                    current.second + 1
                });
            }
        }
    }

    std::vector<RelationViewRecord> result;

    for (const auto& relation : all)
    {
        if (selectedIds.find(relation.id) ==
            selectedIds.end())
        {
            continue;
        }

        if (!targetObjectId.empty() &&
            relation.targetId != targetObjectId &&
            relation.sourceId != targetObjectId)
        {
            continue;
        }

        result.push_back(relation);
    }

    return result;
}

StorageStatus InvestigationService::SaveRelation(
    const std::string& sourceObjectId,
    const std::string& targetObjectId,
    const std::string& relationType,
    const std::string& note,
    const std::string& actor
)
{
    if (sourceObjectId.empty() ||
        targetObjectId.empty() ||
        sourceObjectId == targetObjectId ||
        !ValidRelationType(relationType))
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "关系起点、终点或类型无效"
        );
    }

    const std::string id =
        RelationId(
            sourceObjectId,
            targetObjectId,
            relationType
        );

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT INTO object_relations("
            "id, source_object_id, target_object_id, "
            "relation_type, status, note, created_by"
            ") VALUES(?, ?, ?, ?, 'confirmed', ?, ?) "
            "ON CONFLICT("
            "source_object_id, target_object_id, relation_type"
            ") DO UPDATE SET "
            "status='confirmed', "
            "note=excluded.note, "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!statement.IsValid())
        {
            return Failure(
                handle,
                statement.Status(),
                "无法准备关系保存"
            );
        }

        Bind(statement.Get(), 1, id);
        Bind(statement.Get(), 2, sourceObjectId);
        Bind(statement.Get(), 3, targetObjectId);
        Bind(statement.Get(), 4, relationType);
        Bind(statement.Get(), 5, note);
        Bind(statement.Get(), 6, actor);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return Failure(handle, result, "无法保存对象关系");
        }

        return AuditRepository(database_).Append(
            actor,
            "relation",
            "save",
            "object_relation",
            id,
            "{\"source\":\"" +
                EscapeJson(sourceObjectId) +
                "\",\"target\":\"" +
                EscapeJson(targetObjectId) +
                "\",\"type\":\"" +
                EscapeJson(relationType) +
                "\"}"
        );
    });
}

}
