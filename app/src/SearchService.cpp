#include "SearchService.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

#include <sqlite3.h>

namespace continuum
{
namespace
{

class Statement final
{
public:
    Statement(
        sqlite3* database,
        const std::string& sql
    )
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

std::string ColumnText(
    sqlite3_stmt* statement,
    int column
)
{
    const auto* value = sqlite3_column_text(statement, column);

    return value == nullptr
        ? std::string()
        : reinterpret_cast<const char*>(value);
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

std::string EscapeJson(const std::string& value)
{
    std::string output;
    output.reserve(value.size());

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            output += "\\\\";
            break;
        case '"':
            output += "\\\"";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            output += character;
            break;
        }
    }

    return output;
}

struct QueueItem
{
    std::string fileId;
    std::string operation;
};

std::vector<QueueItem> ReadPendingQueue(
    Database& database,
    int limit
)
{
    std::lock_guard<std::recursive_mutex> lock(database.Mutex());
    std::vector<QueueItem> items;
    sqlite3* handle = database.Handle();

    if (handle == nullptr)
    {
        return items;
    }

    Statement statement(
        handle,
        "SELECT file_id, operation "
        "FROM search_index_queue "
        "ORDER BY queued_at, file_id "
        "LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return items;
    }

    sqlite3_bind_int(
        statement.Get(),
        1,
        std::max(1, limit)
    );

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        QueueItem item;
        item.fileId = ColumnText(statement.Get(), 0);
        item.operation = ColumnText(statement.Get(), 1);
        items.push_back(std::move(item));
    }

    return items;
}

}

SearchService::SearchService(Database& database)
    : database_(database)
{
}

StorageStatus SearchService::EnsureSchema()
{
    if (!database_.IsOpen())
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "数据库尚未打开"
        );
    }

    auto status = database_.Execute(R"sql(
CREATE TABLE IF NOT EXISTS search_index_queue (
    file_id TEXT PRIMARY KEY,
    operation TEXT NOT NULL DEFAULT 'upsert',
    queued_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);
)sql");

    if (!status.success)
    {
        return status;
    }

    status = database_.Execute(R"sql(
CREATE VIRTUAL TABLE IF NOT EXISTS search_fts
USING fts5(
    file_id UNINDEXED,
    title,
    author,
    subject,
    content,
    tokenize='unicode61 remove_diacritics 2'
);
)sql");

    if (!status.success)
    {
        return StorageStatus::Error(
            status.code,
            "无法创建 FTS5 全文索引。"
            "请确认 SQLite 已启用 FTS5: " +
            status.message
        );
    }

    return StorageStatus::Ok();
}

StorageStatus SearchService::CheckAvailability()
{
    auto status = EnsureSchema();

    if (!status.success)
    {
        return status;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    Statement statement(
        handle,
        "SELECT count(*) FROM search_fts "
        "WHERE search_fts MATCH 'continuum_fts_probe';"
    );

    if (!statement.IsValid())
    {
        return SqliteError(
            handle,
            statement.Status(),
            "FTS5 可用性检查失败"
        );
    }

    const int result = sqlite3_step(statement.Get());

    if (result != SQLITE_ROW &&
        result != SQLITE_DONE)
    {
        return SqliteError(
            handle,
            result,
            "FTS5 查询测试失败"
        );
    }

    return StorageStatus::Ok();
}

StorageStatus SearchService::QueueFile(
    const std::string& fileId,
    const std::string& operation
)
{
    if (fileId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "索引队列缺少文件编号"
        );
    }

    if (operation != "upsert" &&
        operation != "delete")
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "不支持的索引操作: " + operation
        );
    }

    auto status = EnsureSchema();

    if (!status.success)
    {
        return status;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    Statement statement(
        handle,
        "INSERT INTO search_index_queue("
        "file_id, operation, queued_at"
        ") VALUES(?, ?, CURRENT_TIMESTAMP) "
        "ON CONFLICT(file_id) DO UPDATE SET "
        "operation=excluded.operation, "
        "queued_at=CURRENT_TIMESTAMP;"
    );

    if (!statement.IsValid())
    {
        return SqliteError(
            handle,
            statement.Status(),
            "无法准备索引任务入队"
        );
    }

    BindText(statement.Get(), 1, fileId);
    BindText(statement.Get(), 2, operation);

    const int result = sqlite3_step(statement.Get());

    return result == SQLITE_DONE
        ? StorageStatus::Ok()
        : SqliteError(
            handle,
            result,
            "无法加入索引队列"
        );
}

