#include "ExternalDocumentService.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <sstream>
#include <thread>
#include <utility>

namespace continuum
{
namespace
{

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

std::string CurrentTimestamp()
{
    const auto now =
        std::chrono::system_clock::now();

    const auto value =
        std::chrono::system_clock::to_time_t(now);

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

std::string EnvironmentValue(
    const std::string& name
)
{
    const char* value =
        std::getenv(name.c_str());

    return value == nullptr
        ? std::string()
        : std::string(value);
}

std::vector<std::string> SplitPath(
    const std::string& value
)
{
    std::vector<std::string> items;

#if defined(_WIN32)
    const char separator = ';';
#else
    const char separator = ':';
#endif

    std::stringstream stream(value);
    std::string item;

    while (std::getline(stream, item, separator))
    {
        if (!item.empty())
        {
            items.push_back(item);
        }
    }

    return items;
}

bool IsExecutableFile(
    const std::filesystem::path& path
)
{
    std::error_code error;

    return std::filesystem::exists(path, error) &&
        std::filesystem::is_regular_file(path, error);
}

std::string QuoteShellArgument(
    const std::string& value
)
{
#if defined(_WIN32)
    std::string result = "\"";

    unsigned int backslashes = 0;

    for (const char character : value)
    {
        if (character == '\\')
        {
            ++backslashes;
            continue;
        }

        if (character == '"')
        {
            result.append(
                backslashes * 2 + 1,
                '\\'
            );
            result.push_back('"');
            backslashes = 0;
            continue;
        }

        result.append(backslashes, '\\');
        backslashes = 0;
        result.push_back(character);
    }

    result.append(backslashes * 2, '\\');
    result.push_back('"');
    return result;
#else
    std::string result = "'";

    for (const char character : value)
    {
        if (character == '\'')
        {
            result += "'\\''";
        }
        else
        {
            result += character;
        }
    }

    result += "'";
    return result;
#endif
}

std::string ReadBinaryText(
    const std::filesystem::path& path
)
{
    std::ifstream stream(
        path,
        std::ios::binary
    );

    if (!stream)
    {
        return std::string();
    }

    std::string value(
        (std::istreambuf_iterator<char>(stream)),
        std::istreambuf_iterator<char>()
    );

    if (value.size() >= 3 &&
        static_cast<unsigned char>(value[0]) == 0xefU &&
        static_cast<unsigned char>(value[1]) == 0xbbU &&
        static_cast<unsigned char>(value[2]) == 0xbfU)
    {
        value.erase(0, 3);
    }

    return value;
}

std::vector<ParsedSection> BuildSections(
    const std::string& content
)
{
    std::vector<ParsedSection> sections;

    if (content.empty())
    {
        return sections;
    }

    constexpr std::size_t sectionSize = 4000;
    constexpr std::size_t overlap = 200;

    std::size_t start = 0;
    int ordinal = 0;

    while (start < content.size())
    {
        std::size_t end = std::min(
            content.size(),
            start + sectionSize
        );

        if (end < content.size())
        {
            const auto paragraph =
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
        section.content = Trim(
            content.substr(start, end - start)
        );

        if (!section.content.empty())
        {
            sections.push_back(
                std::move(section)
            );
        }

        if (end >= content.size())
        {
            break;
        }

        start = end > overlap
            ? end - overlap
            : end;
    }

    return sections;
}

std::string NormalizeText(
    const std::string& input
)
{
    std::string output;
    output.reserve(input.size());

    bool previousSpace = false;
    int newlines = 0;

    for (const unsigned char character : input)
    {
        if (character == '\r')
        {
            continue;
        }

        if (character == '\n')
        {
            while (!output.empty() &&
                   output.back() == ' ')
            {
                output.pop_back();
            }

            ++newlines;

            if (newlines <= 2)
            {
                output.push_back('\n');
            }

            previousSpace = false;
            continue;
        }

        newlines = 0;

        if (character == '\t' ||
            character == '\v' ||
            character == '\f')
        {
            if (!previousSpace &&
                !output.empty() &&
                output.back() != '\n')
            {
                output.push_back(' ');
            }

            previousSpace = true;
            continue;
        }

        output.push_back(
            static_cast<char>(character)
        );

        previousSpace = character == ' ';
    }

    return Trim(output);
}

class TemporaryDirectoryGuard final
{
public:
    TemporaryDirectoryGuard(
        std::string directory,
        bool keep
    )
        : directory_(std::move(directory)),
          keep_(keep)
    {
    }

    ~TemporaryDirectoryGuard()
    {
        if (!keep_ && !directory_.empty())
        {
            std::error_code error;

            std::filesystem::remove_all(
                std::filesystem::u8path(directory_),
                error
            );
        }
    }

    const std::string& Path() const
    {
        return directory_;
    }

private:
    std::string directory_;
    bool keep_;
};

}

ExternalDocumentService::ExternalDocumentService(
    const ExternalParserOptions& options
)
    : options_(options)
{
    options_.timeoutSeconds =
        std::max(5, options_.timeoutSeconds);

    if (options_.ocrLanguages.empty())
    {
        options_.ocrLanguages = "eng";
    }
}

std::string ExternalDocumentService::Extension(
    const FileRecord& file
)
{
    return Lower(
        std::filesystem::u8path(
            file.relativePath
        ).extension().u8string()
    );
}

bool ExternalDocumentService::IsPdf(
    const FileRecord& file
)
{
    return Extension(file) == ".pdf" ||
        file.mediaType == "application/pdf";
}

bool ExternalDocumentService::IsOfficeDocument(
    const FileRecord& file
)
{
    const std::string extension = Extension(file);

    static const std::array<const char*, 10> extensions = {{
        ".doc", ".docx", ".xls", ".xlsx", ".ppt",
        ".pptx", ".odt", ".ods", ".odp", ".rtf"
    }};

    return std::find(
        extensions.begin(),
        extensions.end(),
        extension
    ) != extensions.end();
}

bool ExternalDocumentService::IsImage(
    const FileRecord& file
)
{
    const std::string extension = Extension(file);

    static const std::array<const char*, 7> extensions = {{
        ".png", ".jpg", ".jpeg", ".tif",
        ".tiff", ".bmp", ".webp"
    }};

    return std::find(
        extensions.begin(),
        extensions.end(),
        extension
    ) != extensions.end();
}

bool ExternalDocumentService::CanParse(
    const FileRecord& file
) const
{
    return IsPdf(file) ||
        IsOfficeDocument(file) ||
        IsImage(file);
}

std::string ExternalDocumentService::EscapeJson(
    const std::string& value
)
{
    std::string result;

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

std::string ExternalDocumentService::FindExecutable(
    const std::string& environmentVariable,
    const std::vector<std::string>& candidates
)
{
    const std::string configured =
        EnvironmentValue(environmentVariable);

    if (!configured.empty() &&
        IsExecutableFile(
            std::filesystem::u8path(configured)
        ))
    {
        return configured;
    }

    const auto directories = SplitPath(
        EnvironmentValue("PATH")
    );

    for (const auto& candidate : candidates)
    {
        const std::filesystem::path candidatePath =
            std::filesystem::u8path(candidate);

        if (candidatePath.has_parent_path() &&
            IsExecutableFile(candidatePath))
        {
            return candidatePath.u8string();
        }

        for (const auto& directory : directories)
        {
            auto path =
                std::filesystem::u8path(directory) /
                candidate;

            if (IsExecutableFile(path))
            {
                return path.u8string();
            }

#if defined(_WIN32)
            if (path.extension().empty())
            {
                path += ".exe";

                if (IsExecutableFile(path))
                {
                    return path.u8string();
                }
            }
#endif
        }
    }

    return std::string();
}

std::string ExternalDocumentService::MakeTemporaryDirectory()
{
    static std::atomic<unsigned long long> counter{0};

    const auto ticks =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(
            std::chrono::system_clock::now()
                .time_since_epoch()
        ).count();

    const auto directory =
        std::filesystem::temp_directory_path() /
        (
            "continuum-extract-" +
            std::to_string(ticks) +
            "-" +
            std::to_string(counter.fetch_add(1))
        );

    std::error_code error;

    std::filesystem::create_directories(
        directory,
        error
    );

    return error
        ? std::string()
        : directory.u8string();
}

ExternalCommandResult
ExternalDocumentService::RunCommand(
    const std::string& executable,
    const std::vector<std::string>& arguments,
    int timeoutSeconds
)
{
    ExternalCommandResult result;

    if (executable.empty())
    {
        return result;
    }

    const std::string temporaryDirectory =
        MakeTemporaryDirectory();

    if (temporaryDirectory.empty())
    {
        result.standardError =
            "无法创建外部命令临时目录";

        return result;
    }

    TemporaryDirectoryGuard guard(
        temporaryDirectory,
        false
    );

    const auto stdoutPath =
        std::filesystem::u8path(
            temporaryDirectory
        ) / "stdout.txt";

    const auto stderrPath =
        std::filesystem::u8path(
            temporaryDirectory
        ) / "stderr.txt";

    std::ostringstream command;
    command << QuoteShellArgument(executable);

    for (const auto& argument : arguments)
    {
        command
            << " "
            << QuoteShellArgument(argument);
    }

    command
        << " >"
        << QuoteShellArgument(
            stdoutPath.u8string()
        )
        << " 2>"
        << QuoteShellArgument(
            stderrPath.u8string()
        );

    result.started = true;

    auto future = std::async(
        std::launch::async,
        [commandText = command.str()]() {
            return std::system(commandText.c_str());
        }
    );

    const auto waitResult = future.wait_for(
        std::chrono::seconds(
            std::max(1, timeoutSeconds)
        )
    );

    if (waitResult == std::future_status::ready)
    {
        result.exitCode = future.get();
    }
    else
    {
        /*
         * std::system 没有跨平台安全终止接口。
         * 此处将任务标记超时；future 析构前仍会等待子进程退出。
         * 后续 Windows 发布层可替换为 Job Object，
         * POSIX 可替换为 process group + kill。
         */
        result.timedOut = true;
        result.standardError =
            "外部工具执行超过时间限制";

        future.wait();
        result.exitCode = future.get();
    }

    result.standardOutput =
        ReadBinaryText(stdoutPath);

    const std::string capturedError =
        ReadBinaryText(stderrPath);

    if (!capturedError.empty())
    {
        if (!result.standardError.empty())
        {
            result.standardError += "\n";
        }

        result.standardError += capturedError;
    }

    return result;
}

std::string ExternalDocumentService::ReadTextFile(
    const std::string& path
)
{
    return ReadBinaryText(
        std::filesystem::u8path(path)
    );
}

ParseResult ExternalDocumentService::BuildTextResult(
    const FileRecord& file,
    const std::string& text,
    const std::string& parserName,
    const std::string& metadataJson
) const
{
    ParseResult result;

    const std::string normalized =
        NormalizeText(text);

    if (normalized.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "外部解析器没有提取到可索引文本"
        );

        result.requiresExternalParser = true;

        if (IsPdf(file) || IsImage(file))
        {
            result.requiresOcr = true;
        }

        return result;
    }

    result.document.fileId = file.id;
    result.document.fingerprint =
        file.fingerprint;
    result.document.parserName = parserName;
    result.document.title =
        std::filesystem::u8path(
            file.displayName
        ).stem().u8string();
    result.document.content = normalized;
    result.document.language = "und";
    result.document.metadataJson =
        metadataJson.empty()
            ? "{}"
            : metadataJson;
    result.document.parsedAt =
        CurrentTimestamp();
    result.document.sections =
        BuildSections(normalized);
    result.status = StorageStatus::Ok();

    return result;
}

ParseResult ExternalDocumentService::ParsePdf(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ParseResult result;

    if (!options_.enablePdfTextExtraction)
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "PDF 文本提取已禁用"
        );
        result.requiresExternalParser = true;
        return result;
    }

