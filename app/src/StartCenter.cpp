
#include "StartCenter.h"

#include "FileScanner.h"
#include "SecurityService.h"
#include "Theme.h"
#include "WorkspaceService.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <sqlite3.h>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dirdlg.h>
#include <wx/filename.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
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

wxStaticText* Text(
    wxWindow* parent,
    const wxString& value,
    int size,
    const wxColour& colour,
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* control = new wxStaticText(
        parent,
        wxID_ANY,
        value
    );

    control->SetForegroundColour(colour);
    control->SetFont(Theme::Font(size, weight));
    return control;
}

wxButton* Button(
    wxWindow* parent,
    const wxString& label,
    bool primary = false
)
{
    auto* control = new wxButton(
        parent,
        wxID_ANY,
        label,
        wxDefaultPosition,
        wxSize(-1, 42),
        wxBORDER_NONE
    );

    control->SetBackgroundColour(
        primary ? Theme::Blue() : Theme::Surface2()
    );
    control->SetForegroundColour(Theme::Text());
    control->SetFont(
        Theme::Font(10, wxFONTWEIGHT_SEMIBOLD)
    );

    return control;
}

wxTextCtrl* Input(
    wxWindow* parent,
    const wxString& value = wxEmptyString,
    long style = 0
)
{
    auto* control = new wxTextCtrl(
        parent,
        wxID_ANY,
        value,
        wxDefaultPosition,
        wxSize(-1, 40),
        wxBORDER_NONE | style
    );

    control->SetBackgroundColour(Theme::Input());
    control->SetForegroundColour(Theme::Text());
    control->SetFont(Theme::Font(10));
    return control;
}

std::filesystem::path RegistryPath()
{
    const wxString base =
        wxStandardPaths::Get().GetUserLocalDataDir();

    return (
        std::filesystem::u8path(Utf8(base)) /
        "recent-workspaces.txt"
    );
}

std::vector<std::string> ReadRecent()
{
    std::vector<std::string> values;
    std::ifstream input(
        RegistryPath(),
        std::ios::binary
    );

    std::string line;

    while (std::getline(input, line))
    {
        if (!line.empty() &&
            line.back() == '\r')
        {
            line.pop_back();
        }

        if (line.empty())
        {
            continue;
        }

        std::error_code error;
        const auto database =
            std::filesystem::u8path(line) /
            "continuum.db";

        if (std::filesystem::is_regular_file(
                database,
                error
            ) &&
            !error)
        {
            values.push_back(line);
        }

        if (values.size() >= 20)
        {
            break;
        }
    }

    return values;
}

void WriteRecent(
    const std::vector<std::string>& values
)
{
    const auto path = RegistryPath();
    std::error_code error;

    std::filesystem::create_directories(
        path.parent_path(),
        error
    );

    if (error)
    {
        return;
    }

    const auto temporary =
        std::filesystem::u8path(
            path.u8string() + ".tmp"
        );

    {
        std::ofstream output(
            temporary,
            std::ios::binary |
            std::ios::trunc
        );

        if (!output)
        {
            return;
        }

        for (const auto& value : values)
        {
            output << value << '\n';
        }
    }

    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(
        temporary,
        path,
        error
    );
}

