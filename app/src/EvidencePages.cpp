#include "EvidencePages.h"

#include "Theme.h"
#include "UiDataService.h"

#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace continuum
{
namespace
{

wxString Wx(const std::string& value)
{
    return wxString::FromUTF8(value.c_str());
}

std::string Utf8(const wxString& value)
{
    const wxScopedCharBuffer buffer = value.ToUTF8();

    return buffer.data() == nullptr
        ? std::string()
        : std::string(buffer.data());
}

wxStaticText* MakeText(
    wxWindow* parent,
    const wxString& value,
    int size,
    const wxColour& color,
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* label = new wxStaticText(
        parent,
        wxID_ANY,
        value
    );

    label->SetForegroundColour(color);
    label->SetFont(Theme::Font(size, weight));
    return label;
}

wxButton* MakeButton(
    wxWindow* parent,
    const wxString& label,
    bool primary = false,
    bool danger = false
)
{
    auto* button = new wxButton(
        parent,
        wxID_ANY,
        label,
        wxDefaultPosition,
        wxSize(-1, 38),
        wxBORDER_NONE
    );

    button->SetBackgroundColour(
        danger
            ? Theme::Red()
            : (
                primary
                    ? Theme::Blue()
                    : Theme::Surface2()
            )
    );

    button->SetForegroundColour(Theme::Text());
    button->SetFont(
        Theme::Font(
            9,
            wxFONTWEIGHT_SEMIBOLD
        )
    );

    return button;
}

wxListCtrl* MakeList(wxWindow* parent)
{
    auto* list = new wxListCtrl(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxLC_REPORT |
            wxLC_SINGLE_SEL |
            wxBORDER_NONE
    );

    list->SetBackgroundColour(Theme::Surface());
    list->SetForegroundColour(Theme::Text());
    list->SetFont(Theme::Font(9));
    return list;
}

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle
)
{
    root->Add(
        MakeText(
            parent,
            code,
            9,
            Theme::Blue(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );

    root->Add(
        MakeText(
            parent,
            title,
            22,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );

    root->Add(
        MakeText(
            parent,
            subtitle,
            10,
            Theme::Muted()
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
}

void ShowFilePreviewDialog(
    wxWindow* parent,
    const std::string& fileId
);

class ReviewInboxPage final : public wxPanel
{
public:
    explicit ReviewInboxPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          list_(nullptr),
          detail_(nullptr),
          status_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("P04"),
            U("审查收件箱"),
            U("审查数据库中真实存在的待确认、已变化或未验证证据")
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* refresh = MakeButton(
            this,
            U("刷新")
        );

        auto* reject = MakeButton(
            this,
            U("拒绝选中证据"),
            false,
            true
        );

        auto* accept = MakeButton(
            this,
            U("接受选中证据"),
            true
        );

        actions->Add(refresh, 0, wxRIGHT, 10);
        actions->Add(reject, 0, wxRIGHT, 10);
        actions->Add(accept, 0);

        root->Add(
            actions,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        list_ = MakeList(this);
        list_->InsertColumn(
            0,
            U("编号"),
            wxLIST_FORMAT_LEFT,
            190
        );
        list_->InsertColumn(
            1,
            U("标题"),
            wxLIST_FORMAT_LEFT,
            280
        );
        list_->InsertColumn(
            2,
            U("状态"),
            wxLIST_FORMAT_LEFT,
            120
        );
        list_->InsertColumn(
            3,
            U("文件编号"),
            wxLIST_FORMAT_LEFT,
            190
        );
        list_->InsertColumn(
            4,
            U("更新时间"),
            wxLIST_FORMAT_LEFT,
            170
        );

        detail_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(420, -1),
            wxTE_MULTILINE |
                wxTE_READONLY |
                wxBORDER_NONE
        );

        detail_->SetBackgroundColour(Theme::Input());
        detail_->SetForegroundColour(Theme::Text());
        detail_->SetFont(Theme::Font(10));

        body->Add(list_, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail_, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        status_ = MakeText(
            this,
            wxEmptyString,
            9,
            Theme::Muted()
        );

        root->Add(
            status_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                ShowDetail(event.GetIndex());
            }
        );

        refresh->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RefreshData();
            }
        );

        accept->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ChangeState("verified");
            }
        );

        reject->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ChangeState("rejected");
            }
        );

        RefreshData();
    }

