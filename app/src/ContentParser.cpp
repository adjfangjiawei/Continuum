#include "ContentParser.h"
#include "ExternalDocumentService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <regex>
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

std::string ReplaceAll(
    std::string value,
    const std::string& from,
    const std::string& to
)
{
    if (from.empty())
    {
        return value;
    }

    std::size_t position = 0;

    while ((position = value.find(from, position)) !=
           std::string::npos)
    {
        value.replace(position, from.size(), to);
        position += to.size();
    }

    return value;
}

std::string DecodeHtmlEntities(std::string value)
{
    const std::array<std::pair<const char*, const char*>, 8> entities = {{
        {"&nbsp;", " "},
        {"&amp;", "&"},
        {"&lt;", "<"},
        {"&gt;", ">"},
        {"&quot;", "\""},
        {"&#39;", "'"},
        {"&apos;", "'"},
        {"&mdash;", "-"}
    }};

    for (const auto& entity : entities)
    {
        value = ReplaceAll(
            value,
            entity.first,
            entity.second
        );
    }

    return value;
}

std::string NormalizeNewlines(std::string value)
{
    value = ReplaceAll(value, "\r\n", "\n");
    value = ReplaceAll(value, "\r", "\n");
    return value;
}

std::string CollapseWhitespace(const std::string& value)
{
    std::string result;
    bool previousSpace = false;
    int newlineCount = 0;

    for (const unsigned char character : value)
    {
        if (character == '\n')
        {
            while (!result.empty() &&
                   result.back() == ' ')
            {
                result.pop_back();
            }

            ++newlineCount;

            if (newlineCount <= 2)
            {
                result.push_back('\n');
            }

            previousSpace = false;
        }
        else if (std::isspace(character) != 0)
        {
            if (!previousSpace &&
                !result.empty() &&
                result.back() != '\n')
            {
                result.push_back(' ');
            }

            previousSpace = true;
            newlineCount = 0;
        }
        else
        {
            result.push_back(
                static_cast<char>(character)
            );
            previousSpace = false;
            newlineCount = 0;
        }
    }

    return Trim(result);
}

std::string Utf16ToUtf8(
    const std::vector<unsigned char>& bytes,
    bool littleEndian
)
{
    std::string output;

    const auto appendCodePoint =
        [&output](std::uint32_t value) {
            if (value <= 0x7fU)
            {
                output.push_back(
                    static_cast<char>(value)
                );
            }
            else if (value <= 0x7ffU)
            {
                output.push_back(
                    static_cast<char>(
                        0xc0U | (value >> 6U)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U | (value & 0x3fU)
                    )
                );
            }
            else if (value <= 0xffffU)
            {
                output.push_back(
                    static_cast<char>(
                        0xe0U | (value >> 12U)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U |
                        ((value >> 6U) & 0x3fU)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U | (value & 0x3fU)
                    )
                );
            }
            else
            {
                output.push_back(
                    static_cast<char>(
                        0xf0U | (value >> 18U)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U |
                        ((value >> 12U) & 0x3fU)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U |
                        ((value >> 6U) & 0x3fU)
                    )
                );
                output.push_back(
                    static_cast<char>(
                        0x80U | (value & 0x3fU)
                    )
                );
            }
        };

    std::size_t index = 2;

    while (index + 1 < bytes.size())
    {
        const auto readWord =
            [&](std::size_t offset) {
                if (littleEndian)
                {
                    return static_cast<std::uint16_t>(
                        bytes[offset] |
                        (
                            static_cast<std::uint16_t>(
                                bytes[offset + 1]
                            ) << 8U
                        )
                    );
                }

                return static_cast<std::uint16_t>(
                    (
                        static_cast<std::uint16_t>(
                            bytes[offset]
                        ) << 8U
                    ) |
                    bytes[offset + 1]
                );
            };

        const std::uint16_t first = readWord(index);
        index += 2;

        if (first >= 0xd800U &&
            first <= 0xdbffU &&
            index + 1 < bytes.size())
        {
            const std::uint16_t second =
                readWord(index);

            if (second >= 0xdc00U &&
                second <= 0xdfffU)
            {
                index += 2;

                const std::uint32_t codePoint =
                    0x10000U +
                    (
                        (
                            static_cast<std::uint32_t>(
                                first - 0xd800U
                            ) << 10U
                        ) |
                        static_cast<std::uint32_t>(
                            second - 0xdc00U
                        )
                    );

                appendCodePoint(codePoint);
                continue;
            }
        }

        appendCodePoint(first);
    }

    return output;
}

