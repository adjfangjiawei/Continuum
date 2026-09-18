#include "InvestigationPages.h"

#include "InvestigationService.h"
#include "Theme.h"
#include "WorkspaceService.h"

#include <algorithm>
#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>

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
    auto* text = new wxStaticText(parent, wxID_ANY, value);
    text->SetForegroundColour(color);
    text->SetFont(Theme::Font(size, weight));
    return text;
}

wxPanel* MakeCard(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    Theme::Apply(panel, Theme::Surface());
    return panel;
}

wxButton* MakeButton(
    wxWindow* parent,
    const wxString& label,
    bool primary = false
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
        primary ? Theme::Blue() : Theme::Surface2()
    );
    button->SetForegroundColour(Theme::Text());
    button->SetFont(Theme::Font(9, wxFONTWEIGHT_SEMIBOLD));
    return button;
}

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle,
    wxSizer* actions
)
{
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* labels = new wxBoxSizer(wxVERTICAL);

    labels->Add(
        MakeText(
            parent,
            code,
            9,
            Theme::Blue(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxBOTTOM,
        6
    );

    labels->Add(
        MakeText(
            parent,
            title,
            22,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxBOTTOM,
        6
    );

    labels->Add(
        MakeText(parent, subtitle, 10, Theme::Muted()),
        0
    );

    row->Add(labels, 1, wxEXPAND);

    if (actions != nullptr)
    {
        row->Add(
            actions,
            0,
            wxALIGN_BOTTOM
        );
    }

    root->Add(
        row,
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        26
    );
}

class ConflictPage final : public wxPanel
{
public:
    explicit ConflictPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        auto* detect = MakeButton(this, U("运行检测"));
        auto* refresh = MakeButton(this, U("刷新"), true);

        actions->Add(detect, 0, wxRIGHT, 10);
        actions->Add(refresh, 0);

        AddHeading(
            this,
            root,
            U("P10"),
            U("冲突中心"),
            U("比较相反主张、来源证据和时间顺序，并记录人工裁决"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);
        filterSizer->Add(
            MakeText(
                filters,
                U("冲突类型"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        type_ = new wxChoice(filters, wxID_ANY);
        type_->Append(U("全部冲突"), new wxStringClientData(""));
        type_->Append(U("缺少证据"), new wxStringClientData("missing_evidence"));
        type_->Append(U("时间逻辑错误"), new wxStringClientData("temporal_logic"));
        type_->Append(U("失效引用"), new wxStringClientData("invalid_reference"));
        type_->Append(U("状态冲突"), new wxStringClientData("status_value"));
        type_->SetSelection(0);

        status_ = new wxChoice(filters, wxID_ANY);
        status_->Append(U("待处理"), new wxStringClientData("open"));
        status_->Append(U("全部状态"), new wxStringClientData(""));
        status_->Append(U("已解决"), new wxStringClientData("resolved"));
        status_->Append(U("保留冲突"), new wxStringClientData("retained"));
        status_->Append(U("信息不足"), new wxStringClientData("insufficient"));
        status_->SetSelection(0);

        filterSizer->Add(
            MakeText(filters, U("类型"), 8, Theme::Muted()),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            16
        );
        filterSizer->Add(type_, 0, wxEXPAND | wxALL, 16);
        filterSizer->Add(
            MakeText(filters, U("处理状态"), 8, Theme::Muted()),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            16
        );
        filterSizer->Add(status_, 0, wxEXPAND | wxALL, 16);
        filterSizer->AddStretchSpacer();
        filters->SetSizer(filterSizer);

        auto* queue = MakeCard(this);
        auto* queueSizer = new wxBoxSizer(wxVERTICAL);
        queueSizer->Add(
            MakeText(
                queue,
                U("冲突队列"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        list_ = new wxListCtrl(
            queue,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE
        );
        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->InsertColumn(0, U("严重度"), wxLIST_FORMAT_LEFT, 80);
        list_->InsertColumn(1, U("冲突"), wxLIST_FORMAT_LEFT, 240);
        list_->InsertColumn(2, U("类型"), wxLIST_FORMAT_LEFT, 115);
        list_->InsertColumn(3, U("对象"), wxLIST_FORMAT_LEFT, 125);
        queueSizer->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );
        queue->SetSizer(queueSizer);

        auto* comparison = MakeCard(this);
        auto* compareSizer = new wxBoxSizer(wxVERTICAL);
        compareSizer->Add(
            MakeText(
                comparison,
                U("冲突比较"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        conflictTitle_ = MakeText(
            comparison,
            wxEmptyString,
            15,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        );

        compareSizer->Add(
            conflictTitle_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* claims = new wxBoxSizer(wxHORIZONTAL);
        claimA_ = new wxTextCtrl(
            comparison,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY
        );
        claimB_ = new wxTextCtrl(
            comparison,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY
        );

        claimA_->SetBackgroundColour(Theme::Input());
        claimA_->SetForegroundColour(Theme::Text());
        claimB_->SetBackgroundColour(Theme::Input());
        claimB_->SetForegroundColour(Theme::Text());

        claims->Add(claimA_, 1, wxRIGHT, 10);
        claims->Add(claimB_, 1);
        compareSizer->Add(
            claims,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        compareSizer->Add(
            MakeText(
                comparison,
                U("处理依据"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT,
            16
        );

        note_ = new wxTextCtrl(
            comparison,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(-1, 70),
            wxTE_MULTILINE
        );
        note_->SetBackgroundColour(Theme::Input());
        note_->SetForegroundColour(Theme::Text());

        compareSizer->Add(
            note_,
            0,
            wxEXPAND | wxALL,
            16
        );

        auto* resolution = new wxBoxSizer(wxHORIZONTAL);
        auto* retain = MakeButton(comparison, U("保留冲突"));
        auto* insufficient = MakeButton(comparison, U("信息不足"));
        auto* resolve = MakeButton(
            comparison,
            U("确认处理并记录审计"),
            true
        );

        resolution->Add(retain, 0, wxRIGHT, 10);
        resolution->Add(insufficient, 0, wxRIGHT, 10);
        resolution->Add(resolve, 1);

        compareSizer->Add(
            resolution,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        comparison->SetSizer(compareSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 14);
        filters->SetMinSize(wxSize(220, -1));
        body->Add(queue, 0, wxEXPAND | wxRIGHT, 14);
        queue->SetMinSize(wxSize(440, -1));
        body->Add(comparison, 1, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            RefreshData();
        });

        detect->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            auto& workspace = WorkspaceService::Instance();

            if (!workspace.IsInitialized())
            {
                ShowError(U("工作区尚未初始化。"));
                return;
            }

            const auto result =
                InvestigationService(
                    workspace.GetDatabase()
                ).DetectConflicts();

            if (!result.success)
            {
                ShowError(Wx(result.message));
                return;
            }

            RefreshData();
        });

        type_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            RefreshData();
        });
        status_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            RefreshData();
        });
        list_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
            ShowConflict(static_cast<std::size_t>(event.GetIndex()));
        });

        retain->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("retain");
        });
        insufficient->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("insufficient");
        });
        resolve->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("resolved");
        });

        RefreshData();
    }

private:
    std::string ChoiceValue(wxChoice* choice) const
    {
        const int selection = choice->GetSelection();

        if (selection == wxNOT_FOUND)
        {
            return {};
        }

        auto* data = dynamic_cast<wxStringClientData*>(
            choice->GetClientObject(selection)
        );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    void ShowError(const wxString& message)
    {
        wxMessageBox(
            message,
            U("冲突中心"),
            wxOK | wxICON_ERROR,
            this
        );
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();
        claimA_->Clear();
        claimB_->Clear();
        conflictTitle_->SetLabel(wxEmptyString);

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        records_ = InvestigationService(
            workspace.GetDatabase()
        ).ListConflicts(
            ChoiceValue(status_),
            ChoiceValue(type_),
            1000
        );

        for (std::size_t index = 0;
             index < records_.size();
             ++index)
        {
            const auto& record = records_[index];

            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.severity)
            );
            list_->SetItem(row, 1, Wx(record.title));
            list_->SetItem(row, 2, Wx(record.type));
            list_->SetItem(row, 3, Wx(record.objectId));
        }

        if (!records_.empty())
        {
            list_->SetItemState(
                0,
                wxLIST_STATE_SELECTED,
                wxLIST_STATE_SELECTED
            );
            ShowConflict(0);
        }
    }

    void ShowConflict(std::size_t index)
    {
        if (index >= records_.size())
        {
            return;
        }

        selected_ = index;
        const auto& record = records_[index];

        conflictTitle_->SetLabel(Wx(record.title));

        claimA_->SetValue(
            U("主张 A\n\n") +
            Wx(record.claimA) +
            U("\n\n来源\n") +
            Wx(record.sourceA)
        );

        claimB_->SetValue(
            U("主张 B\n\n") +
            Wx(record.claimB) +
            U("\n\n来源\n") +
            Wx(record.sourceB)
        );

        note_->SetValue(Wx(record.resolutionNote));
        Layout();
    }

    void Resolve(const std::string& resolution)
    {
        if (selected_ >= records_.size())
        {
            return;
        }

        const std::string note = Utf8(note_->GetValue());

        if (note.empty())
        {
            ShowError(U("必须填写处理依据。"));
            return;
        }

        auto& workspace = WorkspaceService::Instance();
        const auto result =
            InvestigationService(
                workspace.GetDatabase()
            ).ResolveConflict(
                records_[selected_].id,
                resolution,
                note
            );

        if (!result.success)
        {
            ShowError(Wx(result.message));
            return;
        }

        RefreshData();
    }

    wxChoice* type_ = nullptr;
    wxChoice* status_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxStaticText* conflictTitle_ = nullptr;
    wxTextCtrl* claimA_ = nullptr;
    wxTextCtrl* claimB_ = nullptr;
    wxTextCtrl* note_ = nullptr;
    std::vector<ConflictViewRecord> records_;
    std::size_t selected_ = 0;
};

