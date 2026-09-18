#include "ContentParser.h"
#include "SearchService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-16-test";

    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base / "source");

    {
        std::ofstream first(
            base / "source" / "decision.md",
            std::ios::binary
        );

        first
            << "# Migration Decision\n\n"
            << "The database migration plan was approved.\n"
            << "The deployment will start on Monday.\n";
    }

    {
        std::ofstream second(
            base / "source" / "meeting.txt",
            std::ios::binary
        );

        second
            << "Weekly planning meeting\n"
            << "The team discussed testing and release tasks.\n";
    }

    {
        std::ofstream third(
            base / "source" / "notice.eml",
            std::ios::binary
        );

        third
            << "From: manager@example.local\r\n"
            << "To: team@example.local\r\n"
            << "Subject: Security review\r\n"
            << "Content-Type: text/plain; charset=UTF-8\r\n"
            << "\r\n"
            << "The security review was completed.\r\n";
    }

    Database database;

    const auto open =
        database.Open((base / "test.db").u8string());

    if (!open.success)
    {
        std::cerr << open.message << "\n";
        return 1;
    }

    DataSourceRepository sources(database);
    FileRepository files(database);

    DataSourceRecord source;
    source.id = "SRC-SEARCH";
    source.name = "Search Test";
    source.sourceType = "local_folder";
    source.rootPath = (base / "source").u8string();

    if (!sources.Save(source, "test").success)
    {
        std::cerr << "无法保存测试数据源\n";
        return 2;
    }

    FileScanner scanner(database, sources, files);
    const auto scan = scanner.ScanSource(source.id);

    if (scan.added != 3 ||
        scan.parseJobs != 3 ||
        scan.failed != 0)
    {
        std::cerr << "测试文件扫描失败\n";
        return 3;
    }

    ParseService parser(database, files);

    if (parser.ExecutePending(10, "test") != 3)
    {
        std::cerr << "测试文件解析失败\n";
        return 4;
    }

    SearchService search(database);

    const auto availability = search.CheckAvailability();

    if (!availability.success)
    {
        std::cerr
            << "FTS5 不可用: "
            << availability.message
            << "\n";
        return 5;
    }

    if (search.PendingCount() != 3)
    {
        std::cerr
            << "解析结果未进入索引队列，pending="
            << search.PendingCount()
            << "\n";
        return 6;
    }

    const auto sync = search.SyncPending(20, "test");

    if (sync.indexed != 3 ||
        sync.failed != 0 ||
        search.IndexedDocumentCount() != 3)
    {
        std::cerr
            << "增量索引同步失败，indexed="
            << sync.indexed
            << " failed="
            << sync.failed
            << "\n";
        return 7;
    }

    SearchRequest migration;
    migration.query = "migration approved";

    const auto migrationResult = search.Search(migration);

    if (!migrationResult.status.success ||
        migrationResult.total != 1 ||
        migrationResult.hits.size() != 1)
    {
        std::cerr << "普通关键词搜索失败\n";
        return 8;
    }

    if (migrationResult.hits[0].title.find(
            "Migration Decision"
        ) == std::string::npos ||
        migrationResult.hits[0].snippet.find(
            "<mark>"
        ) == std::string::npos)
    {
        std::cerr << "搜索标题或高亮摘要异常\n";
        return 9;
    }

    SearchRequest filtered;
    filtered.query = "review";
    filtered.sourceId = "SRC-SEARCH";
    filtered.mediaType = "message/rfc822";

    const auto filteredResult = search.Search(filtered);

    if (!filteredResult.status.success ||
        filteredResult.total != 1 ||
        filteredResult.hits[0].subject.find(
            "Security review"
        ) == std::string::npos)
    {
        std::cerr << "媒体类型过滤搜索失败\n";
        return 10;
    }

    SearchRequest advanced;
    advanced.query = "testing OR deployment";
    advanced.advancedSyntax = true;

    const auto advancedResult = search.Search(advanced);

    if (!advancedResult.status.success ||
        advancedResult.total != 2)
    {
        std::cerr << "高级 FTS 查询失败\n";
        return 11;
    }

    const auto decisionFile =
        files.FindByPath(
            "SRC-SEARCH",
            "decision.md"
        );

    if (!decisionFile)
    {
        std::cerr << "找不到待删除索引文件\n";
        return 12;
    }

    ParsedContentRepository content(database);

    if (!content.RemoveForFile(
            decisionFile->id,
            "test"
        ).success)
    {
        std::cerr << "删除解析内容失败\n";
        return 13;
    }

    const auto deleteSync =
        search.SyncPending(20, "test");

    if (deleteSync.removed != 1)
    {
        std::cerr << "删除索引同步失败\n";
        return 14;
    }

    const auto afterDelete = search.Search(migration);

    if (!afterDelete.status.success ||
        afterDelete.total != 0)
    {
        std::cerr << "索引删除后仍返回旧结果\n";
        return 15;
    }

    const auto rebuild = search.Rebuild("test");

    if (!rebuild.success ||
        search.IndexedDocumentCount() != 2)
    {
        std::cerr << "索引重建失败\n";
        return 16;
    }

    database.Close();
    std::filesystem::remove_all(base, error);

    std::cout << "round-16 search tests passed\n";
    return 0;
}