StorageStatus ReadTextFile(
    const std::string& path,
    std::string& content
)
{
    content.clear();

    constexpr std::uintmax_t maximumTextFileSize =
        static_cast<std::uintmax_t>(64) * 1024 * 1024;

    std::error_code sizeError;
    const auto fileSize = std::filesystem::file_size(
        std::filesystem::u8path(path),
        sizeError
    );

    if (sizeError)
    {
        return StorageStatus::Error(
            SQLITE_IOERR_READ,
            "无法读取待解析文件大小: " + path
        );
    }

    if (fileSize > maximumTextFileSize)
    {
        return StorageStatus::Error(
            SQLITE_TOOBIG,
            "文本文件超过 64 MiB 解析限制: " + path
        );
    }

    std::ifstream stream(
        std::filesystem::u8path(path),
        std::ios::binary
    );

    if (!stream)
    {
        return StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法打开待解析文件: " + path
        );
    }

    std::vector<unsigned char> bytes(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>()
    );

    if (!stream.eof() && stream.fail())
    {
        return StorageStatus::Error(
            SQLITE_IOERR_READ,
            "读取文件失败: " + path
        );
    }

    if (bytes.size() >= 3 &&
        bytes[0] == 0xefU &&
        bytes[1] == 0xbbU &&
        bytes[2] == 0xbfU)
    {
        content.assign(
            reinterpret_cast<const char*>(bytes.data() + 3),
            bytes.size() - 3
        );
        return StorageStatus::Ok();
    }

    if (bytes.size() >= 2 &&
        bytes[0] == 0xffU &&
        bytes[1] == 0xfeU)
    {
        content = Utf16ToUtf8(bytes, true);
        return StorageStatus::Ok();
    }

    if (bytes.size() >= 2 &&
        bytes[0] == 0xfeU &&
        bytes[1] == 0xffU)
    {
        content = Utf16ToUtf8(bytes, false);
        return StorageStatus::Ok();
    }

    const std::size_t probe =
        std::min<std::size_t>(bytes.size(), 8192);

    std::size_t nullCount = 0;

    for (std::size_t index = 0;
         index < probe;
         ++index)
    {
        if (bytes[index] == 0)
        {
            ++nullCount;
        }
    }

    if (probe > 0 && nullCount > probe / 20)
    {
        return StorageStatus::Error(
            SQLITE_MISMATCH,
            "文件包含大量二进制数据"
        );
    }

    content.assign(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size()
    );

    return StorageStatus::Ok();
}

std::string RemoveMarkdown(const std::string& input)
{
    std::string value = NormalizeNewlines(input);

    value = std::regex_replace(
        value,
        std::regex(R"(```[\s\S]*?```)"),
        "\n"
    );
    value = std::regex_replace(
        value,
        std::regex(R"(`([^`]*)`)"),
        "$1"
    );
    value = std::regex_replace(
        value,
        std::regex(R"(!?\[([^\]]*)\]\([^\)]*\))"),
        "$1"
    );
    value = std::regex_replace(
        value,
        std::regex(R"(^\s{0,3}#{1,6}\s*)",
                   std::regex_constants::multiline),
        ""
    );
    value = std::regex_replace(
        value,
        std::regex(R"(^\s*>\s?)",
                   std::regex_constants::multiline),
        ""
    );
    value = std::regex_replace(
        value,
        std::regex(R"(^\s*[-*+]\s+)",
                   std::regex_constants::multiline),
        ""
    );
    value = std::regex_replace(
        value,
        std::regex(R"(\*\*([^*]+)\*\*)"),
        "$1"
    );
    value = std::regex_replace(
        value,
        std::regex(R"(__([^_]+)__)"),
        "$1"
    );

    return CollapseWhitespace(value);
}

std::string RemoveHtml(const std::string& input)
{
    std::string value = input;

    value = std::regex_replace(
        value,
        std::regex(
            R"(<script\b[^>]*>[\s\S]*?</script>)",
            std::regex_constants::icase
        ),
        " "
    );
    value = std::regex_replace(
        value,
        std::regex(
            R"(<style\b[^>]*>[\s\S]*?</style>)",
            std::regex_constants::icase
        ),
        " "
    );
    value = std::regex_replace(
        value,
        std::regex(
            R"(<(br|p|div|li|tr|h[1-6])\b[^>]*>)",
            std::regex_constants::icase
        ),
        "\n"
    );
    value = std::regex_replace(
        value,
        std::regex(R"(<[^>]+>)"),
        " "
    );

    return CollapseWhitespace(
        DecodeHtmlEntities(value)
    );
}

