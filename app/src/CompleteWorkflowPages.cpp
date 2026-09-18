#include "CompleteWorkflowPages.h"

#include "SecurityService.h"
#include "FileScanner.h"
#include "SearchService.h"
#include "UiDataService.h"
#include "Theme.h"
#include "WorkspaceService.h" 

#include <algorithm>
#include <atomic>
#include <future>
#include <map>
#include <set>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <sqlite3.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/timer.h>
#include <wx/utils.h>

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

class Statement final
{
public:
    Statement(sqlite3* database, const std::string& sql)
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

    bool Valid() const
    {
        return status_ == SQLITE_OK &&
            statement_ != nullptr;
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

std::string Text(
    sqlite3_stmt* statement,
    int column
)
{
    const auto* value =
        sqlite3_column_text(statement, column);

    return value == nullptr
        ? std::string()
        : reinterpret_cast<const char*>(value);
}

StorageStatus Error(
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

std::string NewId(const std::string& prefix)
{
    const auto now =
        std::chrono::system_clock::now();

    const auto ticks =
        std::chrono::duration_cast<
            std::chrono::microseconds
        >(now.time_since_epoch()).count();

    return prefix + "-" +
        ContentHasher::Sha256Text(
            prefix + ":" + std::to_string(ticks)
        ).substr(0, 24);
}

std::string Html(const std::string& value)
{
    std::string result;

    for (const char character : value)
    {
        switch (character)
        {
        case '&':
            result += "&amp;";
            break;
        case '<':
            result += "&lt;";
            break;
        case '>':
            result += "&gt;";
            break;
        case '"':
            result += "&quot;";
            break;
        default:
            result += character;
            break;
        }
    }

    return result;
}

wxStaticText* Label(
    wxWindow* parent,
    const wxString& value,
    int size = 10,
    const wxColour& colour = Theme::Text(),
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* label =
        new wxStaticText(parent, wxID_ANY, value);

    label->SetForegroundColour(colour);
    label->SetFont(Theme::Font(size, weight));
    return label;
}

wxPanel* Card(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    Theme::Apply(panel, Theme::Surface());
    return panel;
}

wxButton* Button(
    wxWindow* parent,
    const wxString& text,
    bool primary = false,
    bool destructive = false
)
{
    auto* button = new wxButton(
        parent,
        wxID_ANY,
        text,
        wxDefaultPosition,
        wxSize(-1, 38),
        wxBORDER_NONE
    );

    button->SetFont(
        Theme::Font(10, wxFONTWEIGHT_SEMIBOLD)
    );

    button->SetBackgroundColour(
        destructive
            ? Theme::Red()
            : primary
                ? Theme::Blue()
                : Theme::Surface2()
    );

    button->SetForegroundColour(Theme::Text());
    return button;
}

wxTextCtrl* Input(
    wxWindow* parent,
    long style = 0
)
{
    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxDefaultSize,
        style
    );

    input->SetBackgroundColour(Theme::Input());
    input->SetForegroundColour(Theme::Text());
    input->SetFont(Theme::Font(9));
    return input;
}

void AddField(
    wxBoxSizer* layout,
    wxWindow* parent,
    const wxString& title,
    wxWindow* control
)
{
    layout->Add(
        Label(parent, title, 8, Theme::Muted()),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        16
    );

    layout->Add(
        control,
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        16
    );
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
    auto* heading = new wxBoxSizer(wxVERTICAL);

    heading->Add(
        Label(
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

    heading->Add(
        Label(
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

    heading->Add(
        Label(parent, subtitle, 10, Theme::Muted()),
        0
    );

    row->Add(heading, 1, wxEXPAND);

    if (actions != nullptr)
    {
        row->Add(actions, 0, wxALIGN_BOTTOM);
    }

    root->Add(
        row,
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        26
    );
}

void ShowStorageError(
    wxWindow* parent,
    const wxString& title,
    const StorageStatus& status
)
{
    wxMessageBox(
        Wx(status.message),
        title,
        wxOK | wxICON_ERROR,
        parent
    );
}

StorageStatus SetMeta(
    Database& database,
    const std::string& key,
    const std::string& value,
    const std::string& actor
)
{
    return database.Transaction([&]() {
        sqlite3* handle = database.Handle();

        Statement statement(
            handle,
            "INSERT INTO workspace_meta("
            "key, value, updated_at"
            ") VALUES(?, ?, CURRENT_TIMESTAMP) "
            "ON CONFLICT(key) DO UPDATE SET "
            "value=excluded.value, "
            "updated_at=CURRENT_TIMESTAMP;"
        );

        if (!statement.Valid())
        {
            return Error(
                handle,
                statement.Status(),
                "无法准备工作区配置保存"
            );
        }

        BindText(statement.Get(), 1, key);
        BindText(statement.Get(), 2, value);

        const int result =
            sqlite3_step(statement.Get());

        if (result != SQLITE_DONE)
        {
            return Error(
                handle,
                result,
                "无法保存工作区配置"
            );
        }

        return AuditRepository(database).Append(
            actor,
            "workspace_setting",
            "save",
            "workspace_meta",
            key,
            "{}"
        );
    });
}

std::string GetMeta(
    Database& database,
    const std::string& key,
    const std::string& fallback
)
{
    std::lock_guard<std::recursive_mutex> lock(
        database.Mutex()
    );

    sqlite3* handle = database.Handle();

    if (handle == nullptr)
    {
        return fallback;
    }

    Statement statement(
        handle,
        "SELECT value FROM workspace_meta "
        "WHERE key=? LIMIT 1;"
    );

    if (!statement.Valid())
    {
        return fallback;
    }

    BindText(statement.Get(), 1, key);

    if (sqlite3_step(statement.Get()) != SQLITE_ROW)
    {
        return fallback;
    }

    return Text(statement.Get(), 0);
}

class EntityPage final : public wxPanel
{
public:
    explicit EntityPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* duplicates =
            Button(this, U("检测重复"));
        auto* create =
            Button(this, U("新建实体"), true);

        actions->Add(duplicates, 0, wxRIGHT, 10);
        actions->Add(create, 0);

        AddHeading(
            this,
            root,
            U("M05"),
            U("标签与实体管理"),
            U("规范项目中的标签、人员、组织、系统与术语"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* scope = Card(this);
        auto* scopeLayout = new wxBoxSizer(wxVERTICAL);

        scopeLayout->Add(
            Label(
                scope,
                U("管理范围"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        type_ = new wxChoice(scope, wxID_ANY);
        type_->Append(U("全部实体"), new wxStringClientData(""));
        type_->Append(U("人员"), new wxStringClientData("person"));
        type_->Append(U("组织"), new wxStringClientData("organization"));
        type_->Append(U("项目"), new wxStringClientData("project"));
        type_->Append(U("产品与系统"), new wxStringClientData("system"));
        type_->Append(U("术语"), new wxStringClientData("term"));
        type_->SetSelection(0);

        scopeLayout->Add(
            type_,
            0,
            wxEXPAND | wxALL,
            16
        );

        scopeLayout->Add(
            Label(
                scope,
                U("质量视图"),
                10,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            16
        );

        duplicateCount_ = Label(
            scope,
            U("可能重复：0"),
            10,
            Theme::Yellow()
        );

        scopeLayout->Add(
            duplicateCount_,
            0,
            wxALL,
            16
        );

        autoNormalize_ = new wxCheckBox(
            scope,
            wxID_ANY,
            U("自动生成归一化候选")
        );
        autoNormalize_->SetForegroundColour(Theme::Text());

        scopeLayout->Add(
            autoNormalize_,
            0,
            wxALL,
            16
        );

        scopeLayout->AddStretchSpacer();
        scope->SetSizer(scopeLayout);
        scope->SetMinSize(wxSize(238, -1));

        auto* records = Card(this);
        auto* recordsLayout = new wxBoxSizer(wxVERTICAL);

        recordsLayout->Add(
            Label(
                records,
                U("实体"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        search_ = Input(records);
        search_->SetHint(U("搜索名称或别名"));

        recordsLayout->Add(
            search_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        list_ = new wxListCtrl(
            records,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->InsertColumn(0, U("标准名称"), wxLIST_FORMAT_LEFT, 190);
        list_->InsertColumn(1, U("类型"), wxLIST_FORMAT_LEFT, 110);
        list_->InsertColumn(2, U("别名"), wxLIST_FORMAT_LEFT, 210);
        list_->InsertColumn(3, U("状态"), wxLIST_FORMAT_LEFT, 90);

        recordsLayout->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        records->SetSizer(recordsLayout);
        records->SetMinSize(wxSize(592, -1));

        auto* detail = Card(this);
        auto* detailLayout = new wxBoxSizer(wxVERTICAL);

        detailLayout->Add(
            Label(
                detail,
                U("实体详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        name_ = Input(detail);
        aliases_ = Input(detail, wxTE_MULTILINE);

        AddField(
            detailLayout,
            detail,
            U("标准名称"),
            name_
        );

        AddField(
            detailLayout,
            detail,
            U("别名（每行一个）"),
            aliases_
        );

        aliases_->SetMinSize(wxSize(-1, 110));

        confirmed_ = new wxCheckBox(
            detail,
            wxID_ANY,
            U("已确认标准实体")
        );
        confirmed_->SetForegroundColour(Theme::Text());

        detailLayout->Add(
            confirmed_,
            0,
            wxALL,
            16
        );

        auto* mergeRow = new wxBoxSizer(wxHORIZONTAL);
        auto* merge = Button(detail, U("预览合并"));
        auto* split = Button(detail, U("拆分引用"));

        mergeRow->Add(merge, 1, wxRIGHT, 8);
        mergeRow->Add(split, 1);

        detailLayout->Add(
            mergeRow,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            16
        );

        auto* saveRow = new wxBoxSizer(wxHORIZONTAL);
        auto* remove = Button(detail, U("删除"), false, true);
        auto* save = Button(detail, U("保存修改"), true);

        saveRow->Add(remove, 1, wxRIGHT, 8);
        saveRow->Add(save, 1);

        detailLayout->AddStretchSpacer();
        detailLayout->Add(
            saveRow,
            0,
            wxEXPAND | wxALL,
            16
        );

        detail->SetSizer(detailLayout);
        detail->SetMinSize(wxSize(416, -1));

        body->Add(scope, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(records, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(detail, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        create->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            selectedId_.clear();
            name_->Clear();
            aliases_->Clear();
            confirmed_->SetValue(false);
            name_->SetFocus();
        });

        duplicates->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                DetectDuplicates();
            }
        );

        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Save();
        });

        remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Remove();
        });

        merge->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Merge();
        });

        split->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            SplitAliases();
        });

        type_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            RefreshData();
        });

        search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            RefreshData();
        });

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                Select(
                    static_cast<std::size_t>(
                        event.GetIndex()
                    )
                );
            }
        );

        RefreshData();
    }

private:
    struct Record
    {
        std::string id;
        std::string type;
        std::string name;
        std::string aliases;
        bool confirmed = false;
    };

    std::string TypeValue() const
    {
        const int selection = type_->GetSelection();

        if (selection == wxNOT_FOUND)
        {
            return {};
        }

        auto* data =
            dynamic_cast<wxStringClientData*>(
                type_->GetClientObject(selection)
            );

        return data == nullptr
            ? std::string()
            : Utf8(data->GetData());
    }

    std::string EntityTypeForSave() const
    {
        const std::string value = TypeValue();
        return value.empty() ? "person" : value;
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        Statement statement(
            handle,
            "SELECT id, entity_type, canonical_name, "
            "aliases, confirmed "
            "FROM entities "
            "WHERE deleted=0 "
            "AND (?='' OR entity_type=?) "
            "AND (?='' OR canonical_name LIKE '%'||?||'%' "
            "OR aliases LIKE '%'||?||'%') "
            "ORDER BY canonical_name COLLATE NOCASE;"
        );

        if (!statement.Valid())
        {
            return;
        }

        const std::string type = TypeValue();
        const std::string search = Utf8(search_->GetValue());

        BindText(statement.Get(), 1, type);
        BindText(statement.Get(), 2, type);
        BindText(statement.Get(), 3, search);
        BindText(statement.Get(), 4, search);
        BindText(statement.Get(), 5, search);

        while (sqlite3_step(statement.Get()) == SQLITE_ROW)
        {
            Record record;
            record.id = Text(statement.Get(), 0);
            record.type = Text(statement.Get(), 1);
            record.name = Text(statement.Get(), 2);
            record.aliases = Text(statement.Get(), 3);
            record.confirmed =
                sqlite3_column_int(statement.Get(), 4) != 0;

            records_.push_back(record);

            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.name)
            );

            list_->SetItem(row, 1, Wx(record.type));
            list_->SetItem(row, 2, Wx(record.aliases));
            list_->SetItem(
                row,
                3,
                record.confirmed
                    ? U("已确认")
                    : U("待确认")
            );
        }

        DetectDuplicates(false);
    }

    void Select(std::size_t index)
    {
        if (index >= records_.size())
        {
            return;
        }

        const auto& record = records_[index];
        selectedId_ = record.id;
        name_->SetValue(Wx(record.name));
        aliases_->SetValue(Wx(record.aliases));
        confirmed_->SetValue(record.confirmed);
    }

    void Save()
    {
        const std::string name = Utf8(name_->GetValue());

        if (name.empty())
        {
            wxMessageBox(
                U("标准名称不能为空。"),
                U("标签与实体管理"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        if (selectedId_.empty())
        {
            selectedId_ = NewId("ENT");
        }

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "INSERT INTO entities("
                "id, entity_type, canonical_name, aliases, "
                "confirmed, deleted, created_at, updated_at"
                ") VALUES(?, ?, ?, ?, ?, 0, "
                "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
                "ON CONFLICT(id) DO UPDATE SET "
                "entity_type=excluded.entity_type, "
                "canonical_name=excluded.canonical_name, "
                "aliases=excluded.aliases, "
                "confirmed=excluded.confirmed, "
                "deleted=0, "
                "updated_at=CURRENT_TIMESTAMP;"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备实体保存"
                );
            }

            BindText(statement.Get(), 1, selectedId_);
            BindText(statement.Get(), 2, EntityTypeForSave());
            BindText(statement.Get(), 3, name);
            BindText(statement.Get(), 4, Utf8(aliases_->GetValue()));
            sqlite3_bind_int(
                statement.Get(),
                5,
                confirmed_->GetValue() ? 1 : 0
            );

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法保存实体");
            }

            return workspace.Audit().Append(
                "ui",
                "entity",
                "save",
                "entity",
                selectedId_,
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("标签与实体管理"),
                status
            );
            return;
        }

        RefreshData();
    }

    void Remove()
    {
        if (selectedId_.empty())
        {
            return;
        }

        if (wxMessageBox(
                U("确认删除所选实体？"),
                U("标签与实体管理"),
                wxYES_NO | wxICON_WARNING,
                this
            ) != wxYES)
        {
            return;
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "UPDATE entities SET deleted=1, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=?;"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备实体删除"
                );
            }

            BindText(statement.Get(), 1, selectedId_);

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法删除实体");
            }