StorageStatus SearchService::IndexFile(
    const std::string& fileId,
    const std::string& actor
)
{
    if (fileId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "索引文件编号不能为空"
        );
    }

    auto status = EnsureSchema();

    if (!status.success)
    {
        return status;
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement remove(
            handle,
            "DELETE FROM search_fts WHERE file_id=?;"
        );

        if (!remove.IsValid())
        {
            return SqliteError(
                handle,
                remove.Status(),
                "无法准备旧索引删除"
            );
        }

        BindText(remove.Get(), 1, fileId);

        if (sqlite3_step(remove.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法删除旧全文索引"
            );
        }

        Statement insert(
            handle,
            "INSERT INTO search_fts("
            "file_id, title, author, subject, content"
            ") "
            "SELECT file_id, title, author, subject, content "
            "FROM parsed_documents "
            "WHERE file_id=?;"
        );

        if (!insert.IsValid())
        {
            return SqliteError(
                handle,
                insert.Status(),
                "无法准备全文索引写入"
            );
        }

        BindText(insert.Get(), 1, fileId);

        if (sqlite3_step(insert.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法写入全文索引"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "文件不存在解析内容，无法建立索引"
            );
        }

        Statement dequeue(
            handle,
            "DELETE FROM search_index_queue "
            "WHERE file_id=?;"
        );

        if (!dequeue.IsValid())
        {
            return SqliteError(
                handle,
                dequeue.Status(),
                "无法准备索引队列清理"
            );
        }

        BindText(dequeue.Get(), 1, fileId);

        if (sqlite3_step(dequeue.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法清理索引队列"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "search",
            "index",
            "file",
            fileId,
            "{}"
        );
    });
}

StorageStatus SearchService::RemoveFile(
    const std::string& fileId,
    const std::string& actor
)
{
    if (fileId.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "待移除索引的文件编号不能为空"
        );
    }

    auto status = EnsureSchema();

    if (!status.success)
    {
        return status;
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement remove(
            handle,
            "DELETE FROM search_fts WHERE file_id=?;"
        );

        if (!remove.IsValid())
        {
            return SqliteError(
                handle,
                remove.Status(),
                "无法准备全文索引移除"
            );
        }

        BindText(remove.Get(), 1, fileId);

        if (sqlite3_step(remove.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法移除全文索引"
            );
        }

        Statement dequeue(
            handle,
            "DELETE FROM search_index_queue "
            "WHERE file_id=?;"
        );

        if (!dequeue.IsValid())
        {
            return SqliteError(
                handle,
                dequeue.Status(),
                "无法准备索引队列清理"
            );
        }

        BindText(dequeue.Get(), 1, fileId);

        if (sqlite3_step(dequeue.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法清理索引删除任务"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "search",
            "remove_index",
            "file",
            fileId,
            "{}"
        );
    });
}

IndexSyncSummary SearchService::SyncPending(
    int limit,
    const std::string& actor
)
{
    IndexSyncSummary summary;

    const auto schemaStatus = EnsureSchema();

    if (!schemaStatus.success)
    {
        summary.failed = 1;
        summary.errors.push_back(schemaStatus.message);
        return summary;
    }

    const auto items = ReadPendingQueue(
        database_,
        std::max(1, limit)
    );

    for (const auto& item : items)
    {
        ++summary.visited;

        StorageStatus status;

        if (item.operation == "delete")
        {
            status = RemoveFile(item.fileId, actor);

            if (status.success)
            {
                ++summary.removed;
            }
        }
        else
        {
            status = IndexFile(item.fileId, actor);

            if (status.success)
            {
                ++summary.indexed;
            }
        }

        if (!status.success)
        {
            ++summary.failed;
            summary.errors.push_back(
                item.fileId + ": " + status.message
            );
        }
    }

    return summary;
}