std::string StructuredText(
    const std::string& input
)
{
    std::string value = NormalizeNewlines(input);
    std::string result;
    bool quoted = false;
    bool escaped = false;

    for (const char character : value)
    {
        if (escaped)
        {
            result.push_back(character);
            escaped = false;
            continue;
        }

        if (character == '\\' && quoted)
        {
            escaped = true;
            continue;
        }

        if (character == '"')
        {
            quoted = !quoted;
            result.push_back(' ');
            continue;
        }

        if (!quoted &&
            (
                character == '{' ||
                character == '}' ||
                character == '[' ||
                character == ']' ||
                character == ',' ||
                character == ':' ||
                character == '<' ||
                character == '>' ||
                character == '='
            ))
        {
            result.push_back(
                character == ',' ? '\n' : ' '
            );
            continue;
        }

        result.push_back(character);
    }

    return CollapseWhitespace(result);
}

std::vector<ParsedSection> SplitSections(
    const std::string& content
)
{
    std::vector<ParsedSection> sections;

    constexpr std::size_t maximumSectionSize = 4000;
    constexpr std::size_t overlapSize = 200;

    if (content.empty())
    {
        return sections;
    }

    std::size_t start = 0;
    int ordinal = 0;

    while (start < content.size())
    {
        std::size_t end = std::min(
            content.size(),
            start + maximumSectionSize
        );

        if (end < content.size())
        {
            const std::size_t paragraph =
                content.rfind("\n\n", end);

            if (paragraph != std::string::npos &&
                paragraph > start + 1000)
            {
                end = paragraph;
            }
        }

        ParsedSection section;
        section.ordinal = ordinal++;
        section.anchor =
            "char:" + std::to_string(start);
        section.content =
            Trim(content.substr(start, end - start));
        sections.push_back(std::move(section));

        if (end >= content.size())
        {
            break;
        }

        start = end > overlapSize
            ? end - overlapSize
            : end;
    }

    return sections;
}

std::string ExtensionOf(const FileRecord& file)
{
    return Lower(
        std::filesystem::u8path(
            file.relativePath
        ).extension().u8string()
    );
}

std::string BaseNameWithoutExtension(
    const FileRecord& file
)
{
    return std::filesystem::u8path(
        file.displayName
    ).stem().u8string();
}

std::map<std::string, std::string> ParseEmailHeaders(
    const std::string& headerBlock
)
{
    std::map<std::string, std::string> headers;
    std::string currentName;
    std::istringstream stream(headerBlock);
    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty() &&
            (line.front() == ' ' || line.front() == '\t') &&
            !currentName.empty())
        {
            headers[currentName] += " " + Trim(line);
            continue;
        }

        const auto colon = line.find(':');

        if (colon == std::string::npos)
        {
            continue;
        }

        currentName = Lower(Trim(line.substr(0, colon)));
        headers[currentName] = Trim(line.substr(colon + 1));
    }

    return headers;
}

}

bool TextContentParser::Supports(
    const FileRecord& file
) const
{
    const std::string extension = ExtensionOf(file);

    static const std::array<const char*, 30> extensions = {{
        ".txt", ".md", ".markdown", ".rst", ".csv",
        ".tsv", ".json", ".xml", ".yaml", ".yml",
        ".html", ".htm", ".rtf", ".log", ".ini",
        ".cfg", ".conf", ".cpp", ".cc", ".c",
        ".h", ".hpp", ".py", ".js", ".ts",
        ".tsx", ".java", ".cs", ".sql", ".sh"
    }};

    return std::find(
        extensions.begin(),
        extensions.end(),
        extension
    ) != extensions.end();
}

