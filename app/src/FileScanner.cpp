#include "FileScanner.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

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
    stream << std::put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S"
    );
    return stream.str();
}

std::string FileTimestamp(
    const std::filesystem::file_time_type& fileTime
)
{
    const auto systemTime =
        std::chrono::time_point_cast<
            std::chrono::system_clock::duration
        >(
            fileTime -
            std::filesystem::file_time_type::clock::now() +
            std::chrono::system_clock::now()
        );

    const auto value =
        std::chrono::system_clock::to_time_t(systemTime);

    std::tm localTime{};

#if defined(_WIN32)
    localtime_s(&localTime, &value);
#else
    localtime_r(&value, &localTime);
#endif

    std::ostringstream stream;
    stream << std::put_time(
        &localTime,
        "%Y-%m-%d %H:%M:%S"
    );
    return stream.str();
}

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        }
    );
    return value;
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

std::string MakeStableFileId(
    const std::string& sourceId,
    const std::string& relativePath
)
{
    const std::string digest =
        ContentHasher::Sha256Text(
            sourceId + "\n" + Lower(relativePath)
        );

    return "FILE-" + digest.substr(0, 24);
}

std::string MakeParseJobId(
    const std::string& fileId,
    const std::string& fingerprint
)
{
    const std::string digest =
        ContentHasher::Sha256Text(
            "parse\n" + fileId + "\n" + fingerprint
        );

    return "JOB-" + digest.substr(0, 24);
}

bool IsHiddenName(const std::string& name)
{
    return !name.empty() &&
        name != "." &&
        name != ".." &&
        name.front() == '.';
}

bool IsIgnoredDirectoryName(const std::string& name)
{
    const std::string lower = Lower(name);

    static const std::set<std::string> ignored = {
        ".git",
        ".svn",
        ".hg",
        ".idea",
        ".vs",
        ".vscode",
        "node_modules",
        "__pycache__",
        "build",
        "dist",
        "out",
        "target"
    };

    return ignored.find(lower) != ignored.end();
}

std::string ExtensionOf(const std::string& path)
{
    return Lower(
        std::filesystem::u8path(path).extension().u8string()
    );
}

class Sha256 final
{
public:
    Sha256()
    {
        Reset();
    }

    void Reset()
    {
        dataLength_ = 0;
        bitLength_ = 0;

        state_[0] = 0x6a09e667U;
        state_[1] = 0xbb67ae85U;
        state_[2] = 0x3c6ef372U;
        state_[3] = 0xa54ff53aU;
        state_[4] = 0x510e527fU;
        state_[5] = 0x9b05688cU;
        state_[6] = 0x1f83d9abU;
        state_[7] = 0x5be0cd19U;
    }

    void Update(
        const unsigned char* data,
        std::size_t length
    )
    {
        for (std::size_t index = 0; index < length; ++index)
        {
            data_[dataLength_++] = data[index];

            if (dataLength_ == 64)
            {
                Transform();
                bitLength_ += 512;
                dataLength_ = 0;
            }
        }
    }

    std::string Final()
    {
        std::size_t index = dataLength_;

        if (dataLength_ < 56)
        {
            data_[index++] = 0x80U;

            while (index < 56)
            {
                data_[index++] = 0x00U;
            }
        }
        else
        {
            data_[index++] = 0x80U;

            while (index < 64)
            {
                data_[index++] = 0x00U;
            }

            Transform();
            std::memset(data_.data(), 0, 56);
        }

        bitLength_ +=
            static_cast<std::uint64_t>(dataLength_) * 8U;

        data_[63] =
            static_cast<unsigned char>(bitLength_);
        data_[62] =
            static_cast<unsigned char>(bitLength_ >> 8U);
        data_[61] =
            static_cast<unsigned char>(bitLength_ >> 16U);
        data_[60] =
            static_cast<unsigned char>(bitLength_ >> 24U);
        data_[59] =
            static_cast<unsigned char>(bitLength_ >> 32U);
        data_[58] =
            static_cast<unsigned char>(bitLength_ >> 40U);
        data_[57] =
            static_cast<unsigned char>(bitLength_ >> 48U);
        data_[56] =
            static_cast<unsigned char>(bitLength_ >> 56U);

        Transform();

        std::ostringstream stream;
        stream << std::hex << std::setfill('0');

        for (const auto value : state_)
        {
            stream << std::setw(8) << value;
        }

        return stream.str();
    }

private:
    static std::uint32_t RotateRight(
        std::uint32_t value,
        std::uint32_t count
    )
    {
        return (value >> count) |
            (value << (32U - count));
    }

