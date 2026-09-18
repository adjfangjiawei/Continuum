#include "ContentParser.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-15-test";

    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base / "source");

    {
        std::ofstream markdown(
            base / "source" / "sample.md",
            std::ios::binary
        );
        markdown
            << "# Project Decision\n\n"
            << "**Approved** migration plan.\n\n"
            << "- First item\n"
            << "- Second item\n";
    }

    {
        std::ofstream email(
            base / "source" / "message.eml",
            std::ios::binary
        );
        email
            << "From: alice@example.local\r\n"
            << "To: team@example.local\r\n"
            << "Subject: Migration approved\r\n"
            << "Content-Type: text/plain; charset=UTF-8\r\n"
            << "\r\n"
            << "The migration plan was approved.\r\n";
    }

    {
        std::ofstream html(
            base / "source" / "page.html",
            std::ios::binary
        );
        html
            << "<html><body><h1>Status</h1>"
            << "<p>Ready &amp; verified.</p>"
            << "<script>hidden()</script>"
            << "</body></html>";
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
    source.id = "SRC-TEST";
    source.name = "Parser Test";
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
        std::cerr
            << "扫描结果异常: added="
            << scan.added
            << " jobs="
            << scan.parseJobs
            << " failed="
            << scan.failed
            << "\n";
        return 3;
    }

    ParseService parser(database, files);
    const int completed = parser.ExecutePending(10, "test");

    if (completed != 3)
    {
        std::cerr
            << "解析任务完成数量异常: "
            << completed
            << "\n";
        return 4;
    }

    ParsedContentRepository content(database);

    const auto markdownFile =
        files.FindByPath("SRC-TEST", "sample.md");

    if (!markdownFile)
    {
        std::cerr << "未找到 Markdown 文件记录\n";
        return 5;
    }

    const std::string markdownText =
        content.ContentForFile(markdownFile->id);

    if (markdownText.find("Project Decision") ==
            std::string::npos ||
        markdownText.find("Approved") ==
            std::string::npos ||
        markdownText.find("**") !=
            std::string::npos)
    {
        std::cerr << "Markdown 解析结果异常\n";
        return 6;
    }

    const auto emailFile =
        files.FindByPath("SRC-TEST", "message.eml");

    if (!emailFile)
    {
        std::cerr << "未找到邮件文件记录\n";
        return 7;
    }

    const std::string emailText =
        content.ContentForFile(emailFile->id);

    if (emailText.find("Migration approved") ==
            std::string::npos ||
        emailText.find("alice@example.local") ==
            std::string::npos)
    {
        std::cerr << "邮件解析结果异常\n";
        return 8;
    }

    const auto htmlFile =
        files.FindByPath("SRC-TEST", "page.html");

    if (!htmlFile)
    {
        std::cerr << "未找到 HTML 文件记录\n";
        return 9;
    }

    const std::string htmlText =
        content.ContentForFile(htmlFile->id);

    if (htmlText.find("Ready & verified") ==
            std::string::npos ||
        htmlText.find("hidden") !=
            std::string::npos)
    {
        std::cerr << "HTML 解析结果异常\n";
        return 10;
    }

    if (content.SectionsForFile(markdownFile->id).empty())
    {
        std::cerr << "解析分段未保存\n";
        return 11;
    }

    const auto parsedFile =
        files.FindById(markdownFile->id);

    if (!parsedFile ||
        parsedFile->parseState != "parsed")
    {
        std::cerr << "文件解析状态未更新\n";
        return 12;
    }

    database.Close();
    std::filesystem::remove_all(base, error);

    std::cout << "round-15 parser tests passed\n";
    return 0;
}
