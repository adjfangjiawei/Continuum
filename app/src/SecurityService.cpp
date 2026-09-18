#include "SecurityService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <random>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#else
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#endif

namespace continuum
{
namespace
{

std::string EnvironmentValue(const char* name)
{
    const char* value = std::getenv(name);

    return value == nullptr
        ? std::string()
        : std::string(value);
}

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return value;
}

std::string Trim(const std::string& value)
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

std::string EscapeValue(const std::string& value)
{
    std::string result;

    for (const char character : value)
    {
        switch (character)
        {
        case '\\':
            result += "\\\\";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '=':
            result += "\\=";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
}

std::string UnescapeValue(const std::string& value)
{
    std::string result;
    bool escaped = false;

    for (const char character : value)
    {
        if (!escaped)
        {
            if (character == '\\')
            {
                escaped = true;
            }
            else
            {
                result += character;
            }

            continue;
        }

        switch (character)
        {
        case 'n':
            result += '\n';
            break;
        case 'r':
            result += '\r';
            break;
        case '=':
            result += '=';
            break;
        case '\\':
            result += '\\';
            break;
        default:
            result += character;
            break;
        }

        escaped = false;
    }

    if (escaped)
    {
        result += '\\';
    }

    return result;
}

std::size_t FindSeparator(const std::string& line)
{
    bool escaped = false;

    for (std::size_t index = 0;
         index < line.size();
         ++index)
    {
        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (line[index] == '\\')
        {
            escaped = true;
            continue;
        }

        if (line[index] == '=')
        {
            return index;
        }
    }

    return std::string::npos;
}

bool ConstantTimeEqual(
    const std::string& first,
    const std::string& second
)
{
    if (first.size() != second.size())
    {
        return false;
    }

    unsigned char difference = 0;

    for (std::size_t index = 0;
         index < first.size();
         ++index)
    {
        difference |=
            static_cast<unsigned char>(
                first[index] ^ second[index]
            );
    }

    return difference == 0;
}

std::string KeyChecksum(const std::string& key)
{
    /*
     * 该校验只用于发现密钥文件损坏，不作为密码学认证。
     * 数据库加密安全性由 SQLCipher 提供。
     */
    std::uint64_t first = 1469598103934665603ULL;
    std::uint64_t second = 1099511628211ULL;

    for (const unsigned char character : key)
    {
        first ^= static_cast<std::uint64_t>(character);
        first *= 1099511628211ULL;

        second += static_cast<std::uint64_t>(character);
        second ^= second << 13U;
        second ^= second >> 7U;
        second ^= second << 17U;
    }

    std::ostringstream stream;
    stream
        << std::hex
        << std::setfill('0')
        << std::setw(16)
        << first
        << std::setw(16)
        << second;

    return stream.str();
}

StorageStatus ReadKeyDocument(
    const std::string& path,
    std::map<std::string, std::string>& values
)
{
    values.clear();

    std::ifstream stream(
        std::filesystem::u8path(path),
        std::ios::binary
    );

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法读取数据库密钥文件"
        );
    }

    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        const auto separator =
            FindSeparator(line);

        if (separator == std::string::npos)
        {
            continue;
        }

        const std::string key =
            Trim(line.substr(0, separator));

        const std::string value =
            UnescapeValue(
                line.substr(separator + 1)
            );

        values[key] = value;
    }

    if (!stream.eof() && stream.fail())
    {
        return StorageStatus::Error(
            SQLITE_IOERR_READ,
            "读取数据库密钥文件失败"
        );
    }

    return StorageStatus::Ok();
}

}