    static std::uint32_t Choose(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t z
    )
    {
        return (x & y) ^ (~x & z);
    }

    static std::uint32_t Majority(
        std::uint32_t x,
        std::uint32_t y,
        std::uint32_t z
    )
    {
        return (x & y) ^ (x & z) ^ (y & z);
    }

    static std::uint32_t Sigma0(std::uint32_t value)
    {
        return RotateRight(value, 2U) ^
            RotateRight(value, 13U) ^
            RotateRight(value, 22U);
    }

    static std::uint32_t Sigma1(std::uint32_t value)
    {
        return RotateRight(value, 6U) ^
            RotateRight(value, 11U) ^
            RotateRight(value, 25U);
    }

    static std::uint32_t Gamma0(std::uint32_t value)
    {
        return RotateRight(value, 7U) ^
            RotateRight(value, 18U) ^
            (value >> 3U);
    }

    static std::uint32_t Gamma1(std::uint32_t value)
    {
        return RotateRight(value, 17U) ^
            RotateRight(value, 19U) ^
            (value >> 10U);
    }

    void Transform()
    {
        static const std::array<std::uint32_t, 64> constants = {{
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
            0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
            0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
            0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
            0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
            0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
            0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
            0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
            0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
            0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
            0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
        }};

        std::array<std::uint32_t, 64> words{};

        for (std::size_t index = 0, offset = 0;
             index < 16;
             ++index, offset += 4)
        {
            words[index] =
                (static_cast<std::uint32_t>(data_[offset]) << 24U) |
                (static_cast<std::uint32_t>(data_[offset + 1]) << 16U) |
                (static_cast<std::uint32_t>(data_[offset + 2]) << 8U) |
                static_cast<std::uint32_t>(data_[offset + 3]);
        }

        for (std::size_t index = 16; index < 64; ++index)
        {
            words[index] =
                Gamma1(words[index - 2]) +
                words[index - 7] +
                Gamma0(words[index - 15]) +
                words[index - 16];
        }

        std::uint32_t a = state_[0];
        std::uint32_t b = state_[1];
        std::uint32_t c = state_[2];
        std::uint32_t d = state_[3];
        std::uint32_t e = state_[4];
        std::uint32_t f = state_[5];
        std::uint32_t g = state_[6];
        std::uint32_t h = state_[7];

        for (std::size_t index = 0; index < 64; ++index)
        {
            const std::uint32_t first =
                h +
                Sigma1(e) +
                Choose(e, f, g) +
                constants[index] +
                words[index];

            const std::uint32_t second =
                Sigma0(a) +
                Majority(a, b, c);

            h = g;
            g = f;
            f = e;
            e = d + first;
            d = c;
            c = b;
            b = a;
            a = first + second;
        }

        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<unsigned char, 64> data_{};
    std::uint32_t dataLength_ = 0;
    std::uint64_t bitLength_ = 0;
    std::array<std::uint32_t, 8> state_{};
};

}

StorageStatus ContentHasher::Sha256File(
    const std::string& path,
    std::string& digest
)
{
    digest.clear();

    std::ifstream stream(
        std::filesystem::u8path(path),
        std::ios::binary
    );

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法读取文件: " + path
        );
    }

    Sha256 hasher;
    std::array<unsigned char, 1024 * 1024> buffer{};

    while (stream)
    {
        stream.read(
            reinterpret_cast<char*>(buffer.data()),
            static_cast<std::streamsize>(buffer.size())
        );

        const auto count = stream.gcount();

        if (count > 0)
        {
            hasher.Update(
                buffer.data(),
                static_cast<std::size_t>(count)
            );
        }
    }

    if (!stream.eof())
    {
        return StorageStatus::Error(
            SQLITE_IOERR_READ,
            "读取文件时发生错误: " + path
        );
    }

    digest = hasher.Final();
    return StorageStatus::Ok();
}

