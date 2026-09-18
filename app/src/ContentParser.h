#pragma once

#include "FileScanner.h"
#include "Storage.h"
#include "WorkspaceService.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace continuum
{

struct ParsedSection
{
    int ordinal = 0;
    std::string heading;
    std::string anchor;
    std::string content;
};

struct ParsedDocument
{
    std::string fileId;
    std::string fingerprint;
    std::string parserName;
    std::string title;
    std::string author;
    std::string subject;
    std::string content;
    std::string language;
    std::string metadataJson;
    std::string parsedAt;
    std::vector<ParsedSection> sections;
};

struct ParseResult
{
    StorageStatus status;
    ParsedDocument document;
    bool requiresExternalParser = false;
    bool requiresOcr = false;
    std::vector<std::string> warnings;
};

class ContentParser
{
public:
    virtual ~ContentParser() = default;

    virtual bool Supports(
        const FileRecord& file
    ) const = 0;

    virtual ParseResult Parse(
        const FileRecord& file,
        const std::string& absolutePath
    ) const = 0;

    virtual std::string Name() const = 0;
};

class TextContentParser final : public ContentParser
{
public:
    bool Supports(
        const FileRecord& file
    ) const override;

    ParseResult Parse(
        const FileRecord& file,
        const std::string& absolutePath
    ) const override;

    std::string Name() const override;
};

class EmailContentParser final : public ContentParser
{
public:
    bool Supports(
        const FileRecord& file
    ) const override;

    ParseResult Parse(
        const FileRecord& file,
        const std::string& absolutePath
    ) const override;

    std::string Name() const override;
};

class ExternalDocumentParser final : public ContentParser
{
public:
    bool Supports(
        const FileRecord& file
    ) const override;

    ParseResult Parse(
        const FileRecord& file,
        const std::string& absolutePath
    ) const override;

    std::string Name() const override;
};

class ParserRegistry final
{
public:
    ParserRegistry();

    const ContentParser* Find(
        const FileRecord& file
    ) const;

private:
    std::vector<std::unique_ptr<ContentParser>> parsers_;
};

class ParsedContentRepository final
{
public:
    explicit ParsedContentRepository(Database& database);

    StorageStatus EnsureSchema();

    StorageStatus Save(
        const ParsedDocument& document,
        const std::string& actor = "parser"
    );

    bool Exists(
        const std::string& fileId,
        const std::string& fingerprint
    ) const;

    std::string ContentForFile(
        const std::string& fileId
    ) const;

    std::vector<ParsedSection> SectionsForFile(
        const std::string& fileId
    ) const;

    StorageStatus RemoveForFile(
        const std::string& fileId,
        const std::string& actor = "parser"
    );

private:
    Database& database_;
};

class ParseService final
{
public:
    ParseService(
        Database& database,
        FileRepository& files
    );

    ParseResult ParseFile(
        const std::string& fileId,
        const std::string& absolutePath,
        const std::string& actor = "parser"
    );

    StorageStatus ExecuteJob(
        const JobRecord& job,
        const std::string& actor = "worker"
    );

    int ExecutePending(
        int limit = 20,
        const std::string& actor = "worker"
    );

private:
    std::string ResolveAbsolutePath(
        const FileRecord& file
    ) const;

    static std::string ExtractJsonString(
        const std::string& json,
        const std::string& key
    );

    Database& database_;
    FileRepository& files_;
    DataSourceRepository dataSources_;
    JobRepository jobs_;
    ParsedContentRepository content_;
    ParserRegistry registry_;
};

}
