#include "OperationsPages.h"

#include "FileScanner.h"
#include "Theme.h"
#include "UiDataService.h"

#include <algorithm>
#include <cctype>
#include <string>

#include <sqlite3.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dirdlg.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/window.h>

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
    const wxString& value = wxEmptyString,
    long extraStyle = 0
)
{
    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        value,
        wxDefaultPosition,
        wxSize(-1, 38),
        wxBORDER_NONE | extraStyle
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

UiOperationResult SaveLocalSource(
    const wxString& name,
    const wxString& directory,
    bool queueScan
)
{
    const std::string sourceName = Utf8(name);
    const std::string sourceDirectory = Utf8(directory);

    if (sourceName.empty())
    {
        return {
            false,
            SQLITE_CONSTRAINT,
            "数据源名称不能为空"
        };
    }

    if (sourceDirectory.empty())
    {
        return {
            false,
            SQLITE_CONSTRAINT,
            "数据源目录不能为空"
        };
    }

    DataSourceRecord record;
    record.id =
        "SRC-" +
        ContentHasher::Sha256Text(
            Lower(sourceDirectory)
        ).substr(0, 24);
    record.name = sourceName;
    record.sourceType = "local_folder";
    record.rootPath = sourceDirectory;
    record.enabled = true;

    auto result =
        UiDataService::Instance().SaveDataSource(
            record
        );

    if (!result.success || !queueScan)
    {
        return result;
    }

    return UiDataService::Instance()
        .QueueSourceScan(record.id);
}

void ShowResult(
    wxWindow* parent,
    const wxString& title,
    const UiOperationResult& result
)
{
    wxMessageBox(
        result.success
            ? (
                result.message.empty()
                    ? U("操作成功。")
                    : Wx(result.message)
            )
            : Wx(result.message),
        title,
        wxOK |
            (
                result.success
                    ? wxICON_INFORMATION
                    : wxICON_ERROR
            ),
        parent
    );
}

class DataSourcesPage final : public wxPanel
{
public:
    explicit DataSourcesPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M01"),
            U("数据源管理"),
            U("管理真实的本地文件夹数据源并提交扫描任务")
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* add = MakeButton(
            this,
            U("添加本地文件夹"),
            true
        );

        auto* scan = MakeButton(
            this,
            U("扫描选中数据源")
        );
        scan->SetName("sources.scan");

        auto* refresh = MakeButton(
            this,
            U("刷新")
        );

        actions->Add(add, 0, wxRIGHT, 10);
        actions->Add(scan, 0, wxRIGHT, 10);
        actions->Add(refresh, 0);

        root->Add(
            actions,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* list = MakeList(this);
        list->SetName("sources.list");

        root->Add(
            list,
            1,
            wxEXPAND | wxALL,
            26
        );

        auto* status = MakeText(
            this,
            U("请选择数据源后执行扫描。"),
            9,
            Theme::Muted()
        );
        status->SetName("sources.status");

        root->Add(
            status,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        add->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                wxDirDialog dialog(
                    this,
                    U("选择数据源文件夹"),
                    wxEmptyString,
                    wxDD_DEFAULT_STYLE |
                        wxDD_DIR_MUST_EXIST
                );

                if (dialog.ShowModal() != wxID_OK)
                {
                    return;
                }

                const wxString directory =
                    dialog.GetPath();

                wxString name =
                    wxFileName(directory).GetFullName();

                if (name.empty())
                {
                    name = directory;
                }

                const auto result =
                    SaveLocalSource(
                        name,
                        directory,
                        true
                    );

                ShowResult(
                    this,
                    U("添加数据源"),
                    result
                );

                if (result.success)
                {
                    UiDataService::Instance().Refresh(
                        UiChangeType::DataSources
                    );
                }
            }
        );

        refresh->Bind(
            wxEVT_BUTTON,
            [](wxCommandEvent&)
            {
                UiDataService::Instance().Refresh(
                    UiChangeType::DataSources
                );
            }
        );
    }
};