ParseResult TextContentParser::Parse(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ParseResult result;
    std::string raw;

    result.status = ReadTextFile(absolutePath, raw);

    if (!result.status.success)
    {
        return result;
    }

    const std::string extension = ExtensionOf(file);
    std::string content;

    if (extension == ".md" ||
        extension == ".markdown" ||
        extension == ".rst")
    {
        content = RemoveMarkdown(raw);
    }
    else if (extension == ".html" ||
             extension == ".htm")
    {
        content = RemoveHtml(raw);
    }
    else if (extension == ".json" ||
             extension == ".xml" ||
             extension == ".yaml" ||
             extension == ".yml")
    {
        content = StructuredText(raw);
    }
    else if (extension == ".csv" ||
             extension == ".tsv")
    {
        content = raw;

        const char separator =
            extension == ".tsv" ? '\t' : ',';

        std::replace(
            content.begin(),
            content.end(),
            separator,
            ' '
        );

        content = CollapseWhitespace(content);
    }
    else if (extension == ".rtf")
    {
        content = std::regex_replace(
            raw,
            std::regex(R"(\\[a-zA-Z]+\d*\s?)"),
            " "
        );
        content = std::regex_replace(
            content,
            std::regex(R"([{}])"),
            " "
        );
        content = CollapseWhitespace(content);
        result.warnings.push_back(
            "RTF 使用基础文本清理，复杂格式可能被忽略"
        );
    }
    else
    {
        content = CollapseWhitespace(
            NormalizeNewlines(raw)
        );
    }

    result.document.fileId = file.id;
    result.document.fingerprint = file.fingerprint;
    result.document.parserName = Name();
    result.document.title =
        BaseNameWithoutExtension(file);
    result.document.content = content;
    result.document.language = "und";
    result.document.metadataJson =
        "{\"media_type\":\"" +
        EscapeJson(file.mediaType) +
        "\"}";
    result.document.parsedAt = CurrentTimestamp();
    result.document.sections = SplitSections(content);
    result.status = StorageStatus::Ok();

    return result;
}

std::string TextContentParser::Name() const
{
    return "builtin-text-parser/1";
}

bool EmailContentParser::Supports(
    const FileRecord& file
) const
{
    return ExtensionOf(file) == ".eml" ||
        file.mediaType == "message/rfc822";
}

ParseResult EmailContentParser::Parse(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ParseResult result;
    std::string raw;

    result.status = ReadTextFile(absolutePath, raw);

    if (!result.status.success)
    {
        return result;
    }

    raw = NormalizeNewlines(raw);

    const auto boundary = raw.find("\n\n");

    const std::string headerBlock =
        boundary == std::string::npos
            ? raw
            : raw.substr(0, boundary);

    std::string body =
        boundary == std::string::npos
            ? std::string()
            : raw.substr(boundary + 2);

    const auto headers =
        ParseEmailHeaders(headerBlock);

    const auto valueOf =
        [&headers](const std::string& name) {
            const auto iterator = headers.find(name);

            return iterator == headers.end()
                ? std::string()
                : iterator->second;
        };

    const std::string contentType =
        Lower(valueOf("content-type"));

    if (contentType.find("text/html") !=
        std::string::npos)
    {
        body = RemoveHtml(body);
    }
    else
    {
        body = CollapseWhitespace(body);
    }

    const std::string subject = valueOf("subject");
    const std::string from = valueOf("from");
    const std::string to = valueOf("to");
    const std::string date = valueOf("date");

    std::ostringstream searchable;
    searchable
        << "主题 " << subject << "\n"
        << "发件人 " << from << "\n"
        << "收件人 " << to << "\n"
        << "日期 " << date << "\n\n"
        << body;

    result.document.fileId = file.id;
    result.document.fingerprint = file.fingerprint;
    result.document.parserName = Name();
    result.document.title = subject.empty()
        ? BaseNameWithoutExtension(file)
        : subject;
    result.document.author = from;
    result.document.subject = subject;
    result.document.content =
        CollapseWhitespace(searchable.str());
    result.document.language = "und";
    result.document.metadataJson =
        "{\"from\":\"" + EscapeJson(from) +
        "\",\"to\":\"" + EscapeJson(to) +
        "\",\"date\":\"" + EscapeJson(date) +
        "\"}";
    result.document.parsedAt = CurrentTimestamp();
    result.document.sections =
        SplitSections(result.document.content);
    result.status = StorageStatus::Ok();

    if (contentType.find("multipart/") !=
        std::string::npos)
    {
        result.warnings.push_back(
            "多段 MIME 邮件使用基础正文解析，附件尚未展开"
        );
    }

    return result;
}

std::string EmailContentParser::Name() const
{
    return "builtin-eml-parser/1";
}

