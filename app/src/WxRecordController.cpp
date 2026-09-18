#include "WxRecordController.h"

#include "UiEventBus.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

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

std::string Utf8(const wxString& value)
{
    const wxScopedCharBuffer buffer =
        value.ToUTF8();

    return buffer.data() == nullptr
        ? std::string()
        : std::string(buffer.data());
}

wxString Wx(const std::string& value)
{
    return wxString::FromUTF8(
        value.c_str()
    );
}

wxWindow* FindRecursively(
    wxWindow* parent,
    const std::string& normalizedName
)
{
    if (parent == nullptr)
    {
        return nullptr;
    }

    if (Lower(Utf8(parent->GetName())) ==
        normalizedName)
    {
        return parent;
    }

    for (wxWindow* child : parent->GetChildren())
    {
        if (auto* found = FindRecursively(
                child,
                normalizedName
            ))
        {
            return found;
        }
    }

    return nullptr;
}

void PrepareList(
    wxListCtrl* list,
    const std::vector<wxString>& headings,
    const std::vector<int>& widths
)
{
    if (list == nullptr)
    {
        return;
    }

    list->Freeze();
    list->ClearAll();

    for (std::size_t index = 0;
         index < headings.size();
         ++index)
    {
        list->InsertColumn(
            static_cast<long>(index),
            headings[index]
        );

        if (index < widths.size())
        {
            list->SetColumnWidth(
                static_cast<int>(index),
                widths[index]
            );
        }
    }

    list->Thaw();
}

}

WxRecordController&
WxRecordController::Instance()
{
    static WxRecordController instance;
    return instance;
}

WxRecordController::WxRecordController()
    : root_(nullptr),
      commandsBound_(false)
{
}

WxRecordController::~WxRecordController()
{
    Detach();
}

UiOperationResult WxRecordController::Attach(
    wxWindow* root
)
{
    if (root == nullptr)
    {
        return {
            false,
            SQLITE_MISUSE,
            "无法把记录控制器绑定到空窗口"
        };
    }

    Detach();
    root_ = root;

    root_->Bind(
        wxEVT_CONTINUUM_UI_CHANGE,
        &WxRecordController::OnUiChange,
        this
    );

    BindCommands();
    RefreshAll();

    return UiOperationResult::Ok(
        "对象与证据页面控制器已启动"
    );
}

void WxRecordController::Detach()
{
    if (root_ != nullptr)
    {
        root_->Unbind(
            wxEVT_CONTINUUM_UI_CHANGE,
            &WxRecordController::OnUiChange,
            this
        );
    }

    root_ = nullptr;
    commandsBound_ = false;
    objectCache_.clear();
}

bool WxRecordController::IsAttached() const
{
    return root_ != nullptr;
}

wxWindow* WxRecordController::FindControl(
    const std::string& name
) const
{
    return FindRecursively(
        root_,
        Lower(name)
    );
}

std::string WxRecordController::ReadValue(
    const std::string& name
) const
{
    wxWindow* control = FindControl(name);

    if (auto* text =
            dynamic_cast<wxTextCtrl*>(control))
    {
        return Utf8(text->GetValue());
    }

    if (auto* choice =
            dynamic_cast<wxChoice*>(control))
    {
        return Utf8(
            choice->GetStringSelection()
        );
    }

    if (auto* combo =
            dynamic_cast<wxComboBox*>(control))
    {
        return Utf8(combo->GetValue());
    }

    if (auto* label =
            dynamic_cast<wxStaticText*>(control))
    {
        return Utf8(label->GetLabel());
    }

    return std::string();
}

void WxRecordController::WriteValue(
    const std::string& name,
    const std::string& value
)
{
    wxWindow* control = FindControl(name);

    if (auto* text =
            dynamic_cast<wxTextCtrl*>(control))
    {
        text->ChangeValue(Wx(value));
        return;
    }

    if (auto* choice =
            dynamic_cast<wxChoice*>(control))
    {
        if (!choice->SetStringSelection(
                Wx(value)
            ) &&
            !value.empty())
        {
            choice->Append(Wx(value));
            choice->SetStringSelection(
                Wx(value)
            );
        }

        return;
    }

    if (auto* combo =
            dynamic_cast<wxComboBox*>(control))
    {
        combo->ChangeValue(Wx(value));
        return;
    }

    if (auto* label =
            dynamic_cast<wxStaticText*>(control))
    {
        label->SetLabel(Wx(value));
    }
}

void WxRecordController::SetStatus(
    const std::string& name,
    const UiOperationResult& result
)
{
    WriteValue(
        name,
        result.success
            ? (
                result.message.empty()
                    ? "操作成功"
                    : result.message
            )
            : (
                "操作失败：" +
                result.message
            )
    );
}