StorageStatus SaveMeta(
    Database& database,
    const std::string& key,
    const std::string& value
)
{
    return database.Transaction([&]() {
        sqlite3* handle = database.Handle();
        sqlite3_stmt* statement = nullptr;

        const int prepare = sqlite3_prepare_v2(
            handle,
            "INSERT INTO workspace_meta("
            "key, value, updated_at"
            ") VALUES(?, ?, CURRENT_TIMESTAMP) "
            "ON CONFLICT(key) DO UPDATE SET "
            "value=excluded.value, "
            "updated_at=CURRENT_TIMESTAMP;",
            -1,
            &statement,
            nullptr
        );

        if (prepare != SQLITE_OK ||
            statement == nullptr)
        {
            if (statement != nullptr)
            {
                sqlite3_finalize(statement);
            }

            return StorageStatus::Error(
                prepare,
                "无法准备工作区基本信息保存"
            );
        }

        sqlite3_bind_text(
            statement,
            1,
            key.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        sqlite3_bind_text(
            statement,
            2,
            value.c_str(),
            -1,
            SQLITE_TRANSIENT
        );

        const int result = sqlite3_step(statement);
        sqlite3_finalize(statement);

        return result == SQLITE_DONE
            ? StorageStatus::Ok()
            : StorageStatus::Error(
                result,
                "无法保存工作区基本信息"
            );
    });
}

class WorkspaceWizard final : public wxDialog
{
public:
    explicit WorkspaceWizard(wxWindow* parent)
        : wxDialog(
            parent,
            wxID_ANY,
            U("创建工作区"),
            wxDefaultPosition,
            wxSize(980, 720),
            wxDEFAULT_DIALOG_STYLE |
                wxRESIZE_BORDER
        )
    {
        Theme::Apply(this, Theme::Window());
        SetMinSize(wxSize(820, 620));

        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* header = new wxBoxSizer(wxVERTICAL);
        stepLabel_ = Text(
            this,
            U("P02 · 步骤 1 / 5"),
            9,
            Theme::Blue(),
            wxFONTWEIGHT_BOLD
        );

        title_ = Text(
            this,
            U("创建工作区"),
            22,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        );

        header->Add(stepLabel_, 0, wxBOTTOM, 6);
        header->Add(title_, 0, wxBOTTOM, 5);
        header->Add(
            Text(
                this,
                U("设置基本信息、安全方式、初始数据源和扫描策略"),
                10,
                Theme::Muted()
            ),
            0
        );

        root->Add(
            header,
            0,
            wxEXPAND | wxALL,
            24
        );

        book_ = new wxSimplebook(this, wxID_ANY);
        Theme::Apply(book_, Theme::Window());

        BuildBasicPage();
        BuildSecurityPage();
        BuildSourcePage();
        BuildPolicyPage();
        BuildConfirmationPage();

        root->Add(
            book_,
            1,
            wxEXPAND | wxLEFT | wxRIGHT,
            24
        );

        root->Add(
            new wxStaticLine(this),
            0,
            wxEXPAND | wxTOP,
            18
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);

        back_ = Button(this, U("上一步"));
        next_ = Button(this, U("下一步"), true);
        auto* cancel = Button(this, U("取消"));

        cancel->SetId(wxID_CANCEL);

        actions->Add(back_, 0, wxRIGHT, 10);
        actions->AddStretchSpacer();
        actions->Add(cancel, 0, wxRIGHT, 10);
        actions->Add(next_, 0);

        root->Add(
            actions,
            0,
            wxEXPAND | wxALL,
            20
        );

        SetSizer(root);

        back_->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                if (step_ > 0)
                {
                    --step_;
                    UpdateStep();
                }
            }
        );

        next_->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                Continue();
            }
        );

        UpdateStep();
        CentreOnParent();
    }

    const wxString& CreatedDirectory() const
    {
        return createdDirectory_;
    }