bool ExternalDocumentParser::Supports(
    const FileRecord& file
) const
{
    ExternalDocumentService service;
    return service.CanParse(file);
}

ParseResult ExternalDocumentParser::Parse(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ExternalDocumentService service;
    return service.Parse(file, absolutePath);
}

std::string ExternalDocumentParser::Name() const
{
    return "external-document-adapter/1";
}

ParserRegistry::ParserRegistry()
{
    parsers_.push_back(
        std::make_unique<EmailContentParser>()
    );
    parsers_.push_back(
        std::make_unique<TextContentParser>()
    );
    parsers_.push_back(
        std::make_unique<ExternalDocumentParser>()
    );
}

const ContentParser* ParserRegistry::Find(
    const FileRecord& file
) const
{
    for (const auto& parser : parsers_)
    {
        if (parser->Supports(file))
        {
            return parser.get();
        }
    }

    return nullptr;
}

ParsedContentRepository::ParsedContentRepository(
    Database& database
)
    : database_(database)
{
}

StorageStatus ParsedContentRepository::EnsureSchema()
{
    return database_.Execute(R"sql(
CREATE TABLE IF NOT EXISTS parsed_documents (
    file_id TEXT PRIMARY KEY,
    fingerprint TEXT NOT NULL,
    parser_name TEXT NOT NULL,
    title TEXT NOT NULL DEFAULT '',
    author TEXT NOT NULL DEFAULT '',
    subject TEXT NOT NULL DEFAULT '',
    content TEXT NOT NULL DEFAULT '',
    language TEXT NOT NULL DEFAULT 'und',
    metadata_json TEXT NOT NULL DEFAULT '{}',
    parsed_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY(file_id) REFERENCES files(id)
        ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS parsed_sections (
    file_id TEXT NOT NULL,
    ordinal INTEGER NOT NULL,
    heading TEXT NOT NULL DEFAULT '',
    anchor TEXT NOT NULL DEFAULT '',
    content TEXT NOT NULL,
    PRIMARY KEY(file_id, ordinal),
    FOREIGN KEY(file_id) REFERENCES parsed_documents(file_id)
        ON DELETE CASCADE
);

CREATE INDEX IF NOT EXISTS idx_parsed_documents_fingerprint
ON parsed_documents(fingerprint);

CREATE INDEX IF NOT EXISTS idx_parsed_sections_file
ON parsed_sections(file_id, ordinal);

CREATE TABLE IF NOT EXISTS search_index_queue (
    file_id TEXT PRIMARY KEY,
    operation TEXT NOT NULL DEFAULT 'upsert',
    queued_at TEXT NOT NULL DEFAULT CURRENT_TIMESTAMP
);

CREATE TRIGGER IF NOT EXISTS trg_parsed_documents_search_insert
AFTER INSERT ON parsed_documents
BEGIN
    INSERT INTO search_index_queue(
        file_id,
        operation,
        queued_at
    )
    VALUES(
        NEW.file_id,
        'upsert',
        CURRENT_TIMESTAMP
    )
    ON CONFLICT(file_id) DO UPDATE SET
        operation='upsert',
        queued_at=CURRENT_TIMESTAMP;
END;

CREATE TRIGGER IF NOT EXISTS trg_parsed_documents_search_update
AFTER UPDATE ON parsed_documents
BEGIN
    INSERT INTO search_index_queue(
        file_id,
        operation,
        queued_at
    )
    VALUES(
        NEW.file_id,
        'upsert',
        CURRENT_TIMESTAMP
    )
    ON CONFLICT(file_id) DO UPDATE SET
        operation='upsert',
        queued_at=CURRENT_TIMESTAMP;
END;

CREATE TRIGGER IF NOT EXISTS trg_parsed_documents_search_delete
AFTER DELETE ON parsed_documents
BEGIN
    INSERT INTO search_index_queue(
        file_id,
        operation,
        queued_at
    )
    VALUES(
        OLD.file_id,
        'delete',
        CURRENT_TIMESTAMP
    )
    ON CONFLICT(file_id) DO UPDATE SET
        operation='delete',
        queued_at=CURRENT_TIMESTAMP;
END;

)sql");
}

StorageStatus ParsedContentRepository::Save(
    const ParsedDocument& document,
    const std::string& actor
)
{
    if (document.fileId.empty() ||
        document.fingerprint.empty())
    {
        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "解析结果必须包含文件编号和内容指纹"
        );
    }

    auto schemaStatus = EnsureSchema();

    if (!schemaStatus.success)
    {
        return schemaStatus;
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement documentStatement(
            handle,
            "INSERT INTO parsed_documents("
            "file_id, fingerprint, parser_name, title, author, "
            "subject, content, language, metadata_json, parsed_at"
            ") VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, "
            "COALESCE(NULLIF(?, ''), CURRENT_TIMESTAMP)) "
            "ON CONFLICT(file_id) DO UPDATE SET "
            "fingerprint=excluded.fingerprint, "
            "parser_name=excluded.parser_name, "
            "title=excluded.title, "
            "author=excluded.author, "
            "subject=excluded.subject, "
            "content=excluded.content, "
            "language=excluded.language, "
            "metadata_json=excluded.metadata_json, "
            "parsed_at=excluded.parsed_at;"
        );

        if (!documentStatement.IsValid())
        {
            return SqliteError(
                handle,
                documentStatement.Status(),
                "无法准备解析文档保存"
            );
        }

        BindText(documentStatement.Get(), 1, document.fileId);
        BindText(documentStatement.Get(), 2, document.fingerprint);
        BindText(documentStatement.Get(), 3, document.parserName);
        BindText(documentStatement.Get(), 4, document.title);
        BindText(documentStatement.Get(), 5, document.author);
        BindText(documentStatement.Get(), 6, document.subject);
        BindText(documentStatement.Get(), 7, document.content);
        BindText(
            documentStatement.Get(),
            8,
            document.language.empty() ? "und" : document.language
        );
        BindText(
            documentStatement.Get(),
            9,
            document.metadataJson.empty()
                ? "{}"
                : document.metadataJson
        );
        BindText(documentStatement.Get(), 10, document.parsedAt);

        const int documentResult =
            sqlite3_step(documentStatement.Get());

        if (documentResult != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                documentResult,
                "无法保存解析文档"
            );
        }

        Statement removeSections(
            handle,
            "DELETE FROM parsed_sections WHERE file_id=?;"
        );

        if (!removeSections.IsValid())
        {
            return SqliteError(
                handle,
                removeSections.Status(),
                "无法准备旧分段删除"
            );
        }

        BindText(removeSections.Get(), 1, document.fileId);

        if (sqlite3_step(removeSections.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法删除旧解析分段"
            );
        }

        Statement sectionStatement(
            handle,
            "INSERT INTO parsed_sections("
            "file_id, ordinal, heading, anchor, content"
            ") VALUES(?, ?, ?, ?, ?);"
        );

        if (!sectionStatement.IsValid())
        {
            return SqliteError(
                handle,
                sectionStatement.Status(),
                "无法准备解析分段保存"
            );
        }

        for (const auto& section : document.sections)
        {
            sqlite3_reset(sectionStatement.Get());
            sqlite3_clear_bindings(sectionStatement.Get());

            BindText(sectionStatement.Get(), 1, document.fileId);
            sqlite3_bind_int(
                sectionStatement.Get(),
                2,
                section.ordinal
            );
            BindText(sectionStatement.Get(), 3, section.heading);
            BindText(sectionStatement.Get(), 4, section.anchor);
            BindText(sectionStatement.Get(), 5, section.content);

            if (sqlite3_step(sectionStatement.Get()) != SQLITE_DONE)
            {
                return SqliteError(
                    handle,
                    sqlite3_errcode(handle),
                    "无法保存解析分段"
                );
            }
        }

        return AuditRepository(database_).Append(
            actor,
            "parser",
            "content_saved",
            "file",
            document.fileId,
            "{\"parser\":\"" +
                EscapeJson(document.parserName) +
                "\",\"sections\":" +
                std::to_string(document.sections.size()) +
                "}"
        );
    });
}