std::string WxRecordController::SelectedId(
    const std::string& listName
) const
{
    auto* list =
        FindAs<wxListCtrl>(listName);

    if (list == nullptr)
    {
        return std::string();
    }

    const long selected = list->GetNextItem(
        -1,
        wxLIST_NEXT_ALL,
        wxLIST_STATE_SELECTED
    );

    if (selected < 0)
    {
        return std::string();
    }

    return Utf8(
        list->GetItemText(selected, 0)
    );
}

DomainObjectRecord*
WxRecordController::FindCachedObject(
    const std::string& id
)
{
    const auto iterator = std::find_if(
        objectCache_.begin(),
        objectCache_.end(),
        [&id](const DomainObjectRecord& record) {
            return record.id == id;
        }
    );

    return iterator == objectCache_.end()
        ? nullptr
        : &(*iterator);
}

std::string
WxRecordController::GenerateObjectId()
{
    const auto ticks =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(
            std::chrono::system_clock::now()
                .time_since_epoch()
        ).count();

    std::ostringstream stream;
    stream
        << "OBJ-UI-"
        << ticks;

    return stream.str();
}

void WxRecordController::ClearObjectForm()
{
    WriteValue(
        "objects.id",
        GenerateObjectId()
    );
    WriteValue("objects.type", "fact");
    WriteValue("objects.title", "");
    WriteValue("objects.description", "");
    WriteValue("objects.status", "active");
    WriteValue("objects.priority", "normal");
    WriteValue("objects.owner", "");
    WriteValue(
        "objects.time_precision",
        "exact"
    );

    WriteValue(
        "objects.status_message",
        "正在创建新对象"
    );
}

void WxRecordController::LoadObjectForm(
    const DomainObjectRecord& record
)
{
    WriteValue("objects.id", record.id);
    WriteValue(
        "objects.type",
        record.objectType
    );
    WriteValue(
        "objects.title",
        record.title
    );
    WriteValue(
        "objects.description",
        record.description
    );
    WriteValue(
        "objects.status",
        record.status
    );
    WriteValue(
        "objects.priority",
        record.priority
    );
    WriteValue(
        "objects.owner",
        record.owner
    );
    WriteValue(
        "objects.time_precision",
        record.timePrecision
    );
}

void WxRecordController::BindCommands()
{
    if (root_ == nullptr ||
        commandsBound_)
    {
        return;
    }

    if (auto* list =
            FindAs<wxListCtrl>("objects.list"))
    {
        list->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            &WxRecordController::OnObjectSelected,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("objects.new"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnNewObject,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("objects.save"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnSaveObject,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("objects.delete"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnDeleteObject,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("objects.restore"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnRestoreObject,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("evidence.verify"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnVerifyEvidence,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("evidence.reject"))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::OnRejectEvidence,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>(
                "evidence.needs_review"
            ))
    {
        button->Bind(
            wxEVT_BUTTON,
            &WxRecordController::
                OnMarkEvidenceForReview,
            this
        );
    }

    commandsBound_ = true;
}

void WxRecordController::RefreshAll()
{
    if (root_ == nullptr)
    {
        return;
    }

    /*
     * 页面可能由路由器延迟创建，因此完整刷新时
     * 重新查找并绑定控件。
     */
    commandsBound_ = false;
    BindCommands();

    RefreshObjects();
    RefreshEvidence();
}

void WxRecordController::RefreshObjects()
{
    auto* list =
        FindAs<wxListCtrl>("objects.list");

    if (list == nullptr)
    {
        return;
    }

    objectCache_ =
        UiDataService::Instance().ListObjects(
            std::string(),
            true,
            1000
        );

    PrepareList(
        list,
        {
            "编号",
            "类型",
            "标题",
            "状态",
            "优先级",
            "负责人"
        },
        {
            220,
            110,
            300,
            100,
            90,
            140
        }
    );

    list->Freeze();

    for (const auto& record : objectCache_)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(record.id)
        );

        list->SetItem(
            row,
            1,
            Wx(record.objectType)
        );
        list->SetItem(
            row,
            2,
            Wx(record.title)
        );
        list->SetItem(
            row,
            3,
            Wx(record.status)
        );
        list->SetItem(
            row,
            4,
            Wx(record.priority)
        );
        list->SetItem(
            row,
            5,
            Wx(record.owner)
        );
    }

    list->Thaw();
}

void WxRecordController::RefreshEvidence()
{
    auto* list =
        FindAs<wxListCtrl>(
            "evidence.review_list"
        );

    if (list == nullptr)
    {
        return;
    }

    const auto records =
        UiDataService::Instance()
            .EvidenceForReview(1000);

    PrepareList(
        list,
        {
            "证据编号",
            "操作"
        },
        {
            360,
            240
        }
    );

    list->Freeze();

    for (const auto& record : records)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(record.id)
        );

        list->SetItem(
            row,
            1,
            "等待人工审查"
        );
    }

    list->Thaw();

    WriteValue(
        "evidence.status_message",
        "待审证据：" +
            std::to_string(records.size())
    );
}