StorageStatus DatabaseSecurity::ValidateSqlCipherConnection(
    sqlite3* database,
    std::string* version
)
{
    if (version != nullptr)
    {
        version->clear();
    }

    if (database == nullptr)
    {
        return StorageStatus::Error(
            SQLITE_MISUSE,
            "SQLCipher 检测时数据库连接为空"
        );
    }

    sqlite3_stmt* statement = nullptr;

    const int prepareResult = sqlite3_prepare_v2(
        database,
        "PRAGMA cipher_version;",
        -1,
        &statement,
        nullptr
    );

    if (prepareResult != SQLITE_OK)
    {
        return StorageStatus::Error(
            prepareResult,
            "当前 SQLite 不支持 SQLCipher cipher_version"
        );
    }

    const int stepResult = sqlite3_step(statement);
    std::string detectedVersion;

    if (stepResult == SQLITE_ROW)
    {
        const auto* value =
            sqlite3_column_text(statement, 0);

        if (value != nullptr)
        {
            detectedVersion =
                reinterpret_cast<const char*>(value);
        }
    }

    sqlite3_finalize(statement);

    if (detectedVersion.empty())
    {
        return StorageStatus::Error(
            SQLITE_NOTFOUND,
            "当前 SQLite 未启用 SQLCipher；"
            "数据库密钥不能被安全应用"
        );
    }

    if (version != nullptr)
    {
        *version = detectedVersion;
    }

    return StorageStatus::Ok();
}

bool DatabaseSecurity::IsSqlCipherAvailable(
    std::string* version
)
{
    if (version != nullptr)
    {
        version->clear();
    }

    sqlite3* database = nullptr;

    const int openResult = sqlite3_open_v2(
        ":memory:",
        &database,
        SQLITE_OPEN_READWRITE |
        SQLITE_OPEN_CREATE |
        SQLITE_OPEN_FULLMUTEX,
        nullptr
    );

    if (openResult != SQLITE_OK)
    {
        if (database != nullptr)
        {
            sqlite3_close_v2(database);
        }

        return false;
    }

    const auto status =
        ValidateSqlCipherConnection(
            database,
            version
        );

    sqlite3_close_v2(database);

    return status.success;
}

bool DatabaseSecurity::EncryptionRequiredByEnvironment()
{
    const std::string value = Lower(
        Trim(
            EnvironmentValue(
                "CONTINUUM_REQUIRE_ENCRYPTION"
            )
        )
    );

    return value == "1" ||
        value == "true" ||
        value == "yes" ||
        value == "on";
}

std::string DatabaseSecurity::GenerateRandomKey(
    std::size_t byteCount
)
{
    byteCount = std::max<std::size_t>(
        16,
        byteCount
    );

    std::vector<unsigned char> bytes(byteCount);

#if defined(_WIN32)
    if (BCryptGenRandom(
            nullptr,
            bytes.data(),
            static_cast<ULONG>(bytes.size()),
            BCRYPT_USE_SYSTEM_PREFERRED_RNG
        ) != 0)
    {
        bytes.clear();
    }
#endif

    if (bytes.empty())
    {
        bytes.resize(byteCount);
    }

#if !defined(_WIN32)
    std::ifstream randomStream(
        "/dev/urandom",
        std::ios::binary
    );

    if (randomStream)
    {
        randomStream.read(
            reinterpret_cast<char*>(bytes.data()),
            static_cast<std::streamsize>(
                bytes.size()
            )
        );

        if (!randomStream)
        {
            randomStream.close();

            std::random_device device;

            for (auto& value : bytes)
            {
                value = static_cast<unsigned char>(
                    device()
                );
            }
        }
    }
    else
    {
        std::random_device device;

        for (auto& value : bytes)
        {
            value = static_cast<unsigned char>(
                device()
            );
        }
    }
#else
    if (std::all_of(
            bytes.begin(),
            bytes.end(),
            [](unsigned char value) {
                return value == 0;
            }
        ))
    {
        std::random_device device;

        for (auto& value : bytes)
        {
            value = static_cast<unsigned char>(
                device()
            );
        }
    }
#endif

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    for (const auto value : bytes)
    {
        stream << std::setw(2)
               << static_cast<unsigned int>(value);
    }

    std::fill(
        bytes.begin(),
        bytes.end(),
        0
    );

    return stream.str();
}