std::string ContentHasher::Sha256Text(
    const std::string& value
)
{
    Sha256 hasher;
    hasher.Update(
        reinterpret_cast<const unsigned char*>(value.data()),
        value.size()
    );
    return hasher.Final();
}

JobRepository::JobRepository(Database& database)
    : database_(database)
{
}

StorageStatus JobRepository::Enqueue(
    const JobRecord& record,
    const std::string& actor
)
{
    if (record.id.empty() || record.jobType.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "任务编号和任务类型不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "INSERT OR IGNORE INTO jobs("
            "id, job_type, state, progress, payload, "
            "error_message, created_at"
            ") VALUES("
            "?, ?, ?, ?, ?, ?, "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP)"
            ");"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备任务入队"
            );
        }

        BindText(statement.Get(), 1, record.id);
        BindText(statement.Get(), 2, record.jobType);
        BindText(
            statement.Get(),
            3,
            record.state.empty() ? "queued" : record.state
        );
        sqlite3_bind_int(
            statement.Get(),
            4,
            std::max(0, std::min(100, record.progress))
        );
        BindText(statement.Get(), 5, record.payload);
        BindText(statement.Get(), 6, record.errorMessage);
        BindText(statement.Get(), 7, record.createdAt);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法将任务加入队列"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Ok();
        }

        return AuditRepository(database_).Append(
            actor,
            "job",
            "enqueue",
            "job",
            record.id,
            "{\"type\":\"" +
                EscapeJson(record.jobType) +
                "\"}"
        );
    });
}

StorageStatus JobRepository::EnqueueParse(
    const std::string& fileId,
    const std::string& fingerprint,
    const std::string& actor
)
{
    if (fileId.empty() || fingerprint.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "解析任务必须提供文件编号和内容指纹"
        );
    }

    JobRecord record;
    record.id = MakeParseJobId(fileId, fingerprint);
    record.jobType = "parse_file";
    record.state = "queued";
    record.payload =
        "{\"file_id\":\"" +
        EscapeJson(fileId) +
        "\",\"fingerprint\":\"" +
        EscapeJson(fingerprint) +
        "\"}";

    return Enqueue(record, actor);
}

std::vector<JobRecord> JobRepository::ListPending(
    int limit
) const
{
    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    std::vector<JobRecord> records;
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return records;
    }

    Statement statement(
        handle,
        "SELECT id, job_type, state, progress, payload, "
        "error_message, created_at, started_at, completed_at "
        "FROM jobs WHERE state IN ('queued', 'retry') "
        "ORDER BY created_at, id LIMIT ?;"
    );

    if (!statement.IsValid())
    {
        return records;
    }

    sqlite3_bind_int(statement.Get(), 1, std::max(1, limit));

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        JobRecord record;
        record.id = ColumnText(statement.Get(), 0);
        record.jobType = ColumnText(statement.Get(), 1);
        record.state = ColumnText(statement.Get(), 2);
        record.progress = sqlite3_column_int(statement.Get(), 3);
        record.payload = ColumnText(statement.Get(), 4);
        record.errorMessage = ColumnText(statement.Get(), 5);
        record.createdAt = ColumnText(statement.Get(), 6);
        record.startedAt = ColumnText(statement.Get(), 7);
        record.completedAt = ColumnText(statement.Get(), 8);
        records.push_back(std::move(record));
    }

    return records;
}

StorageStatus JobRepository::SetState(
    const std::string& id,
    const std::string& state,
    int progress,
    const std::string& errorMessage,
    const std::string& actor
)
{
    if (id.empty() || state.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "任务编号和状态不能为空"
        );
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "UPDATE jobs SET "
            "state=?, progress=?, error_message=?, "
            "started_at=CASE "
            "WHEN ?='running' AND started_at IS NULL "
            "THEN CURRENT_TIMESTAMP ELSE started_at END, "
            "completed_at=CASE "
            "WHEN ? IN ('completed', 'failed', 'cancelled') "
            "THEN CURRENT_TIMESTAMP ELSE completed_at END "
            "WHERE id=?;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备任务状态更新"
            );
        }

        BindText(statement.Get(), 1, state);
        sqlite3_bind_int(
            statement.Get(),
            2,
            std::max(0, std::min(100, progress))
        );
        BindText(statement.Get(), 3, errorMessage);
        BindText(statement.Get(), 4, state);
        BindText(statement.Get(), 5, state);
        BindText(statement.Get(), 6, id);

        const int result = sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                result,
                "无法更新任务状态"
            );
        }

        if (sqlite3_changes(handle) == 0)
        {
            return StorageStatus::Error(
                SQLITE_NOTFOUND,
                "任务不存在"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "job",
            "state",
            "job",
            id,
            "{\"state\":\"" +
                EscapeJson(state) +
                "\"}"
        );
    });
}