StorageStatus SearchService::Rebuild(
    const std::string& actor
)
{
    auto status = EnsureSchema();

    if (!status.success)
    {
        return status;
    }

    status = database_.Transaction([&]() {
        auto clearStatus = database_.Execute(
            "DELETE FROM search_fts;"
        );

        if (!clearStatus.success)
        {
            return clearStatus;
        }

        clearStatus = database_.Execute(
            "DELETE FROM search_index_queue;"
        );

        if (!clearStatus.success)
        {
            return clearStatus;
        }

        clearStatus = database_.Execute(R"sql(
INSERT INTO search_index_queue(
    file_id,
    operation,
    queued_at
)
SELECT
    file_id,
    'upsert',
    CURRENT_TIMESTAMP
FROM parsed_documents;
)sql");

        if (!clearStatus.success)
        {
            return clearStatus;
        }

        return AuditRepository(database_).Append(
            actor,
            "search",
            "rebuild_started",
            "search_index",
            "default",
            "{}"
        );
    });

    if (!status.success)
    {
        return status;
    }

    while (PendingCount() > 0)
    {
        const auto summary = SyncPending(500, actor);

        if (summary.failed > 0)
        {
            return StorageStatus::Error(
                SQLITE_ERROR,
                summary.errors.empty()
                    ? "全文索引重建失败"
                    : summary.errors.front()
            );
        }

        if (summary.visited == 0)
        {
            return StorageStatus::Error(
                SQLITE_ERROR,
                "索引队列无法继续处理"
            );
        }
    }

    return AuditRepository(database_).Append(
        actor,
        "search",
        "rebuild_completed",
        "search_index",
        "default",
        "{\"documents\":" +
            std::to_string(IndexedDocumentCount()) +
            "}"
    );
}

std::string SearchService::Trim(
    const std::string& value
)
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

std::string SearchService::BuildSafeQuery(
    const std::string& query
)
{
    std::istringstream stream(query);
    std::vector<std::string> terms;
    std::string term;

    while (stream >> term)
    {
        std::string escaped;
        escaped.reserve(term.size());

        for (const char character : term)
        {
            if (character == '"')
            {
                escaped += "\"\"";
            }
            else
            {
                escaped += character;
            }
        }

        if (!escaped.empty())
        {
            terms.push_back("\"" + escaped + "\"");
        }
    }

    std::ostringstream output;

    for (std::size_t index = 0;
         index < terms.size();
         ++index)
    {
        if (index > 0)
        {
            output << " AND ";
        }

        output << terms[index];
    }

    return output.str();
}

