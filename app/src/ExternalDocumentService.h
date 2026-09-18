#pragma once

#include "ContentParser.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <string>
#include <vector>

namespace continuum
{

struct ExternalToolStatus
{
    std::string name;
    std::string executable;
    bool available = false;
    std::string message;
};

struct ExternalParserOptions
{
    bool enablePdfTextExtraction = true;
    bool enableOfficeConversion = true;
    bool enableOcr = true;
    bool keepTemporaryFiles = false;
    int timeoutSeconds = 120;
    std::string ocrLanguages = "eng+chi_sim";
};

struct ExternalCommandResult
{
    int exitCode = -1;
    bool started = false;
    bool timedOut = false;
    std::string standardOutput;
    std::string standardError;
};

class ExternalDocumentService final
{
public:
    explicit ExternalDocumentService(
        const ExternalParserOptions& options =
            ExternalParserOptions()
    );

    ParseResult Parse(
        const FileRecord& file,
        const std::string& absolutePath
    ) const;

    std::vector<ExternalToolStatus>
    DiagnoseTools() const;

    bool CanParse(
        const FileRecord& file
    ) const;

    static bool IsPdf(
        const FileRecord& file
    );

    static bool IsOfficeDocument(
        const FileRecord& file
    );

    static bool IsImage(
        const FileRecord& file
    );

private:
    ParseResult ParsePdf(
        const FileRecord& file,
        const std::string& absolutePath
    ) const;

    ParseResult ParseOffice(
        const FileRecord& file,
        const std::string& absolutePath
    ) const;

    ParseResult ParseImageOcr(
        const FileRecord& file,
        const std::string& absolutePath
    ) const;

    ParseResult BuildTextResult(
        const FileRecord& file,
        const std::string& text,
        const std::string& parserName,
        const std::string& metadataJson
    ) const;

    static ExternalCommandResult RunCommand(
        const std::string& executable,
        const std::vector<std::string>& arguments,
        int timeoutSeconds
    );

    static std::string FindExecutable(
        const std::string& environmentVariable,
        const std::vector<std::string>& candidates
    );

    static std::string ReadTextFile(
        const std::string& path
    );

    static std::string MakeTemporaryDirectory();

    static std::string Extension(
        const FileRecord& file
    );

    static std::string EscapeJson(
        const std::string& value
    );

    ExternalParserOptions options_;
};

}
