#include "SecurityService.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#if !defined(_WIN32)
#include <sys/stat.h>
#endif

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-20-test";

    std::error_code error;

    std::filesystem::remove_all(
        base,
        error
    );

    std::filesystem::create_directories(
        base,
        error
    );

    if (error)
    {
        std::cerr
            << "无法创建测试目录\n";
        return 1;
    }

    SecureKeyStore store(base.u8string());

    const auto first =
        store.LoadOrCreateDatabaseKey();

    if (!first.status.success)
    {
        std::cerr
            << "创建密钥失败: "
            << first.status.message
            << "\n";
        return 2;
    }

    if (!first.created ||
        first.key.size() != 64 ||
        first.provider.empty())
    {
        std::cerr << "首次密钥结果异常\n";
        return 3;
    }

    if (!store.Exists())
    {
        std::cerr << "密钥文件未创建\n";
        return 4;
    }

    const auto second =
        store.LoadOrCreateDatabaseKey();

    if (!second.status.success ||
        second.created ||
        second.key != first.key)
    {
        std::cerr << "密钥重复加载结果异常\n";
        return 5;
    }

#if !defined(_WIN32)
    struct stat information{};

    if (::stat(
            store.KeyFilePath().c_str(),
            &information
        ) != 0)
    {
        std::cerr << "无法读取密钥文件权限\n";
        return 6;
    }

    const mode_t permissions =
        information.st_mode & 0777;

    if (permissions != 0600)
    {
        std::cerr
            << "密钥文件权限不是 0600: "
            << std::oct
            << permissions
            << "\n";
        return 7;
    }
#endif

    const auto capabilities =
        DatabaseSecurity::Diagnose();

    std::string version;

    const bool cipherAvailable =
        DatabaseSecurity::IsSqlCipherAvailable(
            &version
        );

    if (cipherAvailable !=
        capabilities.sqlCipherAvailable)
    {
        std::cerr << "SQLCipher 诊断结果不一致\n";
        return 8;
    }

    Database database;

    if (cipherAvailable)
    {
        const auto open =
            database.Open(
                (base / "encrypted.db").u8string(),
                first.key
            );

        if (!open.success)
        {
            std::cerr
                << "SQLCipher 数据库打开失败: "
                << open.message
                << "\n";
            return 9;
        }

        database.Close();
    }
    else
    {
        const auto open =
            database.Open(
                (base / "must-not-silently-open.db")
                    .u8string(),
                first.key
            );

        if (open.success)
        {
            std::cerr
                << "普通 SQLite 静默接受了加密密钥\n";
            return 10;
        }
    }

    {
        std::fstream stream(
            store.KeyFilePath(),
            std::ios::in |
            std::ios::out |
            std::ios::binary
        );

        if (!stream)
        {
            std::cerr << "无法打开密钥文件进行损坏测试\n";
            return 11;
        }

        std::string content(
            (std::istreambuf_iterator<char>(stream)),
            std::istreambuf_iterator<char>()
        );

        const auto data = content.find("data=");

        if (data == std::string::npos ||
            data + 6 >= content.size())
        {
            std::cerr << "密钥文件没有 data 字段\n";
            return 12;
        }

        content[data + 6] =
            content[data + 6] == '0' ? '1' : '0';

        stream.clear();
        stream.seekp(0);
        stream.write(
            content.data(),
            static_cast<std::streamsize>(
                content.size()
            )
        );
        stream.flush();
    }

    std::string corruptedKey;

    const auto corrupted =
        store.LoadDatabaseKey(corruptedKey);

    if (corrupted.success)
    {
        std::cerr << "未检测到损坏的密钥文件\n";
        return 13;
    }

    if (!store.RemoveDatabaseKey().success ||
        store.Exists())
    {
        std::cerr << "删除测试密钥失败\n";
        return 14;
    }

    std::filesystem::remove_all(
        base,
        error
    );

    std::cout
        << "round-20 security service tests passed\n";

    return 0;
}