    const std::string executable =
        FindExecutable(
            "CONTINUUM_PDFTOTEXT",
            {
                "pdftotext"
            }
        );

    if (executable.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "未找到 pdftotext；请安装 Poppler 工具"
        );
        result.requiresExternalParser = true;
        result.requiresOcr = true;
        return result;
    }

    const std::string temporaryDirectory =
        MakeTemporaryDirectory();

    if (temporaryDirectory.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法创建 PDF 解析临时目录"
        );
        return result;
    }

    TemporaryDirectoryGuard guard(
        temporaryDirectory,
        options_.keepTemporaryFiles
    );

    const auto output =
        std::filesystem::u8path(
            temporaryDirectory
        ) / "document.txt";

    const auto command = RunCommand(
        executable,
        {
            "-enc",
            "UTF-8",
            "-layout",
            absolutePath,
            output.u8string()
        },
        options_.timeoutSeconds
    );

    if (!command.started ||
        command.exitCode != 0 ||
        command.timedOut)
    {
        result.status = StorageStatus::Error(
            command.timedOut
                ? SQLITE_INTERRUPT
                : SQLITE_ERROR,
            "pdftotext 执行失败: " +
                command.standardError
        );
        result.requiresExternalParser = true;
        result.requiresOcr = true;
        return result;
    }

    result = BuildTextResult(
        file,
        ReadTextFile(output.u8string()),
        "poppler-pdftotext/1",
        "{\"tool\":\"" +
            EscapeJson(executable) +
            "\"}"
    );

    if (!result.status.success)
    {
        result.requiresOcr = true;
        result.warnings.push_back(
            "PDF 可能是扫描文档，需要 OCR"
        );
    }

    return result;
}