bool ParsedContentRepository::Exists(
    const std::string& fileId,
    const std::string& fingerprint
) const
{
    auto* self =
        const_cast<ParsedContentRepository*>(this);

    if (!self->EnsureSchema().success)
    {
        return false;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return false;
    }

    Statement statement(
        handle,
        "SELECT 1 FROM parsed_documents "
        "WHERE file_id=? AND fingerprint=? LIMIT 1;"
    );

    if (!statement.IsValid())
    {
        return false;
    }

    BindText(statement.Get(), 1, fileId);
    BindText(statement.Get(), 2, fingerprint);

    return sqlite3_step(statement.Get()) == SQLITE_ROW;
}

std::string ParsedContentRepository::ContentForFile(
    const std::string& fileId
) const
{
    auto* self =
        const_cast<ParsedContentRepository*>(this);

    if (!self->EnsureSchema().success)
    {
        return std::string();
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return std::string();
    }

    Statement statement(
        handle,
        "SELECT content FROM parsed_documents "
        "WHERE file_id=? LIMIT 1;"
    );

    if (!statement.IsValid())
    {
        return std::string();
    }

    BindText(statement.Get(), 1, fileId);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return std::string();
    }

    return ColumnText(statement.Get(), 0);
}

std::vector<ParsedSection>
ParsedContentRepository::SectionsForFile(
    const std::string& fileId
) const
{
    std::vector<ParsedSection> sections;

    auto* self =
        const_cast<ParsedContentRepository*>(this);

    if (!self->EnsureSchema().success)
    {
        return sections;
    }

    std::lock_guard<std::recursive_mutex> lock(database_.Mutex());
    sqlite3* handle = database_.Handle();

    if (handle == nullptr)
    {
        return sections;
    }

    Statement statement(
        handle,
        "SELECT ordinal, heading, anchor, content "
        "FROM parsed_sections WHERE file_id=? "
        "ORDER BY ordinal;"
    );

    if (!statement.IsValid())
    {
        return sections;
    }

    BindText(statement.Get(), 1, fileId);

    while (sqlite3_step(statement.Get()) == SQLITE_ROW)
    {
        ParsedSection section;
        section.ordinal =
            sqlite3_column_int(statement.Get(), 0);
        section.heading = ColumnText(statement.Get(), 1);
        section.anchor = ColumnText(statement.Get(), 2);
        section.content = ColumnText(statement.Get(), 3);
        sections.push_back(std::move(section));
    }

    return sections;
}