class AddSourceWizardPage final : public wxPanel
{
public:
    explicit AddSourceWizardPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M02"),
            U("添加数据源"),
            U("选择一个真实存在的 Windows 文件夹并加入工作区")
        );

        auto* form = new wxPanel(this, wxID_ANY);
        Theme::Apply(form, Theme::Surface());

        auto* formSizer = new wxBoxSizer(wxVERTICAL);

        formSizer->Add(
            MakeText(
                form,
                U("数据源名称"),
                9,
                Theme::Text(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            20
        );

        auto* name = MakeInput(form);
        name->SetHint(U("例如：项目文档"));

        formSizer->Add(
            name,
            0,
            wxEXPAND | wxALL,
            20
        );

        formSizer->Add(
            MakeText(
                form,
                U("本地文件夹"),
                9,
                Theme::Text(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxLEFT | wxRIGHT,
            20
        );

        auto* pathRow = new wxBoxSizer(wxHORIZONTAL);
        auto* directory = MakeInput(
            form,
            wxEmptyString,
            wxTE_READONLY
        );
        directory->SetHint(U("请选择文件夹"));

        auto* browse = MakeButton(
            form,
            U("浏览…")
        );

        pathRow->Add(directory, 1, wxRIGHT, 10);
        pathRow->Add(browse, 0);

        formSizer->Add(
            pathRow,
            0,
            wxEXPAND | wxALL,
            20
        );

        auto* scanImmediately = new wxCheckBox(
            form,
            wxID_ANY,
            U("保存后立即提交首次扫描")
        );

        scanImmediately->SetValue(true);
        scanImmediately->SetForegroundColour(
            Theme::Text()
        );

        formSizer->Add(
            scanImmediately,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            20
        );

        auto* save = MakeButton(
            form,
            U("保存数据源"),
            true
        );

        formSizer->Add(
            save,
            0,
            wxALIGN_RIGHT | wxLEFT |
                wxRIGHT | wxBOTTOM,
            20
        );

        auto* status = MakeText(
            form,
            wxEmptyString,
            9,
            Theme::Muted()
        );

        formSizer->Add(
            status,
            0,
            wxEXPAND | wxLEFT |
                wxRIGHT | wxBOTTOM,
            20
        );

        form->SetSizer(formSizer);

        root->Add(
            form,
            0,
            wxEXPAND | wxALL,
            26
        );

        root->AddStretchSpacer();
        SetSizer(root);

        browse->Bind(
            wxEVT_BUTTON,
            [this, directory, name](wxCommandEvent&)
            {
                wxDirDialog dialog(
                    this,
                    U("选择数据源文件夹"),
                    directory->GetValue(),
                    wxDD_DEFAULT_STYLE |
                        wxDD_DIR_MUST_EXIST
                );

                if (dialog.ShowModal() != wxID_OK)
                {
                    return;
                }

                directory->ChangeValue(
                    dialog.GetPath()
                );

                if (name->GetValue().empty())
                {
                    wxString suggested =
                        wxFileName(
                            dialog.GetPath()
                        ).GetFullName();

                    if (suggested.empty())
                    {
                        suggested = dialog.GetPath();
                    }

                    name->ChangeValue(suggested);
                }
            }
        );

        save->Bind(
            wxEVT_BUTTON,
            [this,
             name,
             directory,
             scanImmediately,
             status](wxCommandEvent&)
            {
                const auto result =
                    SaveLocalSource(
                        name->GetValue(),
                        directory->GetValue(),
                        scanImmediately->GetValue()
                    );

                status->SetLabel(
                    result.success
                        ? (
                            scanImmediately->GetValue()
                                ? U("数据源已保存，扫描任务已提交。")
                                : U("数据源已保存。")
                        )
                        : Wx(result.message)
                );

                if (!result.success)
                {
                    wxMessageBox(
                        Wx(result.message),
                        U("保存数据源失败"),
                        wxOK | wxICON_ERROR,
                        this
                    );
                }
            }
        );
    }
};

class BackgroundTasksPage final : public wxPanel
{
public:
    explicit BackgroundTasksPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M03"),
            U("后台任务"),
            U("查看真实任务状态，并取消或重试选中的任务")
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* cancel = MakeButton(
            this,
            U("取消选中任务"),
            false,
            true
        );
        cancel->SetName("jobs.cancel");

        auto* retry = MakeButton(
            this,
            U("重试选中任务"),
            true
        );
        retry->SetName("jobs.retry");

        auto* refresh = MakeButton(
            this,
            U("刷新")
        );

        actions->Add(cancel, 0, wxRIGHT, 10);
        actions->Add(retry, 0, wxRIGHT, 10);
        actions->Add(refresh, 0);

        root->Add(
            actions,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* list = MakeList(this);
        list->SetName("jobs.list");

        root->Add(
            list,
            1,
            wxEXPAND | wxALL,
            26
        );

        auto* status = MakeText(
            this,
            U("任务变化会立即刷新，并每十秒执行一次状态同步。"),
            9,
            Theme::Muted()
        );
        status->SetName("jobs.status");

        root->Add(
            status,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        refresh->Bind(
            wxEVT_BUTTON,
            [](wxCommandEvent&)
            {
                UiDataService::Instance().Refresh(
                    UiChangeType::Jobs
                );
            }
        );
    }
};

class BackupSnapshotsPage final : public wxPanel
{
public:
    explicit BackupSnapshotsPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M04"),
            U("快照与备份"),
            U("查看工作区中真实存在的备份并创建新备份")
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        auto* create = MakeButton(
            this,
            U("创建备份"),
            true
        );
        create->SetName("backups.create");

        auto* refresh = MakeButton(
            this,
            U("刷新")
        );

        actions->Add(create, 0, wxRIGHT, 10);
        actions->Add(refresh, 0);

        root->Add(
            actions,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        auto* list = MakeList(this);
        list->SetName("backups.list");

        root->Add(
            list,
            1,
            wxEXPAND | wxALL,
            26
        );

        auto* status = MakeText(
            this,
            U("尚未执行备份操作。"),
            9,
            Theme::Muted()
        );
        status->SetName("backups.status");

        root->Add(
            status,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);

        refresh->Bind(
            wxEVT_BUTTON,
            [](wxCommandEvent&)
            {
                UiDataService::Instance().Refresh(
                    UiChangeType::Backups
                );
            }
        );
    }
};

}

wxWindow* CreateOperationsPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("M01"))
    {
        return new DataSourcesPage(parent);
    }

    if (pageCode == U("M02"))
    {
        return new AddSourceWizardPage(parent);
    }

    if (pageCode == U("M03"))
    {
        return new BackgroundTasksPage(parent);
    }

    if (pageCode == U("M04"))
    {
        return new BackupSnapshotsPage(parent);
    }

    return nullptr;
}

}