void WxRecordController::OnUiChange(
    wxThreadEvent& event
)
{
    const auto change =
        event.GetPayload<UiChangeEvent>();

    switch (change.type)
    {
    case UiChangeType::Objects:
        RefreshObjects();
        break;

    case UiChangeType::Evidence:
        RefreshEvidence();
        break;

    case UiChangeType::Workspace:
        RefreshAll();
        break;

    default:
        break;
    }
}

void WxRecordController::OnObjectSelected(
    wxListEvent& event
)
{
    const std::string id = Utf8(
        event.GetText()
    );

    if (auto* record =
            FindCachedObject(id))
    {
        LoadObjectForm(*record);
    }
}

void WxRecordController::OnNewObject(
    wxCommandEvent&
)
{
    ClearObjectForm();
}

void WxRecordController::OnSaveObject(
    wxCommandEvent&
)
{
    DomainObjectRecord record;

    record.id = ReadValue("objects.id");

    if (record.id.empty())
    {
        record.id = GenerateObjectId();
    }

    record.objectType =
        ReadValue("objects.type");
    record.title =
        ReadValue("objects.title");
    record.description =
        ReadValue("objects.description");
    record.status =
        ReadValue("objects.status");
    record.priority =
        ReadValue("objects.priority");
    record.owner =
        ReadValue("objects.owner");
    record.timePrecision =
        ReadValue("objects.time_precision");

    if (record.objectType.empty())
    {
        record.objectType = "fact";
    }

    if (record.title.empty())
    {
        SetStatus(
            "objects.status_message",
            {
                false,
                SQLITE_CONSTRAINT,
                "标题不能为空"
            }
        );

        return;
    }

    if (record.status.empty())
    {
        record.status = "active";
    }

    if (record.priority.empty())
    {
        record.priority = "normal";
    }

    if (record.timePrecision.empty())
    {
        record.timePrecision = "exact";
    }

    const auto result =
        UiDataService::Instance()
            .SaveObject(record);

    SetStatus(
        "objects.status_message",
        result
    );

    if (result.success)
    {
        WriteValue("objects.id", record.id);
        RefreshObjects();
    }
}

void WxRecordController::OnDeleteObject(
    wxCommandEvent&
)
{
    std::string id =
        SelectedId("objects.list");

    if (id.empty())
    {
        id = ReadValue("objects.id");
    }

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一个业务对象。",
            "删除对象",
            wxOK | wxICON_INFORMATION,
            root_
        );

        return;
    }

    const int confirmation = wxMessageBox(
        "确定要把所选对象移入回收站吗？",
        "确认删除",
        wxYES_NO | wxICON_WARNING,
        root_
    );

    if (confirmation != wxYES)
    {
        return;
    }

    const auto result =
        UiDataService::Instance()
            .DeleteObject(
                id,
                "用户通过界面删除"
            );

    SetStatus(
        "objects.status_message",
        result
    );

    if (result.success)
    {
        RefreshObjects();
    }
}

void WxRecordController::OnRestoreObject(
    wxCommandEvent&
)
{
    std::string id =
        SelectedId("objects.list");

    if (id.empty())
    {
        id = ReadValue("objects.id");
    }

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一个业务对象。",
            "恢复对象",
            wxOK | wxICON_INFORMATION,
            root_
        );

        return;
    }

    const auto result =
        UiDataService::Instance()
            .RestoreObject(id);

    SetStatus(
        "objects.status_message",
        result
    );

    if (result.success)
    {
        RefreshObjects();
    }
}

void WxRecordController::OnVerifyEvidence(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedId("evidence.review_list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一条证据。",
            "证据审查",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .SetEvidenceReviewState(
                id,
                "verified"
            );

    SetStatus(
        "evidence.status_message",
        result
    );

    if (result.success)
    {
        RefreshEvidence();
    }
}

void WxRecordController::OnRejectEvidence(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedId("evidence.review_list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一条证据。",
            "证据审查",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .SetEvidenceReviewState(
                id,
                "rejected"
            );

    SetStatus(
        "evidence.status_message",
        result
    );

    if (result.success)
    {
        RefreshEvidence();
    }
}

void WxRecordController::OnMarkEvidenceForReview(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedId("evidence.review_list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一条证据。",
            "证据审查",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .SetEvidenceReviewState(
                id,
                "needs_review"
            );

    SetStatus(
        "evidence.status_message",
        result
    );

    if (result.success)
    {
        RefreshEvidence();
    }
}

}