StorageStatus ParsedContentRepository::RemoveForFile(
    const std::string& fileId,
    const std::string& actor
)
{
    auto schemaStatus = EnsureSchema();

    if (!schemaStatus.success)
    {
        return schemaStatus;
    }

    return database_.Transaction([&]() {
        sqlite3* handle = database_.Handle();

        Statement statement(
            handle,
            "DELETE FROM parsed_documents WHERE file_id=?;"
        );

        if (!statement.IsValid())
        {
            return SqliteError(
                handle,
                statement.Status(),
                "无法准备解析内容删除"
            );
        }

        BindText(statement.Get(), 1, fileId);

        if (sqlite3_step(statement.Get()) != SQLITE_DONE)
        {
            return SqliteError(
                handle,
                sqlite3_errcode(handle),
                "无法删除解析内容"
            );
        }

        return AuditRepository(database_).Append(
            actor,
            "parser",
            "content_removed",
            "file",
            fileId,
            "{}"
        );
    });
}

ParseService::ParseService(
    Database& database,
    FileRepository& files
)
    : database_(database),
      files_(files),
      dataSources_(database),
      jobs_(database),
      content_(database),
      registry_()
{
    content_.EnsureSchema();
}

ParseResult ParseService::ParseFile(
    const std::string& fileId,
    const std::string& absolutePath,
    const std::string& actor
)
{
    ParseResult result;

    const auto file = files_.FindById(fileId, false);

    if (!file)
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "待解析文件记录不存在: " + fileId
        );
        return result;
    }

    if (content_.Exists(file->id, file->fingerprint))
    {
        result.document.fileId = file->id;
        result.document.fingerprint = file->fingerprint;
        result.document.content =
            content_.ContentForFile(file->id);
        result.document.sections =
            content_.SectionsForFile(file->id);
        result.status = StorageStatus::Ok();
        return result;
    }

    const ContentParser* parser = registry_.Find(*file);

    if (parser == nullptr)
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "没有适用于该文件的内容解析器"
        );
        files_.SetParseState(fileId, "unsupported", actor);
        return result;
    }

    files_.SetParseState(fileId, "parsing", actor);

    result = parser->Parse(*file, absolutePath);

    if (!result.status.success)
    {
        files_.SetParseState(
            fileId,
            result.requiresExternalParser
                ? "external_required"
                : "failed",
            actor
        );
        return result;
    }

    std::string currentFingerprint;
    const auto fingerprintStatus =
        ContentHasher::Sha256File(
            absolutePath,
            currentFingerprint
        );

    if (!fingerprintStatus.success)
    {
        result.status = fingerprintStatus;
        files_.SetParseState(fileId, "failed", actor);
        return result;
    }

    if (currentFingerprint != file->fingerprint)
    {
        result.status = StorageStatus::Error(
            SQLITE_BUSY,
            "文件在解析期间发生变化"
        );
        files_.SetParseState(fileId, "changed", actor);
        return result;
    }

    const auto saveStatus =
        content_.Save(result.document, actor);

    if (!saveStatus.success)
    {
        result.status = saveStatus;
        files_.SetParseState(fileId, "failed", actor);
        return result;
    }

    const auto stateStatus =
        files_.SetParseState(fileId, "parsed", actor);

    if (!stateStatus.success)
    {
        result.status = stateStatus;
        return result;
    }

    result.status = StorageStatus::Ok();
    return result;
}

