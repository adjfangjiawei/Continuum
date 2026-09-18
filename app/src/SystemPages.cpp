#include "SystemPages.h"

#include "SecurityService.h"
#include "Theme.h"
#include "WorkspaceService.h"

#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/listctrl.h>
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

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle
)
{
    root->Add(
        MakeText(parent, code, 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
    root->Add(
        MakeText(parent, title, 22, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
    root->Add(
        MakeText(parent, subtitle, 10, Theme::Muted()),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
}

wxString SecurityReport()
{
    const auto capabilities = DatabaseSecurity::Diagnose();

    wxString report;

    report += U("SQLCipher 可用：");
    report += capabilities.sqlCipherAvailable ? U("是") : U("否");
    report += U("\nSQLCipher 版本：");
    report += Wx(capabilities.sqlCipherVersion);
    report += U("\n操作系统密钥保护可用：");
    report += capabilities.operatingSystemProtectionAvailable
        ? U("是")
        : U("否");
    report += U("\n受限权限文件回退：");
    report += capabilities.restrictedFileFallback
        ? U("可用")
        : U("不可用");
    report += U("\n密钥保护提供者：");
    report += Wx(capabilities.keyProtectionProvider);

    report += U("\n\n诊断警告：\n");

    if (capabilities.warnings.empty())
    {
        report += U("无");
    }
    else
    {
        for (const auto& warning : capabilities.warnings)
        {
            report += U("• ");
            report += Wx(warning);
            report += U("\n");
        }
    }

    return report;
}

class SecurityPage final : public wxPanel
{
public:
    explicit SecurityPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M09"),
            U("安全与加密"),
            U("显示当前运行环境中实际检测到的数据库加密和密钥保护能力")
        );

        auto* report = new wxTextCtrl(
            this,
            wxID_ANY,
            SecurityReport(),
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );

        report->SetBackgroundColour(Theme::Input());
        report->SetForegroundColour(Theme::Text());
        report->SetFont(Theme::Font(10));

        root->Add(report, 1, wxEXPAND | wxALL, 26);

        auto* warning = MakeText(
            this,
            U("安全策略由工作区设置统一管理；数据库加密能力和密钥保护状态以上方实际检测结果为准。"),
            9,
            Theme::Yellow()
        );
        warning->Wrap(900);

        root->Add(
            warning,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            26
        );

        SetSizer(root);
    }
};

class AuditLogPage final : public wxPanel
{
public:
    explicit AuditLogPage(wxWindow* parent)
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
            U("M10"),
            U("审计日志"),
            U("读取工作区数据库中的真实审计事件")
        );

        auto* refresh = new wxButton(
            this,
            wxID_ANY,
            U("刷新"),
            wxDefaultPosition,
            wxSize(100, 38)
        );

        root->Add(
            refresh,
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
            wxLC_REPORT | wxLC_SINGLE_SEL | wxBORDER_NONE
        );

        list_->SetBackgroundColour(Theme::Surface());
        list_->SetForegroundColour(Theme::Text());
        list_->SetFont(Theme::Font(9));

        list_->InsertColumn(0, U("序号"), wxLIST_FORMAT_RIGHT, 85);
        list_->InsertColumn(1, U("时间"), wxLIST_FORMAT_LEFT, 180);
        list_->InsertColumn(2, U("操作者"), wxLIST_FORMAT_LEFT, 130);
        list_->InsertColumn(3, U("类别"), wxLIST_FORMAT_LEFT, 120);
        list_->InsertColumn(4, U("操作"), wxLIST_FORMAT_LEFT, 140);
        list_->InsertColumn(5, U("对象编号"), wxLIST_FORMAT_LEFT, 220);

        detail_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxSize(460, -1),
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );

        detail_->SetBackgroundColour(Theme::Input());
        detail_->SetForegroundColour(Theme::Text());
        detail_->SetFont(Theme::Font(9));

        body->Add(list_, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail_, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);

        status_ = MakeText(this, wxEmptyString, 9, Theme::Muted());

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

        RefreshData();
    }

private:
    void RefreshData()
    {
        records_.clear();
        list_->DeleteAllItems();
        detail_->ChangeValue(wxEmptyString);

        auto& workspace = WorkspaceService::Instance();

        if (!workspace.IsInitialized())
        {
            status_->SetLabel(U("工作区尚未初始化。"));
            return;
        }

        records_ = workspace.Audit().Recent(2000);

        list_->Freeze();

        for (const auto& event : records_)
        {
            const long row = list_->InsertItem(
                list_->GetItemCount(),
                wxString::Format(
                    "%lld",
                    static_cast<long long>(event.sequence)
                )
            );

            list_->SetItem(row, 1, Wx(event.occurredAt));
            list_->SetItem(row, 2, Wx(event.actor));
            list_->SetItem(row, 3, Wx(event.category));
            list_->SetItem(row, 4, Wx(event.action));
            list_->SetItem(row, 5, Wx(event.objectId));
        }

        list_->Thaw();

        status_->SetLabel(
            U("真实审计事件：") +
            wxString::Format(
                "%d",
                static_cast<int>(records_.size())
            ) +
            U("。")
        );

        if (!records_.empty())
        {
            list_->SetItemState(
                0,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED
            );
            ShowDetail(0);
        }
    }

    void ShowDetail(long row)
    {
        if (row < 0 ||
            static_cast<std::size_t>(row) >= records_.size())
        {
            return;
        }

        const auto& event =
            records_[static_cast<std::size_t>(row)];

        detail_->ChangeValue(
            U("事件编号：") + Wx(event.eventId) +
            U("\n序号：") +
            wxString::Format(
                "%lld",
                static_cast<long long>(event.sequence)
            ) +
            U("\n发生时间：") + Wx(event.occurredAt) +
            U("\n操作者：") + Wx(event.actor) +
            U("\n类别：") + Wx(event.category) +
            U("\n操作：") + Wx(event.action) +
            U("\n对象类型：") + Wx(event.objectType) +
            U("\n对象编号：") + Wx(event.objectId) +
            U("\n\n载荷：\n") + Wx(event.payload) +
            U("\n\n前序哈希：\n") + Wx(event.previousHash) +
            U("\n\n事件哈希：\n") + Wx(event.eventHash)
        );
    }

    wxListCtrl* list_;
    wxTextCtrl* detail_;
    wxStaticText* status_;
    std::vector<AuditEventRecord> records_;
};