FileScanner::FileScanner(
    Database& database,
    DataSourceRepository& dataSources,
    FileRepository& files
)
    : database_(database),
      dataSources_(dataSources),
      files_(files),
      jobs_(database)
{
}

bool FileScanner::IsSupportedFile(
    const std::string& path
)
{
    static const std::set<std::string> extensions = {
        ".txt",
        ".md",
        ".markdown",
        ".rst",
        ".csv",
        ".tsv",
        ".json",
        ".xml",
        ".yaml",
        ".yml",
        ".html",
        ".htm",
        ".pdf",
        ".doc",
        ".docx",
        ".xls",
        ".xlsx",
        ".ppt",
        ".pptx",
        ".odt",
        ".ods",
        ".odp",
        ".eml",
        ".msg",
        ".png",
        ".jpg",
        ".jpeg",
        ".tif",
        ".tiff",
        ".bmp",
        ".webp",
        ".rtf",
        ".log",
        ".ini",
        ".cfg",
        ".conf",
        ".cpp",
        ".cc",
        ".c",
        ".h",
        ".hpp",
        ".py",
        ".js",
        ".ts",
        ".tsx",
        ".java",
        ".cs",
        ".sql"
    };

    return extensions.find(ExtensionOf(path)) != extensions.end();
}

std::string FileScanner::DetectMediaType(
    const std::string& path
)
{
    const std::string extension = ExtensionOf(path);

    if (extension == ".pdf")
    {
        return "application/pdf";
    }

    if (extension == ".png")
    {
        return "image/png";
    }

    if (extension == ".jpg" ||
        extension == ".jpeg")
    {
        return "image/jpeg";
    }

    if (extension == ".tif" ||
        extension == ".tiff")
    {
        return "image/tiff";
    }

    if (extension == ".bmp")
    {
        return "image/bmp";
    }

    if (extension == ".webp")
    {
        return "image/webp";
    }


    if (extension == ".docx")
    {
        return "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
    }

    if (extension == ".xlsx")
    {
        return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
    }

    if (extension == ".pptx")
    {
        return "application/vnd.openxmlformats-officedocument.presentationml.presentation";
    }

    if (extension == ".doc")
    {
        return "application/msword";
    }

    if (extension == ".xls")
    {
        return "application/vnd.ms-excel";
    }

    if (extension == ".ppt")
    {
        return "application/vnd.ms-powerpoint";
    }

    if (extension == ".eml")
    {
        return "message/rfc822";
    }

    if (extension == ".json")
    {
        return "application/json";
    }

    if (extension == ".xml")
    {
        return "application/xml";
    }

    if (extension == ".html" || extension == ".htm")
    {
        return "text/html";
    }

    if (extension == ".csv")
    {
        return "text/csv";
    }

    if (extension == ".md" || extension == ".markdown")
    {
        return "text/markdown";
    }

    return "text/plain";
}

