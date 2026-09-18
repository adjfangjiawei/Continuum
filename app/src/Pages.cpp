#include "Pages.h"

#include "EvidencePages.h"
#include "LedgerPages.h"
#include "InvestigationPages.h"
#include "DeliveryPages.h"
#include "OperationsPages.h"
#include "GovernancePages.h"
#include "SystemPages.h"

#include "Theme.h"

#include <wx/button.h>
#include <wx/scrolwin.h>
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

wxButton* MakeActionButton(
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

    button->SetFont(Theme::Font(10, wxFONTWEIGHT_SEMIBOLD));
    button->SetBackgroundColour(primary ? Theme::Blue() : Theme::Surface2());
    button->SetForegroundColour(Theme::Text());
    return button;
}

wxPanel* MakeSection(wxWindow* parent)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    Theme::Apply(panel, Theme::Surface());
    panel->SetMinSize(wxSize(-1, 80));
    return panel;
}

}

OverviewPage::OverviewPage(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    Theme::Apply(this, Theme::Window());

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* headingRow = new wxBoxSizer(wxHORIZONTAL);
    auto* heading = new wxBoxSizer(wxVERTICAL);

    heading->Add(
        MakeText(this, U("P01"), 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    heading->Add(
        MakeText(this, U("项目概览"), 22, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    heading->Add(
        MakeText(
            this,
            U("集中查看当前事实、决策、承诺、风险、冲突和来源变化"),
            10,
            Theme::Muted()
        ),
        0
    );

    headingRow->Add(heading, 1, wxEXPAND);
    headingRow->Add(MakeActionButton(this, U("创建对象")), 0, wxRIGHT, 10);
    headingRow->Add(MakeActionButton(this, U("添加数据源"), true), 0);

    root->Add(headingRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 26);

    auto* metricRow = new wxBoxSizer(wxHORIZONTAL);
    metricRow->Add(
        CreateStatCard(
            this,
            U("18"),
            U("当前有效事实"),
            U("今天新增 3 项"),
            Theme::Green()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("7"),
            U("有效决策"),
            U("11 项需要复核"),
            Theme::Purple()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("11"),
            U("未完成承诺"),
            U("3 项即将到期"),
            Theme::Cyan()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("6"),
            U("未解决冲突"),
            U("2 项高严重度"),
            Theme::Red()
        ),
        1
    );

    root->Add(metricRow, 0, wxEXPAND | wxALL, 26);

    auto* body = new wxBoxSizer(wxHORIZONTAL);

    auto* activityPanel = MakeSection(this);
    auto* activitySizer = new wxBoxSizer(wxVERTICAL);

    auto* activityHeader = new wxBoxSizer(wxHORIZONTAL);
    activityHeader->Add(
        MakeText(
            activityPanel,
            U("最近项目活动"),
            12,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        1,
        wxALIGN_CENTER_VERTICAL
    );
    activityHeader->Add(
        MakeText(activityPanel, U("查看全部 ›"), 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL
    );

    activitySizer->Add(activityHeader, 0, wxEXPAND | wxALL, 18);
    activitySizer->Add(
        CreateActivityRow(
            activityPanel,
            U("12:34"),
            U("证据复核"),
            U("E-1097 被标记为需要复核"),
            Theme::Yellow()
        ),
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        12
    );
    activitySizer->Add(
        CreateActivityRow(
            activityPanel,
            U("12:20"),
            U("冲突发现"),
            U("交付日期出现两个当前值"),
            Theme::Red()
        ),
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        12
    );
    activitySizer->Add(
        CreateActivityRow(
            activityPanel,
            U("12:18"),
            U("文件变化"),
            U("requirements-v7.pdf 更新为 v7"),
            Theme::Blue()
        ),
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        12
    );
    activitySizer->Add(
        CreateActivityRow(
            activityPanel,
            U("10:30"),
            U("承诺延期"),
            U("安全复核报告延期至 9 月 25 日"),
            Theme::Cyan()
        ),
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        12
    );

    activityPanel->SetSizer(activitySizer);

    auto* focusPanel = MakeSection(this);
    auto* focusSizer = new wxBoxSizer(wxVERTICAL);

    focusSizer->Add(
        MakeText(
            focusPanel,
            U("需要关注"),
            12,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxALL,
        18
    );

    const struct
    {
        const char* title;
        const char* detail;
        wxColour color;
    } focusItems[] = {
        {
            "2 项高严重度冲突",
            "交付日期与旧协议退出时间",
            Theme::Red()
        },
        {
            "3 个来源发生变化",
            "相关证据需要重新锚定",
            Theme::Yellow()
        },
        {
            "3 项承诺即将到期",
            "最近截止日期为 9 月 20 日",
            Theme::Cyan()
        },
        {
            "1 个数据源不可访问",
            "E:\\Legal\\contracts",
            Theme::Orange()
        }
    };

    for (const auto& item : focusItems)
    {
        auto* row = new wxPanel(focusPanel, wxID_ANY);
        Theme::Apply(row, Theme::Surface2());

        auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* marker = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(5, -1));
        marker->SetBackgroundColour(item.color);

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                row,
                U(item.title),
                10,
                Theme::Text(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        labels->Add(
            MakeText(row, U(item.detail), 8, Theme::Muted()),
            0
        );

        rowSizer->Add(marker, 0, wxEXPAND | wxRIGHT, 12);
        rowSizer->Add(labels, 1, wxEXPAND | wxALL, 12);
        row->SetSizer(rowSizer);

        focusSizer->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    }

    focusPanel->SetSizer(focusSizer);

    body->Add(activityPanel, 2, wxEXPAND | wxRIGHT, 14);
    body->Add(focusPanel, 1, wxEXPAND);

    root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
    SetSizer(root);
}

wxPanel* OverviewPage::CreateStatCard(
    wxWindow* parent,
    const wxString& value,
    const wxString& label,
    const wxString& detail,
    const wxColour& accent
)
{
    auto* card = new wxPanel(parent, wxID_ANY);
    Theme::Apply(card, Theme::Surface());
    card->SetMinSize(wxSize(190, 112));

    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* marker = new wxPanel(card, wxID_ANY, wxDefaultPosition, wxSize(-1, 4));
    marker->SetBackgroundColour(accent);

    layout->Add(marker, 0, wxEXPAND);
    layout->Add(
        MakeText(card, value, 24, accent, wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        16
    );
    layout->Add(
        MakeText(card, label, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        16
    );
    layout->Add(
        MakeText(card, detail, 8, Theme::Muted()),
        0,
        wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
        16
    );

    card->SetSizer(layout);
    return card;
}

wxPanel* OverviewPage::CreateActivityRow(
    wxWindow* parent,
    const wxString& time,
    const wxString& type,
    const wxString& message,
    const wxColour& accent
)
{
    auto* row = new wxPanel(parent, wxID_ANY);
    Theme::Apply(row, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 58));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* marker = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(5, -1));
    marker->SetBackgroundColour(accent);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, type, 9, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(row, message, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0
    );

    layout->Add(marker, 0, wxEXPAND | wxRIGHT, 12);
    layout->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxALL, 10);
    layout->Add(
        MakeText(row, time, 8, Theme::Muted()),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        14
    );

    row->SetSizer(layout);
    return row;
}

ModulePage::ModulePage(wxWindow* parent, const PageDescriptor& descriptor)
    : wxPanel(parent, wxID_ANY)
{
    Theme::Apply(this, Theme::Window());

    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* headingRow = new wxBoxSizer(wxHORIZONTAL);
    auto* heading = new wxBoxSizer(wxVERTICAL);

    heading->Add(
        MakeText(this, descriptor.code, 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    heading->Add(
        MakeText(this, descriptor.title, 22, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    heading->Add(
        MakeText(this, descriptor.subtitle, 10, Theme::Muted()),
        0
    );

    headingRow->Add(heading, 1, wxEXPAND);
    headingRow->Add(MakeActionButton(this, U("刷新")), 0, wxRIGHT, 10);
    headingRow->Add(MakeActionButton(this, U("创建"), true), 0);

    root->Add(headingRow, 0, wxEXPAND | wxALL, 26);

    auto* metrics = new wxBoxSizer(wxHORIZONTAL);
    metrics->Add(
        CreateMetric(
            this,
            U("24"),
            U("全部项目"),
            U("当前查询范围"),
            Theme::Blue()
        ),
        1,
        wxRIGHT,
        12
    );
    metrics->Add(
        CreateMetric(
            this,
            U("11"),
            U("需要复核"),
            U("等待人工确认"),
            Theme::Yellow()
        ),
        1,
        wxRIGHT,
        12
    );
    metrics->Add(
        CreateMetric(
            this,
            U("3"),
            U("近期变化"),
            U("过去 24 小时"),
            Theme::Cyan()
        ),
        1
    );

    root->Add(metrics, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);

    auto* listPanel = MakeSection(this);
    auto* listSizer = new wxBoxSizer(wxVERTICAL);

    auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
    auto* search = new wxTextCtrl(
        listPanel,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(360, 38),
        wxBORDER_NONE
    );
    search->SetHint(U("搜索当前页面"));
    search->SetBackgroundColour(Theme::Input());
    search->SetForegroundColour(Theme::Text());
    search->SetHint(U("搜索对象、编号或内容"));

    toolbar->Add(search, 0, wxRIGHT, 10);
    toolbar->Add(MakeActionButton(listPanel, U("筛选")), 0, wxRIGHT, 10);
    toolbar->Add(MakeActionButton(listPanel, U("排序")), 0);
    toolbar->AddStretchSpacer();
    toolbar->Add(
        MakeText(listPanel, U("显示 5 / 24 项"), 9, Theme::Muted()),
        0,
        wxALIGN_CENTER_VERTICAL
    );

    listSizer->Add(toolbar, 0, wxEXPAND | wxALL, 18);

    const struct
    {
        const char* identifier;
        const char* title;
        const char* detail;
        const char* state;
        wxColour color;
    } rows[] = {
        {
            "ITEM-001",
            "采用双轨迁移方案",
            "更新于今天 12:34 · 关联 4 项证据",
            "需要复核",
            Theme::Yellow()
        },
        {
            "ITEM-002",
            "完成合作方环境验证",
            "负责人：周启明 · 截止 9 月 20 日",
            "进行中",
            Theme::Cyan()
        },
        {
            "ITEM-003",
            "旧协议保留至第四季度",
            "来源：requirements-v7.pdf",
            "当前有效",
            Theme::Green()
        },
        {
            "ITEM-004",
            "双轨运行增加审计复杂度",
            "高影响 · 中等可能性",
            "活跃",
            Theme::Red()
        },
        {
            "ITEM-005",
            "合作方最终切换日期",
            "等待交付委员会确认",
            "未决",
            Theme::Orange()
        }
    };

    for (const auto& row : rows)
    {
        listSizer->Add(
            CreateListRow(
                listPanel,
                U(row.identifier),
                U(row.title),
                U(row.detail),
                U(row.state),
                row.color
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
    }

    listPanel->SetSizer(listSizer);
    root->Add(listPanel, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);

    SetSizer(root);
}

wxPanel* ModulePage::CreateMetric(
    wxWindow* parent,
    const wxString& value,
    const wxString& title,
    const wxString& description,
    const wxColour& accent
)
{
    auto* card = new wxPanel(parent, wxID_ANY);
    Theme::Apply(card, Theme::Surface());
    card->SetMinSize(wxSize(190, 95));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* marker = new wxPanel(card, wxID_ANY, wxDefaultPosition, wxSize(5, -1));
    marker->SetBackgroundColour(accent);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(card, value, 21, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    labels->Add(
        MakeText(card, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(card, description, 8, Theme::Muted()),
        0
    );

    layout->Add(marker, 0, wxEXPAND | wxRIGHT, 14);
    layout->Add(labels, 1, wxALL, 14);
    card->SetSizer(layout);

    return card;
}

wxPanel* ModulePage::CreateListRow(
    wxWindow* parent,
    const wxString& identifier,
    const wxString& title,
    const wxString& detail,
    const wxString& state,
    const wxColour& accent
)
{
    auto* row = new wxPanel(parent, wxID_ANY);
    Theme::Apply(row, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 66));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* marker = new wxPanel(row, wxID_ANY, wxDefaultPosition, wxSize(5, -1));
    marker->SetBackgroundColour(accent);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, identifier, 8, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(row, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(row, detail, 8, Theme::Muted()),
        0
    );

    auto* stateLabel = MakeText(
        row,
        state,
        9,
        accent,
        wxFONTWEIGHT_BOLD
    );

    layout->Add(marker, 0, wxEXPAND | wxRIGHT, 14);
    layout->Add(labels, 1, wxALL, 12);
    layout->Add(stateLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 20);
    layout->Add(
        MakeText(row, U("›"), 15, Theme::Faint(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        16
    );

    row->SetSizer(layout);
    return row;
}

wxWindow* CreatePage(wxWindow* parent, const PageDescriptor& descriptor)
{
    if (descriptor.code == U("P01"))
    {
        return new OverviewPage(parent);
    }

    if (auto* workflowPage = CreateEvidenceWorkflowPage(
            parent,
            descriptor.code
        ))
    {
        return workflowPage;
    }

    if (auto* ledgerPage = CreateLedgerWorkflowPage(
            parent,
            descriptor.code
        ))
    {
        return ledgerPage;
    }

    if (auto* investigationPage = CreateInvestigationPage(
            parent,
            descriptor.code
        ))
    {
        return investigationPage;
    }

    if (auto* deliveryPage = CreateDeliveryPage(
            parent,
            descriptor.code
        ))
    {
        return deliveryPage;
    }


    if (auto* operationsPage = CreateOperationsPage(
            parent,
            descriptor.code
        ))
    {
        return operationsPage;
    }


    if (auto* governancePage = CreateGovernancePage(
            parent,
            descriptor.code
        ))
    {
        return governancePage;
    }


    if (auto* systemPage = CreateSystemPage(
            parent,
            descriptor.code
        ))
    {
        return systemPage;
    }

    return new ModulePage(parent, descriptor);
}

}
