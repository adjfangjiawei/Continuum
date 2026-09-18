#include "FileScanner.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    using namespace continuum;

    const std::string emptyDigest =
        ContentHasher::Sha256Text("");

    const std::string abcDigest =
        ContentHasher::Sha256Text("abc");

    if (emptyDigest !=
        "e3b0c44298fc1c149afbf4c8996fb924"
        "27ae41e4649b934ca495991b7852b855")
    {
        std::cerr << "空字符串 SHA-256 校验失败\n";
        return 1;
    }

    if (abcDigest !=
        "ba7816bf8f01cfea414140de5dae2223"
        "b00361a396177a9cb410ff61f20015ad")
    {
        std::cerr << "abc SHA-256 校验失败\n";
        return 2;
    }

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-14-test";

    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base / "source");

    {
        std::ofstream file(base / "source" / "test.md");
        file << "# Continuum\n\nscanner test\n";
    }

    Database database;
    const auto openStatus =
        database.Open((base / "test.db").u8string());

    if (!openStatus.success)
    {
        std::cerr << openStatus.message << "\n";
        return 3;
    }

    DataSourceRepository sources(database);
    FileRepository files(database);

    DataSourceRecord source;
    source.id = "SRC-TEST";
    source.name = "Scanner Test";
    source.sourceType = "local_folder";
    source.rootPath = (base / "source").u8string();

    const auto saveStatus = sources.Save(source, "test");

    if (!saveStatus.success)
    {
        std::cerr << saveStatus.message << "\n";
        return 4;
    }

    FileScanner scanner(database, sources, files);
    const auto first = scanner.ScanSource(source.id);

    if (first.added != 1 ||
        first.parseJobs != 1 ||
        first.failed != 0)
    {
        std::cerr << "首次扫描结果异常\n";
        return 5;
    }

    const auto second = scanner.ScanSource(source.id);

    if (second.unchanged != 1 ||
        second.added != 0 ||
        second.changed != 0)
    {
        std::cerr << "重复扫描结果异常\n";
        return 6;
    }

    {
        std::ofstream file(
            base / "source" / "test.md",
            std::ios::app
        );
        file << "changed\n";
    }

    const auto third = scanner.ScanSource(source.id);

    if (third.changed != 1)
    {
        std::cerr << "变化检测失败\n";
        return 7;
    }

    std::filesystem::remove(
        base / "source" / "test.md",
        error
    );

    const auto fourth = scanner.ScanSource(source.id);

    if (fourth.missing != 1)
    {
        std::cerr << "缺失检测失败\n";
        return 8;
    }

    database.Close();
    std::filesystem::remove_all(base, error);

    std::cout << "round-14 scanner test passed\n";
    return 0;
}