ParseResult ExternalDocumentService::ParseOffice(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ParseResult result;

    if (!options_.enableOfficeConversion)
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "Office 文档转换已禁用"
        );
        result.requiresExternalParser = true;
        return result;
    }

    const std::string executable =
        FindExecutable(
            "CONTINUUM_SOFFICE",
            {
                "soffice",
                "libreoffice"
            }
        );

    if (executable.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "未找到 LibreOffice/soffice"
        );
        result.requiresExternalParser = true;
        return result;
    }

    const std::string temporaryDirectory =
        MakeTemporaryDirectory();

    if (temporaryDirectory.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法创建 Office 转换临时目录"
        );
        return result;
    }

    TemporaryDirectoryGuard guard(
        temporaryDirectory,
        options_.keepTemporaryFiles
    );

    const auto command = RunCommand(
        executable,
        {
            "--headless",
            "--convert-to",
            "txt:Text",
            "--outdir",
            temporaryDirectory,
            absolutePath
        },
        options_.timeoutSeconds
    );

    if (!command.started ||
        command.exitCode != 0 ||
        command.timedOut)
    {
        result.status = StorageStatus::Error(
            command.timedOut
                ? SQLITE_INTERRUPT
                : SQLITE_ERROR,
            "LibreOffice 转换失败: " +
                command.standardError
        );
        result.requiresExternalParser = true;
        return result;
    }

    const auto expected =
        std::filesystem::u8path(
            temporaryDirectory
        ) /
        (
            std::filesystem::u8path(
                absolutePath
            ).stem().u8string() +
            ".txt"
        );

    std::filesystem::path output = expected;
    std::error_code error;

    if (!std::filesystem::exists(output, error))
    {
        for (std::filesystem::directory_iterator iterator(
                 std::filesystem::u8path(
                     temporaryDirectory
                 ),
                 std::filesystem::directory_options::
                     skip_permission_denied,
                 error
             ), end;
             iterator != end;
             iterator.increment(error))
        {
            if (error)
            {
                error.clear();
                continue;
            }

            if (Lower(
                    iterator->path()
                        .extension()
                        .u8string()
                ) == ".txt")
            {
                output = iterator->path();
                break;
            }
        }
    }

    result = BuildTextResult(
        file,
        ReadTextFile(output.u8string()),
        "libreoffice-text-converter/1",
        "{\"tool\":\"" +
            EscapeJson(executable) +
            "\",\"source_extension\":\"" +
            EscapeJson(Extension(file)) +
            "\"}"
    );

    if (!result.status.success)
    {
        result.requiresExternalParser = true;
    }

    return result;
}