private:
    void RefreshData()
    {
        records_ =
            UiDataService::Instance()
                .EvidenceForReview(500);

        list_->Freeze();
        list_->DeleteAllItems();

        for (const auto& record : records_)
        {
            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.id)
            );

            list_->SetItem(
                row,
                1,
                Wx(record.title)
            );
            list_->SetItem(
                row,
                2,
                Wx(record.reviewState)
            );
            list_->SetItem(
                row,
                3,
                Wx(record.fileId)
            );
            list_->SetItem(
                row,
                4,
                Wx(record.updatedAt)
            );
        }

        list_->Thaw();

        status_->SetLabel(
            U("待审查证据：") +
            wxString::Format(
                "%d",
                static_cast<int>(records_.size())
            )
        );

        if (records_.empty())
        {
            detail_->ChangeValue(
                U("当前没有待审查证据。")
            );
        }
        else
        {
            list_->SetItemState(
                0,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED
            );

            ShowDetail(0);
        }
    }

    void ShowDetail(long row)
    {
        if (row < 0 ||
            static_cast<std::size_t>(row) >=
                records_.size())
        {
            detail_->ChangeValue(wxEmptyString);
            return;
        }

        const auto& record =
            records_[static_cast<std::size_t>(row)];

        detail_->ChangeValue(
            U("证据编号：") +
            Wx(record.id) +
            U("\n\n标题：") +
            Wx(record.title) +
            U("\n\n审查状态：") +
            Wx(record.reviewState) +
            U("\n\n数据源编号：") +
            Wx(record.sourceId) +
            U("\n\n文件编号：") +
            Wx(record.fileId) +
            U("\n\n来源锚点：\n") +
            Wx(record.anchor) +
            U("\n\n内容指纹：\n") +
            Wx(record.fingerprint) +
            U("\n\n证据原文：\n") +
            Wx(record.quote)
        );
    }

    std::string SelectedId() const
    {
        const long selected =
            list_->GetNextItem(
                -1,
                wxLIST_NEXT_ALL,
                wxLIST_STATE_SELECTED
            );

        if (selected < 0)
        {
            return std::string();
        }

        return Utf8(
            list_->GetItemText(selected, 0)
        );
    }

    void ChangeState(const std::string& state)
    {
        const std::string id = SelectedId();

        if (id.empty())
        {
            wxMessageBox(
                U("请先选择一条证据。"),
                U("证据审查"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        const wxString action =
            state == "verified"
                ? U("接受")
                : U("拒绝");

        if (wxMessageBox(
                U("确定要") +
                    action +
                    U("证据 ") +
                    Wx(id) +
                    U(" 吗？"),
                U("证据审查"),
                wxYES_NO | wxICON_QUESTION,
                this
            ) != wxYES)
        {
            return;
        }

        const auto result =
            UiDataService::Instance()
                .SetEvidenceReviewState(
                    id,
                    state
                );

        if (!result.success)
        {
            wxMessageBox(
                Wx(result.message),
                U("更新证据状态失败"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        RefreshData();
    }

    wxListCtrl* list_;
    wxTextCtrl* detail_;
    wxStaticText* status_;
    std::vector<EvidenceRecord> records_;
};

class LibraryPage final : public wxPanel
{
public:
    explicit LibraryPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          query_(nullptr),
          results_(nullptr),
          status_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("P05"),
            U("资料库"),
            U("在真实全文索引中搜索已解析文件")
        );

        auto* searchRow = new wxBoxSizer(wxHORIZONTAL);

        query_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(-1, 38),
            wxBORDER_NONE | wxTE_PROCESS_ENTER
        );

        query_->SetBackgroundColour(Theme::Input());
        query_->SetForegroundColour(Theme::Text());
        query_->SetFont(Theme::Font(10));
        query_->SetHint(U("输入文件名或文档内容"));

        auto* search = MakeButton(
            this,
            U("搜索"),
            true
        );

        searchRow->Add(query_, 1, wxRIGHT, 10);
        searchRow->Add(search, 0);

        root->Add(
            searchRow,
            0,
            wxEXPAND | wxALL,
            26
        );

        results_ = MakeList(this);
        results_->InsertColumn(
            0,
            U("文件编号"),
            wxLIST_FORMAT_LEFT,
            190
        );
        results_->InsertColumn(
            1,
            U("文件名"),
            wxLIST_FORMAT_LEFT,
            230
        );
        results_->InsertColumn(
            2,
            U("相对路径"),
            wxLIST_FORMAT_LEFT,
            300
        );
        results_->InsertColumn(
            3,
            U("类型"),
            wxLIST_FORMAT_LEFT,
            130
        );
        results_->InsertColumn(
            4,
            U("内容摘要"),
            wxLIST_FORMAT_LEFT,
            520
        );

        root->Add(
            results_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        status_ = MakeText(
            this,
            U("请输入搜索内容。当前后端尚未提供无条件列出全部文件的接口。"),
            9,
            Theme::Muted()
        );

        root->Add(
            status_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        const auto execute =
            [this]()
            {
                RunSearch();
            };

        query_->Bind(
            wxEVT_TEXT_ENTER,
            [execute](wxCommandEvent&)
            {
                execute();
            }
        );

        search->Bind(
            wxEVT_BUTTON,
            [execute](wxCommandEvent&)
            {
                execute();
            }
        );

        results_->Bind(
            wxEVT_LIST_ITEM_ACTIVATED,
            [this](wxListEvent& event)
            {
                const long row = event.GetIndex();

                if (row < 0)
                {
                    return;
                }

                const std::string fileId = Utf8(
                    results_->GetItemText(row, 0)
                );

                if (!fileId.empty())
                {
                    ShowFilePreviewDialog(
                        this,
                        fileId
                    );
                }
            }
        );
    }

private:
    void RunSearch()
    {
        wxString queryText = query_->GetValue();
        queryText.Trim(true);
        queryText.Trim(false);

        const std::string query = Utf8(queryText);

        if (query.empty())
        {
            // 旧查询结果不能继续显示，否则会被误认为是空查询的结果。
            results_->Freeze();
            results_->DeleteAllItems();
            results_->Thaw();

            status_->SetLabel(
                U("请输入搜索内容。")
            );
            return;
        }

        SearchRequest request;
        request.query = query;
        request.limit = 200;

        const auto response =
            UiDataService::Instance().Search(request);

        results_->Freeze();
        results_->DeleteAllItems();

        if (response.status.success)
        {
            for (const auto& hit : response.hits)
            {
                const long row =
                    results_->InsertItem(
                        results_->GetItemCount(),
                        Wx(hit.fileId)
                    );

                results_->SetItem(
                    row,
                    1,
                    Wx(hit.displayName)
                );
                results_->SetItem(
                    row,
                    2,
                    Wx(hit.relativePath)
                );
                results_->SetItem(
                    row,
                    3,
                    Wx(hit.mediaType)
                );
                results_->SetItem(
                    row,
                    4,
                    Wx(hit.snippet)
                );
            }
        }

        results_->Thaw();

        status_->SetLabel(
            response.status.success
                ? (
                    U("找到 ") +
                    wxString::Format(
                        "%d",
                        response.total
                    ) +
                    U(" 条真实索引结果。")
                )
                : (
                    U("搜索失败：") +
                    Wx(response.status.message)
                )
        );
    }

    wxTextCtrl* query_;
    wxListCtrl* results_;
    wxStaticText* status_;
};

wxString FileSummary(
    const ParsedFileContent& data
)
{
    wxString summary =
        U("文件编号：") + Wx(data.file.id) +
        U("\n文件名：") + Wx(data.file.displayName) +
        U("\n相对路径：") + Wx(data.file.relativePath) +
        U("\n媒体类型：") + Wx(data.file.mediaType) +
        U("\n解析状态：") + Wx(data.file.parseState) +
        U("\n文件指纹：") + Wx(data.file.fingerprint) +
        U("\n正文 UTF-8 字节数：") +
        wxString::Format(
            "%lld",
            static_cast<long long>(
                data.contentBytes
            )
        ) +
        U("\n解析内容对应当前文件：") +
        (
            data.matchesCurrentFileFingerprint
                ? U("是")
                : U("否，文件可能已变化或解析结果已过期")
        );

    if (data.contentTruncated)
    {
        summary +=
            U("\n正文预览已截断，以避免大型文档卡死界面。");
    }

    if (data.sectionsTruncated)
    {
        summary +=
            U("\n分段列表已截断，仅显示前 500 个分段。");
    }

    return summary;
}

void ShowFilePreviewDialog(
    wxWindow* parent,
    const std::string& fileId
)
{
    const auto data =
        UiDataService::Instance().LoadParsedFile(
            fileId
        );

    if (!data.status.success)
    {
        wxMessageBox(
            Wx(data.status.message),
            U("无法打开文件"),
            wxOK | wxICON_ERROR,
            parent
        );
        return;
    }

    wxDialog dialog(
        parent,
        wxID_ANY,
        U("文件阅读器 · ") +
            Wx(data.file.displayName),
        wxDefaultPosition,
        wxSize(1100, 760),
        wxDEFAULT_DIALOG_STYLE |
            wxRESIZE_BORDER
    );
    Theme::Apply(&dialog, Theme::Window());

    auto* root = new wxBoxSizer(wxVERTICAL);

    root->Add(
        MakeText(
            &dialog,
            FileSummary(data),
            9,
            data.matchesCurrentFileFingerprint
                ? Theme::Muted()
                : Theme::Yellow()
        ),
        0,
        wxEXPAND | wxALL,
        16
    );

    auto* content = new wxTextCtrl(
        &dialog,
        wxID_ANY,
        Wx(data.content),
        wxDefaultPosition,
        wxDefaultSize,
        wxTE_MULTILINE |
            wxTE_READONLY |
            wxTE_RICH2 |
            wxBORDER_NONE
    );
    content->SetBackgroundColour(Theme::Input());
    content->SetForegroundColour(Theme::Text());
    content->SetFont(Theme::Font(10));

    root->Add(
        content,
        1,
        wxEXPAND | wxLEFT | wxRIGHT,
        16
    );

    root->Add(
        new wxButton(
            &dialog,
            wxID_OK,
            U("关闭")
        ),
        0,
        wxALIGN_RIGHT | wxALL,
        16
    );

    dialog.SetSizer(root);
    dialog.ShowModal();
}

class FileReaderPage final : public wxPanel
{
public:
    explicit FileReaderPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          fileId_(nullptr),
          summary_(nullptr),
          sections_(nullptr),
          content_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("P04"),
            U("文件阅读器"),
            U("按文件编号阅读真实解析正文和内容分段")
        );

        auto* input = new wxBoxSizer(wxHORIZONTAL);

        fileId_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(-1, 38),
            wxBORDER_NONE |
                wxTE_PROCESS_ENTER
        );
        fileId_->SetHint(U("输入文件编号"));
        fileId_->SetBackgroundColour(Theme::Input());
        fileId_->SetForegroundColour(Theme::Text());

        auto* open = MakeButton(
            this,
            U("打开文件"),
            true
        );

        input->Add(fileId_, 1, wxRIGHT, 10);
        input->Add(open, 0);

        root->Add(
            input,
            0,
            wxEXPAND | wxALL,
            26
        );

        summary_ = MakeText(
            this,
            U("请输入资料库搜索结果中的文件编号。"),
            9,
            Theme::Muted()
        );

        root->Add(
            summary_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        sections_ = MakeList(this);
        sections_->SetMinSize(wxSize(320, -1));
        sections_->InsertColumn(
            0,
            U("序号"),
            wxLIST_FORMAT_LEFT,
            70
        );
        sections_->InsertColumn(
            1,
            U("锚点"),
            wxLIST_FORMAT_LEFT,
            130
        );
        sections_->InsertColumn(
            2,
            U("标题"),
            wxLIST_FORMAT_LEFT,
            180
        );

        content_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE |
                wxTE_READONLY |
                wxTE_RICH2 |
                wxBORDER_NONE
        );
        content_->SetBackgroundColour(Theme::Input());
        content_->SetForegroundColour(Theme::Text());
        content_->SetFont(Theme::Font(10));

        body->Add(
            sections_,
            0,
            wxEXPAND | wxRIGHT,
            12
        );
        body->Add(
            content_,
            1,
            wxEXPAND
        );

        root->Add(
            body,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        const auto load = [this]()
        {
            Load();
        };

        open->Bind(
            wxEVT_BUTTON,
            [load](wxCommandEvent&)
            {
                load();
            }
        );

        fileId_->Bind(
            wxEVT_TEXT_ENTER,
            [load](wxCommandEvent&)
            {
                load();
            }
        );

        sections_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                const long row = event.GetIndex();

                if (row >= 0 &&
                    static_cast<std::size_t>(row) <
                        data_.sections.size())
                {
                    content_->ChangeValue(
                        Wx(
                            data_.sections[
                                static_cast<std::size_t>(row)
                            ].content
                        )
                    );
                }
            }
        );
    }

private:
    void Load()
    {
        wxString value = fileId_->GetValue();
        value.Trim(true);
        value.Trim(false);

        if (value.empty())
        {
            wxMessageBox(
                U("请输入文件编号。"),
                U("文件阅读器"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        data_ =
            UiDataService::Instance().LoadParsedFile(
                Utf8(value)
            );

        sections_->DeleteAllItems();
        content_->ChangeValue(wxEmptyString);

        if (!data_.status.success)
        {
            summary_->SetLabel(
                U("打开失败：") +
                Wx(data_.status.message)
            );
            summary_->SetForegroundColour(
                Theme::Red()
            );
            Layout();
            return;
        }

        summary_->SetLabel(
            FileSummary(data_)
        );
        summary_->SetForegroundColour(
            data_.matchesCurrentFileFingerprint
                ? Theme::Muted()
                : Theme::Yellow()
        );

        sections_->Freeze();

        for (const auto& section : data_.sections)
        {
            const long row = sections_->InsertItem(
                sections_->GetItemCount(),
                wxString::Format(
                    "%d",
                    section.ordinal
                )
            );

            sections_->SetItem(
                row,
                1,
                Wx(section.anchor)
            );
            sections_->SetItem(
                row,
                2,
                Wx(section.heading)
            );
        }

        sections_->Thaw();

        if (!data_.sections.empty())
        {
            sections_->SetItemState(
                0,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED
            );
            content_->ChangeValue(
                Wx(data_.sections.front().content)
            );
        }
        else
        {
            content_->ChangeValue(
                Wx(data_.content)
            );
        }

        Layout();
    }

    wxTextCtrl* fileId_;
    wxStaticText* summary_;
    wxListCtrl* sections_;
    wxTextCtrl* content_;
    ParsedFileContent data_;
};

bool ParseCharacterAnchor(
    const std::string& anchor,
    std::size_t& position
)
{
    const std::string prefix = "char:";

    if (anchor.compare(0, prefix.size(), prefix) != 0 ||
        anchor.size() == prefix.size())
    {
        return false;
    }

    std::size_t value = 0;

    for (std::size_t index = prefix.size();
         index < anchor.size();
         ++index)
    {
        const char character = anchor[index];

        if (character < '0' || character > '9')
        {
            return false;
        }

        const std::size_t digit =
            static_cast<std::size_t>(
                character - '0'
            );

        if (value >
            (static_cast<std::size_t>(-1) - digit) / 10)
        {
            return false;
        }

        value = value * 10 + digit;
    }

    position = value;
    return true;
}

class EvidenceInspectorPage final : public wxPanel
{
public:
    explicit EvidenceInspectorPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          evidenceId_(nullptr),
          result_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("P05"),
            U("证据检查器"),
            U("检查证据原文、来源锚点和当前解析文件的一致性")
        );

        auto* input = new wxBoxSizer(wxHORIZONTAL);

        evidenceId_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(-1, 38),
            wxBORDER_NONE |
                wxTE_PROCESS_ENTER
        );
        evidenceId_->SetHint(U("输入证据编号"));
        evidenceId_->SetBackgroundColour(Theme::Input());
        evidenceId_->SetForegroundColour(Theme::Text());

        auto* verify = MakeButton(
            this,
            U("验证证据"),
            true
        );

        input->Add(evidenceId_, 1, wxRIGHT, 10);
        input->Add(verify, 0);

        root->Add(
            input,
            0,
            wxEXPAND | wxALL,
            26
        );

        result_ = new wxTextCtrl(
            this,
            wxID_ANY,
            U("请输入审查收件箱中的证据编号。"),
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE |
                wxTE_READONLY |
                wxTE_RICH2 |
                wxBORDER_NONE
        );
        result_->SetBackgroundColour(Theme::Input());
        result_->SetForegroundColour(Theme::Text());
        result_->SetFont(Theme::Font(10));

        root->Add(
            result_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        const auto execute = [this]()
        {
            Verify();
        };

        verify->Bind(
            wxEVT_BUTTON,
            [execute](wxCommandEvent&)
            {
                execute();
            }
        );

        evidenceId_->Bind(
            wxEVT_TEXT_ENTER,
            [execute](wxCommandEvent&)
            {
                execute();
            }
        );
    }

private:
    void Verify()
    {
        wxString value = evidenceId_->GetValue();
        value.Trim(true);
        value.Trim(false);

        if (value.empty())
        {
            wxMessageBox(
                U("请输入证据编号。"),
                U("证据检查器"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        const auto evidence =
            UiDataService::Instance().FindEvidence(
                Utf8(value)
            );

        if (!evidence)
        {
            result_->ChangeValue(
                U("验证失败：证据不存在或已经删除。")
            );
            return;
        }

        wxString report =
            U("证据编号：") + Wx(evidence->id) +
            U("\n标题：") + Wx(evidence->title) +
            U("\n审查状态：") +
                Wx(evidence->reviewState) +
            U("\n数据源编号：") +
                Wx(evidence->sourceId) +
            U("\n文件编号：") +
                Wx(evidence->fileId) +
            U("\n来源锚点：") +
                Wx(evidence->anchor) +
            U("\n存储的证据指纹：") +
                Wx(evidence->fingerprint) +
            U("\n\n证据原文：\n") +
                Wx(evidence->quote) +
            U("\n\n========== 验证结果 ==========\n");

        if (evidence->fileId.empty())
        {
            report +=
                U("失败：该证据没有关联文件，无法验证来源。\n");
            result_->ChangeValue(report);
            return;
        }

        const auto file =
            UiDataService::Instance().LoadParsedFile(
                evidence->fileId,
                4 * 1024 * 1024,
                1000
            );

        if (!file.status.success)
        {
            report +=
                U("失败：无法读取关联文件：") +
                Wx(file.status.message) +
                U("\n");
            result_->ChangeValue(report);
            return;
        }

        report +=
            U("文件存在：是\n");

        const bool sourceMatches =
            evidence->sourceId.empty() ||
            evidence->sourceId ==
                file.file.sourceId;

        report +=
            U("数据源一致：") +
            (
                sourceMatches
                    ? U("是\n")
                    : U("否\n")
            );

        report +=
            U("解析内容对应当前文件指纹：") +
            (
                file.matchesCurrentFileFingerprint
                    ? U("是\n")
                    : U("否\n")
            );

        const std::size_t quotePosition =
            evidence->quote.empty()
                ? std::string::npos
                : file.content.find(evidence->quote);

        if (evidence->quote.empty())
        {
            report +=
                U("证据原文检查：失败，证据原文为空\n");
        }
        else if (quotePosition != std::string::npos)
        {
            report +=
                U("证据原文仍存在于解析正文：是\n");
        }
        else if (file.contentTruncated)
        {
            report +=
                U("证据原文仍存在于解析正文：无法确定，正文预览被截断\n");
        }
        else
        {
            report +=
                U("证据原文仍存在于解析正文：否\n");
        }

        std::size_t anchorPosition = 0;

        if (!ParseCharacterAnchor(
                evidence->anchor,
                anchorPosition
            ))
        {
            report +=
                U("锚点检查：不支持或格式无效；当前支持 char:N\n");
        }
        else if (anchorPosition >=
                 static_cast<std::size_t>(
                     file.contentBytes
                 ))
        {
            report +=
                U("锚点检查：失败，锚点超出正文范围\n");
        }
        else if (anchorPosition >=
                 file.content.size())
        {
            report +=
                U("锚点检查：无法确定，锚点位于截断部分\n");
        }
        else if (!evidence->quote.empty() &&
                 file.content.compare(
                     anchorPosition,
                     evidence->quote.size(),
                     evidence->quote
                 ) == 0)
        {
            report +=
                U("锚点与证据原文一致：是\n");
        }
        else
        {
            report +=
                U("锚点与证据原文一致：否\n");
        }

        report +=
            U("\n说明：当前未重新计算证据指纹，"
              "因为存储层尚未声明该字段的标准生成算法。");

        result_->ChangeValue(report);
    }

    wxTextCtrl* evidenceId_;
    wxTextCtrl* result_;
};

class UnsupportedEvidencePage final : public wxPanel
{
public:
    UnsupportedEvidencePage(
        wxWindow* parent,
        const wxString& code,
        const wxString& title,
        const wxString& reason
    )
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            code,
            title,
            U("当前功能状态")
        );

        root->Add(
            MakeText(
                this,
                reason,
                11,
                Theme::Yellow(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            26
        );

        root->Add(
            MakeText(
                this,
                U("为避免误导，原先硬编码的文件正文、页码、锚点、指纹和验证结论已移除。"),
                9,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        root->AddStretchSpacer();
        SetSizer(root);
    }
};

}

wxWindow* CreateEvidenceWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P04"))
    {
        return new ReviewInboxPage(parent);
    }

    if (pageCode == U("P05"))
    {
        return new LibraryPage(parent);
    }

    /*
     * P06 是最终设计中的“文档与证据”上下文页面。
     * 当前可执行的文件阅读能力由资料库双击记录进入，
     * 不再在一级导航中制造两个错误页面编号。
     */
    if (pageCode == U("P06"))
    {
        return new FileReaderPage(parent);
    }

    return nullptr;
}

}