ScanSummary FileScanner::ScanSource(
    const std::string& sourceId,
    const ScanOptions& options
)
{
    ScanSummary summary;
    summary.sourceId = sourceId;
    summary.startedAt = CurrentTimestamp();

    const auto source = dataSources_.FindById(sourceId);

    if (!source)
    {
        summary.failed = 1;
        summary.issues.push_back({
            sourceId,
            "数据源不存在"
        });
        summary.completedAt = CurrentTimestamp();
        return summary;
    }

    if (!source->enabled)
    {
        summary.skipped = 1;
        summary.issues.push_back({
            source->rootPath,
            "数据源已停用"
        });
        summary.completedAt = CurrentTimestamp();
        return summary;
    }

    if (source->sourceType != "local_folder" &&
        source->sourceType != "folder")
    {
        summary.skipped = 1;
        summary.issues.push_back({
            source->rootPath,
            "当前扫描器只处理本地文件夹数据源"
        });
        summary.completedAt = CurrentTimestamp();
        return summary;
    }

    const std::filesystem::path root =
        std::filesystem::u8path(source->rootPath);

    std::error_code error;

    if (!std::filesystem::exists(root, error) ||
        !std::filesystem::is_directory(root, error))
    {
        summary.failed = 1;
        summary.issues.push_back({
            source->rootPath,
            "数据源目录不存在或不可访问"
        });
        summary.completedAt = CurrentTimestamp();

        AuditRepository(database_).Append(
            "scanner",
            "data_source",
            "scan_failed",
            "data_source",
            sourceId,
            "{\"reason\":\"directory_unavailable\"}"
        );

        return summary;
    }

    const auto knownFiles =
        files_.ListBySource(sourceId, true, 1000000);

    std::set<std::string> seenPaths;

    auto processFile =
        [&](const std::filesystem::path& absolutePath) {
            ++summary.visited;

            const std::string absolute =
                absolutePath.u8string();

            if (!IsSupportedFile(absolute))
            {
                ++summary.skipped;
                return;
            }

            ++summary.supported;

            std::error_code fileError;

            const auto size =
                std::filesystem::file_size(
                    absolutePath,
                    fileError
                );

            if (fileError)
            {
                ++summary.failed;
                summary.issues.push_back({
                    absolute,
                    "无法读取文件大小: " +
                        fileError.message()
                });
                return;
            }

            if (size > options.maximumFileSize)
            {
                ++summary.skipped;
                summary.issues.push_back({
                    absolute,
                    "文件超过扫描大小限制"
                });
                return;
            }

            const auto relativePath =
                std::filesystem::relative(
                    absolutePath,
                    root,
                    fileError
                );

            if (fileError)
            {
                ++summary.failed;
                summary.issues.push_back({
                    absolute,
                    "无法计算相对路径: " +
                        fileError.message()
                });
                return;
            }

            const std::string relative =
                relativePath.generic_u8string();

            seenPaths.insert(Lower(relative));

            std::string fingerprint;
            auto hashStatus =
                ContentHasher::Sha256File(
                    absolute,
                    fingerprint
                );

            if (!hashStatus.success)
            {
                ++summary.failed;
                summary.issues.push_back({
                    absolute,
                    hashStatus.message
                });
                return;
            }

            const auto existing =
                files_.FindByPath(
                    sourceId,
                    relative,
                    true
                );

            FileRecord record;
            record.id = existing
                ? existing->id
                : MakeStableFileId(sourceId, relative);
            record.sourceId = sourceId;
            record.relativePath = relative;
            record.displayName =
                absolutePath.filename().u8string();
            record.mediaType = DetectMediaType(absolute);
            record.sizeBytes =
                static_cast<std::int64_t>(size);
            record.fingerprint = fingerprint;
            record.deleted = false;

            const auto modified =
                std::filesystem::last_write_time(
                    absolutePath,
                    fileError
                );

            if (!fileError)
            {
                record.modifiedAt =
                    FileTimestamp(modified);
            }

            const bool isNew = !existing;
            const bool isChanged =
                existing &&
                (
                    existing->deleted ||
                    existing->fingerprint != fingerprint ||
                    existing->sizeBytes != record.sizeBytes
                );

            if (isNew)
            {
                record.parseState = "pending";
            }
            else if (isChanged)
            {
                record.parseState = "changed";
                record.createdAt = existing->createdAt;
            }
            else
            {
                record.parseState = existing->parseState;
                record.createdAt = existing->createdAt;
            }

            if (isNew || isChanged)
            {
                const auto saveStatus =
                    files_.Save(record, "scanner");

                if (!saveStatus.success)
                {
                    ++summary.failed;
                    summary.issues.push_back({
                        relative,
                        saveStatus.message
                    });
                    return;
                }

                if (isNew)
                {
                    ++summary.added;
                }
                else
                {
                    ++summary.changed;
                }

                if (options.enqueueParseJobs)
                {
                    const auto jobStatus =
                        jobs_.EnqueueParse(
                            record.id,
                            fingerprint,
                            "scanner"
                        );

                    if (jobStatus.success)
                    {
                        ++summary.parseJobs;
                    }
                    else
                    {
                        ++summary.failed;
                        summary.issues.push_back({
                            relative,
                            jobStatus.message
                        });
                    }
                }
            }
            else
            {
                ++summary.unchanged;
            }
        };

    try
    {
        if (options.recursive)
        {
            const auto directoryOptions =
                std::filesystem::directory_options::
                    skip_permission_denied;

            std::filesystem::recursive_directory_iterator iterator(
                root,
                directoryOptions,
                error
            );
            const std::filesystem::recursive_directory_iterator end;

            while (iterator != end)
            {
                if (error)
                {
                    ++summary.failed;
                    summary.issues.push_back({
                        iterator->path().u8string(),
                        error.message()
                    });
                    error.clear();
                    iterator.increment(error);
                    continue;
                }

                const auto path = iterator->path();
                const std::string name =
                    path.filename().u8string();

                if (iterator->is_directory(error))
                {
                    if (IsIgnoredDirectoryName(name) ||
                        (!options.includeHidden &&
                         IsHiddenName(name)))
                    {
                        iterator.disable_recursion_pending();
                        ++summary.skipped;
                    }
                }
                else if (iterator->is_regular_file(error))
                {
                    if (options.includeHidden ||
                        !IsHiddenName(name))
                    {
                        processFile(path);
                    }
                    else
                    {
                        ++summary.skipped;
                    }
                }

                error.clear();
                iterator.increment(error);
            }
        }
        else
        {
            for (std::filesystem::directory_iterator iterator(
                     root,
                     std::filesystem::directory_options::
                         skip_permission_denied,
                     error
                 ), end;
                 iterator != end;
                 iterator.increment(error))
            {
                if (error)
                {
                    ++summary.failed;
                    summary.issues.push_back({
                        root.u8string(),
                        error.message()
                    });
                    error.clear();
                    continue;
                }

                const auto path = iterator->path();
                const std::string name =
                    path.filename().u8string();

                if (iterator->is_regular_file(error) &&
                    (options.includeHidden ||
                     !IsHiddenName(name)))
                {
                    processFile(path);
                }
            }
        }
    }
    catch (const std::exception& exception)
    {
        ++summary.failed;
        summary.issues.push_back({
            source->rootPath,
            exception.what()
        });
    }

    if (options.markMissingFiles)
    {
        for (const auto& known : knownFiles)
        {
            if (known.deleted)
            {
                continue;
            }

            if (seenPaths.find(Lower(known.relativePath)) ==
                seenPaths.end())
            {
                const auto status =
                    files_.MarkMissing(
                        known.id,
                        "scanner"
                    );

                if (status.success)
                {
                    ++summary.missing;
                }
                else
                {
                    ++summary.failed;
                    summary.issues.push_back({
                        known.relativePath,
                        status.message
                    });
                }
            }
        }
    }

    summary.completedAt = CurrentTimestamp();

    const auto scanStatus =
        dataSources_.UpdateLastScan(
            sourceId,
            summary.completedAt,
            "scanner"
        );

    if (!scanStatus.success)
    {
        ++summary.failed;
        summary.issues.push_back({
            sourceId,
            scanStatus.message
        });
    }

    std::ostringstream payload;
    payload
        << "{\"visited\":" << summary.visited
        << ",\"supported\":" << summary.supported
        << ",\"added\":" << summary.added
        << ",\"changed\":" << summary.changed
        << ",\"unchanged\":" << summary.unchanged
        << ",\"missing\":" << summary.missing
        << ",\"failed\":" << summary.failed
        << ",\"parse_jobs\":" << summary.parseJobs
        << "}";

    AuditRepository(database_).Append(
        "scanner",
        "data_source",
        "scan_summary",
        "data_source",
        sourceId,
        payload.str()
    );

    return summary;
}

std::vector<ScanSummary> FileScanner::ScanEnabledSources(
    const ScanOptions& options
)
{
    std::vector<ScanSummary> summaries;

    for (const auto& source : dataSources_.List(false, 1000))
    {
        summaries.push_back(
            ScanSource(source.id, options)
        );
    }

    return summaries;
}

}
