#include "ExternalDocumentService.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-19-test";

    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base);

#if defined(_WIN32)
    const auto fakeTool =
        base / "fake-pdftotext.cmd";

    {
        std::ofstream script(fakeTool);
        script
            << "@echo off\n"
            << "set input=%4\n"
            << "set output=%5\n"
            << "copy /Y \"%input%\" \"%output%\" >nul\n";
    }

    _putenv_s(
        "CONTINUUM_PDFTOTEXT",
        fakeTool.u8string().c_str()
    );
#else
    const auto fakeTool =
        base / "fake-pdftotext";

    {
        std::ofstream script(fakeTool);
        script
            << "#!/bin/sh\n"
            << "input=\"$4\"\n"
            << "output=\"$5\"\n"
            << "cp \"$input\" \"$output\"\n";
    }

    std::filesystem::permissions(
        fakeTool,
        std::filesystem::perms::owner_read |
        std::filesystem::perms::owner_write |
        std::filesystem::perms::owner_exec,
        std::filesystem::perm_options::replace,
        error
    );

    setenv(
        "CONTINUUM_PDFTOTEXT",
        fakeTool.u8string().c_str(),
        1
    );
#endif

    const auto fakePdf =
        base / "sample.pdf";

    {
        std::ofstream file(
            fakePdf,
            std::ios::binary
        );

        file
            << "Extracted PDF migration evidence.\n"
            << "The document parser is operational.\n";
    }

    FileRecord pdf;
    pdf.id = "FILE-PDF-TEST";
    pdf.sourceId = "SRC-TEST";
    pdf.relativePath = "sample.pdf";
    pdf.displayName = "sample.pdf";
    pdf.mediaType = "application/pdf";
    pdf.fingerprint =
        ContentHasher::Sha256Text("pdf-test");
    pdf.parseState = "pending";

    ExternalParserOptions options;
    options.timeoutSeconds = 10;

    ExternalDocumentService service(options);

    if (!service.CanParse(pdf) ||
        !ExternalDocumentService::IsPdf(pdf))
    {
        std::cerr << "PDF 类型识别失败\n";
        return 1;
    }

    const auto result =
        service.Parse(
            pdf,
            fakePdf.u8string()
        );

    if (!result.status.success)
    {
        std::cerr
            << "模拟 PDF 解析失败: "
            << result.status.message
            << "\n";
        return 2;
    }

    if (result.document.content.find(
            "migration evidence"
        ) == std::string::npos)
    {
        std::cerr << "PDF 提取内容异常\n";
        return 3;
    }

    if (result.document.sections.empty())
    {
        std::cerr << "PDF 内容未生成分段\n";
        return 4;
    }

    if (result.document.parserName !=
        "poppler-pdftotext/1")
    {
        std::cerr << "PDF 解析器名称异常\n";
        return 5;
    }

    FileRecord image;
    image.relativePath = "scan.png";
    image.mediaType = "image/png";

    if (!ExternalDocumentService::IsImage(image))
    {
        std::cerr << "图片类型识别失败\n";
        return 6;
    }

    FileRecord office;
    office.relativePath = "report.docx";

    if (!ExternalDocumentService::IsOfficeDocument(
            office
        ))
    {
        std::cerr << "Office 类型识别失败\n";
        return 7;
    }

    const auto diagnostics =
        service.DiagnoseTools();

    if (diagnostics.size() != 3 ||
        !diagnostics.front().available)
    {
        std::cerr << "外部工具诊断结果异常\n";
        return 8;
    }

    std::filesystem::remove_all(base, error);

    std::cout
        << "round-19 external parser tests passed\n";

    return 0;
}