            return workspace.Audit().Append(
                "ui",
                "entity",
                "delete",
                "entity",
                selectedId_,
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("标签与实体管理"),
                status
            );
            return;
        }

        selectedId_.clear();
        name_->Clear();
        aliases_->Clear();
        RefreshData();
    }

    void DetectDuplicates(bool showResult = true)
    {
        int count = 0;

        for (std::size_t left = 0;
             left < records_.size();
             ++left)
        {
            for (std::size_t right = left + 1;
                 right < records_.size();
                 ++right)
            {
                const wxString a =
                    Wx(records_[left].name).Lower();
                const wxString b =
                    Wx(records_[right].name).Lower();

                const wxString aliasesA =
                    Wx(records_[left].aliases).Lower();
                const wxString aliasesB =
                    Wx(records_[right].aliases).Lower();

                if (a == b ||
                    aliasesA.Contains(b) ||
                    aliasesB.Contains(a))
                {
                    ++count;
                }
            }
        }

        duplicateCount_->SetLabel(
            U("可能重复：") +
            wxString::Format("%d", count)
        );

        if (showResult)
        {
            wxMessageBox(
                U("已完成重复检测，共发现 ") +
                    wxString::Format("%d", count) +
                    U(" 组候选。"),
                U("重复检测"),
                wxOK | wxICON_INFORMATION,
                this
            );
        }
    }

    void Merge()
    {
        if (selectedId_.empty())
        {
            return;
        }

        const wxString target = wxGetTextFromUser(
            U("输入要合并到的目标实体编号"),
            U("预览合并"),
            wxEmptyString,
            this
        );

        if (target.empty() ||
            Utf8(target) == selectedId_)
        {
            return;
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();
        const std::string targetId = Utf8(target);

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement source(
                handle,
                "SELECT canonical_name, aliases "
                "FROM entities "
                "WHERE id=? AND deleted=0 LIMIT 1;"
            );

            Statement targetRecord(
                handle,
                "SELECT aliases FROM entities "
                "WHERE id=? AND deleted=0 LIMIT 1;"
            );

            if (!source.Valid() || !targetRecord.Valid())
            {
                return Error(
                    handle,
                    sqlite3_errcode(handle),
                    "无法准备实体合并"
                );
            }

            BindText(source.Get(), 1, selectedId_);
            BindText(targetRecord.Get(), 1, targetId);

            if (sqlite3_step(source.Get()) != SQLITE_ROW ||
                sqlite3_step(targetRecord.Get()) != SQLITE_ROW)
            {
                return StorageStatus::Error(
                    SQLITE_NOTFOUND,
                    "源实体或目标实体不存在"
                );
            }

            std::string aliases = Text(targetRecord.Get(), 0);

            if (!aliases.empty())
            {
                aliases += "\n";
            }

            aliases += Text(source.Get(), 0);

            const std::string sourceAliases =
                Text(source.Get(), 1);

            if (!sourceAliases.empty())
            {
                aliases += "\n";
                aliases += sourceAliases;
            }

            Statement updateTarget(
                handle,
                "UPDATE entities SET aliases=?, "
                "confirmed=1, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=?;"
            );

            Statement removeSource(
                handle,
                "UPDATE entities SET deleted=1, "
                "updated_at=CURRENT_TIMESTAMP "
                "WHERE id=?;"
            );

            if (!updateTarget.Valid() ||
                !removeSource.Valid())
            {
                return Error(
                    handle,
                    sqlite3_errcode(handle),
                    "无法准备实体合并写入"
                );
            }

            BindText(updateTarget.Get(), 1, aliases);
            BindText(updateTarget.Get(), 2, targetId);
            BindText(removeSource.Get(), 1, selectedId_);

            if (sqlite3_step(updateTarget.Get()) != SQLITE_DONE ||
                sqlite3_step(removeSource.Get()) != SQLITE_DONE)
            {
                return Error(
                    handle,
                    sqlite3_errcode(handle),
                    "无法合并实体"
                );
            }

            return workspace.Audit().Append(
                "ui",
                "entity",
                "merge",
                "entity",
                targetId,
                "{\"source\":\"" + selectedId_ + "\"}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(this, U("预览合并"), status);
            return;
        }

        selectedId_.clear();
        RefreshData();
    }

    void SplitAliases()
    {
        if (selectedId_.empty())
        {
            return;
        }

        const wxString alias = wxGetTextFromUser(
            U("输入要拆分为新实体的别名"),
            U("拆分引用"),
            wxEmptyString,
            this
        );

        if (alias.empty())
        {
            return;
        }

        const std::string newId = NewId("ENT");
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "INSERT INTO entities("
                "id, entity_type, canonical_name, aliases, "
                "confirmed, deleted"
                ") VALUES(?, ?, ?, '', 0, 0);"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备实体拆分"
                );
            }

            BindText(statement.Get(), 1, newId);
            BindText(statement.Get(), 2, EntityTypeForSave());
            BindText(statement.Get(), 3, Utf8(alias));

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法拆分实体");
            }

            return workspace.Audit().Append(
                "ui",
                "entity",
                "split",
                "entity",
                newId,
                "{\"source\":\"" + selectedId_ + "\"}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(this, U("拆分引用"), status);
            return;
        }

        RefreshData();
    }

    wxChoice* type_ = nullptr;
    wxTextCtrl* search_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxStaticText* duplicateCount_ = nullptr;
    wxCheckBox* autoNormalize_ = nullptr;
    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* aliases_ = nullptr;
    wxCheckBox* confirmed_ = nullptr;
    std::string selectedId_;
    std::vector<Record> records_;
};

class SavedQueryPage final : public wxPanel
{
public:
    explicit SavedQueryPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* importDefinition =
            Button(this, U("导入定义"));
        auto* create =
            Button(this, U("新建查询"), true);

        actions->Add(importDefinition, 0, wxRIGHT, 10);
        actions->Add(create, 0);