SecurityCapabilities DatabaseSecurity::Diagnose()
{
    SecurityCapabilities capabilities;

    capabilities.sqlCipherAvailable =
        IsSqlCipherAvailable(
            &capabilities.sqlCipherVersion
        );

#if defined(_WIN32)
    capabilities.operatingSystemProtectionAvailable = true;
    capabilities.keyProtectionProvider = "windows-dpapi";
#else
    capabilities.operatingSystemProtectionAvailable = false;
    capabilities.restrictedFileFallback = true;
    capabilities.keyProtectionProvider =
        "restricted-file";

    capabilities.warnings.push_back(
        "当前平台未接入桌面密钥环；"
        "数据库密钥使用权限为 0600 的受限文件保存"
    );
#endif

    if (!capabilities.sqlCipherAvailable)
    {
        capabilities.warnings.push_back(
            "当前 SQLite 未启用 SQLCipher，"
            "工作区数据库不会被加密"
        );
    }

    return capabilities;
}

SecureKeyStore::SecureKeyStore(
    const std::string& workspaceDirectory
)
    : workspaceDirectory_(workspaceDirectory),
      keyFilePath_(
          (
              std::filesystem::u8path(
                  workspaceDirectory
              ) /
              "security" /
              "database.key"
          ).u8string()
      )
{
}

std::string SecureKeyStore::HexEncode(
    const unsigned char* data,
    std::size_t size
)
{
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');

    for (std::size_t index = 0;
         index < size;
         ++index)
    {
        stream
            << std::setw(2)
            << static_cast<unsigned int>(
                data[index]
            );
    }

    return stream.str();
}

StorageStatus SecureKeyStore::HexDecode(
    const std::string& value,
    std::vector<unsigned char>& output
)
{
    output.clear();

    if (value.empty() ||
        value.size() % 2 != 0)
    {
        return StorageStatus::Error(
            SQLITE_FORMAT,
            "受保护密钥数据不是有效十六进制"
        );
    }

    const auto decodeDigit =
        [](char character) -> int {
            if (character >= '0' &&
                character <= '9')
            {
                return character - '0';
            }

            if (character >= 'a' &&
                character <= 'f')
            {
                return character - 'a' + 10;
            }

            if (character >= 'A' &&
                character <= 'F')
            {
                return character - 'A' + 10;
            }

            return -1;
        };

    output.reserve(value.size() / 2);

    for (std::size_t index = 0;
         index < value.size();
         index += 2)
    {
        const int high = decodeDigit(value[index]);
        const int low = decodeDigit(value[index + 1]);

        if (high < 0 || low < 0)
        {
            output.clear();

            return StorageStatus::Error(
                SQLITE_FORMAT,
                "受保护密钥数据包含无效十六进制字符"
            );
        }

        output.push_back(
            static_cast<unsigned char>(
                (high << 4) | low
            )
        );
    }

    return StorageStatus::Ok();
}

StorageStatus SecureKeyStore::Protect(
    const std::string& plainText,
    std::string& protectedData,
    std::string& provider
)
{
    protectedData.clear();
    provider.clear();

    if (plainText.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "不能保护空数据库密钥"
        );
    }

#if defined(_WIN32)
    DATA_BLOB input{};
    DATA_BLOB output{};

    input.pbData = reinterpret_cast<BYTE*>(
        const_cast<char*>(plainText.data())
    );
    input.cbData = static_cast<DWORD>(
        plainText.size()
    );

    if (!CryptProtectData(
            &input,
            L"Continuum database key",
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &output
        ))
    {
        return StorageStatus::Error(
            SQLITE_AUTH,
            "Windows DPAPI 无法保护数据库密钥"
        );
    }

    protectedData = HexEncode(
        output.pbData,
        output.cbData
    );

    SecureZeroMemory(
        output.pbData,
        output.cbData
    );
    LocalFree(output.pbData);

    provider = "windows-dpapi";
#else
    protectedData = HexEncode(
        reinterpret_cast<const unsigned char*>(
            plainText.data()
        ),
        plainText.size()
    );

    provider = "restricted-file";
#endif

    return StorageStatus::Ok();
}

