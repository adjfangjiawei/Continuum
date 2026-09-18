#include "LedgerPages.h"

#include "Theme.h"
#include "UiDataService.h"

#include <atomic>
#include <chrono>
#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
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

wxTextCtrl* MakeInput(
    wxWindow* parent,
    long style = 0
)
{
    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(-1, 38),
        wxBORDER_NONE | style
    );

    input->SetBackgroundColour(Theme::Input());
    input->SetForegroundColour(Theme::Text());
    input->SetFont(Theme::Font(9));
    return input;
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

std::string MakeObjectId(
    const std::string& type
)
{
    static std::atomic<unsigned long long> sequence{0};

    const auto ticks =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(
            std::chrono::system_clock::now()
                .time_since_epoch()
        ).count();

    return
        "OBJ-" +
        type +
        "-" +
        std::to_string(ticks) +
        "-" +
        std::to_string(++sequence);
}

class LedgerPage final : public wxPanel
{
public:
    explicit LedgerPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          list_(nullptr),
          type_(nullptr),
          title_(nullptr),
          description_(nullptr),
          statusField_(nullptr),
          priority_(nullptr),
          owner_(nullptr),
          message_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("P08"),
            U("项目台账"),
            U("查看、创建、修改和软删除数据库中的真实业务对象")
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* refresh = MakeButton(this, U("刷新"));
        auto* create = MakeButton(
            this,
            U("新建对象")
        );
        auto* save = MakeButton(
            this,
            U("保存对象"),
            true
        );
        auto* remove = MakeButton(
            this,
            U("移入回收站"),
            false,
            true
        );

        actions->Add(refresh, 0, wxRIGHT, 10);
        actions->Add(create, 0, wxRIGHT, 10);
        actions->Add(save, 0, wxRIGHT, 10);
        actions->Add(remove, 0);

        root->Add(
            actions,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

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
            U("编号"),
            wxLIST_FORMAT_LEFT,
            230
        );
        list_->InsertColumn(
            1,
            U("类型"),
            wxLIST_FORMAT_LEFT,
            100
        );
        list_->InsertColumn(
            2,
            U("标题"),
            wxLIST_FORMAT_LEFT,
            280
        );
        list_->InsertColumn(
            3,
            U("状态"),
            wxLIST_FORMAT_LEFT,
            110
        );
        list_->InsertColumn(
            4,
            U("优先级"),
            wxLIST_FORMAT_LEFT,
            90
        );
        list_->InsertColumn(
            5,
            U("负责人"),
            wxLIST_FORMAT_LEFT,
            140
        );
        list_->InsertColumn(
            6,
            U("更新时间"),
            wxLIST_FORMAT_LEFT,
            170
        );

        auto* editor = new wxPanel(
            this,
            wxID_ANY
        );
        Theme::Apply(editor, Theme::Surface());
        editor->SetMinSize(wxSize(430, -1));

        auto* form = new wxBoxSizer(wxVERTICAL);

        form->Add(
            MakeText(
                editor,
                U("对象编辑"),
                14,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            18
        );

        wxArrayString types;
        types.Add(U("fact"));
        types.Add(U("decision"));
        types.Add(U("commitment"));
        types.Add(U("risk"));
        types.Add(U("issue"));
        types.Add(U("assumption"));
        types.Add(U("external_dependency"));

        type_ = new wxChoice(
            editor,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            types
        );
        type_->SetSelection(0);

        title_ = MakeInput(editor);
        description_ = MakeInput(
            editor,
            wxTE_MULTILINE
        );
        description_->SetMinSize(
            wxSize(-1, 130)
        );
        statusField_ = MakeInput(editor);
        priority_ = MakeInput(editor);
        owner_ = MakeInput(editor);

        AddField(
            editor,
            form,
            U("对象类型"),
            type_
        );
        AddField(
            editor,
            form,
            U("标题"),
            title_
        );
        AddField(
            editor,
            form,
            U("描述"),
            description_
        );
        AddField(
            editor,
            form,
            U("状态"),
            statusField_
        );
        AddField(
            editor,
            form,
            U("优先级"),
            priority_
        );
        AddField(
            editor,
            form,
            U("负责人"),
            owner_
        );

        message_ = MakeText(
            editor,
            wxEmptyString,
            9,
            Theme::Muted()
        );

        form->Add(
            message_,
            0,
            wxEXPAND | wxALL,
            18
        );

        form->AddStretchSpacer();
        editor->SetSizer(form);

        body->Add(list_, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(editor, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                LoadRecord(event.GetIndex());
            }
        );

        refresh->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RefreshData();
            }
        );

        create->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ClearEditor();
            }
        );

        save->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                SaveCurrent();
            }
        );

        remove->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                DeleteCurrent();
            }
        );

        ClearEditor();
        RefreshData();
    }