        AddHeading(
            this,
            root,
            U("M06"),
            U("保存的查询"),
            U("管理可复用的调查条件、动态视图和概览快捷入口"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* groups = Card(this);
        auto* groupsLayout = new wxBoxSizer(wxVERTICAL);

        groupsLayout->Add(
            Label(
                groups,
                U("查询分组"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        group_ = new wxChoice(groups, wxID_ANY);
        group_->Append(U("全部查询"));
        group_->Append(U("日常审查"));
        group_->Append(U("项目健康"));
        group_->Append(U("交付审计"));
        group_->Append(U("个人视图"));
        group_->SetSelection(0);

        groupsLayout->Add(
            group_,
            0,
            wxEXPAND | wxALL,
            16
        );

        groupsLayout->Add(
            Label(
                groups,
                U("保存的是查询定义，不复制结果。\n"
                  "结果随工作区数据实时变化。"),
                9,
                Theme::Muted()
            ),
            0,
            wxALL,
            16
        );

        groupsLayout->AddStretchSpacer();
        groups->SetSizer(groupsLayout);
        groups->SetMinSize(wxSize(270, -1));

        auto* records = Card(this);
        auto* recordsLayout = new wxBoxSizer(wxVERTICAL);

        recordsLayout->Add(
            Label(
                records,
                U("已保存查询"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        search_ = Input(records);
        search_->SetHint(U("搜索查询名称或语法"));

        recordsLayout->Add(
            search_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        list_ = new wxListCtrl(
            records,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->InsertColumn(0, U("名称"), wxLIST_FORMAT_LEFT, 230);
        list_->InsertColumn(1, U("查询条件"), wxLIST_FORMAT_LEFT, 260);
        list_->InsertColumn(2, U("固定"), wxLIST_FORMAT_LEFT, 75);
        list_->InsertColumn(3, U("结果"), wxLIST_FORMAT_RIGHT, 65);

        recordsLayout->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        records->SetSizer(recordsLayout);
        records->SetMinSize(wxSize(605, -1));

        auto* detail = Card(this);
        auto* detailLayout = new wxBoxSizer(wxVERTICAL);

        detailLayout->Add(
            Label(
                detail,
                U("查询详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        name_ = Input(detail);
        expression_ = Input(detail, wxTE_MULTILINE);
        description_ = Input(detail, wxTE_MULTILINE);

        AddField(detailLayout, detail, U("名称"), name_);
        AddField(
            detailLayout,
            detail,
            U("查询条件"),
            expression_
        );
        AddField(
            detailLayout,
            detail,
            U("说明"),
            description_
        );

        expression_->SetMinSize(wxSize(-1, 95));
        description_->SetMinSize(wxSize(-1, 70));

        pinned_ = new wxCheckBox(
            detail,
            wxID_ANY,
            U("固定到项目概览")
        );
        pinned_->SetForegroundColour(Theme::Text());

        autoRefresh_ = new wxCheckBox(
            detail,
            wxID_ANY,
            U("启动时自动刷新")
        );
        autoRefresh_->SetForegroundColour(Theme::Text());

        detailLayout->Add(pinned_, 0, wxALL, 16);
        detailLayout->Add(
            autoRefresh_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        result_ = Label(
            detail,
            U("当前结果：0 项"),
            11,
            Theme::Yellow(),
            wxFONTWEIGHT_BOLD
        );

        detailLayout->Add(
            result_,
            0,
            wxALL,
            16
        );

        auto* firstRow = new wxBoxSizer(wxHORIZONTAL);
        auto* copy = Button(detail, U("复制为新查询"));
        auto* run = Button(detail, U("立即运行"), true);

        firstRow->Add(copy, 1, wxRIGHT, 8);
        firstRow->Add(run, 1);

        detailLayout->Add(
            firstRow,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            16
        );

        auto* secondRow = new wxBoxSizer(wxHORIZONTAL);
        auto* exportDefinition =
            Button(detail, U("导出定义"));
        auto* remove =
            Button(detail, U("删除"), false, true);

        secondRow->Add(exportDefinition, 1, wxRIGHT, 8);
        secondRow->Add(remove, 1);

        detailLayout->Add(
            secondRow,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            16
        );

        auto* save = Button(detail, U("保存修改"), true);

        detailLayout->Add(
            save,
            0,
            wxEXPAND | wxALL,
            16
        );

        detail->SetSizer(detailLayout);
        detail->SetMinSize(wxSize(371, -1));

        body->Add(groups, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(records, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(detail, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        create->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            selectedId_.clear();
            name_->Clear();
            expression_->Clear();
            description_->Clear();
            pinned_->SetValue(false);
            autoRefresh_->SetValue(true);
            result_->SetLabel(U("当前结果：0 项"));
            name_->SetFocus();
        });

        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Save();
        });

        run->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Run();
        });

        copy->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            selectedId_.clear();
            name_->SetValue(
                name_->GetValue() + U("（副本）")
            );
            Save();
        });

        remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Remove();
        });

        exportDefinition->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ExportDefinition();
            }
        );

        importDefinition->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ImportDefinition();
            }
        );

        search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            RefreshData();
        });

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                Select(
                    static_cast<std::size_t>(
                        event.GetIndex()
                    )
                );
            }
        );

        RefreshData();
    }