class ChangeReviewPage final : public wxPanel
{
public:
    explicit ChangeReviewPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        auto* refresh = MakeButton(this, U("重新比较"));
        auto* confirm = MakeButton(this, U("批量确认"), true);
        actions->Add(refresh, 0, wxRIGHT, 10);
        actions->Add(confirm, 0);

        AddHeading(
            this,
            root,
            U("P11"),
            U("变化审查"),
            U("比较文件版本差异，重新锚定证据并分析业务影响"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* queue = MakeCard(this);
        auto* queueSizer = new wxBoxSizer(wxVERTICAL);
        queueSizer->Add(
            MakeText(
                queue,
                U("文件变化队列"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        state_ = new wxChoice(queue, wxID_ANY);
        state_->Append(U("需处理"), new wxStringClientData("pending"));
        state_->Append(U("全部"), new wxStringClientData(""));
        state_->Append(U("已完成"), new wxStringClientData("completed"));
        state_->SetSelection(0);
        queueSizer->Add(
            state_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        list_ = new wxListCtrl(
            queue,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE
        );
        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->InsertColumn(0, U("文件"), wxLIST_FORMAT_LEFT, 180);
        list_->InsertColumn(1, U("变化"), wxLIST_FORMAT_LEFT, 80);
        list_->InsertColumn(2, U("影响"), wxLIST_FORMAT_LEFT, 80);
        queueSizer->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );
        queue->SetSizer(queueSizer);
        queue->SetMinSize(wxSize(300, -1));

        auto* difference = MakeCard(this);
        auto* differenceSizer = new wxBoxSizer(wxVERTICAL);
        differenceSizer->Add(
            MakeText(
                difference,
                U("版本差异"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        fileTitle_ = MakeText(
            difference,
            wxEmptyString,
            11,
            Theme::Text(),
            wxFONTWEIGHT_SEMIBOLD
        );
        differenceSizer->Add(
            fileTitle_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* versions = new wxBoxSizer(wxHORIZONTAL);

        oldContent_ = new wxTextCtrl(
            difference,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY
        );
        newContent_ = new wxTextCtrl(
            difference,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY
        );

        oldContent_->SetBackgroundColour(Theme::Input());
        oldContent_->SetForegroundColour(Theme::Red());
        newContent_->SetBackgroundColour(Theme::Input());
        newContent_->SetForegroundColour(Theme::Green());

        versions->Add(oldContent_, 1, wxRIGHT, 10);
        versions->Add(newContent_, 1);

        differenceSizer->Add(
            versions,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        note_ = new wxTextCtrl(
            difference,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(-1, 60),
            wxTE_MULTILINE
        );
        note_->SetBackgroundColour(Theme::Input());
        note_->SetForegroundColour(Theme::Text());

        differenceSizer->Add(
            note_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        auto* invalidate = MakeButton(
            difference,
            U("保留旧证据并失效")
        );
        auto* noImpact = MakeButton(
            difference,
            U("标记无业务影响")
        );
        auto* reanchor = MakeButton(
            difference,
            U("重新锚定到新内容"),
            true
        );

        buttons->Add(invalidate, 0, wxRIGHT, 10);
        buttons->Add(noImpact, 0, wxRIGHT, 10);
        buttons->Add(reanchor, 1);

        differenceSizer->Add(
            buttons,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );
        difference->SetSizer(differenceSizer);

        auto* impact = MakeCard(this);
        auto* impactSizer = new wxBoxSizer(wxVERTICAL);
        impactSizer->Add(
            MakeText(
                impact,
                U("业务影响树"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        impactText_ = MakeText(
            impact,
            wxEmptyString,
            10,
            Theme::Yellow(),
            wxFONTWEIGHT_SEMIBOLD
        );
        impactText_->Wrap(220);
        impactSizer->Add(
            impactText_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );
        impactSizer->AddStretchSpacer();
        impact->SetSizer(impactSizer);
        impact->SetMinSize(wxSize(260, -1));

        body->Add(queue, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(difference, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(impact, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );
        SetSizer(root);

        refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            RefreshData();
        });
        state_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            RefreshData();
        });
        list_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
            ShowRecord(static_cast<std::size_t>(event.GetIndex()));
        });
        invalidate->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("invalidated");
        });
        noImpact->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("no_impact");
        });
        reanchor->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Resolve("reanchored");
        });
        confirm->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            for (const auto& record : records_)
            {
                InvestigationService(
                    WorkspaceService::Instance().GetDatabase()
                ).ResolveChange(
                    record.id,
                    "completed",
                    "批量确认当前变化审查"
                );
            }
            RefreshData();
        });

        RefreshData();
    }

private:
    std::string StateValue() const
    {
        auto* data = dynamic_cast<wxStringClientData*>(
            state_->GetClientObject(state_->GetSelection())
        );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();
        oldContent_->Clear();
        newContent_->Clear();
        fileTitle_->SetLabel(wxEmptyString);
        impactText_->SetLabel(wxEmptyString);

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        records_ = InvestigationService(
            workspace.GetDatabase()
        ).ListChanges(StateValue(), 1000);

        for (const auto& record : records_)
        {
            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.fileName)
            );
            list_->SetItem(row, 1, Wx(record.changeType));
            list_->SetItem(
                row,
                2,
                wxString::Format(
                    "%d",
                    record.affectedEvidence
                )
            );
        }