private:
    wxPanel* NewPage()
    {
        auto* page = new wxPanel(book_, wxID_ANY);
        Theme::Apply(page, Theme::Surface());
        book_->AddPage(page, wxEmptyString, false);
        return page;
    }

    void AddField(
        wxPanel* page,
        wxBoxSizer* layout,
        const wxString& label,
        wxWindow* control
    )
    {
        layout->Add(
            Text(
                page,
                label,
                9,
                Theme::Muted(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        layout->Add(
            control,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            22
        );
    }

    void BuildBasicPage()
    {
        auto* page = NewPage();
        auto* layout = new wxBoxSizer(wxVERTICAL);

        layout->Add(
            Text(
                page,
                U("工作区基本信息"),
                17,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            22
        );

        name_ = Input(page);
        name_->SetHint(U("例如：先锋计划"));

        directory_ = Input(page);
        directory_->SetHint(U("工作区目录"));

        language_ = new wxChoice(page, wxID_ANY);
        language_->Append(U("简体中文"));
        language_->Append(U("English"));
        language_->SetSelection(0);

        timezone_ = new wxChoice(page, wxID_ANY);
        timezone_->Append(U("Asia/Shanghai  UTC+08:00"));
        timezone_->Append(U("UTC"));
        timezone_->SetSelection(0);

        description_ = Input(
            page,
            wxEmptyString,
            wxTE_MULTILINE
        );
        description_->SetMinSize(wxSize(-1, 90));

        AddField(page, layout, U("工作区名称 *"), name_);
        AddField(page, layout, U("保存位置 *"), directory_);
        AddField(page, layout, U("默认语言"), language_);
        AddField(page, layout, U("默认时区"), timezone_);
        AddField(page, layout, U("项目说明"), description_);

        page->SetSizer(layout);
    }

    void BuildSecurityPage()
    {
        auto* page = NewPage();
        auto* layout = new wxBoxSizer(wxVERTICAL);

        layout->Add(
            Text(
                page,
                U("安全设置"),
                17,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            22
        );

        encrypted_ = new wxCheckBox(
            page,
            wxID_ANY,
            U("使用 SQLCipher 加密工作区数据库")
        );

        encrypted_->SetForegroundColour(Theme::Text());

        const auto capabilities =
            DatabaseSecurity::Diagnose();

        encrypted_->Enable(
            capabilities.sqlCipherAvailable
        );

        layout->Add(
            encrypted_,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            22
        );

        wxString status =
            capabilities.sqlCipherAvailable
                ? U("SQLCipher 可用。随机数据库密钥将由操作系统凭据保护。")
                : U("当前 SQLite 未启用 SQLCipher，只能创建普通工作区。");

        layout->Add(
            Text(
                page,
                status,
                10,
                capabilities.sqlCipherAvailable
                    ? Theme::Green()
                    : Theme::Yellow()
            ),
            0,
            wxALL,
            22
        );

        layout->Add(
            Text(
                page,
                U(
                    "密钥不会以明文写入数据库。Windows 使用 DPAPI；"
                    "其他平台使用权限受限的本地密钥文件。"
                ),
                9,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT,
            22
        );

        page->SetSizer(layout);
    }

    void BuildSourcePage()
    {
        auto* page = NewPage();
        auto* layout = new wxBoxSizer(wxVERTICAL);

        layout->Add(
            Text(
                page,
                U("初始数据源"),
                17,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            22
        );

        sourceName_ = Input(page);
        sourceName_->SetValue(U("项目文档"));

        sourcePath_ = Input(page);
        sourcePath_->SetHint(
            U("可留空，稍后在数据源管理中添加")
        );

        AddField(
            page,
            layout,
            U("数据源名称"),
            sourceName_
        );

        AddField(
            page,
            layout,
            U("本地目录"),
            sourcePath_
        );

        layout->Add(
            Text(
                page,
                U("应用只读取数据源，不会移动或修改源文件。"),
                9,
                Theme::Blue()
            ),
            0,
            wxALL,
            22
        );

        page->SetSizer(layout);
    }

    void BuildPolicyPage()
    {
        auto* page = NewPage();
        auto* layout = new wxBoxSizer(wxVERTICAL);

        layout->Add(
            Text(
                page,
                U("扫描策略"),
                17,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            22
        );

        recursive_ = new wxCheckBox(
            page,
            wxID_ANY,
            U("扫描所有子目录")
        );

        initialScan_ = new wxCheckBox(
            page,
            wxID_ANY,
            U("创建后加入首次扫描任务")
        );

        recursive_->SetValue(true);
        initialScan_->SetValue(true);

        for (auto* value : {
                 recursive_,
                 initialScan_})
        {
            value->SetForegroundColour(Theme::Text());

            layout->Add(
                value,
                0,
                wxLEFT | wxRIGHT | wxTOP,
                22
            );
        }

        layout->Add(
            Text(
                page,
                U(
                    "首次扫描在主窗口打开后由后台服务执行，"
                    "不会阻塞工作区创建。"
                ),
                9,
                Theme::Muted()
            ),
            0,
            wxALL,
            22
        );

        page->SetSizer(layout);
    }

    void BuildConfirmationPage()
    {
        auto* page = NewPage();
        auto* layout = new wxBoxSizer(wxVERTICAL);

        layout->Add(
            Text(
                page,
                U("确认创建"),
                17,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            22
        );

        summary_ = Text(
            page,
            wxEmptyString,
            11,
            Theme::Text()
        );

        layout->Add(
            summary_,
            0,
            wxEXPAND | wxALL,
            22
        );

        page->SetSizer(layout);
    }

    bool ValidateStep()
    {
        if (step_ != 0)
        {
            return true;
        }

        wxString name = name_->GetValue();
        wxString directory = directory_->GetValue();
        name.Trim(true).Trim(false);
        directory.Trim(true).Trim(false);

        if (name.empty() || directory.empty())
        {
            wxMessageBox(
                U("工作区名称和保存位置不能为空。"),
                U("创建工作区"),
                wxOK | wxICON_ERROR,
                this
            );
            return false;
        }

        return true;
    }

    void UpdateStep()
    {
        book_->SetSelection(step_);

        stepLabel_->SetLabel(
            wxString::Format(
                U("P02 · 步骤 %d / 5"),
                step_ + 1
            )
        );

        static const wxString titles[] = {
            U("基本信息"),
            U("安全设置"),
            U("初始数据源"),
            U("扫描策略"),
            U("确认创建")
        };

        title_->SetLabel(
            U("创建工作区 · ") + titles[step_]
        );

        back_->Enable(step_ > 0);
        next_->SetLabel(
            step_ == 4
                ? U("创建工作区")
                : U("下一步")
        );

        if (step_ == 4)
        {
            summary_->SetLabel(
                U("名称：") + name_->GetValue() +
                U("\n位置：") + directory_->GetValue() +
                U("\n安全：") +
                    (
                        encrypted_->GetValue()
                            ? U("SQLCipher 加密")
                            : U("普通本地工作区")
                    ) +
                U("\n初始数据源：") +
                    (
                        sourcePath_->GetValue().empty()
                            ? U("不添加")
                            : sourcePath_->GetValue()
                    ) +
                U("\n首次扫描：") +
                    (
                        initialScan_->GetValue()
                            ? U("加入后台任务")
                            : U("暂不扫描")
                    )
            );
        }

        Layout();
    }

    void Continue()
    {
        if (!ValidateStep())
        {
            return;
        }

        if (step_ < 4)
        {
            ++step_;
            UpdateStep();
            return;
        }

        Create();
    }

    void Create()
    {
        next_->Enable(false);
        back_->Enable(false);

        auto& workspace =
            WorkspaceService::Instance();

        auto status = workspace.CreateWorkspace(
            Utf8(directory_->GetValue()),
            encrypted_->GetValue()
        );

        if (status.success)
        {
            Database& database =
                workspace.GetDatabase();

            status = SaveMeta(
                database,
                "workspace.name",
                Utf8(name_->GetValue())
            );

            if (status.success)
            {
                status = SaveMeta(
                    database,
                    "workspace.description",
                    Utf8(description_->GetValue())
                );
            }

            if (status.success)
            {
                status = SaveMeta(
                    database,
                    "workspace.language",
                    language_->GetSelection() == 1
                        ? "en"
                        : "zh-CN"
                );
            }

            if (status.success)
            {
                status = SaveMeta(
                    database,
                    "workspace.timezone",
                    timezone_->GetSelection() == 1
                        ? "UTC"
                        : "Asia/Shanghai"
                );
            }
        }

        if (status.success &&
            !sourcePath_->GetValue().empty())
        {
            std::error_code error;
            const auto source =
                std::filesystem::absolute(
                    std::filesystem::u8path(
                        Utf8(sourcePath_->GetValue())
                    )
                ).lexically_normal();

            if (!std::filesystem::is_directory(
                    source,
                    error
                ) ||
                error)
            {
                status = StorageStatus::Error(
                    SQLITE_CANTOPEN,
                    "初始数据源目录不存在或不可访问"
                );
            }
            else
            {
                DataSourceRecord record;
                record.id =
                    "SOURCE-" +
                    ContentHasher::Sha256Text(
                        source.u8string()
                    ).substr(0, 24);
                record.name =
                    Utf8(sourceName_->GetValue());
                record.sourceType = "folder";
                record.rootPath = source.u8string();
                record.enabled = true;

                status = workspace.DataSources().Save(
                    record,
                    "workspace-wizard"
                );

                if (status.success &&
                    initialScan_->GetValue())
                {
                    JobRecord job;
                    job.id =
                        "JOB-SCAN-" +
                        ContentHasher::Sha256Text(
                            record.id +
                            std::to_string(
                                std::chrono::system_clock::now()
                                    .time_since_epoch()
                                    .count()
                            )
                        ).substr(0, 24);
                    job.jobType = "scan_source";
                    job.state = "queued";
                    job.payload =
                        "{\"source_id\":\"" +
                        record.id +
                        "\"}";

                    status = JobRepository(
                        workspace.GetDatabase()
                    ).Enqueue(
                        job,
                        "workspace-wizard"
                    );
                }
            }
        }

        if (!status.success)
        {
            workspace.Shutdown();

            wxMessageBox(
                Wx(status.message),
                U("创建工作区失败"),
                wxOK | wxICON_ERROR,
                this
            );

            next_->Enable(true);
            back_->Enable(true);
            return;
        }

        createdDirectory_ =
            Wx(workspace.WorkspaceDirectory());

        EndModal(wxID_OK);
    }

    wxSimplebook* book_ = nullptr;
    wxStaticText* stepLabel_ = nullptr;
    wxStaticText* title_ = nullptr;
    wxStaticText* summary_ = nullptr;
    wxButton* back_ = nullptr;
    wxButton* next_ = nullptr;

    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* directory_ = nullptr;
    wxChoice* language_ = nullptr;
    wxChoice* timezone_ = nullptr;
    wxTextCtrl* description_ = nullptr;

    wxCheckBox* encrypted_ = nullptr;
    wxTextCtrl* sourceName_ = nullptr;
    wxTextCtrl* sourcePath_ = nullptr;
    wxCheckBox* recursive_ = nullptr;
    wxCheckBox* initialScan_ = nullptr;

    int step_ = 0;
    wxString createdDirectory_;
};

}

StartCenterDialog::StartCenterDialog(
    wxWindow* parent
)
    : wxDialog(
        parent,
        wxID_ANY,
        U("续证 Continuum · 启动中心"),
        wxDefaultPosition,
        wxSize(1180, 760),
        wxDEFAULT_DIALOG_STYLE |
            wxRESIZE_BORDER
    )
{
    Theme::Apply(this, Theme::Window());
    SetMinSize(wxSize(920, 620));
    BuildInterface();
    RefreshRecent();
    Centre();
}

const wxString&
StartCenterDialog::OpenedWorkspace() const
{
    return openedWorkspace_;
}

void StartCenterDialog::BuildInterface()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* heading = new wxBoxSizer(wxVERTICAL);
    heading->Add(
        Text(
            this,
            U("P01"),
            9,
            Theme::Blue(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxBOTTOM,
        6
    );
    heading->Add(
        Text(
            this,
            U("启动中心"),
            24,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxBOTTOM,
        6
    );
    heading->Add(
        Text(
            this,
            U("创建、打开、导入或恢复一个本地项目工作区"),
            10,
            Theme::Muted()
        ),
        0
    );

    root->Add(
        heading,
        0,
        wxEXPAND | wxALL,
        26
    );

    auto* body = new wxBoxSizer(wxHORIZONTAL);

    auto* actionsPanel =
        new wxPanel(this, wxID_ANY);
    Theme::Apply(actionsPanel, Theme::Surface());

    auto* actions =
        new wxBoxSizer(wxVERTICAL);

    actions->Add(
        Text(
            actionsPanel,
            U("开始使用"),
            17,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxALL,
        20
    );

    auto* create =
        Button(actionsPanel, U("＋  新建工作区"), true);
    auto* open =
        Button(actionsPanel, U("↗  打开工作区"));
    auto* importPackage =
        Button(actionsPanel, U("⇩  导入工作区包"));
    auto* restore =
        Button(actionsPanel, U("↺  从备份恢复"));

    for (auto* value : {
             create,
             open,
             importPackage,
             restore})
    {
        actions->Add(
            value,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            20
        );
    }

    actions->AddStretchSpacer();

    actions->Add(
        Text(
            actionsPanel,
            U("工作区数据完全保存在本机。"),
            9,
            Theme::Muted()
        ),
        0,
        wxALL,
        20
    );

    actionsPanel->SetSizer(actions);
    actionsPanel->SetMinSize(wxSize(350, -1));

    auto* recentPanel =
        new wxPanel(this, wxID_ANY);
    Theme::Apply(recentPanel, Theme::Surface());

    auto* recentLayout =
        new wxBoxSizer(wxVERTICAL);

    auto* recentHeader =
        new wxBoxSizer(wxHORIZONTAL);

    recentHeader->Add(
        Text(
            recentPanel,
            U("最近工作区"),
            17,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        1,
        wxALIGN_CENTER_VERTICAL
    );

    auto* browse =
        Button(recentPanel, U("浏览磁盘"));

    recentHeader->Add(browse, 0);

    recentLayout->Add(
        recentHeader,
        0,
        wxEXPAND | wxALL,
        20
    );

    recentList_ = new wxListBox(
        recentPanel,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        0,
        nullptr,
        wxLB_SINGLE | wxBORDER_NONE
    );

    recentList_->SetBackgroundColour(
        Theme::Input()
    );
    recentList_->SetForegroundColour(
        Theme::Text()
    );
    recentList_->SetFont(Theme::Font(10));

    recentLayout->Add(
        recentList_,
        1,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        20
    );

    auto* openRecent =
        Button(recentPanel, U("打开所选工作区"), true);

    recentLayout->Add(
        openRecent,
        0,
        wxALIGN_RIGHT | wxLEFT | wxRIGHT | wxBOTTOM,
        20
    );

    recentPanel->SetSizer(recentLayout);

    body->Add(
        actionsPanel,
        0,
        wxEXPAND | wxRIGHT,
        16
    );
    body->Add(recentPanel, 1, wxEXPAND);

    root->Add(
        body,
        1,
        wxEXPAND | wxLEFT | wxRIGHT,
        26
    );

    status_ = Text(
        this,
        U("请选择一个工作区。"),
        9,
        Theme::Muted()
    );

    root->Add(
        status_,
        0,
        wxEXPAND | wxALL,
        26
    );

    SetSizer(root);

    create->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            CreateWorkspace();
        }
    );

    open->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            OpenWorkspace();
        }
    );

    browse->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            OpenWorkspace();
        }
    );

    openRecent->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            OpenRecent();
        }
    );

    recentList_->Bind(
        wxEVT_LISTBOX_DCLICK,
        [this](wxCommandEvent&)
        {
            OpenRecent();
        }
    );

    restore->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            RestoreBackupCopy();
        }
    );

    importPackage->Bind(
        wxEVT_BUTTON,
        [this](wxCommandEvent&)
        {
            ImportWorkspacePackage();
        }
    );
}

void StartCenterDialog::RefreshRecent()
{
    recentList_->Clear();

    for (const auto& directory : ReadRecent())
    {
        const auto path =
            std::filesystem::u8path(directory);

        const std::string name =
            path.filename().u8string();

        recentList_->Append(
            Wx(
                (
                    name.empty()
                        ? directory
                        : name
                ) +
                "  ·  " +
                directory
            ),
            new wxStringClientData(
                Wx(directory)
            )
        );
    }

    if (recentList_->GetCount() > 0)
    {
        recentList_->SetSelection(0);
    }
}

void StartCenterDialog::AddRecent(
    const wxString& directory
)
{
    std::vector<std::string> values =
        ReadRecent();

    const std::string normalized =
        std::filesystem::absolute(
            std::filesystem::u8path(
                Utf8(directory)
            )
        ).lexically_normal().u8string();

    values.erase(
        std::remove(
            values.begin(),
            values.end(),
            normalized
        ),
        values.end()
    );

    values.insert(values.begin(), normalized);

    if (values.size() > 20)
    {
        values.resize(20);
    }

    WriteRecent(values);
}

bool StartCenterDialog::CompleteOpen(
    const wxString& directory
)
{
    status_->SetLabel(U("正在检查工作区……"));

    const auto status =
        WorkspaceService::Instance().OpenWorkspace(
            Utf8(directory)
        );

    if (!status.success)
    {
        status_->SetLabel(U("无法打开所选工作区。"));

        wxMessageBox(
            Wx(status.message),
            U("打开工作区失败"),
            wxOK | wxICON_ERROR,
            this
        );

        return false;
    }

    openedWorkspace_ =
        Wx(
            WorkspaceService::Instance().
                WorkspaceDirectory()
        );

    AddRecent(openedWorkspace_);
    EndModal(wxID_OK);
    return true;
}

void StartCenterDialog::CreateWorkspace()
{
    WorkspaceWizard wizard(this);

    if (wizard.ShowModal() != wxID_OK)
    {
        return;
    }

    openedWorkspace_ =
        wizard.CreatedDirectory();

    AddRecent(openedWorkspace_);
    EndModal(wxID_OK);
}

void StartCenterDialog::OpenWorkspace()
{
    wxDirDialog dialog(
        this,
        U("选择包含 continuum.db 的工作区目录"),
        wxEmptyString,
        wxDD_DEFAULT_STYLE |
            wxDD_DIR_MUST_EXIST
    );

    if (dialog.ShowModal() == wxID_OK)
    {
        CompleteOpen(dialog.GetPath());
    }
}

void StartCenterDialog::OpenRecent()
{
    const int selection =
        recentList_->GetSelection();

    if (selection == wxNOT_FOUND)
    {
        wxMessageBox(
            U("请先选择最近工作区。"),
            U("启动中心"),
            wxOK | wxICON_INFORMATION,
            this
        );
        return;
    }

    auto* data =
        dynamic_cast<wxStringClientData*>(
            recentList_->GetClientObject(
                selection
            )
        );

    if (data != nullptr)
    {
        CompleteOpen(data->GetData());
    }
}

void StartCenterDialog::ImportWorkspacePackage()
{
    /*
     * 当前仓库没有独立交换包格式。这里仅接受已经展开且包含
     * continuum.db 的工作区目录，避免伪造不兼容的导入协议。
     */
    OpenWorkspace();
}

void StartCenterDialog::RestoreBackupCopy()
{
    wxDirDialog sourceDialog(
        this,
        U("选择包含 continuum.db 的备份目录"),
        wxEmptyString,
        wxDD_DEFAULT_STYLE |
            wxDD_DIR_MUST_EXIST
    );

    if (sourceDialog.ShowModal() != wxID_OK)
    {
        return;
    }

    const auto sourceDirectory =
        std::filesystem::u8path(
            Utf8(sourceDialog.GetPath())
        );

    const auto sourceDatabase =
        sourceDirectory / "continuum.db";

    std::error_code error;

    if (!std::filesystem::is_regular_file(
            sourceDatabase,
            error
        ) ||
        error)
    {
        wxMessageBox(
            U("所选目录不包含备份数据库 continuum.db。"),
            U("从备份恢复"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    wxDirDialog targetDialog(
        this,
        U("选择用于创建恢复工作区的空目录"),
        wxEmptyString,
        wxDD_DEFAULT_STYLE |
            wxDD_NEW_DIR_BUTTON
    );

    if (targetDialog.ShowModal() != wxID_OK)
    {
        return;
    }

    const auto targetDirectory =
        std::filesystem::u8path(
            Utf8(targetDialog.GetPath())
        );

    const auto targetDatabase =
        targetDirectory / "continuum.db";

    if (std::filesystem::exists(
            targetDatabase,
            error
        ) &&
        !error)
    {
        wxMessageBox(
            U("目标目录已经包含 continuum.db。"),
            U("从备份恢复"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    std::filesystem::create_directories(
        targetDirectory,
        error
    );

    if (!error)
    {
        std::filesystem::copy_file(
            sourceDatabase,
            targetDatabase,
            std::filesystem::copy_options::
                overwrite_existing,
            error
        );
    }

    if (error)
    {
        wxMessageBox(
            U("无法复制备份数据库：\n") +
                Wx(error.message()),
            U("从备份恢复"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    if (!CompleteOpen(
            Wx(targetDirectory.u8string())
        ))
    {
        std::filesystem::remove(
            targetDatabase,
            error
        );
    }
}

}