ParseResult ExternalDocumentService::ParseImageOcr(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    ParseResult result;

    if (!options_.enableOcr)
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "OCR 已禁用"
        );
        result.requiresExternalParser = true;
        result.requiresOcr = true;
        return result;
    }

    const std::string executable =
        FindExecutable(
            "CONTINUUM_TESSERACT",
            {
                "tesseract"
            }
        );

    if (executable.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_NOTFOUND,
            "未找到 Tesseract OCR"
        );
        result.requiresExternalParser = true;
        result.requiresOcr = true;
        return result;
    }

    const std::string temporaryDirectory =
        MakeTemporaryDirectory();

    if (temporaryDirectory.empty())
    {
        result.status = StorageStatus::Error(
            SQLITE_CANTOPEN,
            "无法创建 OCR 临时目录"
        );
        return result;
    }

    TemporaryDirectoryGuard guard(
        temporaryDirectory,
        options_.keepTemporaryFiles
    );

    const auto outputBase =
        std::filesystem::u8path(
            temporaryDirectory
        ) / "ocr-result";

    const auto command = RunCommand(
        executable,
        {
            absolutePath,
            outputBase.u8string(),
            "-l",
            options_.ocrLanguages,
            "--psm",
            "3"
        },
        options_.timeoutSeconds
    );

    if (!command.started ||
        command.exitCode != 0 ||
        command.timedOut)
    {
        result.status = StorageStatus::Error(
            command.timedOut
                ? SQLITE_INTERRUPT
                : SQLITE_ERROR,
            "Tesseract OCR 执行失败: " +
                command.standardError
        );
        result.requiresExternalParser = true;
        result.requiresOcr = true;
        return result;
    }

    const auto output =
        std::filesystem::u8path(
            outputBase.u8string() + ".txt"
        );

    result = BuildTextResult(
        file,
        ReadTextFile(output.u8string()),
        "tesseract-ocr/1",
        "{\"tool\":\"" +
            EscapeJson(executable) +
            "\",\"languages\":\"" +
            EscapeJson(options_.ocrLanguages) +
            "\"}"
    );

    result.requiresOcr =
        !result.status.success;

    return result;
}