private:
    void AddField(
        wxWindow* parent,
        wxBoxSizer* form,
        const wxString& label,
        wxWindow* control
    )
    {
        form->Add(
            MakeText(
                parent,
                label,
                8,
                Theme::Muted(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            18
        );

        form->Add(
            control,
            0,
            wxEXPAND | wxLEFT |
                wxRIGHT | wxTOP,
            18
        );
    }

    void RefreshData()
    {
        records_ =
            UiDataService::Instance()
                .ListObjects(
                    std::string(),
                    false,
                    1000
                );

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
                Wx(record.objectType)
            );
            list_->SetItem(
                row,
                2,
                Wx(record.title)
            );
            list_->SetItem(
                row,
                3,
                Wx(record.status)
            );
            list_->SetItem(
                row,
                4,
                Wx(record.priority)
            );
            list_->SetItem(
                row,
                5,
                Wx(record.owner)
            );
            list_->SetItem(
                row,
                6,
                Wx(record.updatedAt)
            );
        }

        list_->Thaw();

        message_->SetLabel(
            U("当前对象数量：") +
            wxString::Format(
                "%d",
                static_cast<int>(records_.size())
            )
        );

        if (!currentId_.empty())
        {
            bool found = false;

            for (std::size_t index = 0;
                 index < records_.size();
                 ++index)
            {
                if (records_[index].id == currentId_)
                {
                    list_->SetItemState(
                        static_cast<long>(index),
                        wxLIST_STATE_SELECTED |
                            wxLIST_STATE_FOCUSED,
                        wxLIST_STATE_SELECTED |
                            wxLIST_STATE_FOCUSED
                    );

                    found = true;
                    break;
                }
            }

            if (!found)
            {
                ClearEditor();
            }
        }
    }

    void ClearEditor()
    {
        currentId_.clear();
        type_->SetSelection(0);
        title_->ChangeValue(wxEmptyString);
        description_->ChangeValue(wxEmptyString);
        statusField_->ChangeValue(U("active"));
        priority_->ChangeValue(U("normal"));
        owner_->ChangeValue(wxEmptyString);
        message_->SetLabel(
            U("正在创建新对象。")
        );
        title_->SetFocus();
    }

    void LoadRecord(long row)
    {
        if (row < 0 ||
            static_cast<std::size_t>(row) >=
                records_.size())
        {
            return;
        }

        const auto& record =
            records_[static_cast<std::size_t>(row)];

        currentId_ = record.id;

        const int typeIndex =
            type_->FindString(
                Wx(record.objectType)
            );

        if (typeIndex == wxNOT_FOUND)
        {
            type_->Append(
                Wx(record.objectType)
            );
            type_->SetSelection(
                static_cast<int>(
                    type_->GetCount() - 1
                )
            );
        }
        else
        {
            type_->SetSelection(typeIndex);
        }

        title_->ChangeValue(Wx(record.title));
        description_->ChangeValue(
            Wx(record.description)
        );
        statusField_->ChangeValue(
            Wx(record.status)
        );
        priority_->ChangeValue(
            Wx(record.priority)
        );
        owner_->ChangeValue(Wx(record.owner));

        message_->SetLabel(
            U("正在编辑对象：") +
            Wx(record.id)
        );
    }

    void SaveCurrent()
    {
        const std::string objectType =
            Utf8(type_->GetStringSelection());

        const std::string title =
            Utf8(title_->GetValue());

        if (objectType.empty() ||
            title.empty())
        {
            wxMessageBox(
                U("对象类型和标题不能为空。"),
                U("保存对象"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        DomainObjectRecord record;

        if (!currentId_.empty())
        {
            for (const auto& existing : records_)
            {
                if (existing.id == currentId_)
                {
                    record = existing;
                    break;
                }
            }
        }

        if (record.id.empty())
        {
            record.id =
                MakeObjectId(objectType);
        }

        record.objectType = objectType;
        record.title = title;
        record.description =
            Utf8(description_->GetValue());
        record.status =
            Utf8(statusField_->GetValue());

        if (record.status.empty())
        {
            record.status = "active";
        }

        record.priority =
            Utf8(priority_->GetValue());

        if (record.priority.empty())
        {
            record.priority = "normal";
        }

        record.owner =
            Utf8(owner_->GetValue());
        record.deleted = false;

        const auto result =
            UiDataService::Instance()
                .SaveObject(record);

        if (!result.success)
        {
            wxMessageBox(
                Wx(result.message),
                U("保存对象失败"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        currentId_ = record.id;
        RefreshData();
    }

    void DeleteCurrent()
    {
        if (currentId_.empty())
        {
            wxMessageBox(
                U("请先选择一个对象。"),
                U("删除对象"),
                wxOK | wxICON_INFORMATION,
                this
            );
            return;
        }

        if (wxMessageBox(
                U("确定将对象 ") +
                    Wx(currentId_) +
                    U(" 移入回收站吗？"),
                U("删除对象"),
                wxYES_NO | wxICON_WARNING,
                this
            ) != wxYES)
        {
            return;
        }

        const auto result =
            UiDataService::Instance()
                .DeleteObject(
                    currentId_,
                    "用户从项目台账删除"
                );

        if (!result.success)
        {
            wxMessageBox(
                Wx(result.message),
                U("删除对象失败"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        ClearEditor();
        RefreshData();
    }

    wxListCtrl* list_;
    wxChoice* type_;
    wxTextCtrl* title_;
    wxTextCtrl* description_;
    wxTextCtrl* statusField_;
    wxTextCtrl* priority_;
    wxTextCtrl* owner_;
    wxStaticText* message_;
    std::vector<DomainObjectRecord> records_;
    std::string currentId_;
};

class DisabledDetailPage final : public wxPanel
{
public:
    DisabledDetailPage(
        wxWindow* parent,
        const wxString& code,
        const wxString& title,
        const wxString& type
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
                U("当前主窗口尚未实现从项目台账向详情页传递选中对象编号，因此无法安全打开真实的") +
                    type +
                    U("详情。"),
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
                U("原先固定展示的示例对象、证据、关系和审计历史已移除，避免用户误以为这些记录真实存在。"),
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

wxWindow* CreateLedgerWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P08"))
    {
        return new LedgerPage(parent);
    }

    /*
     * P09 对象详情必须由台账中的具体对象进入并携带对象编号。
     * 页面工厂不再为无上下文导航创建误导性的静态占位页面。
     */
    return nullptr;
}

}