class DiagnosticsPage final : public wxPanel
{
public:
    explicit DiagnosticsPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY),
          output_(nullptr)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M11"),
            U("诊断中心"),
            U("实际运行数据库完整性检查和运行环境安全能力检查")
        );

        auto* run = new wxButton(
            this,
            wxID_ANY,
            U("运行诊断"),
            wxDefaultPosition,
            wxSize(130, 38)
        );

        root->Add(
            run,
            0,
            wxLEFT | wxRIGHT | wxTOP,
            26
        );

        output_ = new wxTextCtrl(
            this,
            wxID_ANY,
            wxEmptyString,
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );

        output_->SetBackgroundColour(Theme::Input());
        output_->SetForegroundColour(Theme::Text());
        output_->SetFont(Theme::Font(10));

        root->Add(
            output_,
            1,
            wxEXPAND | wxALL,
            26
        );

        SetSizer(root);

        run->Bind(
            wxEVT_BUTTON,
            [this](wxCommandEvent&)
            {
                RunDiagnostics();
            }
        );

        RunDiagnostics();
    }

private:
    void RunDiagnostics()
    {
        wxString report;

        auto& workspace = WorkspaceService::Instance();

        report += U("工作区初始化：");
        report += workspace.IsInitialized()
            ? U("是")
            : U("否");
        report += U("\n");

        if (workspace.IsInitialized())
        {
            const auto integrity =
                workspace.GetDatabase().CheckIntegrity();

            report += U("数据库 quick_check：");
            report += integrity.success
                ? U("通过")
                : U("失败");
            report += U("\n");

            if (!integrity.success)
            {
                report += U("错误：");
                report += Wx(integrity.message);
                report += U("\n");
            }

            report += U("数据库路径：");
            report += Wx(workspace.DatabasePath());
            report += U("\n");
        }

        report += U("\n");
        report += SecurityReport();


        output_->ChangeValue(report);
    }

    wxTextCtrl* output_;
};

class AboutPage final : public wxPanel
{
public:
    explicit AboutPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeading(
            this,
            root,
            U("M12"),
            U("关于续证"),
            U("离线优先的项目证据与决策连续性工作台")
        );

        auto* description = MakeText(
            this,
            U(
                "续证用于管理本地工作区中的来源文件、证据、"
                "结构化业务对象、关系、冲突、交接报告和审计记录。\n\n"
                "所有项目数据保存在用户选择的本地工作区中，"
                "搜索、审查、恢复、备份和导出均围绕当前工作区执行。"
            ),
            11,
            Theme::Text()
        );

        description->Wrap(850);

        root->Add(
            description,
            0,
            wxALL,
            26
        );

        root->AddStretchSpacer();
        SetSizer(root);
    }
};

}

wxWindow* CreateSystemPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("M09"))
    {
        return new SecurityPage(parent);
    }

    if (pageCode == U("M10"))
    {
        return new AuditLogPage(parent);
    }

    if (pageCode == U("M11"))
    {
        return new DiagnosticsPage(parent);
    }

    if (pageCode == U("M12"))
    {
        return new AboutPage(parent);
    }

    return nullptr;
}

}