ParseResult ExternalDocumentService::Parse(
    const FileRecord& file,
    const std::string& absolutePath
) const
{
    std::error_code error;
    const auto path =
        std::filesystem::u8path(absolutePath);

    if (!std::filesystem::exists(path, error) ||
        !std::filesystem::is_regular_file(path, error))
    {
        ParseResult result;
        result.status = StorageStatus::Error(
            SQLITE_CANTOPEN,
            "外部解析文件不存在或不可访问: " +
                absolutePath
        );
        return result;
    }

    if (IsPdf(file))
    {
        return ParsePdf(file, absolutePath);
    }

    if (IsOfficeDocument(file))
    {
        return ParseOffice(file, absolutePath);
    }

    if (IsImage(file))
    {
        return ParseImageOcr(
            file,
            absolutePath
        );
    }

    ParseResult result;
    result.status = StorageStatus::Error(
        SQLITE_NOTFOUND,
        "外部解析服务不支持该文件类型"
    );
    result.requiresExternalParser = true;
    return result;
}

std::vector<ExternalToolStatus>
ExternalDocumentService::DiagnoseTools() const
{
    std::vector<ExternalToolStatus> statuses;

    const auto add =
        [&statuses](
            const std::string& name,
            const std::string& executable,
            const std::string& installHint
        ) {
            ExternalToolStatus status;
            status.name = name;
            status.executable = executable;
            status.available =
                !executable.empty();
            status.message = status.available
                ? "可用"
                : installHint;
            statuses.push_back(
                std::move(status)
            );
        };

    add(
        "pdftotext",
        FindExecutable(
            "CONTINUUM_PDFTOTEXT",
            {"pdftotext"}
        ),
        "未安装 Poppler PDF 工具"
    );

    add(
        "LibreOffice",
        FindExecutable(
            "CONTINUUM_SOFFICE",
            {"soffice", "libreoffice"}
        ),
        "未安装 LibreOffice"
    );

    add(
        "Tesseract OCR",
        FindExecutable(
            "CONTINUUM_TESSERACT",
            {"tesseract"}
        ),
        "未安装 Tesseract OCR"
    );

    return statuses;
}

}