StorageStatus SecureKeyStore::Unprotect(
    const std::string& protectedData,
    const std::string& provider,
    std::string& plainText
)
{
    plainText.clear();

    std::vector<unsigned char> decoded;

    auto decodeStatus = HexDecode(
        protectedData,
        decoded
    );

    if (!decodeStatus.success)
    {
        return decodeStatus;
    }

#if defined(_WIN32)
    if (provider != "windows-dpapi")
    {
        std::fill(
            decoded.begin(),
            decoded.end(),
            0
        );

        return StorageStatus::Error(
            SQLITE_AUTH,
            "Windows 不接受非 DPAPI 数据库密钥文件"
        );
    }

    DATA_BLOB input{};
    DATA_BLOB output{};

    input.pbData = decoded.data();
    input.cbData = static_cast<DWORD>(
        decoded.size()
    );

    if (!CryptUnprotectData(
            &input,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            CRYPTPROTECT_UI_FORBIDDEN,
            &output
        ))
    {
        std::fill(
            decoded.begin(),
            decoded.end(),
            0
        );

        return StorageStatus::Error(
            SQLITE_AUTH,
            "Windows DPAPI 无法解密数据库密钥；"
            "密钥可能属于其他用户或计算机"
        );
    }

    plainText.assign(
        reinterpret_cast<const char*>(
            output.pbData
        ),
        output.cbData
    );

    SecureZeroMemory(
        output.pbData,
        output.cbData
    );
    LocalFree(output.pbData);
#else
    if (provider != "restricted-file")
    {
        std::fill(
            decoded.begin(),
            decoded.end(),
            0
        );

        return StorageStatus::Error(
            SQLITE_AUTH,
            "当前平台不支持密钥文件中的保护提供程序: " +
                provider
        );
    }

    plainText.assign(
        reinterpret_cast<const char*>(
            decoded.data()
        ),
        decoded.size()
    );
#endif

    std::fill(
        decoded.begin(),
        decoded.end(),
        0
    );

    return StorageStatus::Ok();
}

StorageStatus SecureKeyStore::RestrictFilePermissions(
    const std::string& path
)
{
#if defined(_WIN32)
    /*
     * Windows 主要由 DPAPI 保护密钥内容。
     * 文件仍继承工作区 security 目录的 ACL。
     */
    (void)path;
    return StorageStatus::Ok();
#else
    if (::chmod(path.c_str(), S_IRUSR | S_IWUSR) != 0)
    {
        return StorageStatus::Error(
            SQLITE_IOERR,
            std::string("无法把密钥文件权限设置为 0600: ") +
                std::strerror(errno)
        );
    }

    return StorageStatus::Ok();
#endif
}

StorageStatus SecureKeyStore::SaveDatabaseKey(
    const std::string& key,
    std::string* outputProvider
)
{
    if (key.size() < 32)
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "数据库密钥长度不足"
        );
    }

    std::string protectedData;
    std::string provider;

    auto status = Protect(
        key,
        protectedData,
        provider
    );

    if (!status.success)
    {
        return status;
    }

    const auto keyPath =
        std::filesystem::u8path(keyFilePath_);

    const auto securityDirectory =
        keyPath.parent_path();

    std::error_code error;

    std::filesystem::create_directories(
        securityDirectory,
        error
    );

    if (error)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法创建工作区安全目录: " +
                error.message()
        );
    }

#if !defined(_WIN32)
    if (::chmod(
            securityDirectory.u8string().c_str(),
            S_IRWXU
        ) != 0)
    {
        return StorageStatus::Error(
            SQLITE_IOERR,
            std::string("无法把安全目录权限设置为 0700: ") +
                std::strerror(errno)
        );
    }
#endif

    const std::filesystem::path temporaryPath =
        std::filesystem::u8path(
            keyFilePath_ + ".tmp"
        );

    {
        std::ofstream stream(
            temporaryPath,
            std::ios::binary |
            std::ios::trunc
        );

        if (!stream)
        {
            return StorageStatus::Error(
                SQLITE_CANTOPEN,
                "无法创建临时数据库密钥文件"
            );
        }

        stream
            << "format=continuum-database-key-v1\n"
            << "provider="
            << EscapeValue(provider)
            << "\n"
            << "data="
            << protectedData
            << "\n"
            << "checksum="
            << KeyChecksum(key)
            << "\n";

        stream.flush();

        if (!stream)
        {
            return StorageStatus::Error(
                SQLITE_IOERR_WRITE,
                "写入数据库密钥文件失败"
            );
        }
    }

    status = RestrictFilePermissions(
        temporaryPath.u8string()
    );

    if (!status.success)
    {
        std::filesystem::remove(
            temporaryPath,
            error
        );

        return status;
    }

    std::filesystem::remove(
        keyPath,
        error
    );

    error.clear();

    std::filesystem::rename(
        temporaryPath,
        keyPath,
        error
    );

    if (error)
    {
        std::filesystem::remove(
            temporaryPath,
            error
        );

        return StorageStatus::Error(
            SQLITE_IOERR,
            "无法原子提交数据库密钥文件: " +
                error.message()
        );
    }

    status = RestrictFilePermissions(
        keyFilePath_
    );

    if (!status.success)
    {
        return status;
    }

    if (outputProvider != nullptr)
    {
        *outputProvider = provider;
    }

    return StorageStatus::Ok();
}