        if (!records_.empty())
        {
            list_->SetItemState(
                0,
                wxLIST_STATE_SELECTED,
                wxLIST_STATE_SELECTED
            );
            ShowRecord(0);
        }
    }

    void ShowRecord(std::size_t index)
    {
        if (index >= records_.size())
        {
            return;
        }

        selected_ = index;
        const auto& record = records_[index];

        fileTitle_->SetLabel(
            Wx(record.fileName) +
            U(" · ") +
            Wx(record.changeType)
        );

        oldContent_->SetValue(
            U("旧版本\n\n") +
            Wx(record.previousContent)
        );

        newContent_->SetValue(
            U("当前版本\n\n") +
            Wx(record.currentContent)
        );

        impactText_->SetLabel(
            U("受影响证据：") +
            wxString::Format("%d", record.affectedEvidence) +
            U("\n\n受影响对象：") +
            wxString::Format("%d", record.affectedObjects) +
            U("\n\n处理会写入审计记录，源文件保持不变。")
        );

        Layout();
    }

    void Resolve(const std::string& resolution)
    {
        if (selected_ >= records_.size())
        {
            return;
        }

        const auto result =
            InvestigationService(
                WorkspaceService::Instance().GetDatabase()
            ).ResolveChange(
                records_[selected_].id,
                resolution,
                Utf8(note_->GetValue())
            );

        if (!result.success)
        {
            wxMessageBox(
                Wx(result.message),
                U("变化审查"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        RefreshData();
    }

    wxChoice* state_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxStaticText* fileTitle_ = nullptr;
    wxTextCtrl* oldContent_ = nullptr;
    wxTextCtrl* newContent_ = nullptr;
    wxTextCtrl* note_ = nullptr;
    wxStaticText* impactText_ = nullptr;
    std::vector<ChangeReviewViewRecord> records_;
    std::size_t selected_ = 0;
};

class RelationshipPage final : public wxPanel
{
public:
    explicit RelationshipPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        auto* save = MakeButton(this, U("保存关系视图"));
        auto* add = MakeButton(this, U("建立关系"), true);
        actions->Add(save, 0, wxRIGHT, 10);
        actions->Add(add, 0);

        AddHeading(
            this,
            root,
            U("P14"),
            U("关系浏览器"),
            U("调查支持、反对、替代、依赖、阻塞和影响关系"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* query = MakeCard(this);
        auto* querySizer = new wxBoxSizer(wxVERTICAL);
        querySizer->Add(
            MakeText(
                query,
                U("路径查询"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        start_ = new wxChoice(query, wxID_ANY);
        target_ = new wxChoice(query, wxID_ANY);
        type_ = new wxChoice(query, wxID_ANY);
        depth_ = new wxSpinCtrl(
            query,
            wxID_ANY,
            "3",
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            1,
            8,
            3
        );

        type_->Append(U("全部关系"), new wxStringClientData(""));
        type_->Append(U("支持"), new wxStringClientData("support"));
        type_->Append(U("反对"), new wxStringClientData("oppose"));
        type_->Append(U("替代"), new wxStringClientData("replace"));
        type_->Append(U("依赖"), new wxStringClientData("depend"));
        type_->Append(U("阻塞"), new wxStringClientData("block"));
        type_->Append(U("影响"), new wxStringClientData("impact"));
        type_->SetSelection(0);

        AddField(querySizer, query, U("起点对象"), start_);
        AddField(querySizer, query, U("终点对象"), target_);
        AddField(querySizer, query, U("关系类型"), type_);
        AddField(querySizer, query, U("最大深度"), depth_);

        auto* run = MakeButton(query, U("运行路径查询"), true);
        querySizer->Add(
            run,
            0,
            wxEXPAND | wxALL,
            16
        );
        querySizer->AddStretchSpacer();
        query->SetSizer(querySizer);
        query->SetMinSize(wxSize(250, -1));

        auto* graph = MakeCard(this);
        auto* graphSizer = new wxBoxSizer(wxVERTICAL);
        graphSizer->Add(
            MakeText(
                graph,
                U("局部关系图"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        relations_ = new wxListCtrl(
            graph,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE
        );
        relations_->SetBackgroundColour(Theme::Input());
        relations_->SetForegroundColour(Theme::Text());
        relations_->InsertColumn(0, U("起点"), wxLIST_FORMAT_LEFT, 190);
        relations_->InsertColumn(1, U("关系"), wxLIST_FORMAT_LEFT, 85);
        relations_->InsertColumn(2, U("终点"), wxLIST_FORMAT_LEFT, 190);
        relations_->InsertColumn(3, U("状态"), wxLIST_FORMAT_LEFT, 85);
        graphSizer->Add(
            relations_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );
        graph->SetSizer(graphSizer);

        auto* inspector = MakeCard(this);
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);
        inspectorSizer->Add(
            MakeText(
                inspector,
                U("节点检查器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        inspectorText_ = MakeText(
            inspector,
            wxEmptyString,
            10,
            Theme::Text(),
            wxFONTWEIGHT_SEMIBOLD
        );
        inspectorText_->Wrap(240);
        inspectorSizer->Add(
            inspectorText_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );
        inspectorSizer->AddStretchSpacer();
        inspector->SetSizer(inspectorSizer);
        inspector->SetMinSize(wxSize(270, -1));

        body->Add(query, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(graph, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );
        SetSizer(root);

        run->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            RefreshRelations();
        });
        add->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            SaveCurrentRelation();
        });
        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            WorkspaceService::Instance().Audit().Append(
                "ui",
                "relation_view",
                "save",
                "workspace",
                std::string(),
                "{}"
            );
        });
        relations_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event) {
                const std::size_t index =
                    static_cast<std::size_t>(event.GetIndex());

                if (index >= records_.size())
                {
                    return;
                }

                const auto& record = records_[index];

                inspectorText_->SetLabel(
                    Wx(record.sourceId) +
                    U("\n") +
                    Wx(record.sourceTitle) +
                    U("\n\n") +
                    Wx(record.type) +
                    U(" →\n\n") +
                    Wx(record.targetId) +
                    U("\n") +
                    Wx(record.targetTitle) +
                    U("\n\n状态：") +
                    Wx(record.status) +
                    U("\n\n") +
                    Wx(record.note)
                );
                Layout();
            }
        );

        LoadObjects();
        RefreshRelations();
    }

private:
    void AddField(
        wxBoxSizer* sizer,
        wxWindow* parent,
        const wxString& title,
        wxWindow* control
    )
    {
        sizer->Add(
            MakeText(parent, title, 8, Theme::Muted()),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            16
        );
        sizer->Add(
            control,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );
    }

    std::string ObjectValue(wxChoice* choice) const
    {
        if (choice->GetSelection() == wxNOT_FOUND)
        {
            return {};
        }

        auto* data = dynamic_cast<wxStringClientData*>(
            choice->GetClientObject(choice->GetSelection())
        );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    std::string TypeValue() const
    {
        auto* data = dynamic_cast<wxStringClientData*>(
            type_->GetClientObject(type_->GetSelection())
        );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    void LoadObjects()
    {
        start_->Clear();
        target_->Clear();

        start_->Append(U("不限"), new wxStringClientData(""));
        target_->Append(U("不限"), new wxStringClientData(""));

        const auto objects =
            WorkspaceService::Instance().Objects().List(
                std::string(),
                false,
                5000
            );

        for (const auto& object : objects)
        {
            const wxString label =
                Wx(object.id) +
                U("  ") +
                Wx(object.title);

            start_->Append(
                label,
                new wxStringClientData(Wx(object.id))
            );
            target_->Append(
                label,
                new wxStringClientData(Wx(object.id))
            );
        }

        start_->SetSelection(0);
        target_->SetSelection(0);
    }

    void RefreshRelations()
    {
        records_.clear();
        relations_->DeleteAllItems();
        inspectorText_->SetLabel(wxEmptyString);

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        records_ = InvestigationService(
            workspace.GetDatabase()
        ).QueryRelations(
            ObjectValue(start_),
            ObjectValue(target_),
            TypeValue(),
            depth_->GetValue(),
            false
        );

        for (const auto& record : records_)
        {
            const long row = relations_->InsertItem(
                relations_->GetItemCount(),
                Wx(record.sourceId) +
                    U("  ") +
                    Wx(record.sourceTitle)
            );

            relations_->SetItem(row, 1, Wx(record.type));
            relations_->SetItem(
                row,
                2,
                Wx(record.targetId) +
                    U("  ") +
                    Wx(record.targetTitle)
            );
            relations_->SetItem(row, 3, Wx(record.status));
        }

        if (!records_.empty())
        {
            relations_->SetItemState(
                0,
                wxLIST_STATE_SELECTED,
                wxLIST_STATE_SELECTED
            );
        }
    }

    void SaveCurrentRelation()
    {
        const std::string sourceId =
            ObjectValue(start_);
        const std::string targetId =
            ObjectValue(target_);
        const std::string relationType =
            TypeValue();

        if (sourceId.empty() ||
            targetId.empty() ||
            relationType.empty())
        {
            wxMessageBox(
                U("请选择关系起点、终点和关系类型。"),
                U("建立关系"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        if (sourceId == targetId)
        {
            wxMessageBox(
                U("关系起点和终点不能是同一个对象。"),
                U("建立关系"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        const wxString note = wxGetTextFromUser(
            U("填写关系依据或说明"),
            U("建立关系"),
            wxEmptyString,
            this
        );

        if (note.empty())
        {
            return;
        }

        auto& workspace = WorkspaceService::Instance();

        const auto result =
            InvestigationService(
                workspace.GetDatabase()
            ).SaveRelation(
                sourceId,
                targetId,
                relationType,
                Utf8(note)
            );

        if (!result.success)
        {
            wxMessageBox(
                Wx(result.message),
                U("建立关系失败"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        RefreshRelations();
    }

    wxChoice* start_ = nullptr;
    wxChoice* target_ = nullptr;
    wxChoice* type_ = nullptr;
    wxSpinCtrl* depth_ = nullptr;
    wxListCtrl* relations_ = nullptr;
    wxStaticText* inspectorText_ = nullptr;
    std::vector<RelationViewRecord> records_;
};

class TimelinePage final : public wxPanel
{
public:
    explicit TimelinePage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        auto* refresh = MakeButton(this, U("刷新"), true);

        actions->Add(refresh, 0);

        AddHeading(
            this,
            root,
            U("P12"),
            U("项目时间线"),
            U("按审计事件发生时间显示工作区中的真实操作记录"),
            actions
        );

        list_ = new wxListCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Surface());
        list_->SetForegroundColour(Theme::Text());
        list_->SetFont(Theme::Font(9));

        list_->InsertColumn(
            0,
            U("序号"),
            wxLIST_FORMAT_RIGHT,
            90
        );
        list_->InsertColumn(
            1,
            U("发生时间"),
            wxLIST_FORMAT_LEFT,
            170
        );
        list_->InsertColumn(
            2,
            U("操作者"),
            wxLIST_FORMAT_LEFT,
            120
        );
        list_->InsertColumn(
            3,
            U("类别"),
            wxLIST_FORMAT_LEFT,
            130
        );
        list_->InsertColumn(
            4,
            U("操作"),
            wxLIST_FORMAT_LEFT,
            150
        );
        list_->InsertColumn(
            5,
            U("对象类型"),
            wxLIST_FORMAT_LEFT,
            130
        );
        list_->InsertColumn(
            6,
            U("对象编号"),
            wxLIST_FORMAT_LEFT,
            210
        );
        list_->InsertColumn(
            7,
            U("事件编号"),
            wxLIST_FORMAT_LEFT,
            250
        );

        root->Add(
            list_,
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

        refresh->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RefreshData();
            }
        );

        RefreshData();
    }

private:
    void RefreshData()
    {
        list_->DeleteAllItems();

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            status_->SetLabel(
                U("工作区尚未初始化。")
            );
            return;
        }

        const auto events =
            workspace.Audit().Recent(1000);

        list_->Freeze();

        for (const auto& event : events)
        {
            const long row = list_->InsertItem(
                list_->GetItemCount(),
                wxString::Format(
                    "%lld",
                    static_cast<long long>(
                        event.sequence
                    )
                )
            );

            list_->SetItem(
                row,
                1,
                Wx(event.occurredAt)
            );
            list_->SetItem(
                row,
                2,
                Wx(event.actor)
            );
            list_->SetItem(
                row,
                3,
                Wx(event.category)
            );
            list_->SetItem(
                row,
                4,
                Wx(event.action)
            );
            list_->SetItem(
                row,
                5,
                Wx(event.objectType)
            );
            list_->SetItem(
                row,
                6,
                Wx(event.objectId)
            );
            list_->SetItem(
                row,
                7,
                Wx(event.eventId)
            );
        }

        list_->Thaw();

        status_->SetLabel(
            U("最近审计事件：") +
            wxString::Format(
                "%d",
                static_cast<int>(events.size())
            ) +
            (
                events.size() >= 1000
                    ? U("（已达到 1000 条显示上限）")
                    : wxString()
            )
        );
    }

    wxListCtrl* list_ = nullptr;
    wxStaticText* status_ = nullptr;
};

}

wxWindow* CreateInvestigationPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P10"))
    {
        return new ConflictPage(parent);
    }

    if (pageCode == U("P11"))
    {
        return new ChangeReviewPage(parent);
    }

    if (pageCode == U("P12"))
    {
        return new TimelinePage(parent);
    }

    if (pageCode == U("P14"))
    {
        return new RelationshipPage(parent);
    }

    return nullptr;
}

}