SearchResponse SearchService::Search(
    const SearchRequest& request
)
{
    SearchResponse response;

    auto status = EnsureSchema();

    if (!status.success)
    {
        response.status = status;
        return response;
    }

    const std::string originalQuery = Trim(request.query);

    if (originalQuery.empty())
    {
        response.status = StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "搜索关键词不能为空"
        );
        return response;
    }

    const auto sync = SyncPending(1000, "search");

    if (sync.failed > 0)
    {
        response.status = StorageStatus::Error(
            SQLITE_ERROR,
            sync.errors.empty()
                ? "搜索前索引同步失败"
                : sync.errors.front()
        );
        return response;
    }

    response.normalizedQuery =
        request.advancedSyntax
            ? originalQuery
            : BuildSafeQuery(originalQuery);

    if (response.normalizedQuery.empty())
    {
        response.status = StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "搜索关键词无有效内容"
        );
        return response;
    }

    const int limit = std::max(
        1,
        std::min(200, request.limit)
    );
    const int offset = std::max(0, request.offset);

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    Statement countStatement(
        handle,
        "SELECT count(*) "
        "FROM search_fts "
        "JOIN files f ON f.id=search_fts.file_id "
        "WHERE search_fts MATCH ? "
        "AND f.deleted=0 "
        "AND (?='' OR f.source_id=?) "
        "AND (?='' OR f.media_type=?);"
    );

    if (!countStatement.IsValid())
    {
        response.status = SqliteError(
            handle,
            countStatement.Status(),
            "无法准备搜索结果计数"
        );
        return response;
    }

    BindText(
        countStatement.Get(),
        1,
        response.normalizedQuery
    );
    BindText(countStatement.Get(), 2, request.sourceId);
    BindText(countStatement.Get(), 3, request.sourceId);
    BindText(countStatement.Get(), 4, request.mediaType);
    BindText(countStatement.Get(), 5, request.mediaType);

    int result = sqlite3_step(countStatement.Get());

    if (result != SQLITE_ROW)
    {
        response.status = SqliteError(
            handle,
            result,
            "全文查询语法无效或计数失败"
        );
        return response;
    }

    response.total =
        sqlite3_column_int(countStatement.Get(), 0);

    Statement searchStatement(
        handle,
        "SELECT "
        "search_fts.file_id, "
        "f.source_id, "
        "f.relative_path, "
        "f.display_name, "
        "f.media_type, "
        "search_fts.title, "
        "search_fts.author, "
        "search_fts.subject, "
        "snippet("
        "search_fts, 4, '<mark>', '</mark>', ' … ', 24"
        "), "
        "bm25(search_fts, 0.0, 4.0, 2.0, 2.0, 1.0) "
        "FROM search_fts "
        "JOIN files f ON f.id=search_fts.file_id "
        "WHERE search_fts MATCH ? "
        "AND f.deleted=0 "
        "AND (?='' OR f.source_id=?) "
        "AND (?='' OR f.media_type=?) "
        "ORDER BY "
        "bm25(search_fts, 0.0, 4.0, 2.0, 2.0, 1.0), "
        "f.display_name COLLATE NOCASE "
        "LIMIT ? OFFSET ?;"
    );

    if (!searchStatement.IsValid())
    {
        response.status = SqliteError(
            handle,
            searchStatement.Status(),
            "无法准备全文搜索"
        );
        return response;
    }

    BindText(
        searchStatement.Get(),
        1,
        response.normalizedQuery
    );
    BindText(searchStatement.Get(), 2, request.sourceId);
    BindText(searchStatement.Get(), 3, request.sourceId);
    BindText(searchStatement.Get(), 4, request.mediaType);
    BindText(searchStatement.Get(), 5, request.mediaType);
    sqlite3_bind_int(searchStatement.Get(), 6, limit);
    sqlite3_bind_int(searchStatement.Get(), 7, offset);

    while ((result = sqlite3_step(searchStatement.Get())) ==
           SQLITE_ROW)
    {
        SearchHit hit;
        hit.fileId = ColumnText(searchStatement.Get(), 0);
        hit.sourceId = ColumnText(searchStatement.Get(), 1);
        hit.relativePath = ColumnText(searchStatement.Get(), 2);
        hit.displayName = ColumnText(searchStatement.Get(), 3);
        hit.mediaType = ColumnText(searchStatement.Get(), 4);
        hit.title = ColumnText(searchStatement.Get(), 5);
        hit.author = ColumnText(searchStatement.Get(), 6);
        hit.subject = ColumnText(searchStatement.Get(), 7);
        hit.snippet = ColumnText(searchStatement.Get(), 8);
        hit.score = sqlite3_column_double(
            searchStatement.Get(),
            9
        );
        response.hits.push_back(std::move(hit));
    }

    if (result != SQLITE_DONE)
    {
        response.hits.clear();
        response.status = SqliteError(
            handle,
            result,
            "全文搜索执行失败"
        );
        return response;
    }

    response.status = StorageStatus::Ok();

    AuditRepository(database_).Append(
        "search",
        "search",
        "query",
        "search_index",
        "default",
        "{\"query\":\"" +
            EscapeJson(originalQuery) +
            "\",\"results\":" +
            std::to_string(response.total) +
            "}"
    );

    return response;
}

int SearchService::IndexedDocumentCount() const
{
    auto* self = const_cast<SearchService*>(this);

    if (!self->EnsureSchema().success)
    {
        return 0;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    Statement statement(
        handle,
        "SELECT count(*) FROM search_fts;"
    );

    if (!statement.IsValid() ||
        sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return 0;
    }

    return sqlite3_column_int(statement.Get(), 0);
}

int SearchService::PendingCount() const
{
    auto* self = const_cast<SearchService*>(this);

    if (!self->EnsureSchema().success)
    {
        return 0;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    Statement statement(
        handle,
        "SELECT count(*) FROM search_index_queue;"
    );

    if (!statement.IsValid() ||
        sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return 0;
    }

    return sqlite3_column_int(statement.Get(), 0);
}

}