std::string ParseService::ResolveAbsolutePath(
    const FileRecord& file
) const
{
    const auto source =
        dataSources_.FindById(file.sourceId);

    if (!source)
    {
        return std::string();
    }

    return (
        std::filesystem::u8path(source->rootPath) /
        std::filesystem::u8path(file.relativePath)
    ).u8string();
}

std::string ParseService::ExtractJsonString(
    const std::string& json,
    const std::string& key
)
{
    const std::regex expression(
        "\"" + key +
        "\"\\s*:\\s*\"((?:\\\\.|[^\"])*)\""
    );

    std::smatch match;

    if (!std::regex_search(json, match, expression) ||
        match.size() < 2)
    {
        return std::string();
    }

    std::string value = match[1].str();
    value = ReplaceAll(value, "\\\\", "\\");
    value = ReplaceAll(value, "\\\"", "\"");
    value = ReplaceAll(value, "\\n", "\n");
    value = ReplaceAll(value, "\\r", "\r");
    value = ReplaceAll(value, "\\t", "\t");
    return value;
}

StorageStatus ParseService::ExecuteJob(
    const JobRecord& job,
    const std::string& actor,
    bool manageJobState
)
{
    if (job.jobType != "parse_file")
    {
        return StorageStatus::Error(
            SQLITE_MISMATCH,
            "任务类型不是 parse_file"
        );
    }

    const std::string fileId =
        ExtractJsonString(job.payload, "file_id");

    if (fileId.empty())
    {
        if (manageJobState)
        {
            jobs_.SetState(
                job.id,
                "failed",
                100,
                "任务缺少 file_id",
                actor
            );
        }

        return StorageStatus::Error(
            SQLITE_CONSTRAINT,
            "解析任务缺少 file_id"
        );
    }

    if (manageJobState)
    {
        const auto runningStatus =
            jobs_.SetState(
                job.id,
                "running",
                10,
                std::string(),
                actor
            );

        if (!runningStatus.success)
        {
            return runningStatus;
        }
    }

    const auto file = files_.FindById(fileId, false);

    if (!file)
    {
        if (manageJobState)
        {
            jobs_.SetState(
                job.id,
                "failed",
                100,
                "文件记录不存在",
                actor
            );
        }

        return StorageStatus::Error(
            SQLITE_NOTFOUND,
            "文件记录不存在"
        );
    }

    const std::string path = ResolveAbsolutePath(*file);

    if (path.empty())
    {
        if (manageJobState)
        {
            jobs_.SetState(
                job.id,
                "failed",
                100,
                "数据源不存在",
                actor
            );
        }

        return StorageStatus::Error(
            SQLITE_NOTFOUND,
            "文件所属数据源不存在"
        );
    }

    const auto result =
        ParseFile(fileId, path, actor);

    if (!result.status.success)
    {
        if (manageJobState)
        {
            jobs_.SetState(
                job.id,
                result.requiresExternalParser
                    ? "blocked"
                    : "failed",
                100,
                result.status.message,
                actor
            );
        }

        return result.status;
    }

    if (!manageJobState)
    {
        return StorageStatus::Ok();
    }

    return jobs_.SetState(
        job.id,
        "completed",
        100,
        std::string(),
        actor
    );
}

int ParseService::ExecutePending(
    int limit,
    const std::string& actor
)
{
    int completed = 0;

    for (const auto& job : jobs_.ListPending(limit))
    {
        if (job.jobType != "parse_file")
        {
            continue;
        }

        if (ExecuteJob(job, actor).success)
        {
            ++completed;
        }
    }

    return completed;
}

}