StorageStatus SecureKeyStore::LoadDatabaseKey(
    std::string& key,
    std::string* outputProvider
) const
{
    key.clear();

    if (outputProvider != nullptr)
    {
        outputProvider->clear();
    }

    std::map<std::string, std::string> values;

    auto status = ReadKeyDocument(
        keyFilePath_,
        values
    );

    if (!status.success)
    {
        return status;
    }

    if (values["format"] !=
        "continuum-database-key-v1")
    {
        return StorageStatus::Error(
            SQLITE_FORMAT,
            "数据库密钥文件格式无效"
        );
    }

    const std::string provider =
        values["provider"];

    if (provider.empty() ||
        values["data"].empty() ||
        values["checksum"].empty())
    {
        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "数据库密钥文件缺少必要字段"
        );
    }

    status = Unprotect(
        values["data"],
        provider,
        key
    );

    if (!status.success)
    {
        key.clear();
        return status;
    }

    const std::string actualChecksum =
        KeyChecksum(key);

    if (!ConstantTimeEqual(
            actualChecksum,
            values["checksum"]
        ))
    {
        std::fill(
            key.begin(),
            key.end(),
            '\0'
        );

        key.clear();

        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "数据库密钥文件校验失败"
        );
    }

    if (key.size() < 32)
    {
        std::fill(
            key.begin(),
            key.end(),
            '\0'
        );

        key.clear();

        return StorageStatus::Error(
            SQLITE_CORRUPT,
            "数据库密钥长度无效"
        );
    }

    if (outputProvider != nullptr)
    {
        *outputProvider = provider;
    }

    return StorageStatus::Ok();
}

DatabaseKeyResult SecureKeyStore::LoadOrCreateDatabaseKey()
{
    DatabaseKeyResult result;

    if (Exists())
    {
        result.status = LoadDatabaseKey(
            result.key,
            &result.provider
        );

        result.created = false;
        return result;
    }

    result.key =
        DatabaseSecurity::GenerateRandomKey(32);

    if (result.key.size() != 64)
    {
        result.status = StorageStatus::Error(
            SQLITE_ERROR,
            "无法生成 256 位数据库随机密钥"
        );

        result.key.clear();
        return result;
    }

    result.status = SaveDatabaseKey(
        result.key,
        &result.provider
    );

    if (!result.status.success)
    {
        std::fill(
            result.key.begin(),
            result.key.end(),
            '\0'
        );

        result.key.clear();
        return result;
    }

    result.created = true;
    return result;
}

StorageStatus SecureKeyStore::RemoveDatabaseKey()
{
    std::error_code error;

    if (!std::filesystem::exists(
            std::filesystem::u8path(
                keyFilePath_
            ),
            error
        ))
    {
        return StorageStatus::Ok();
    }

    std::filesystem::remove(
        std::filesystem::u8path(
            keyFilePath_
        ),
        error
    );

    if (error)
    {
        return StorageStatus::Error(
            SQLITE_IOERR,
            "无法删除数据库密钥文件: " +
                error.message()
        );
    }

    return StorageStatus::Ok();
}

bool SecureKeyStore::Exists() const
{
    std::error_code error;

    return std::filesystem::exists(
        std::filesystem::u8path(
            keyFilePath_
        ),
        error
    ) && !error;
}

const std::string& SecureKeyStore::KeyFilePath() const
{
    return keyFilePath_;
}

}
