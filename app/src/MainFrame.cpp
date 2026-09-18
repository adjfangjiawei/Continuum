#include "MainFrame.h"

#include "Theme.h"
#include "UiDataService.h"
#include "WorkspaceService.h"

#include <filesystem>
#include "WxRecordController.h"
#include "WxUiBinder.h"

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/display.h>
#include <wx/listbox.h>
#include <wx/listctrl.h>
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
    /*
     * 一级导航严格采用最终设计稿中的工作区导航结构。
     *
     * 文档阅读、对象详情、历史切片和导出验证属于带上下文的
     * 二级页面，必须从资料库、台账或交接流程携带记录编号进入，
     * 不能作为没有上下文的一级入口。
     */
    return {
        {U("P03"), U("项目概览"), U("当前状态、近期变化以及需要人工处理的事项"), U("核心工作区")},
        {U("P04"), U("审查收件箱"), U("确认自动发现的事实、决策、承诺、关系和冲突候选"), U("核心工作区")},
        {U("P05"), U("资料库"), U("浏览文件、内容块和证据"), U("核心工作区")},
        {U("P07"), U("全局搜索"), U("跨文档、证据和业务对象进行调查式查询"), U("核心工作区")},
        {U("P08"), U("项目台账"), U("统一管理事实、决策、承诺、风险、问题、假设和外部依赖"), U("核心工作区")},
        {U("P10"), U("冲突中心"), U("比较相反主张并记录人工裁决"), U("核心工作区")},
        {U("P11"), U("变化审查"), U("比较文件版本并分析业务影响"), U("核心工作区")},
        {U("P12"), U("时间线"), U("按现实发生时间查看项目事件"), U("核心工作区")},
        {U("P14"), U("关系浏览器"), U("调查对象之间的支持和影响关系"), U("核心工作区")},
        {U("P15"), U("交接胶囊"), U("创建可验证的项目交接报告"), U("核心工作区")},
        {U("P16"), U("导出验证"), U("验证引用、附件、脱敏和离线包完整性"), U("核心工作区")},
        {U("P17"), U("回收站"), U("恢复或彻底清理已删除内容"), U("核心工作区")}
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
        {U("M09"), U("安全与加密"), U("查看数据库加密和密钥保护能力"), U("管理中心")},
        {U("M10"), U("审计日志"), U("查看工作区中的真实审计事件"), U("管理中心")},
        {U("M11"), U("诊断中心"), U("运行数据库完整性和安全能力检查"), U("管理中心")},
        {U("M12"), U("关于续证"), U("查看应用说明和当前功能状态"), U("管理中心")}
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

    if (workspaceTitle_ != nullptr &&
        WorkspaceService::Instance().IsInitialized())
    {
        const auto directory =
            std::filesystem::u8path(
                WorkspaceService::Instance().
                    WorkspaceDirectory()
            );

        std::string name =
            directory.filename().u8string();

        if (name.empty())
        {
            name =
                WorkspaceService::Instance().
                    WorkspaceDirectory();
        }

        workspaceTitle_->SetLabel(
            wxString::FromUTF8(name.c_str())
        );
    }

    if (!navigation_.empty())
    {
        NavigateTo(0);
    }
}