private:
    struct Record
    {
        std::string id;
        std::string name;
        std::string description;
        std::string expression;
        bool pinned = false;
        std::string schedule;
        std::int64_t count = 0;
    };

    std::int64_t ExecuteCount(
        const std::string& expression
    ) const
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return 0;
        }

        std::string sql =
            "SELECT count(*) FROM objects o "
            "WHERE o.deleted=0";

        std::vector<std::string> parameters;
        std::istringstream tokens(expression);
        std::string token;

        while (tokens >> token)
        {
            const auto separator = token.find(':');

            if (separator == std::string::npos)
            {
                continue;
            }

            const std::string key =
                token.substr(0, separator);
            const std::string value =
                token.substr(separator + 1);

            if (key == "type")
            {
                sql += " AND o.object_type=?";
                parameters.push_back(value);
            }
            else if (key == "status")
            {
                sql += " AND o.status=?";
                parameters.push_back(value);
            }
            else if (key == "priority")
            {
                sql += " AND o.priority=?";
                parameters.push_back(value);
            }
            else if (key == "owner")
            {
                sql += " AND o.owner LIKE '%'||?||'%'";
                parameters.push_back(value);
            }
            else if (key == "text")
            {
                sql +=
                    " AND (o.title LIKE '%'||?||'%' "
                    "OR o.description LIKE '%'||?||'%')";
                parameters.push_back(value);
                parameters.push_back(value);
            }
            else if (
                key == "evidence.support" &&
                value == "0"
            )
            {
                sql +=
                    " AND NOT EXISTS("
                    "SELECT 1 FROM object_evidence oe "
                    "WHERE oe.object_id=o.id "
                    "AND oe.role='support')";
            }
        }

        sql += ";";

        Statement statement(handle, sql);

        if (!statement.Valid())
        {
            return 0;
        }

        for (std::size_t index = 0;
             index < parameters.size();
             ++index)
        {
            BindText(
                statement.Get(),
                static_cast<int>(index + 1),
                parameters[index]
            );
        }

        if (sqlite3_step(statement.Get()) != SQLITE_ROW)
        {
            return 0;
        }

        return sqlite3_column_int64(statement.Get(), 0);
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        Statement statement(
            handle,
            "SELECT id, name, description, query_json, "
            "pinned, schedule "
            "FROM saved_queries "
            "WHERE ?='' OR name LIKE '%'||?||'%' "
            "OR query_json LIKE '%'||?||'%' "
            "ORDER BY pinned DESC, name COLLATE NOCASE;"
        );

        if (!statement.Valid())
        {
            return;
        }

        const std::string search =
            Utf8(search_->GetValue());

        BindText(statement.Get(), 1, search);
        BindText(statement.Get(), 2, search);
        BindText(statement.Get(), 3, search);

        while (sqlite3_step(statement.Get()) == SQLITE_ROW)
        {
            Record record;
            record.id = Text(statement.Get(), 0);
            record.name = Text(statement.Get(), 1);
            record.description = Text(statement.Get(), 2);
            record.expression = Text(statement.Get(), 3);
            record.pinned =
                sqlite3_column_int(statement.Get(), 4) != 0;
            record.schedule = Text(statement.Get(), 5);
            record.count = ExecuteCount(record.expression);

            records_.push_back(record);

            const long row = list_->InsertItem(
                list_->GetItemCount(),
                Wx(record.name)
            );

            list_->SetItem(row, 1, Wx(record.expression));
            list_->SetItem(
                row,
                2,
                record.pinned ? U("概览") : U("未固定")
            );
            list_->SetItem(
                row,
                3,
                wxString::Format(
                    "%lld",
                    static_cast<long long>(record.count)
                )
            );
        }
    }

    void Select(std::size_t index)
    {
        if (index >= records_.size())
        {
            return;
        }

        const auto& record = records_[index];
        selectedId_ = record.id;
        name_->SetValue(Wx(record.name));
        expression_->SetValue(Wx(record.expression));
        description_->SetValue(Wx(record.description));
        pinned_->SetValue(record.pinned);
        autoRefresh_->SetValue(
            record.schedule == "startup"
        );

        result_->SetLabel(
            U("当前结果：") +
            wxString::Format(
                "%lld",
                static_cast<long long>(record.count)
            ) +
            U(" 项")
        );
    }

    void Save()
    {
        const std::string name = Utf8(name_->GetValue());
        const std::string expression =
            Utf8(expression_->GetValue());

        if (name.empty() || expression.empty())
        {
            wxMessageBox(
                U("查询名称和查询条件不能为空。"),
                U("保存的查询"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        if (selectedId_.empty())
        {
            selectedId_ = NewId("QUERY");
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "INSERT INTO saved_queries("
                "id, name, description, query_json, "
                "pinned, schedule, created_at, updated_at"
                ") VALUES(?, ?, ?, ?, ?, ?, "
                "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
                "ON CONFLICT(id) DO UPDATE SET "
                "name=excluded.name, "
                "description=excluded.description, "
                "query_json=excluded.query_json, "
                "pinned=excluded.pinned, "
                "schedule=excluded.schedule, "
                "updated_at=CURRENT_TIMESTAMP;"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备查询保存"
                );
            }

            BindText(statement.Get(), 1, selectedId_);
            BindText(statement.Get(), 2, name);
            BindText(
                statement.Get(),
                3,
                Utf8(description_->GetValue())
            );
            BindText(statement.Get(), 4, expression);
            sqlite3_bind_int(
                statement.Get(),
                5,
                pinned_->GetValue() ? 1 : 0
            );
            BindText(
                statement.Get(),
                6,
                autoRefresh_->GetValue()
                    ? "startup"
                    : std::string()
            );

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法保存查询");
            }

            return workspace.Audit().Append(
                "ui",
                "saved_query",
                "save",
                "saved_query",
                selectedId_,
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(this, U("保存的查询"), status);
            return;
        }

        Run();
        RefreshData();
    }

    void Run()
    {
        const std::int64_t count =
            ExecuteCount(Utf8(expression_->GetValue()));

        result_->SetLabel(
            U("当前结果：") +
            wxString::Format(
                "%lld",
                static_cast<long long>(count)
            ) +
            U(" 项")
        );

        WorkspaceService::Instance().Audit().Append(
            "ui",
            "saved_query",
            "run",
            "saved_query",
            selectedId_,
            "{\"count\":" +
                std::to_string(count) +
                "}"
        );
    }

    void Remove()
    {
        if (selectedId_.empty())
        {
            return;
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "DELETE FROM saved_queries WHERE id=?;"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备查询删除"
                );
            }

            BindText(statement.Get(), 1, selectedId_);

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法删除查询");
            }

            return workspace.Audit().Append(
                "ui",
                "saved_query",
                "delete",
                "saved_query",
                selectedId_,
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(this, U("保存的查询"), status);
            return;
        }

        selectedId_.clear();
        name_->Clear();
        expression_->Clear();
        description_->Clear();
        RefreshData();
    }

    void ExportDefinition()
    {
        if (name_->GetValue().empty())
        {
            return;
        }

        const auto directory =
            std::filesystem::u8path(
                WorkspaceService::Instance().
                    WorkspaceDirectory()
            ) / "exports";

        std::filesystem::create_directories(directory);

        const auto path =
            directory /
            (
                selectedId_.empty()
                    ? "query-definition.txt"
                    : selectedId_ + ".query"
            );

        std::ofstream output(
            path,
            std::ios::binary | std::ios::trunc
        );

        output
            << Utf8(name_->GetValue()) << "\n"
            << Utf8(description_->GetValue()) << "\n"
            << Utf8(expression_->GetValue()) << "\n"
            << (pinned_->GetValue() ? "1" : "0") << "\n"
            << (autoRefresh_->GetValue() ? "1" : "0") << "\n";

        output.close();

        wxMessageBox(
            U("查询定义已导出到：\n") +
                Wx(path.u8string()),
            U("导出定义"),
            wxOK | wxICON_INFORMATION,
            this
        );
    }

    void ImportDefinition()
    {
        const wxString path = wxGetTextFromUser(
            U("输入查询定义文件路径"),
            U("导入定义"),
            wxEmptyString,
            this
        );

        if (path.empty())
        {
            return;
        }

        std::ifstream input(
            std::filesystem::u8path(Utf8(path)),
            std::ios::binary
        );

        if (!input)
        {
            wxMessageBox(
                U("无法读取查询定义文件。"),
                U("导入定义"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        std::string name;
        std::string description;
        std::string expression;
        std::string pinned;
        std::string automatic;

        std::getline(input, name);
        std::getline(input, description);
        std::getline(input, expression);
        std::getline(input, pinned);
        std::getline(input, automatic);

        selectedId_.clear();
        name_->SetValue(Wx(name));
        description_->SetValue(Wx(description));
        expression_->SetValue(Wx(expression));
        pinned_->SetValue(pinned == "1");
        autoRefresh_->SetValue(automatic == "1");
        Save();
    }

    wxChoice* group_ = nullptr;
    wxTextCtrl* search_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* expression_ = nullptr;
    wxTextCtrl* description_ = nullptr;
    wxCheckBox* pinned_ = nullptr;
    wxCheckBox* autoRefresh_ = nullptr;
    wxStaticText* result_ = nullptr;
    std::string selectedId_;
    std::vector<Record> records_;
};

class RulesPage final : public wxPanel
{
public:
    explicit RulesPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* restore = Button(this, U("恢复默认规则"));
        auto* create = Button(this, U("新建规则"), true);

        actions->Add(restore, 0, wxRIGHT, 10);
        actions->Add(create, 0);

        AddHeading(
            this,
            root,
            U("M07"),
            U("规则管理"),
            U("配置自动提取、冲突检测、实体识别和文件过滤规则"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* categories = Card(this);
        auto* categoryLayout = new wxBoxSizer(wxVERTICAL);

        categoryLayout->Add(
            Label(
                categories,
                U("规则分类"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        category_ = new wxChoice(categories, wxID_ANY);
        category_->Append(U("全部规则"), new wxStringClientData(""));
        category_->Append(U("自动提取"), new wxStringClientData("extract"));
        category_->Append(U("冲突检测"), new wxStringClientData("conflict"));
        category_->Append(U("实体识别"), new wxStringClientData("entity"));
        category_->Append(U("日期与数值"), new wxStringClientData("value"));
        category_->Append(U("文件过滤"), new wxStringClientData("file"));
        category_->SetSelection(0);

        categoryLayout->Add(
            category_,
            0,
            wxEXPAND | wxALL,
            16
        );

        categoryLayout->Add(
            Label(
                categories,
                U("安全限制"),
                10,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            16
        );

        categoryLayout->Add(
            Label(
                categories,
                U("规则仅接受参数化条件，\n"
                  "不执行脚本、宏或任意代码。"),
                9,
                Theme::Muted()
            ),
            0,
            wxALL,
            16
        );

        categoryLayout->AddStretchSpacer();
        categories->SetSizer(categoryLayout);
        categories->SetMinSize(wxSize(260, -1));

        auto* records = Card(this);
        auto* recordsLayout = new wxBoxSizer(wxVERTICAL);

        recordsLayout->Add(
            Label(
                records,
                U("规则"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        list_ = new wxListCtrl(
            records,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Input());
        list_->SetForegroundColour(Theme::Text());
        list_->InsertColumn(0, U("状态"), wxLIST_FORMAT_LEFT, 75);
        list_->InsertColumn(1, U("名称"), wxLIST_FORMAT_LEFT, 235);
        list_->InsertColumn(2, U("类型"), wxLIST_FORMAT_LEFT, 105);
        list_->InsertColumn(3, U("严重度"), wxLIST_FORMAT_LEFT, 80);
        list_->InsertColumn(4, U("命中"), wxLIST_FORMAT_RIGHT, 65);

        recordsLayout->Add(
            list_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        records->SetSizer(recordsLayout);
        records->SetMinSize(wxSize(615, -1));

        auto* detail = Card(this);
        auto* detailLayout = new wxBoxSizer(wxVERTICAL);

        detailLayout->Add(
            Label(
                detail,
                U("规则配置"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        name_ = Input(detail);
        expression_ = Input(detail, wxTE_MULTILINE);

        type_ = new wxChoice(detail, wxID_ANY);
        type_->Append(U("自动提取"), new wxStringClientData("extract"));
        type_->Append(U("冲突检测"), new wxStringClientData("conflict"));
        type_->Append(U("实体识别"), new wxStringClientData("entity"));
        type_->Append(U("日期与数值"), new wxStringClientData("value"));
        type_->Append(U("文件过滤"), new wxStringClientData("file"));
        type_->SetSelection(1);

        severity_ = new wxChoice(detail, wxID_ANY);
        severity_->Append(U("高"), new wxStringClientData("high"));
        severity_->Append(U("中"), new wxStringClientData("medium"));
        severity_->Append(U("低"), new wxStringClientData("low"));
        severity_->SetSelection(1);

        AddField(detailLayout, detail, U("名称"), name_);
        AddField(detailLayout, detail, U("规则类型"), type_);
        AddField(detailLayout, detail, U("严重程度"), severity_);
        AddField(
            detailLayout,
            detail,
            U("参数化触发条件"),
            expression_
        );

        expression_->SetMinSize(wxSize(-1, 130));

        enabled_ = new wxCheckBox(
            detail,
            wxID_ANY,
            U("启用规则")
        );
        enabled_->SetForegroundColour(Theme::Text());

        detailLayout->Add(
            enabled_,
            0,
            wxALL,
            16
        );

        testResult_ = Label(
            detail,
            U("测试命中：0 项"),
            10,
            Theme::Yellow(),
            wxFONTWEIGHT_BOLD
        );

        detailLayout->Add(
            testResult_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        auto* test = Button(detail, U("测试规则"));
        auto* save = Button(detail, U("保存修改"), true);

        buttons->Add(test, 1, wxRIGHT, 8);
        buttons->Add(save, 1);

        detailLayout->AddStretchSpacer();
        detailLayout->Add(
            buttons,
            0,
            wxEXPAND | wxALL,
            16
        );

        detail->SetSizer(detailLayout);
        detail->SetMinSize(wxSize(371, -1));

        body->Add(categories, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(records, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(detail, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        create->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            selectedId_.clear();
            name_->Clear();
            expression_->Clear();
            enabled_->SetValue(true);
            testResult_->SetLabel(U("测试命中：0 项"));
        });

        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Save();
        });

        test->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Test();
        });

        restore->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            RestoreDefaults();
        });

        category_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) {
            RefreshData();
        });

        list_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                Select(
                    static_cast<std::size_t>(
                        event.GetIndex()
                    )
                );
            }
        );

        RefreshData();
    }

private:
    struct Record
    {
        std::string id;
        std::string name;
        std::string type;
        std::string expression;
        std::string severity;
        bool enabled = false;
        int hits = 0;
    };

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

    int TestExpression(const std::string& expression) const
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return 0;
        }

        std::string sql =
            "SELECT count(*) FROM objects o WHERE o.deleted=0";

        std::vector<std::string> parameters;
        std::istringstream stream(expression);
        std::string token;

        while (stream >> token)
        {
            const auto separator = token.find(':');

            if (separator == std::string::npos)
            {
                continue;
            }

            const std::string key =
                token.substr(0, separator);
            const std::string value =
                token.substr(separator + 1);

            if (key == "type")
            {
                sql += " AND o.object_type=?";
                parameters.push_back(value);
            }
            else if (key == "status")
            {
                sql += " AND o.status=?";
                parameters.push_back(value);
            }
            else if (key == "priority")
            {
                sql += " AND o.priority=?";
                parameters.push_back(value);
            }
            else if (key == "owner")
            {
                if (value == "empty")
                {
                    sql += " AND trim(o.owner)=''";
                }
                else
                {
                    sql += " AND o.owner LIKE '%'||?||'%'";
                    parameters.push_back(value);
                }
            }
            else if (key == "validity" &&
                     value == "invalid")
            {
                sql +=
                    " AND o.valid_from IS NOT NULL "
                    "AND o.valid_to IS NOT NULL "
                    "AND o.valid_from>o.valid_to";
            }
            else if (key == "evidence" &&
                     value == "missing")
            {
                sql +=
                    " AND NOT EXISTS("
                    "SELECT 1 FROM object_evidence oe "
                    "JOIN evidence e ON e.id=oe.evidence_id "
                    "WHERE oe.object_id=o.id "
                    "AND e.deleted=0)";
            }
        }

        sql += ";";

        Statement statement(handle, sql);

        if (!statement.Valid())
        {
            return 0;
        }

        for (std::size_t index = 0;
             index < parameters.size();
             ++index)
        {
            BindText(
                statement.Get(),
                static_cast<int>(index + 1),
                parameters[index]
            );
        }

        if (sqlite3_step(statement.Get()) != SQLITE_ROW)
        {
            return 0;
        }

        return sqlite3_column_int(statement.Get(), 0);
    }

    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return;
        }

        Statement statement(
            handle,
            "SELECT id, name, rule_type, expression, "
            "severity, enabled "
            "FROM rules "
            "WHERE (?='' OR rule_type=?) "
            "ORDER BY enabled DESC, name COLLATE NOCASE;"
        );

        if (!statement.Valid())
        {
            return;
        }

        const std::string category =
            ChoiceValue(category_);

        BindText(statement.Get(), 1, category);
        BindText(statement.Get(), 2, category);

        while (sqlite3_step(statement.Get()) == SQLITE_ROW)
        {
            Record record;
            record.id = Text(statement.Get(), 0);
            record.name = Text(statement.Get(), 1);
            record.type = Text(statement.Get(), 2);
            record.expression = Text(statement.Get(), 3);
            record.severity = Text(statement.Get(), 4);
            record.enabled =
                sqlite3_column_int(statement.Get(), 5) != 0;
            record.hits = TestExpression(record.expression);

            records_.push_back(record);

            const long row = list_->InsertItem(
                list_->GetItemCount(),
                record.enabled ? U("已启用") : U("已禁用")
            );

            list_->SetItem(row, 1, Wx(record.name));
            list_->SetItem(row, 2, Wx(record.type));
            list_->SetItem(row, 3, Wx(record.severity));
            list_->SetItem(
                row,
                4,
                wxString::Format("%d", record.hits)
            );
        }
    }

    void SelectChoiceByValue(
        wxChoice* choice,
        const std::string& value
    )
    {
        for (unsigned int index = 0;
             index < choice->GetCount();
             ++index)
        {
            auto* data = dynamic_cast<wxStringClientData*>(
                choice->GetClientObject(index)
            );

            if (data != nullptr &&
                Utf8(data->GetData()) == value)
            {
                choice->SetSelection(index);
                return;
            }
        }
    }

    void Select(std::size_t index)
    {
        if (index >= records_.size())
        {
            return;
        }

        const auto& record = records_[index];

        selectedId_ = record.id;
        name_->SetValue(Wx(record.name));
        expression_->SetValue(Wx(record.expression));
        enabled_->SetValue(record.enabled);

        SelectChoiceByValue(type_, record.type);
        SelectChoiceByValue(severity_, record.severity);

        testResult_->SetLabel(
            U("测试命中：") +
            wxString::Format("%d", record.hits) +
            U(" 项")
        );
    }

    void Save()
    {
        const std::string name = Utf8(name_->GetValue());
        const std::string expression =
            Utf8(expression_->GetValue());

        if (name.empty() || expression.empty())
        {
            wxMessageBox(
                U("规则名称和参数化触发条件不能为空。"),
                U("规则管理"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        const std::vector<std::string> forbidden = {
            ";",
            "--",
            "/*",
            "*/",
            "select ",
            "insert ",
            "update ",
            "delete ",
            "drop ",
            "pragma ",
            "attach ",
            "script",
            "javascript",
            "powershell",
            "cmd.exe"
        };

        std::string lower = expression;

        std::transform(
            lower.begin(),
            lower.end(),
            lower.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(
                    std::tolower(character)
                );
            }
        );

        for (const auto& value : forbidden)
        {
            if (lower.find(value) != std::string::npos)
            {
                wxMessageBox(
                    U("规则只能包含参数化条件，不能包含 SQL、"
                      "脚本、命令或任意代码。"),
                    U("规则管理"),
                    wxOK | wxICON_ERROR,
                    this
                );
                return;
            }
        }

        if (selectedId_.empty())
        {
            selectedId_ = NewId("RULE");
        }

        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            Statement statement(
                handle,
                "INSERT INTO rules("
                "id, name, rule_type, expression, "
                "severity, enabled, created_at, updated_at"
                ") VALUES(?, ?, ?, ?, ?, ?, "
                "CURRENT_TIMESTAMP, CURRENT_TIMESTAMP) "
                "ON CONFLICT(id) DO UPDATE SET "
                "name=excluded.name, "
                "rule_type=excluded.rule_type, "
                "expression=excluded.expression, "
                "severity=excluded.severity, "
                "enabled=excluded.enabled, "
                "updated_at=CURRENT_TIMESTAMP;"
            );

            if (!statement.Valid())
            {
                return Error(
                    handle,
                    statement.Status(),
                    "无法准备规则保存"
                );
            }

            BindText(statement.Get(), 1, selectedId_);
            BindText(statement.Get(), 2, name);
            BindText(statement.Get(), 3, ChoiceValue(type_));
            BindText(statement.Get(), 4, expression);
            BindText(statement.Get(), 5, ChoiceValue(severity_));
            sqlite3_bind_int(
                statement.Get(),
                6,
                enabled_->GetValue() ? 1 : 0
            );

            const int result =
                sqlite3_step(statement.Get());

            if (result != SQLITE_DONE)
            {
                return Error(handle, result, "无法保存规则");
            }

            return workspace.Audit().Append(
                "ui",
                "rule",
                "save",
                "rule",
                selectedId_,
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(this, U("规则管理"), status);
            return;
        }

        Test();
        RefreshData();
    }

    void Test()
    {
        const int hits =
            TestExpression(Utf8(expression_->GetValue()));

        testResult_->SetLabel(
            U("测试命中：") +
            wxString::Format("%d", hits) +
            U(" 项")
        );

        WorkspaceService::Instance().Audit().Append(
            "ui",
            "rule",
            "test",
            "rule",
            selectedId_,
            "{\"hits\":" + std::to_string(hits) + "}"
        );
    }

    void RestoreDefaults()
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        struct DefaultRule
        {
            const char* id;
            const char* name;
            const char* expression;
            const char* severity;
        };

        const DefaultRule defaults[] = {
            {
                "RULE-CONFLICT-VALUE",
                "同主体属性存在不同值",
                "type:fact status:active",
                "high"
            },
            {
                "RULE-CONFLICT-DATE",
                "同一承诺存在多个截止日期",
                "type:commitment status:open",
                "high"
            },
            {
                "RULE-COMPLETION-EVIDENCE",
                "完成承诺缺少完成证据",
                "type:commitment status:completed evidence:missing",
                "medium"
            },
            {
                "RULE-OWNER-MISSING",
                "负责人为空且存在截止日期",
                "type:commitment owner:empty",
                "low"
            },
            {
                "RULE-INVALID-VALIDITY",
                "对象有效时间范围无效",
                "validity:invalid",
                "high"
            }
        };

        const auto status = database.Transaction([&]() {
            sqlite3* handle = database.Handle();

            for (const auto& rule : defaults)
            {
                Statement statement(
                    handle,
                    "INSERT INTO rules("
                    "id, name, rule_type, expression, "
                    "severity, enabled"
                    ") VALUES(?, ?, 'conflict', ?, ?, 1) "
                    "ON CONFLICT(id) DO UPDATE SET "
                    "name=excluded.name, "
                    "rule_type=excluded.rule_type, "
                    "expression=excluded.expression, "
                    "severity=excluded.severity, "
                    "enabled=1, "
                    "updated_at=CURRENT_TIMESTAMP;"
                );

                if (!statement.Valid())
                {
                    return Error(
                        handle,
                        statement.Status(),
                        "无法准备默认规则恢复"
                    );
                }

                BindText(statement.Get(), 1, rule.id);
                BindText(statement.Get(), 2, rule.name);
                BindText(statement.Get(), 3, rule.expression);
                BindText(statement.Get(), 4, rule.severity);

                const int result =
                    sqlite3_step(statement.Get());

                if (result != SQLITE_DONE)
                {
                    return Error(
                        handle,
                        result,
                        "无法恢复默认规则"
                    );
                }
            }

            return workspace.Audit().Append(
                "ui",
                "rule",
                "restore_defaults",
                "workspace",
                std::string(),
                "{}"
            );
        });

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("恢复默认规则"),
                status
            );
            return;
        }

        RefreshData();
    }

    wxChoice* category_ = nullptr;
    wxListCtrl* list_ = nullptr;
    wxTextCtrl* name_ = nullptr;
    wxChoice* type_ = nullptr;
    wxChoice* severity_ = nullptr;
    wxTextCtrl* expression_ = nullptr;
    wxCheckBox* enabled_ = nullptr;
    wxStaticText* testResult_ = nullptr;
    std::string selectedId_;
    std::vector<Record> records_;
};

class WorkspaceSettingsPage final : public wxPanel
{
public:
    explicit WorkspaceSettingsPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* discard = Button(this, U("放弃修改"));
        auto* save = Button(this, U("保存设置"), true);

        actions->Add(discard, 0, wxRIGHT, 10);
        actions->Add(save, 0);

        AddHeading(
            this,
            root,
            U("M08"),
            U("工作区设置"),
            U("配置工作区的安全、监视、索引、备份与数据保留策略"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* categories = Card(this);
        auto* categoriesLayout =
            new wxBoxSizer(wxVERTICAL);

        categoriesLayout->Add(
            Label(
                categories,
                U("设置分类"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        category_ = new wxChoice(categories, wxID_ANY);

        const wxString categoryNames[] = {
            U("基本信息"),
            U("安全与自动锁定"),
            U("文件监视"),
            U("OCR 与内容提取"),
            U("搜索与索引"),
            U("自动备份"),
            U("冲突检测"),
            U("数据保留"),
            U("默认导出"),
            U("磁盘空间")
        };

        for (const auto& name : categoryNames)
        {
            category_->Append(name);
        }

        category_->SetSelection(1);

        categoriesLayout->Add(
            category_,
            0,
            wxEXPAND | wxALL,
            16
        );

        categoriesLayout->AddStretchSpacer();
        categories->SetSizer(categoriesLayout);
        categories->SetMinSize(wxSize(260, -1));

        content_ = Card(this);
        contentLayout_ = new wxBoxSizer(wxVERTICAL);

        title_ = Label(
            content_,
            U("安全与自动锁定"),
            17,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        );

        contentLayout_->Add(
            title_,
            0,
            wxALL,
            22
        );

        securityStatus_ = Label(
            content_,
            wxEmptyString,
            10,
            Theme::Green(),
            wxFONTWEIGHT_BOLD
        );

        contentLayout_->Add(
            securityStatus_,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            22
        );

        rememberCredential_ = AddToggle(
            U("允许使用系统凭据解锁")
        );

        rememberSession_ = AddToggle(
            U("启动后记住解锁状态")
        );

        automaticLock_ = AddToggle(
            U("无操作后自动锁定")
        );

        delayForJobs_ = AddToggle(
            U("后台任务运行时延迟自动锁定")
        );

        cleanTemporary_ = AddToggle(
            U("退出时清理临时预览文件")
        );

        cleanClipboard_ = AddToggle(
            U("锁定时清理剪贴板中的证据文本")
        );

        autoScan_ = AddToggle(
            U("源文件变化后自动执行扫描")
        );

        autoBackup_ = AddToggle(
            U("自动创建工作区备份")
        );

        conflictDetection_ = AddToggle(
            U("扫描完成后运行冲突检测")
        );

        contentLayout_->Add(
            Label(
                content_,
                U("锁定等待时间"),
                9,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        lockMinutes_ = new wxSpinCtrl(
            content_,
            wxID_ANY,
            "15",
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            1,
            240,
            15
        );

        contentLayout_->Add(
            lockMinutes_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            22
        );

        contentLayout_->Add(
            Label(
                content_,
                U("数据保留天数"),
                9,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        retentionDays_ = new wxSpinCtrl(
            content_,
            wxID_ANY,
            "365",
            wxDefaultPosition,
            wxDefaultSize,
            wxSP_ARROW_KEYS,
            1,
            3650,
            365
        );

        contentLayout_->Add(
            retentionDays_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            22
        );

        exportFormat_ = new wxChoice(content_, wxID_ANY);
        exportFormat_->Append(U("离线 HTML"));
        exportFormat_->Append(U("Markdown"));
        exportFormat_->Append(U("纯文本"));
        exportFormat_->SetSelection(0);

        contentLayout_->Add(
            Label(
                content_,
                U("默认导出格式"),
                9,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        contentLayout_->Add(
            exportFormat_,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            22
        );

        auto* directoryRow = new wxBoxSizer(wxHORIZONTAL);

        temporaryPath_ = Label(
            content_,
            wxEmptyString,
            9,
            Theme::Text()
        );

        auto* openDirectory =
            Button(content_, U("打开目录"));

        directoryRow->Add(
            temporaryPath_,
            1,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            12
        );

        directoryRow->Add(openDirectory, 0);

        contentLayout_->AddStretchSpacer();

        contentLayout_->Add(
            directoryRow,
            0,
            wxEXPAND | wxALL,
            22
        );

        content_->SetSizer(contentLayout_);

        body->Add(categories, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(content_, 1, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Save();
        });

        discard->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Load();
        });

        category_->Bind(
            wxEVT_CHOICE,
            [this](wxCommandEvent&)
            {
                title_->SetLabel(
                    category_->GetStringSelection()
                );
                content_->Layout();
            }
        );

        openDirectory->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                const auto directory =
                    std::filesystem::u8path(
                        WorkspaceService::Instance().
                            WorkspaceDirectory()
                    ) / "temp";

                std::filesystem::create_directories(
                    directory
                );

                wxLaunchDefaultApplication(
                    Wx(directory.u8string())
                );
            }
        );

        Load();
    }

private:
    wxCheckBox* AddToggle(const wxString& text)
    {
        auto* value = new wxCheckBox(
            content_,
            wxID_ANY,
            text
        );

        value->SetForegroundColour(Theme::Text());

        contentLayout_->Add(
            value,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        return value;
    }

    static bool Boolean(
        Database& database,
        const std::string& key,
        bool fallback
    )
    {
        return GetMeta(
            database,
            "setting." + key,
            fallback ? "1" : "0"
        ) == "1";
    }

    void Load()
    {
        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database = workspace.GetDatabase();

        rememberCredential_->SetValue(
            Boolean(database, "credential_unlock", true)
        );

        rememberSession_->SetValue(
            Boolean(database, "remember_session", false)
        );

        automaticLock_->SetValue(
            Boolean(database, "automatic_lock", true)
        );

        delayForJobs_->SetValue(
            Boolean(database, "delay_lock_for_jobs", true)
        );

        cleanTemporary_->SetValue(
            Boolean(database, "clean_temporary", true)
        );

        cleanClipboard_->SetValue(
            Boolean(database, "clean_clipboard", true)
        );

        autoScan_->SetValue(
            Boolean(database, "automatic_scan", true)
        );

        autoBackup_->SetValue(
            Boolean(database, "automatic_backup", true)
        );

        conflictDetection_->SetValue(
            Boolean(database, "conflict_detection", true)
        );

        try
        {
            lockMinutes_->SetValue(
                std::stoi(GetMeta(
                    database,
                    "setting.lock_minutes",
                    "15"
                ))
            );

            retentionDays_->SetValue(
                std::stoi(GetMeta(
                    database,
                    "setting.retention_days",
                    "365"
                ))
            );
        }
        catch (...)
        {
            lockMinutes_->SetValue(15);
            retentionDays_->SetValue(365);
        }

        const std::string format = GetMeta(
            database,
            "setting.export_format",
            "html"
        );

        exportFormat_->SetSelection(
            format == "markdown"
                ? 1
                : format == "text"
                    ? 2
                    : 0
        );

        const auto capabilities =
            DatabaseSecurity::Diagnose();

        securityStatus_->SetLabel(
            capabilities.sqlCipherAvailable
                ? U("工作区数据库加密能力正常")
                : U("当前 SQLite 未提供 SQLCipher 加密能力")
        );

        securityStatus_->SetForegroundColour(
            capabilities.sqlCipherAvailable
                ? Theme::Green()
                : Theme::Yellow()
        );

        const auto temporary =
            std::filesystem::u8path(
                workspace.WorkspaceDirectory()
            ) / "temp";

        temporaryPath_->SetLabel(
            U("临时目录：") +
            Wx(temporary.u8string())
        );
    }

    void SaveValue(
        Database& database,
        const std::string& key,
        bool value,
        StorageStatus& status
    )
    {
        if (!status.success)
        {
            return;
        }

        status = SetMeta(
            database,
            "setting." + key,
            value ? "1" : "0",
            "ui"
        );
    }

    void Save()
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        StorageStatus status = StorageStatus::Ok();

        SaveValue(
            database,
            "credential_unlock",
            rememberCredential_->GetValue(),
            status
        );
        SaveValue(
            database,
            "remember_session",
            rememberSession_->GetValue(),
            status
        );
        SaveValue(
            database,
            "automatic_lock",
            automaticLock_->GetValue(),
            status
        );
        SaveValue(
            database,
            "delay_lock_for_jobs",
            delayForJobs_->GetValue(),
            status
        );
        SaveValue(
            database,
            "clean_temporary",
            cleanTemporary_->GetValue(),
            status
        );
        SaveValue(
            database,
            "clean_clipboard",
            cleanClipboard_->GetValue(),
            status
        );
        SaveValue(
            database,
            "automatic_scan",
            autoScan_->GetValue(),
            status
        );
        SaveValue(
            database,
            "automatic_backup",
            autoBackup_->GetValue(),
            status
        );
        SaveValue(
            database,
            "conflict_detection",
            conflictDetection_->GetValue(),
            status
        );

        if (status.success)
        {
            status = SetMeta(
                database,
                "setting.lock_minutes",
                std::to_string(lockMinutes_->GetValue()),
                "ui"
            );
        }

        if (status.success)
        {
            status = SetMeta(
                database,
                "setting.retention_days",
                std::to_string(retentionDays_->GetValue()),
                "ui"
            );
        }

        if (status.success)
        {
            const std::string format =
                exportFormat_->GetSelection() == 1
                    ? "markdown"
                    : exportFormat_->GetSelection() == 2
                        ? "text"
                        : "html";

            status = SetMeta(
                database,
                "setting.export_format",
                format,
                "ui"
            );
        }

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("工作区设置"),
                status
            );
            return;
        }

        wxMessageBox(
            U("工作区设置已保存并立即生效。"),
            U("工作区设置"),
            wxOK | wxICON_INFORMATION,
            this
        );
    }

    wxChoice* category_ = nullptr;
    wxPanel* content_ = nullptr;
    wxBoxSizer* contentLayout_ = nullptr;
    wxStaticText* title_ = nullptr;
    wxStaticText* securityStatus_ = nullptr;
    wxCheckBox* rememberCredential_ = nullptr;
    wxCheckBox* rememberSession_ = nullptr;
    wxCheckBox* automaticLock_ = nullptr;
    wxCheckBox* delayForJobs_ = nullptr;
    wxCheckBox* cleanTemporary_ = nullptr;
    wxCheckBox* cleanClipboard_ = nullptr;
    wxCheckBox* autoScan_ = nullptr;
    wxCheckBox* autoBackup_ = nullptr;
    wxCheckBox* conflictDetection_ = nullptr;
    wxSpinCtrl* lockMinutes_ = nullptr;
    wxSpinCtrl* retentionDays_ = nullptr;
    wxChoice* exportFormat_ = nullptr;
    wxStaticText* temporaryPath_ = nullptr;
};

class HandoverPage final : public wxPanel
{
public:
    HandoverPage(
        wxWindow* parent,
        bool validationMode
    )
        : wxPanel(parent, wxID_ANY),
          validationMode_(validationMode)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* save = Button(this, U("保存为草稿"));
        auto* validate = Button(
            this,
            validationMode
                ? U("开始导出")
                : U("预览并验证"),
            true
        );

        actions->Add(save, 0, wxRIGHT, 10);
        actions->Add(validate, 0);

        AddHeading(
            this,
            root,
            validationMode ? U("P16") : U("P15"),
            validationMode
                ? U("导出预览与验证")
                : U("交接胶囊编辑器"),
            validationMode
                ? U("检查引用、附件、脱敏策略和离线包完整性")
                : U("组织可验证、可脱敏、可离线阅读的项目交接包"),
            actions
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* configuration = Card(this);
        auto* configurationLayout =
            new wxBoxSizer(wxVERTICAL);

        configurationLayout->Add(
            Label(
                configuration,
                U("胶囊配置"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        name_ = Input(configuration);
        purpose_ = Input(configuration, wxTE_MULTILINE);

        AddField(
            configurationLayout,
            configuration,
            U("名称"),
            name_
        );

        AddField(
            configurationLayout,
            configuration,
            U("用途与范围"),
            purpose_
        );

        purpose_->SetMinSize(wxSize(-1, 95));

        redactPaths_ = new wxCheckBox(
            configuration,
            wxID_ANY,
            U("隐藏本机绝对路径")
        );

        includeAttachments_ = new wxCheckBox(
            configuration,
            wxID_ANY,
            U("包含可验证附件")
        );

        offline_ = new wxCheckBox(
            configuration,
            wxID_ANY,
            U("生成完全离线报告")
        );

        for (auto* value : {
                 redactPaths_,
                 includeAttachments_,
                 offline_})
        {
            value->SetForegroundColour(Theme::Text());

            configurationLayout->Add(
                value,
                0,
                wxLEFT | wxRIGHT | wxTOP,
                16
            );
        }

        validation_ = Label(
            configuration,
            wxEmptyString,
            9,
            Theme::Yellow(),
            wxFONTWEIGHT_BOLD
        );

        validation_->Wrap(200);

        configurationLayout->Add(
            validation_,
            0,
            wxALL,
            16
        );

        configurationLayout->AddStretchSpacer();
        configuration->SetSizer(configurationLayout);
        configuration->SetMinSize(wxSize(230, -1));

        auto* structure = Card(this);
        auto* structureLayout =
            new wxBoxSizer(wxVERTICAL);

        auto* sectionHeading =
            new wxBoxSizer(wxHORIZONTAL);

        sectionHeading->Add(
            Label(
                structure,
                U("章节结构"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );

        auto* addSection =
            Button(structure, U("添加章节"));

        sectionHeading->Add(addSection, 0);

        structureLayout->Add(
            sectionHeading,
            0,
            wxEXPAND | wxALL,
            16
        );

        sections_ = new wxListCtrl(
            structure,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT |
                wxLC_SINGLE_SEL |
                wxBORDER_NONE
        );

        sections_->SetBackgroundColour(Theme::Input());
        sections_->SetForegroundColour(Theme::Text());
        sections_->InsertColumn(
            0,
            U("章节"),
            wxLIST_FORMAT_LEFT,
            190
        );
        sections_->InsertColumn(
            1,
            U("项目"),
            wxLIST_FORMAT_RIGHT,
            55
        );

        structureLayout->Add(
            sections_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* ordering = new wxBoxSizer(wxHORIZONTAL);
        auto* moveUp = Button(structure, U("上移"));
        auto* moveDown = Button(structure, U("下移"));

        ordering->Add(moveUp, 1, wxRIGHT, 8);
        ordering->Add(moveDown, 1);

        structureLayout->Add(
            ordering,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        structure->SetSizer(structureLayout);
        structure->SetMinSize(wxSize(292, -1));

        auto* preview = Card(this);
        auto* previewLayout = new wxBoxSizer(wxVERTICAL);

        previewLayout->Add(
            Label(
                preview,
                U("报告预览"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        preview_ = Input(
            preview,
            wxTE_MULTILINE | wxTE_READONLY
        );

        preview_->SetBackgroundColour(
            wxColour(244, 241, 233)
        );
        preview_->SetForegroundColour(
            wxColour(32, 41, 54)
        );

        previewLayout->Add(
            preview_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );

        preview->SetSizer(previewLayout);

        auto* settings = Card(this);
        auto* settingsLayout =
            new wxBoxSizer(wxVERTICAL);

        settingsLayout->Add(
            Label(
                settings,
                U("章节设置"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        sectionTitle_ = Input(settings);
        manualText_ = Input(settings, wxTE_MULTILINE);

        AddField(
            settingsLayout,
            settings,
            U("章节名称"),
            sectionTitle_
        );

        AddField(
            settingsLayout,
            settings,
            U("人工说明"),
            manualText_
        );

        manualText_->SetMinSize(wxSize(-1, 150));

        includeEvidence_ = new wxCheckBox(
            settings,
            wxID_ANY,
            U("包含支持与反对证据")
        );

        includeRelations_ = new wxCheckBox(
            settings,
            wxID_ANY,
            U("显示关系摘要")
        );

        includeHistory_ = new wxCheckBox(
            settings,
            wxID_ANY,
            U("显示完整历史")
        );

        for (auto* value : {
                 includeEvidence_,
                 includeRelations_,
                 includeHistory_})
        {
            value->SetForegroundColour(Theme::Text());

            settingsLayout->Add(
                value,
                0,
                wxLEFT | wxRIGHT | wxTOP,
                16
            );
        }

        auto* apply =
            Button(settings, U("检查本章节"), true);

        settingsLayout->AddStretchSpacer();

        settingsLayout->Add(
            apply,
            0,
            wxEXPAND | wxALL,
            16
        );

        settings->SetSizer(settingsLayout);
        settings->SetMinSize(wxSize(222, -1));

        body->Add(configuration, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(structure, 0, wxEXPAND | wxRIGHT, 14);
        body->Add(preview, 1, wxEXPAND | wxRIGHT, 14);
        body->Add(settings, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            SaveDraft();
        });

        validate->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                ValidateAndExport(validationMode_);
            }
        );

        addSection->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                const wxString title =
                    wxGetTextFromUser(
                        U("输入章节名称"),
                        U("添加章节"),
                        wxEmptyString,
                        this
                    );

                if (!title.empty())
                {
                    Section section;
                    section.title = Utf8(title);
                    section.objectType.clear();
                    section.manual = true;
                    sectionsData_.push_back(section);
                    RefreshSections();
                }
            }
        );

        moveUp->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Move(-1);
        });

        moveDown->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            Move(1);
        });

        sections_->Bind(
            wxEVT_LIST_ITEM_SELECTED,
            [this](wxListEvent& event)
            {
                SelectSection(
                    static_cast<std::size_t>(
                        event.GetIndex()
                    )
                );
            }
        );

        apply->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            ApplySection();
        });

        name_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            RefreshPreview();
        });

        purpose_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) {
            RefreshPreview();
        });

        LoadDraft();
    }

private:
    struct Section
    {
        std::string title;
        std::string objectType;
        std::string manualText;
        bool enabled = true;
        bool includeEvidence = true;
        bool includeRelations = true;
        bool includeHistory = false;
        bool manual = false;
    };

    static std::string Encode(const std::string& value)
    {
        std::string result;

        for (const char character : value)
        {
            if (character == '\\')
            {
                result += "\\\\";
            }
            else if (character == '\n')
            {
                result += "\\n";
            }
            else if (character == '|')
            {
                result += "\\p";
            }
            else
            {
                result += character;
            }
        }

        return result;
    }

    static std::string Decode(const std::string& value)
    {
        std::string result;
        bool escaped = false;

        for (const char character : value)
        {
            if (!escaped && character == '\\')
            {
                escaped = true;
                continue;
            }

            if (escaped)
            {
                result +=
                    character == 'n'
                        ? '\n'
                        : character == 'p'
                            ? '|'
                            : character;

                escaped = false;
                continue;
            }

            result += character;
        }

        if (escaped)
        {
            result += '\\';
        }

        return result;
    }

    int ObjectCount(const Section& section) const
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        if (handle == nullptr)
        {
            return 0;
        }

        Statement statement(
            handle,
            section.objectType.empty()
                ? "SELECT count(*) FROM objects "
                  "WHERE deleted=0;"
                : "SELECT count(*) FROM objects "
                  "WHERE deleted=0 AND object_type=?;"
        );

        if (!statement.Valid())
        {
            return 0;
        }

        if (!section.objectType.empty())
        {
            BindText(
                statement.Get(),
                1,
                section.objectType
            );
        }

        if (sqlite3_step(statement.Get()) != SQLITE_ROW)
        {
            return 0;
        }

        return sqlite3_column_int(statement.Get(), 0);
    }

    void DefaultSections()
    {
        sectionsData_ = {
            {"项目概要", "", "", true, true, true, false, false},
            {"当前关键事实", "fact", "", true, true, true, false, false},
            {"有效决策", "decision", "", true, true, true, false, false},
            {"未完成承诺", "commitment", "", true, true, true, false, false},
            {"活跃风险", "risk", "", true, true, true, false, false},
            {"未决问题", "question", "", true, true, true, false, false},
            {"冲突与复核", "conflict", "", true, true, true, false, false},
            {"推荐阅读顺序", "", "", false, false, false, false, true},
            {"证据索引", "evidence", "", true, true, false, false, false},
            {"完整性清单", "", "", true, false, false, false, true}
        };
    }

    void LoadDraft()
    {
        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            return;
        }

        Database& database = workspace.GetDatabase();

        name_->SetValue(Wx(GetMeta(
            database,
            "handover.name",
            "项目交接包"
        )));

        purpose_->SetValue(Wx(GetMeta(
            database,
            "handover.purpose",
            "项目交接"
        )));

        redactPaths_->SetValue(
            GetMeta(
                database,
                "handover.redact_paths",
                "1"
            ) == "1"
        );

        includeAttachments_->SetValue(
            GetMeta(
                database,
                "handover.attachments",
                "1"
            ) == "1"
        );

        offline_->SetValue(
            GetMeta(
                database,
                "handover.offline",
                "1"
            ) == "1"
        );

        const std::string serialized = GetMeta(
            database,
            "handover.sections",
            std::string()
        );

        sectionsData_.clear();

        if (!serialized.empty())
        {
            std::istringstream lines(serialized);
            std::string line;

            while (std::getline(lines, line))
            {
                std::vector<std::string> fields;
                std::string field;
                bool escaped = false;

                for (const char character : line)
                {
                    if (!escaped && character == '\\')
                    {
                        escaped = true;
                        field += character;
                        continue;
                    }

                    if (!escaped && character == '|')
                    {
                        fields.push_back(Decode(field));
                        field.clear();
                        continue;
                    }

                    field += character;
                    escaped = false;
                }

                fields.push_back(Decode(field));

                if (fields.size() < 8)
                {
                    continue;
                }

                Section section;
                section.title = fields[0];
                section.objectType = fields[1];
                section.manualText = fields[2];
                section.enabled = fields[3] == "1";
                section.includeEvidence = fields[4] == "1";
                section.includeRelations = fields[5] == "1";
                section.includeHistory = fields[6] == "1";
                section.manual = fields[7] == "1";

                sectionsData_.push_back(section);
            }
        }

        if (sectionsData_.empty())
        {
            DefaultSections();
        }

        includeEvidence_->SetValue(true);
        includeRelations_->SetValue(true);
        includeHistory_->SetValue(false);

        RefreshSections();
        RefreshPreview();

        if (validationMode_)
        {
            Validate(false);
        }
    }

    std::string SerializeSections() const
    {
        std::ostringstream output;

        for (const auto& section : sectionsData_)
        {
            output
                << Encode(section.title) << "|"
                << Encode(section.objectType) << "|"
                << Encode(section.manualText) << "|"
                << (section.enabled ? "1" : "0") << "|"
                << (section.includeEvidence ? "1" : "0") << "|"
                << (section.includeRelations ? "1" : "0") << "|"
                << (section.includeHistory ? "1" : "0") << "|"
                << (section.manual ? "1" : "0")
                << "\n";
        }

        return output.str();
    }

    void SaveDraft()
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        StorageStatus status = SetMeta(
            database,
            "handover.name",
            Utf8(name_->GetValue()),
            "ui"
        );

        if (status.success)
        {
            status = SetMeta(
                database,
                "handover.purpose",
                Utf8(purpose_->GetValue()),
                "ui"
            );
        }

        if (status.success)
        {
            status = SetMeta(
                database,
                "handover.redact_paths",
                redactPaths_->GetValue() ? "1" : "0",
                "ui"
            );
        }

        if (status.success)
        {
            status = SetMeta(
                database,
                "handover.attachments",
                includeAttachments_->GetValue()
                    ? "1"
                    : "0",
                "ui"
            );
        }

        if (status.success)
        {
            status = SetMeta(
                database,
                "handover.offline",
                offline_->GetValue() ? "1" : "0",
                "ui"
            );
        }

        if (status.success)
        {
            status = SetMeta(
                database,
                "handover.sections",
                SerializeSections(),
                "ui"
            );
        }

        if (!status.success)
        {
            ShowStorageError(
                this,
                U("保存交接胶囊"),
                status
            );
            return;
        }

        validation_->SetLabel(U("草稿已保存。"));
    }

    void RefreshSections()
    {
        sections_->DeleteAllItems();

        for (const auto& section : sectionsData_)
        {
            const long row = sections_->InsertItem(
                sections_->GetItemCount(),
                section.enabled
                    ? U("✓ ") + Wx(section.title)
                    : U("□ ") + Wx(section.title)
            );

            sections_->SetItem(
                row,
                1,
                wxString::Format(
                    "%d",
                    ObjectCount(section)
                )
            );
        }

        if (!sectionsData_.empty())
        {
            sections_->SetItemState(
                0,
                wxLIST_STATE_SELECTED,
                wxLIST_STATE_SELECTED
            );
            SelectSection(0);
        }
    }

    void SelectSection(std::size_t index)
    {
        if (index >= sectionsData_.size())
        {
            return;
        }

        selectedSection_ = index;
        const auto& section = sectionsData_[index];

        sectionTitle_->SetValue(Wx(section.title));
        manualText_->SetValue(Wx(section.manualText));
        includeEvidence_->SetValue(section.includeEvidence);
        includeRelations_->SetValue(section.includeRelations);
        includeHistory_->SetValue(section.includeHistory);
    }

    void ApplySection()
    {
        if (selectedSection_ >= sectionsData_.size())
        {
            return;
        }

        auto& section = sectionsData_[selectedSection_];

        section.title = Utf8(sectionTitle_->GetValue());
        section.manualText = Utf8(manualText_->GetValue());
        section.includeEvidence =
            includeEvidence_->GetValue();
        section.includeRelations =
            includeRelations_->GetValue();
        section.includeHistory =
            includeHistory_->GetValue();
        section.enabled = !section.title.empty();

        RefreshSections();
        RefreshPreview();
        Validate(false);
    }

    void Move(int direction)
    {
        if (sectionsData_.empty() ||
            selectedSection_ >= sectionsData_.size())
        {
            return;
        }

        const int current =
            static_cast<int>(selectedSection_);
        const int target = current + direction;

        if (target < 0 ||
            target >= static_cast<int>(
                sectionsData_.size()
            ))
        {
            return;
        }

        std::swap(
            sectionsData_[current],
            sectionsData_[target]
        );

        selectedSection_ =
            static_cast<std::size_t>(target);

        RefreshSections();

        sections_->SetItemState(
            target,
            wxLIST_STATE_SELECTED,
            wxLIST_STATE_SELECTED
        );

        RefreshPreview();
    }

    std::string BuildReport(bool html) const
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        std::ostringstream output;

        if (html)
        {
            output
                << "<!doctype html><html lang=\"zh-CN\">"
                << "<head><meta charset=\"utf-8\">"
                << "<meta name=\"viewport\" "
                << "content=\"width=device-width\">"
                << "<title>"
                << Html(Utf8(name_->GetValue()))
                << "</title>"
                << "<style>"
                << "body{font-family:Segoe UI,Microsoft YaHei,"
                << "sans-serif;max-width:980px;margin:40px auto;"
                << "padding:0 32px;color:#202936;background:#f4f1e9}"
                << "h1,h2{color:#315ea8}"
                << "article{background:#fff;padding:18px 22px;"
                << "margin:16px 0;border-radius:8px}"
                << "small{color:#69727d}"
                << "</style></head><body>";
        }

        const std::string reportName =
            Utf8(name_->GetValue());

        output
            << (html ? "<h1>" : "# ")
            << (html ? Html(reportName) : reportName)
            << (html ? "</h1>" : "\n\n");

        const std::string purpose =
            Utf8(purpose_->GetValue());

        output
            << (html ? "<p>" : "")
            << (html ? Html(purpose) : purpose)
            << (html ? "</p>" : "\n\n");

        std::lock_guard<std::recursive_mutex> lock(
            database.Mutex()
        );

        sqlite3* handle = database.Handle();

        for (const auto& section : sectionsData_)
        {
            if (!section.enabled)
            {
                continue;
            }

            output
                << (html ? "<h2>" : "## ")
                << (html
                        ? Html(section.title)
                        : section.title)
                << (html ? "</h2>" : "\n\n");

            if (!section.manualText.empty())
            {
                output
                    << (html ? "<p>" : "")
                    << (html
                            ? Html(section.manualText)
                            : section.manualText)
                    << (html ? "</p>" : "\n\n");
            }

            if (handle == nullptr ||
                section.manual ||
                section.objectType == "evidence")
            {
                continue;
            }

            Statement statement(
                handle,
                section.objectType.empty()
                    ? "SELECT id, object_type, title, "
                      "description, status, priority, owner "
                      "FROM objects WHERE deleted=0 "
                      "ORDER BY priority, updated_at DESC "
                      "LIMIT 200;"
                    : "SELECT id, object_type, title, "
                      "description, status, priority, owner "
                      "FROM objects WHERE deleted=0 "
                      "AND object_type=? "
                      "ORDER BY priority, updated_at DESC "
                      "LIMIT 200;"
            );

            if (!statement.Valid())
            {
                continue;
            }

            if (!section.objectType.empty())
            {
                BindText(
                    statement.Get(),
                    1,
                    section.objectType
                );
            }

            while (sqlite3_step(statement.Get()) == SQLITE_ROW)
            {
                std::string id = Text(statement.Get(), 0);
                std::string type = Text(statement.Get(), 1);
                std::string title = Text(statement.Get(), 2);
                std::string description = Text(statement.Get(), 3);
                std::string status = Text(statement.Get(), 4);
                std::string priority = Text(statement.Get(), 5);
                std::string owner = Text(statement.Get(), 6);

                if (redactPaths_->GetValue())
                {
                    const std::string workspacePath =
                        workspace.WorkspaceDirectory();

                    std::size_t position = 0;

                    while (!workspacePath.empty() &&
                           (position = description.find(
                                workspacePath,
                                position
                            )) != std::string::npos)
                    {
                        description.replace(
                            position,
                            workspacePath.size(),
                            "[工作区路径]"
                        );
                        position += 15;
                    }
                }

                if (html)
                {
                    output
                        << "<article><strong>"
                        << Html(id + "  " + title)
                        << "</strong><br><small>"
                        << Html(
                            type + " · " +
                            status + " · " +
                            priority
                        )
                        << "</small><p>"
                        << Html(description)
                        << "</p>";

                    if (!owner.empty())
                    {
                        output
                            << "<small>负责人："
                            << Html(owner)
                            << "</small>";
                    }

                    output << "</article>";
                }
                else
                {
                    output
                        << "### " << id << "  " << title
                        << "\n"
                        << type << " · " << status
                        << " · " << priority << "\n\n"
                        << description << "\n\n";

                    if (!owner.empty())
                    {
                        output
                            << "负责人：" << owner << "\n\n";
                    }
                }
            }
        }

        if (html)
        {
            output
                << "<hr><small>由 Continuum 本地工作区生成。"
                << "导出前已检查对象引用和证据状态。</small>"
                << "</body></html>";
        }

        return output.str();
    }

    void RefreshPreview()
    {
        preview_->SetValue(
            Wx(BuildReport(false).substr(0, 60000))
        );
    }

    bool Validate(bool showDialog)
    {
        auto& workspace = WorkspaceService::Instance();
        Database& database = workspace.GetDatabase();

        int invalidEvidence = 0;
        int changedEvidence = 0;
        int missingFiles = 0;

        {
            std::lock_guard<std::recursive_mutex> lock(
                database.Mutex()
            );

            sqlite3* handle = database.Handle();

            if (handle != nullptr)
            {
                Statement evidence(
                    handle,
                    "SELECT "
                    "sum(CASE WHEN e.deleted=1 THEN 1 ELSE 0 END), "
                    "sum(CASE WHEN e.review_state IN "
                    "('changed','needs_review','unverified') "
                    "THEN 1 ELSE 0 END), "
                    "sum(CASE WHEN f.parse_state='missing' "
                    "THEN 1 ELSE 0 END) "
                    "FROM evidence e "
                    "LEFT JOIN files f ON f.id=e.file_id;"
                );

                if (evidence.Valid() &&
                    sqlite3_step(evidence.Get()) == SQLITE_ROW)
                {
                    invalidEvidence =
                        sqlite3_column_int(evidence.Get(), 0);
                    changedEvidence =
                        sqlite3_column_int(evidence.Get(), 1);
                    missingFiles =
                        sqlite3_column_int(evidence.Get(), 2);
                }
            }
        }

        const bool valid =
            !name_->GetValue().empty() &&
            invalidEvidence == 0 &&
            changedEvidence == 0 &&
            missingFiles == 0;

        validation_->SetLabel(
            U("引用检查：") +
            wxString::Format(
                "%d 项失效，%d 项待复核，%d 个来源失联",
                invalidEvidence,
                changedEvidence,
                missingFiles
            )
        );

        validation_->SetForegroundColour(
            valid ? Theme::Green() : Theme::Yellow()
        );

        if (showDialog)
        {
            wxMessageBox(
                valid
                    ? U("验证完成：引用、来源和离线报告结构均通过。")
                    : U("验证完成：报告已生成，但存在需要复核的"
                        "引用或来源。导出清单将明确记录这些项目。"),
                U("导出验证"),
                wxOK |
                    (valid
                        ? wxICON_INFORMATION
                        : wxICON_WARNING),
                this
            );
        }

        return valid;
    }

    void ValidateAndExport(bool exportNow)
    {
        const bool valid = Validate(true);
        RefreshPreview();

        if (!exportNow)
        {
            return;
        }

        SaveDraft();

        auto& workspace = WorkspaceService::Instance();

        const auto exportDirectory =
            std::filesystem::u8path(
                workspace.WorkspaceDirectory()
            ) / "exports" / "handover";

        std::filesystem::create_directories(
            exportDirectory
        );

        const auto reportPath =
            exportDirectory / "index.html";

        const auto manifestPath =
            exportDirectory / "integrity.txt";

        const std::string report = BuildReport(true);

        {
            std::ofstream output(
                reportPath,
                std::ios::binary | std::ios::trunc
            );
            output.write(
                report.data(),
                static_cast<std::streamsize>(report.size())
            );
        }

        const std::string digest =
            ContentHasher::Sha256Text(report);

        {
            std::ofstream manifest(
                manifestPath,
                std::ios::binary | std::ios::trunc
            );

            manifest
                << "algorithm=SHA-256\n"
                << "file=index.html\n"
                << "digest=" << digest << "\n"
                << "validation="
                << (valid ? "passed" : "review-required")
                << "\n";
        }

        workspace.Audit().Append(
            "ui",
            "handover",
            "export",
            "handover",
            "current",
            "{\"digest\":\"" + digest + "\"}"
        );

        wxMessageBox(
            U("离线交接包已导出：\n") +
            Wx(exportDirectory.u8string()),
            U("导出完成"),
            wxOK | wxICON_INFORMATION,
            this
        );
    }

    bool validationMode_ = false;
    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* purpose_ = nullptr;
    wxCheckBox* redactPaths_ = nullptr;
    wxCheckBox* includeAttachments_ = nullptr;
    wxCheckBox* offline_ = nullptr;
    wxStaticText* validation_ = nullptr;
    wxListCtrl* sections_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxTextCtrl* sectionTitle_ = nullptr;
    wxTextCtrl* manualText_ = nullptr;
    wxCheckBox* includeEvidence_ = nullptr;
    wxCheckBox* includeRelations_ = nullptr;
    wxCheckBox* includeHistory_ = nullptr;
    std::vector<Section> sectionsData_;
    std::size_t selectedSection_ = 0;
};

#include "SearchRecyclePages.inc"

}

wxWindow* CreateCompleteWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P07"))
    {
        return new GlobalSearchPage(parent);
    }

    if (pageCode == U("P17"))
    {
        return new RecycleBinPage(parent);
    }

    if (pageCode == U("M05"))
    {
        return new EntityPage(parent);
    }

    if (pageCode == U("M06"))
    {
        return new SavedQueryPage(parent);
    }

    if (pageCode == U("M07"))
    {
        return new RulesPage(parent);
    }

    if (pageCode == U("M08"))
    {
        return new WorkspaceSettingsPage(parent);
    }

    if (pageCode == U("P15"))
    {
        return new HandoverPage(parent, false);
    }

    if (pageCode == U("P16"))
    {
        return new HandoverPage(parent, true);
    }

    return nullptr;
}

}
