#include "MainFrame.h"

#include "Theme.h"

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace continuum
{
namespace
{

wxStaticText* MakeText(
    wxWindow* parent,
    const wxString& value,
    int size,
    const wxColour& color,
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* label = new wxStaticText(parent, wxID_ANY, value);
    label->SetForegroundColour(color);
    label->SetFont(Theme::Font(size, weight));
    return label;
}

std::vector<PageDescriptor> BusinessPages()
{
    return {
        {U("P01"), U("项目概览"), U("查看项目当前健康状态与关键变化"), U("核心工作区")},
        {U("P02"), U("审查收件箱"), U("处理等待人工确认的提取结果"), U("核心工作区")},
        {U("P03"), U("资料库"), U("浏览文件、内容块、版本和证据"), U("核心工作区")},
        {U("P04"), U("文件阅读器"), U("阅读文档并创建可追溯证据"), U("核心工作区")},
        {U("P05"), U("证据检查器"), U("验证来源锚点、内容指纹和引用"), U("核心工作区")},
        {U("P06"), U("项目台账"), U("管理事实、决策、承诺、风险与问题"), U("核心工作区")},
        {U("P07"), U("对象详情"), U("查看结构化对象、证据、关系与历史"), U("核心工作区")},
        {U("P08"), U("决策详情"), U("查看决策依据、替代方案和影响"), U("核心工作区")},
        {U("P09"), U("承诺详情"), U("管理负责人、截止日期和完成证据"), U("核心工作区")},
        {U("P10"), U("冲突中心"), U("比较相反主张并记录人工裁决"), U("核心工作区")},
        {U("P11"), U("变化审查"), U("比较文件版本并分析业务影响"), U("核心工作区")},
        {U("P12"), U("项目时间线"), U("按现实发生时间查看项目事件"), U("核心工作区")},
        {U("P13"), U("历史时间切片"), U("重建指定时刻可知的项目状态"), U("核心工作区")},
        {U("P14"), U("关系浏览器"), U("调查对象之间的支持和影响关系"), U("核心工作区")},
        {U("P15"), U("交接胶囊"), U("创建可验证的项目交接报告"), U("核心工作区")},
        {U("P16"), U("导出验证"), U("检查引用、附件和脱敏策略"), U("核心工作区")},
        {U("P17"), U("回收站"), U("恢复或清理已软删除对象"), U("核心工作区")}
    };
}

std::vector<PageDescriptor> ManagementPages()
{
    return {
        {U("M01"), U("数据源管理"), U("管理项目文件来源与监视状态"), U("管理中心")},
        {U("M02"), U("添加数据源"), U("配置导入、监视、OCR 和版本策略"), U("管理中心")},
        {U("M03"), U("后台任务"), U("查看扫描、解析、OCR 和索引任务"), U("管理中心")},
        {U("M04"), U("快照与备份"), U("创建、验证并恢复工作区备份"), U("管理中心")},
        {U("M05"), U("标签与实体"), U("规范标签、人员、组织、系统和术语"), U("管理中心")},
        {U("M06"), U("保存的查询"), U("管理可复用调查条件和动态视图"), U("管理中心")},
        {U("M07"), U("规则管理"), U("配置提取、冲突和文件过滤规则"), U("管理中心")},
        {U("M08"), U("工作区设置"), U("配置当前工作区行为和安全策略"), U("管理中心")},
        {U("M09"), U("全局设置"), U("配置界面、语言、性能和快捷键"), U("管理中心")},
        {U("M10"), U("诊断中心"), U("检查日志、数据库、索引和系统环境"), U("管理中心")},
        {U("M11"), U("完整性检查"), U("验证数据库、证据锚点和内容指纹"), U("管理中心")},
        {U("M12"), U("关于与许可"), U("查看版本信息与第三方许可证"), U("管理中心")}
    };
}

}

MainFrame::MainFrame()
    : wxFrame(
        nullptr,
        wxID_ANY,
        U("续证 Continuum"),
        wxDefaultPosition,
        wxSize(1600, 1000),
        wxDEFAULT_FRAME_STYLE
    )
{
    SetMinSize(wxSize(1180, 760));
    SetBackgroundColour(Theme::Window());
    SetForegroundColour(Theme::Text());

    BuildInterface();
    BindKeyboardShortcuts();
    Centre();

    if (!navigation_.empty())
    {
        NavigateTo(0);
    }
}

void MainFrame::BuildInterface()
{
    auto* root = new wxBoxSizer(wxVERTICAL);

    root->Add(BuildHeader(this), 0, wxEXPAND);

    auto* body = new wxBoxSizer(wxHORIZONTAL);
    body->Add(BuildSidebar(this), 0, wxEXPAND);

    pageBook_ = new wxSimplebook(this, wxID_ANY);
    Theme::Apply(pageBook_, Theme::Window());
    body->Add(pageBook_, 1, wxEXPAND);

    root->Add(body, 1, wxEXPAND);
    root->Add(BuildStatusBar(this), 0, wxEXPAND);

    SetSizer(root);
    Layout();
}

wxPanel* MainFrame::BuildHeader(wxWindow* parent)
{
    auto* header = new wxPanel(parent, wxID_ANY);
    Theme::Apply(header, Theme::Top());
    header->SetMinSize(wxSize(-1, 64));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* brand = MakeText(
        header,
        U("续证  CONTINUUM"),
        12,
        Theme::Text(),
        wxFONTWEIGHT_BOLD
    );

    workspaceTitle_ = MakeText(
        header,
        U("先锋计划"),
        10,
        Theme::Muted(),
        wxFONTWEIGHT_SEMIBOLD
    );

    globalSearch_ = new wxTextCtrl(
        header,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(430, 38),
        wxBORDER_NONE | wxTE_PROCESS_ENTER
    );
    globalSearch_->SetBackgroundColour(Theme::Input());
    globalSearch_->SetForegroundColour(Theme::Text());
    globalSearch_->SetFont(Theme::Font(10));
    globalSearch_->SetHint(U("搜索文档、证据、决策或输入命令"));

    auto* commandButton = new wxButton(
        header,
        wxID_ANY,
        U("Ctrl+K"),
        wxDefaultPosition,
        wxSize(76, 36),
        wxBORDER_NONE
    );
    commandButton->SetBackgroundColour(Theme::Surface2());
    commandButton->SetForegroundColour(Theme::Muted());
    commandButton->SetFont(Theme::Font(9, wxFONTWEIGHT_BOLD));
    commandButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&)
    {
        ShowCommandPalette();
    });

    layout->Add(brand, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 26);
    layout->Add(workspaceTitle_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 38);
    layout->AddStretchSpacer();
    layout->Add(globalSearch_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    layout->Add(commandButton, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 24);

    header->SetSizer(layout);
    return header;
}