MainFrame::~MainFrame()
{
    WxRecordController::Instance().Detach();
    WxUiBinder::Instance().Detach();
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
        U("本地工作区"),
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
    globalSearch_->SetHint(U("搜索已解析文档"));
    globalSearch_->Bind(
        wxEVT_TEXT_ENTER,
        [this](wxCommandEvent&)
        {
            RunGlobalSearch();
        }
    );

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
        MakeText(footer, U("本地工作区"), 10, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxTOP,
        24
    );
    auto* securityStatus = MakeText(
        footer,
        U("正在读取数据库安全状态"),
        8,
        Theme::Muted()
    );
    securityStatus->SetName("app.security_status");

    auto* indexStatus = MakeText(
        footer,
        U("正在读取索引状态"),
        8,
        Theme::Muted(),
        wxFONTWEIGHT_BOLD
    );
    indexStatus->SetName("app.index_status");

    footerSizer->Add(
        securityStatus,
        0,
        wxLEFT | wxTOP,
        24
    );
    footerSizer->Add(
        indexStatus,
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
    pages_.push_back(nullptr);

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

    auto* workspaceStatus = MakeText(
        status,
        U("本地工作区"),
        8,
        Theme::Muted()
    );
    workspaceStatus->SetName("app.workspace_status");

    auto* databaseStatus = MakeText(
        status,
        U("正在连接数据库"),
        8,
        Theme::Muted()
    );
    databaseStatus->SetName("app.database_status");

    auto* jobsStatus = MakeText(
        status,
        U("正在读取后台任务"),
        8,
        Theme::Muted()
    );
    jobsStatus->SetName("app.jobs_status");

    layout->Add(
        workspaceStatus,
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        20
    );
    layout->Add(
        databaseStatus,
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        30
    );
    layout->Add(
        jobsStatus,
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

    const auto& descriptor = navigation_[index].page;

    if (pages_.size() != navigation_.size())
    {
        wxMessageBox(
            U("页面导航状态不一致，无法打开所选页面。"),
            U("页面导航错误"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    wxWindow*& page = pages_[index];
    int bookIndex = wxNOT_FOUND;

    if (page == nullptr)
    {
        page = CreatePage(pageBook_, descriptor);

        if (page == nullptr)
        {
            wxMessageBox(
                U("页面创建失败。"),
                U("页面导航错误"),
                wxOK | wxICON_ERROR,
                this
            );
            return;
        }

        pageBook_->AddPage(page, descriptor.title, false);
        bookIndex = static_cast<int>(pageBook_->GetPageCount()) - 1;
    }
    else
    {
        for (std::size_t candidate = 0;
             candidate < pageBook_->GetPageCount();
             ++candidate)
        {
            if (pageBook_->GetPage(candidate) == page)
            {
                bookIndex = static_cast<int>(candidate);
                break;
            }
        }
    }

    if (bookIndex == wxNOT_FOUND)
    {
        wxMessageBox(
            U("所选页面已经失效，无法继续导航。"),
            U("页面导航错误"),
            wxOK | wxICON_ERROR,
            this
        );
        return;
    }

    pageBook_->SetSelection(bookIndex);
    selectedIndex_ = index;

    workspaceTitle_->SetLabel(
        U("本地工作区  ·  ") + descriptor.code + U("  ") + descriptor.title
    );

    SetTitle(U("续证 Continuum · ") + descriptor.title);
    UpdateNavigationStyles();
    Layout();

    if (WxUiBinder::Instance().IsAttached())
    {
        WxUiBinder::Instance().RefreshAll();
    }

    if (WxRecordController::Instance().IsAttached())
    {
        WxRecordController::Instance().RefreshAll();
    }
}

bool MainFrame::NavigateToCode(
    const wxString& code
)
{
    for (std::size_t index = 0;
         index < navigation_.size();
         ++index)
    {
        if (navigation_[index].page.code == code)
        {
            NavigateTo(index);
            return true;
        }
    }

    return false;
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
        U("打开功能页面"),
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
        wxBORDER_NONE | wxTE_PROCESS_ENTER
    );
    search->SetBackgroundColour(Theme::Input());
    search->SetForegroundColour(Theme::Text());
    search->SetFont(Theme::Font(11));
    search->SetHint(U("输入页面名称"));

    std::vector<std::pair<wxString, wxString>> commands;
    commands.reserve(navigation_.size());

    for (const auto& entry : navigation_)
    {
        commands.emplace_back(
            U("打开 ") + entry.page.title +
                U("  (") + entry.page.code + U(")"),
            entry.page.code
        );
    }

    wxArrayString labels;

    for (const auto& command : commands)
    {
        labels.Add(command.first);
    }

    auto* list = new wxListBox(
        &dialog,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        labels,
        wxLB_SINGLE | wxBORDER_NONE
    );
    list->SetBackgroundColour(Theme::Surface2());
    list->SetForegroundColour(Theme::Text());
    list->SetFont(Theme::Font(10));
    list->SetSelection(0);

    auto* hint = MakeText(
        &dialog,
        U("双击或按 Enter 打开 · Esc 关闭"),
        8,
        Theme::Muted()
    );

    root->Add(title, 0, wxALL, 22);
    root->Add(
        search,
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        22
    );
    root->Add(
        list,
        1,
        wxEXPAND | wxLEFT | wxRIGHT,
        22
    );
    root->Add(hint, 0, wxALL, 22);

    dialog.SetSizer(root);

    const auto execute =
        [&dialog, list]() {
            if (list->GetSelection() != wxNOT_FOUND)
            {
                dialog.EndModal(wxID_OK);
            }
        };

    list->Bind(
        wxEVT_LISTBOX_DCLICK,
        [execute](wxCommandEvent&)
        {
            execute();
        }
    );

    search->Bind(
        wxEVT_TEXT,
        [list, &commands](wxCommandEvent& event)
        {
            const wxString query =
                event.GetString().Lower();

            list->Clear();

            for (const auto& command : commands)
            {
                if (query.empty() ||
                    command.first.Lower().Find(query) !=
                        wxNOT_FOUND)
                {
                    list->Append(
                        command.first,
                        new wxStringClientData(
                            command.second
                        )
                    );
                }
            }

            if (list->GetCount() > 0)
            {
                list->SetSelection(0);
            }
        }
    );

    search->Bind(
        wxEVT_TEXT_ENTER,
        [execute](wxCommandEvent&)
        {
            execute();
        }
    );

    list->Clear();

    for (const auto& command : commands)
    {
        list->Append(
            command.first,
            new wxStringClientData(command.second)
        );
    }

    if (list->GetCount() > 0)
    {
        list->SetSelection(0);
    }

    search->SetFocus();

    if (dialog.ShowModal() == wxID_OK)
    {
        const int selected = list->GetSelection();

        if (selected != wxNOT_FOUND)
        {
            auto* data = dynamic_cast<wxStringClientData*>(
                list->GetClientObject(selected)
            );

            if (data != nullptr)
            {
                NavigateToCode(data->GetData());
            }
        }
    }
}

void MainFrame::RunGlobalSearch()
{
    if (globalSearch_ == nullptr)
    {
        return;
    }

    wxString query = globalSearch_->GetValue();
    query.Trim(true).Trim(false);

    if (query.empty())
    {
        globalSearch_->SetFocus();
        return;
    }

    if (!NavigateToCode(U("P07")))
    {
        return;
    }

    wxWindow* page = pages_[selectedIndex_];
    auto* input = dynamic_cast<wxTextCtrl*>(
        wxWindow::FindWindowByName(
            U("global.search.query"),
            page
        )
    );

    if (input == nullptr)
    {
        return;
    }

    input->SetValue(query);

    wxCommandEvent searchEvent(
        wxEVT_TEXT_ENTER,
        input->GetId()
    );
    searchEvent.SetEventObject(input);
    input->GetEventHandler()->ProcessEvent(searchEvent);
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
