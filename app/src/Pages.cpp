#include "Pages.h"

#include "EvidencePages.h"
#include "LedgerPages.h"
#include "InvestigationPages.h"
#include "DeliveryPages.h"
#include "OperationsPages.h"
#include "GovernancePages.h"
#include "SystemPages.h"
#include "CompleteWorkflowPages.h"

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
        MakeText(this, U("P03"), 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
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

    root->Add(headingRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 26);

    auto* metricRow = new wxBoxSizer(wxHORIZONTAL);
    metricRow->Add(
        CreateStatCard(
            this,
            U("0"),
            U("有效业务对象"),
            U("数据库中的未删除对象"),
            Theme::Green()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("0"),
            U("待复核证据"),
            U("需要人工审查的证据"),
            Theme::Yellow()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("0"),
            U("已启用数据源"),
            U("当前参与扫描的数据源"),
            Theme::Cyan()
        ),
        1,
        wxRIGHT,
        12
    );
    metricRow->Add(
        CreateStatCard(
            this,
            U("0"),
            U("已解析文件"),
            U("已经完成内容解析的文件"),
            Theme::Blue()
        ),
        1
    );

    root->Add(metricRow, 0, wxEXPAND | wxALL, 26);

    auto* statusPanel = MakeSection(this);
    auto* statusSizer = new wxBoxSizer(wxVERTICAL);

    statusSizer->Add(
        MakeText(
            statusPanel,
            U("工作区运行状态"),
            12,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxALL,
        18
    );

    auto* workerStatus = MakeText(
        statusPanel,
        U("正在读取后台服务状态"),
        10,
        Theme::Muted()
    );
    workerStatus->SetName("dashboard.worker_status");

    auto* searchStatus = MakeText(
        statusPanel,
        U("正在读取全文索引状态"),
        10,
        Theme::Muted()
    );
    searchStatus->SetName("dashboard.search_status");

    statusSizer->Add(
        workerStatus,
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        18
    );
    statusSizer->Add(
        searchStatus,
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        18
    );

    statusPanel->SetSizer(statusSizer);

    root->Add(
        statusPanel,
        1,
        wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
        26
    );
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
    auto* valueLabel = MakeText(
        card,
        value,
        24,
        accent,
        wxFONTWEIGHT_BOLD
    );

    if (label == U("有效业务对象"))
    {
        valueLabel->SetName("dashboard.active_objects");
    }
    else if (label == U("待复核证据"))
    {
        valueLabel->SetName("dashboard.evidence_review");
    }
    else if (label == U("已启用数据源"))
    {
        valueLabel->SetName("dashboard.sources");
    }
    else if (label == U("已解析文件"))
    {
        valueLabel->SetName("dashboard.files");
    }

    layout->Add(
        valueLabel,
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


wxWindow* CreatePage(wxWindow* parent, const PageDescriptor& descriptor)
{
    if (descriptor.code == U("P03"))
    {
        return new OverviewPage(parent);
    }

    if (auto* completePage = CreateCompleteWorkflowPage(
            parent,
            descriptor.code
        ))
    {
        return completePage;
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

    wxFAIL_MSG(U("公开页面路由没有对应实现：") + descriptor.code);
    return nullptr;
}

}