wxPanel* MainFrame::BuildSidebar(wxWindow* parent)
{
    auto* container = new wxPanel(parent, wxID_ANY);
    Theme::Apply(container, Theme::Sidebar());
    container->SetMinSize(wxSize(248, -1));

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* workspace = new wxPanel(container, wxID_ANY);
    Theme::Apply(workspace, Theme::Sidebar());

    auto* workspaceSizer = new wxBoxSizer(wxVERTICAL);
    workspaceSizer->Add(
        MakeText(
            workspace,
            U("CONTINUUM"),
            9,
            Theme::Blue(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        24
    );
    workspaceSizer->Add(
        MakeText(
            workspace,
            U("项目记忆取证台"),
            9,
            Theme::Faint()
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
        24
    );
    workspace->SetSizer(workspaceSizer);

    root->Add(workspace, 0, wxEXPAND);

    auto* scroll = new wxScrolledWindow(
        container,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxVSCROLL | wxBORDER_NONE
    );
    Theme::Apply(scroll, Theme::Sidebar());
    scroll->SetScrollRate(0, 14);

    auto* navigationSizer = new wxBoxSizer(wxVERTICAL);

    AddNavigationSection(
        scroll,
        navigationSizer,
        U("核心工作区"),
        BusinessPages()
    );
    AddNavigationSection(
        scroll,
        navigationSizer,
        U("管理中心"),
        ManagementPages()
    );

    navigationSizer->AddSpacer(20);
    scroll->SetSizer(navigationSizer);

    root->Add(scroll, 1, wxEXPAND);

    auto* footer = new wxPanel(container, wxID_ANY);
    Theme::Apply(footer, Theme::Sidebar());

    auto* footerSizer = new wxBoxSizer(wxVERTICAL);
    footerSizer->Add(
        MakeText(footer, U("先锋计划"), 10, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxTOP,
        24
    );
    footerSizer->Add(
        MakeText(footer, U("本地加密工作区"), 8, Theme::Muted()),
        0,
        wxLEFT | wxTOP,
        24
    );
    footerSizer->Add(
        MakeText(footer, U("●  索引正常"), 8, Theme::Green(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxTOP | wxBOTTOM,
        24
    );
    footer->SetSizer(footerSizer);

    root->Add(footer, 0, wxEXPAND);
    container->SetSizer(root);

    return container;
}

void MainFrame::AddNavigationSection(
    wxWindow* parent,
    wxBoxSizer* sizer,
    const wxString& title,
    const std::vector<PageDescriptor>& pages
)
{
    sizer->Add(
        MakeText(parent, title, 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
        18
    );

    for (const auto& page : pages)
    {
        AddNavigationEntry(parent, sizer, page);
    }
}

void MainFrame::AddNavigationEntry(
    wxWindow* parent,
    wxBoxSizer* sizer,
    const PageDescriptor& page
)
{
    const int controlId = wxWindow::NewControlId();

    auto* button = new wxButton(
        parent,
        controlId,
        page.code + U("   ") + page.title,
        wxDefaultPosition,
        wxSize(216, 38),
        wxBORDER_NONE | wxBU_LEFT
    );

    button->SetFont(Theme::Font(9, wxFONTWEIGHT_SEMIBOLD));
    button->SetBackgroundColour(Theme::Sidebar());
    button->SetForegroundColour(Theme::Muted());

    const std::size_t index = navigation_.size();
    navigation_.push_back({page, button, controlId});

    button->Bind(wxEVT_BUTTON, [this, index](wxCommandEvent&)
    {
        NavigateTo(index);
    });

    sizer->Add(button, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
}

wxPanel* MainFrame::BuildStatusBar(wxWindow* parent)
{
    auto* status = new wxPanel(parent, wxID_ANY);
    Theme::Apply(status, Theme::Top());
    status->SetMinSize(wxSize(-1, 30));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    layout->Add(
        MakeText(status, U("本地工作区"), 8, Theme::Muted()),
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        20
    );
    layout->Add(
        MakeText(status, U("●  数据库正常"), 8, Theme::Green()),
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        30
    );
    layout->Add(
        MakeText(status, U("后台任务 1"), 8, Theme::Muted()),
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        30
    );
    layout->AddStretchSpacer();
    layout->Add(
        MakeText(status, U("缩放 100%"), 8, Theme::Muted()),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        20
    );

    status->SetSizer(layout);
    return status;
}

void MainFrame::NavigateTo(std::size_t index)
{
    if (index >= navigation_.size() || pageBook_ == nullptr)
    {
        return;
    }

    selectedIndex_ = index;
    const auto& descriptor = navigation_[index].page;

    while (pageBook_->GetPageCount() <= index)
    {
        const std::size_t pageIndex = pageBook_->GetPageCount();
        pageBook_->AddPage(
            CreatePage(pageBook_, navigation_[pageIndex].page),
            navigation_[pageIndex].page.title,
            false
        );
    }

    pageBook_->SetSelection(index);
    workspaceTitle_->SetLabel(
        U("先锋计划  ·  ") + descriptor.code + U("  ") + descriptor.title
    );

    SetTitle(U("续证 Continuum · ") + descriptor.title);
    UpdateNavigationStyles();
    Layout();
}

void MainFrame::UpdateNavigationStyles()
{
    for (std::size_t index = 0; index < navigation_.size(); ++index)
    {
        const bool active = index == selectedIndex_;
        auto* button = navigation_[index].button;

        button->SetBackgroundColour(
            active ? Theme::Surface3() : Theme::Sidebar()
        );
        button->SetForegroundColour(
            active ? Theme::Text() : Theme::Muted()
        );
        button->SetFont(
            Theme::Font(
                9,
                active ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_SEMIBOLD
            )
        );
        button->Refresh();
    }
}

void MainFrame::ShowCommandPalette()
{
    wxDialog dialog(
        this,
        wxID_ANY,
        U("命令面板"),
        wxDefaultPosition,
        wxSize(720, 520),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
    );
    Theme::Apply(&dialog, Theme::Surface());

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* title = MakeText(
        &dialog,
        U("搜索页面和命令"),
        16,
        Theme::Text(),
        wxFONTWEIGHT_BOLD
    );

    auto* search = new wxTextCtrl(
        &dialog,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(-1, 42),
        wxBORDER_NONE
    );
    search->SetBackgroundColour(Theme::Input());
    search->SetForegroundColour(Theme::Text());
    search->SetFont(Theme::Font(11));
    search->SetHint(U("输入页面、对象或命令名称"));

    wxArrayString commands;
    commands.Add(U("创建决策"));
    commands.Add(U("创建证据并关联当前对象"));
    commands.Add(U("添加数据源"));
    commands.Add(U("运行完整性检查"));
    commands.Add(U("创建工作区快照"));
    commands.Add(U("导出交接胶囊"));
    commands.Add(U("打开冲突中心"));
    commands.Add(U("打开变化审查"));
    commands.Add(U("锁定工作区"));

    auto* list = new wxListBox(
        &dialog,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        commands,
        wxLB_SINGLE | wxBORDER_NONE
    );
    list->SetBackgroundColour(Theme::Surface2());
    list->SetForegroundColour(Theme::Text());
    list->SetFont(Theme::Font(10));
    list->SetSelection(0);

    auto* hint = MakeText(
        &dialog,
        U("↑↓ 选择 · Enter 执行 · Esc 关闭"),
        8,
        Theme::Muted()
    );

    root->Add(title, 0, wxALL, 22);
    root->Add(search, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 22);
    root->Add(list, 1, wxEXPAND | wxLEFT | wxRIGHT, 22);
    root->Add(hint, 0, wxALL, 22);

    dialog.SetSizer(root);
    search->SetFocus();

    list->Bind(wxEVT_LISTBOX_DCLICK, [&dialog](wxCommandEvent&)
    {
        dialog.EndModal(wxID_OK);
    });

    dialog.ShowModal();
}

void MainFrame::BindKeyboardShortcuts()
{
    auto* acceleratorEntries = new wxAcceleratorEntry[2];
    acceleratorEntries[0].Set(wxACCEL_CTRL, static_cast<int>(U("K")[0]), wxID_FIND);
    acceleratorEntries[1].Set(wxACCEL_CTRL, static_cast<int>(U("L")[0]), wxID_HIGHEST + 10);

    wxAcceleratorTable table(2, acceleratorEntries);
    SetAcceleratorTable(table);
    delete[] acceleratorEntries;

    Bind(wxEVT_MENU, [this](wxCommandEvent&)
    {
        ShowCommandPalette();
    }, wxID_FIND);

    Bind(wxEVT_MENU, [this](wxCommandEvent&)
    {
        if (globalSearch_ != nullptr)
        {
            globalSearch_->SetFocus();
        }
    }, wxID_HIGHEST + 10);
}

}
