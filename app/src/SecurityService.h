#pragma once

#include "Storage.h"

#include <string>
#include <vector>

namespace continuum
{

struct SecurityCapabilities
{
    bool sqlCipherAvailable = false;
    bool operatingSystemProtectionAvailable = false;
    bool restrictedFileFallback = false;
    std::string sqlCipherVersion;
    std::string keyProtectionProvider;
    std::vector<std::string> warnings;
};

struct DatabaseKeyResult
{
    StorageStatus status;
    std::string key;
    bool created = false;
    std::string provider;
};

class DatabaseSecurity final
{
public:
    static SecurityCapabilities Diagnose();

    static bool IsSqlCipherAvailable(
        std::string* version = nullptr
    );

    static StorageStatus ValidateSqlCipherConnection(
        sqlite3* database,
        std::string* version = nullptr
    );

    static bool EncryptionRequiredByEnvironment();

    static std::string GenerateRandomKey(
        std::size_t byteCount = 32
    );
};

class SecureKeyStore final
{
public:
    explicit SecureKeyStore(
        const std::string& workspaceDirectory
    );

    DatabaseKeyResult LoadOrCreateDatabaseKey();

    StorageStatus LoadDatabaseKey(
        std::string& key,
        std::string* provider = nullptr
    ) const;

    StorageStatus SaveDatabaseKey(
        const std::string& key,
        std::string* provider = nullptr
    );

    StorageStatus RemoveDatabaseKey();

    bool Exists() const;

    const std::string& KeyFilePath() const;

private:
    static StorageStatus Protect(
        const std::string& plainText,
        std::string& protectedData,
        std::string& provider
    );

    static StorageStatus Unprotect(
        const std::string& protectedData,
        const std::string& provider,
        std::string& plainText
    );

    static std::string HexEncode(
        const unsigned char* data,
        std::size_t size
    );

    static StorageStatus HexDecode(
        const std::string& value,
        std::vector<unsigned char>& output
    );

    static StorageStatus RestrictFilePermissions(
        const std::string& path
    );

    std::string workspaceDirectory_;
    std::string keyFilePath_;
};

}
