#include "InvestigationPages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/gauge.h>
#include <wx/graphics.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/window.h>

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

wxStaticText* MakeWrappedText(
    wxWindow* parent,
    const wxString& value,
    int size,
    const wxColour& color,
    int width,
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* label = MakeText(parent, value, size, color, weight);
    label->Wrap(width);
    return label;
}

wxPanel* MakeCard(
    wxWindow* parent,
    const wxColour& background = Theme::Surface()
)
{
    auto* panel = new wxPanel(parent, wxID_ANY);
    Theme::Apply(panel, background);
    return panel;
}

wxButton* MakeButton(
    wxWindow* parent,
    const wxString& label,
    bool primary = false,
    bool danger = false,
    int width = -1
)
{
    auto* button = new wxButton(
        parent,
        wxID_ANY,
        label,
        wxDefaultPosition,
        wxSize(width, 38),
        wxBORDER_NONE
    );

    button->SetBackgroundColour(
        danger ? Theme::Red() :
        primary ? Theme::Blue() :
        Theme::Surface2()
    );
    button->SetForegroundColour(Theme::Text());
    button->SetFont(Theme::Font(9, wxFONTWEIGHT_SEMIBOLD));
    return button;
}

wxTextCtrl* MakeInput(
    wxWindow* parent,
    const wxString& value,
    int width = -1,
    bool readOnly = false
)
{
    long style = wxBORDER_NONE;

    if (readOnly)
    {
        style |= wxTE_READONLY;
    }

    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        value,
        wxDefaultPosition,
        wxSize(width, 38),
        style
    );
    input->SetBackgroundColour(Theme::Input());
    input->SetForegroundColour(Theme::Text());
    input->SetFont(Theme::Font(9));
    return input;
}

wxPanel* MakePill(
    wxWindow* parent,
    const wxString& label,
    const wxColour& color,
    int width = 96
)
{
    auto* panel = MakeCard(parent, Theme::Surface3());
    panel->SetMinSize(wxSize(width, 25));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    auto* marker = new wxPanel(
        panel,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(7, 7)
    );
    marker->SetBackgroundColour(color);

    layout->Add(marker, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 9);
    layout->Add(
        MakeText(panel, label, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT,
        7
    );

    panel->SetSizer(layout);
    return panel;
}

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle,
    const wxString& primaryAction
)
{
    auto* row = new wxBoxSizer(wxHORIZONTAL);
    auto* labels = new wxBoxSizer(wxVERTICAL);

    labels->Add(
        MakeText(parent, code, 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(parent, title, 22, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    labels->Add(MakeText(parent, subtitle, 10, Theme::Muted()), 0);

    row->Add(labels, 1, wxEXPAND);
    row->Add(
        MakeButton(parent, U("刷新"), false, false, 88),
        0,
        wxALIGN_BOTTOM | wxRIGHT,
        10
    );
    row->Add(
        MakeButton(parent, primaryAction, true, false, 142),
        0,
        wxALIGN_BOTTOM
    );

    root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 26);
}

wxPanel* MakeRow(
    wxWindow* parent,
    const wxString& code,
    const wxString& title,
    const wxString& detail,
    const wxString& state,
    const wxColour& color,
    bool selected = false
)
{
    auto* row = MakeCard(
        parent,
        selected ? Theme::Surface3() : Theme::Surface2()
    );
    row->SetMinSize(wxSize(-1, 72));

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* stripe = new wxPanel(
        row,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(selected ? 5 : 3, -1)
    );
    stripe->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, code, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    labels->Add(
        MakeText(row, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 11);
    root->Add(labels, 1, wxALL, 10);
    root->Add(
        MakePill(row, state, color),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        11
    );
    row->SetSizer(root);
    return row;
}

wxPanel* MakeProperty(
    wxWindow* parent,
    const wxString& label,
    const wxString& value,
    const wxColour& color = Theme::Text()
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 42));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    layout->Add(
        MakeText(row, label, 8, Theme::Muted()),
        1,
        wxALIGN_CENTER_VERTICAL | wxLEFT,
        12
    );
    layout->Add(
        MakeText(row, value, 9, color, wxFONTWEIGHT_SEMIBOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );
    row->SetSizer(layout);
    return row;
}

wxPanel* MakeClaimCard(
    wxWindow* parent,
    const wxString& heading,
    const wxString& value,
    const wxString& source,
    const wxColour& color
)
{
    auto* panel = MakeCard(parent, Theme::Surface2());
    auto* layout = new wxBoxSizer(wxVERTICAL);

    layout->Add(
        MakeText(panel, heading, 9, color, wxFONTWEIGHT_BOLD),
        0,
        wxALL,
        14
    );
    layout->Add(
        MakeText(panel, value, 16, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );
    layout->Add(
        MakeWrappedText(panel, source, 8, Theme::Muted(), 260),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );
    layout->Add(
        MakePill(panel, U("当前标记：有效"), color, 122),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );

    panel->SetSizer(layout);
    return panel;
}

class ConflictCenterPage final : public wxPanel
{
public:
    explicit ConflictCenterPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P10"),
            U("冲突中心"),
            U("比较相反主张、来源证据和时间顺序，并记录人工裁决"),
            U("运行冲突检测")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        filters->SetMinSize(wxSize(220, -1));
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);
        filterSizer->Add(
            MakeText(filters, U("冲突类型"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> types = {{
            {U("全部冲突"), U("6"), Theme::Blue()},
            {U("值冲突"), U("2"), Theme::Red()},
            {U("日期冲突"), U("2"), Theme::Orange()},
            {U("状态冲突"), U("1"), Theme::Yellow()},
            {U("失效引用"), U("1"), Theme::Purple()},
            {U("缺少证据"), U("4"), Theme::Muted()},
            {U("依赖取消"), U("2"), Theme::Cyan()}
        }};

        for (std::size_t index = 0; index < types.size(); ++index)
        {
            auto* row = MakeCard(
                filters,
                index == 0 ? Theme::Surface3() : Theme::Surface2()
            );
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);
            rowSizer->Add(
                MakeText(
                    row,
                    std::get<0>(types[index]),
                    9,
                    index == 0 ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                1,
                wxALL,
                10
            );
            rowSizer->Add(
                MakeText(
                    row,
                    std::get<1>(types[index]),
                    9,
                    std::get<2>(types[index]),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                11
            );
            row->SetSizer(rowSizer);
            filterSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        filterSizer->Add(
            new wxStaticLine(filters, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        filterSizer->Add(
            MakeText(filters, U("严重程度"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakePill(filters, U("高严重度"), Theme::Red(), 105),
            0,
            wxLEFT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakePill(filters, U("中严重度"), Theme::Yellow(), 105),
            0,
            wxLEFT | wxBOTTOM,
            17
        );
        filters->SetSizer(filterSizer);

        auto* list = MakeCard(this);
        list->SetMinSize(wxSize(390, -1));
        auto* listSizer = new wxBoxSizer(wxVERTICAL);
        listSizer->Add(
            MakeText(list, U("待处理冲突"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } conflicts[] = {
            {
                "CF-0061",
                "交付日期存在两个当前值",
                "10 月 15 日与 11 月 30 日",
                "高严重度",
                Theme::Red()
            },
            {
                "CF-0059",
                "旧协议退出时间不一致",
                "第四季度与明年一季度",
                "值冲突",
                Theme::Red()
            },
            {
                "CF-0057",
                "已撤销决策仍被引用",
                "D-0027 被 2 项承诺引用",
                "失效引用",
                Theme::Purple()
            },
            {
                "CF-0054",
                "完成承诺缺少完成证据",
                "C-0036 状态为已完成",
                "缺少证据",
                Theme::Yellow()
            },
            {
                "CF-0051",
                "同一承诺存在两个负责人",
                "周启明与林致远",
                "值冲突",
                Theme::Orange()
            }
        };

        for (std::size_t index = 0; index < 5; ++index)
        {
            listSizer->Add(
                MakeRow(
                    list,
                    U(conflicts[index].code),
                    U(conflicts[index].title),
                    U(conflicts[index].detail),
                    U(conflicts[index].state),
                    conflicts[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }
        list->SetSizer(listSizer);

        auto* comparison = MakeCard(this);
        auto* comparisonSizer = new wxBoxSizer(wxVERTICAL);

        auto* comparisonHeader = new wxBoxSizer(wxHORIZONTAL);
        comparisonHeader->Add(
            MakeText(
                comparison,
                U("冲突比较"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        comparisonHeader->Add(
            MakePill(comparison, U("高严重度"), Theme::Red(), 105),
            0
        );
        comparisonSizer->Add(comparisonHeader, 0, wxEXPAND | wxALL, 17);
        comparisonSizer->Add(
            MakeText(
                comparison,
                U("交付日期存在两个当前值"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* claims = new wxBoxSizer(wxHORIZONTAL);
        claims->Add(
            MakeClaimCard(
                comparison,
                U("主张 A"),
                U("2026 年 10 月 15 日"),
                U("交付计划.xlsx · 里程碑表 · 版本 v5"),
                Theme::Red()
            ),
            1,
            wxRIGHT,
            12
        );
        claims->Add(
            MakeClaimCard(
                comparison,
                U("主张 B"),
                U("2026 年 11 月 30 日"),
                U("requirements-v7.pdf · 第 8 页 · 版本 v7"),
                Theme::Blue()
            ),
            1
        );
        comparisonSizer->Add(
            claims,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        comparisonSizer->Add(
            MakeText(
                comparison,
                U("时间顺序"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        comparisonSizer->Add(
            MakeProperty(
                comparison,
                U("2026-09-02"),
                U("提出 10 月 15 日"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        comparisonSizer->Add(
            MakeProperty(
                comparison,
                U("2026-09-12"),
                U("延期讨论"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        comparisonSizer->Add(
            MakeProperty(
                comparison,
                U("2026-09-18"),
                U("改为 11 月 30 日"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        comparisonSizer->Add(
            MakeInput(comparison, U("主张 B 为当前有效值")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        comparisonSizer->Add(
            MakeInput(
                comparison,
                U("新版本需求文件明确调整了最终交付日期")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(comparison, U("保留冲突")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(comparison, U("信息不足")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(comparison, U("确认处理"), true),
            1
        );
        comparisonSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        comparison->SetSizer(comparisonSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(list, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(comparison, 1, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

class ChangeReviewPage final : public wxPanel
{
public:
    explicit ChangeReviewPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P11"),
            U("变化审查"),
            U("比较源文件版本差异，重新锚定证据并分析业务影响"),
            U("批量确认")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* queue = MakeCard(this);
        queue->SetMinSize(wxSize(330, -1));
        auto* queueSizer = new wxBoxSizer(wxVERTICAL);
        queueSizer->Add(
            MakeText(queue, U("文件变化队列"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } changes[] = {
            {
                "FILE-001",
                "requirements-v7.pdf",
                "修改 · v6 → v7 · 3 处影响",
                "需处理",
                Theme::Yellow()
            },
            {
                "FILE-002",
                "会议纪要-0909.md",
                "修改 · v2 → v3 · 1 处影响",
                "需处理",
                Theme::Yellow()
            },
            {
                "FILE-003",
                "old-protocol.md",
                "源文件已删除 · 2 处影响",
                "失联",
                Theme::Red()
            },
            {
                "FILE-004",
                "安全评审.eml",
                "新增文件 · 无待处理影响",
                "新增",
                Theme::Blue()
            }
        };

        for (std::size_t index = 0; index < 4; ++index)
        {
            queueSizer->Add(
                MakeRow(
                    queue,
                    U(changes[index].code),
                    U(changes[index].title),
                    U(changes[index].detail),
                    U(changes[index].state),
                    changes[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        queueSizer->AddStretchSpacer();
        queueSizer->Add(
            MakeButton(queue, U("重新扫描数据源")),
            0,
            wxEXPAND | wxALL,
            17
        );
        queue->SetSizer(queueSizer);

        auto* diff = MakeCard(this);
        auto* diffSizer = new wxBoxSizer(wxVERTICAL);

        auto* diffHeader = new wxBoxSizer(wxHORIZONTAL);
        diffHeader->Add(
            MakeText(diff, U("版本差异"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        diffHeader->Add(MakePill(diff, U("v6 旧版本"), Theme::Red()), 0, wxRIGHT, 8);
        diffHeader->Add(MakePill(diff, U("v7 当前"), Theme::Blue()), 0);
        diffSizer->Add(diffHeader, 0, wxEXPAND | wxALL, 17);

        auto* oldText = new wxTextCtrl(
            diff,
            wxID_ANY,
            U(
                "− 旧协议保留至 2026 年 10 月 31 日，\n"
                "  之后关闭旧接口并撤销回退通道。"
            ),
            wxDefaultPosition,
            wxSize(-1, 100),
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );
        oldText->SetBackgroundColour(wxColour(57, 30, 38));
        oldText->SetForegroundColour(Theme::Text());
        oldText->SetFont(Theme::Font(10));

        auto* newText = new wxTextCtrl(
            diff,
            wxID_ANY,
            U(
                "+ 旧协议保留至 2026 年第四季度结束，\n"
                "  具体关闭日期由交付委员会另行确认，\n"
                "  在此之前必须保留紧急回退通道。"
            ),
            wxDefaultPosition,
            wxSize(-1, 125),
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );
        newText->SetBackgroundColour(wxColour(25, 55, 48));
        newText->SetForegroundColour(Theme::Text());
        newText->SetFont(Theme::Font(10));

        diffSizer->Add(oldText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 17);
        diffSizer->Add(newText, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 17);
        diffSizer->Add(
            MakeWrappedText(
                diff,
                U("检测结果：日期表达变化、删除确定日期、增加前置条件和回退要求。"),
                9,
                Theme::Yellow(),
                560,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        diffSizer->Add(
            MakeText(diff, U("受影响证据"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        diffSizer->Add(
            MakeRow(
                diff,
                U("E-1097"),
                U("内容被修改"),
                U("原锚点仍可定位，但语义发生变化"),
                U("需重锚定"),
                Theme::Yellow(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        diffSizer->Add(
            MakeRow(
                diff,
                U("E-1102"),
                U("原文片段被删除"),
                U("当前版本中没有精确匹配"),
                U("可能失效"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );

        diffSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(diff, U("保留旧证据并失效")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(diff, U("标记无业务影响")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(diff, U("重新锚定"), true),
            1
        );
        diffSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        diff->SetSizer(diffSizer);

        auto* impact = MakeCard(this);
        impact->SetMinSize(wxSize(330, -1));
        auto* impactSizer = new wxBoxSizer(wxVERTICAL);
        impactSizer->Add(
            MakeText(impact, U("业务影响树"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );
        impactSizer->Add(
            MakeText(
                impact,
                U("2 项证据影响 4 个业务对象"),
                9,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        impactSizer->Add(
            MakeRow(
                impact,
                U("E-1097"),
                U("证据需要复核"),
                U("requirements-v7.pdf"),
                U("证据"),
                Theme::Yellow(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        impactSizer->Add(
            MakeRow(
                impact,
                U("D-0031"),
                U("采用双轨迁移方案"),
                U("决策依据发生变化"),
                U("决策"),
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        impactSizer->Add(
            MakeRow(
                impact,
                U("F-0054"),
                U("旧协议支持期限"),
                U("当前事实失去精确日期"),
                U("事实"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        impactSizer->Add(
            MakeRow(
                impact,
                U("C-0042"),
                U("完成安全复核"),
                U("截止依据可能变化"),
                U("承诺"),
                Theme::Cyan()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );

        impactSizer->AddStretchSpacer();
        impactSizer->Add(
            MakeButton(impact, U("打开第一个受影响对象"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        impact->SetSizer(impactSizer);

        body->Add(queue, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(diff, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(impact, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

wxPanel* MakeTimelineEvent(
    wxWindow* parent,
    const wxString& time,
    const wxString& type,
    const wxString& title,
    const wxColour& color
)
{
    auto* event = MakeCard(parent, Theme::Surface2());
    event->SetMinSize(wxSize(-1, 56));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    auto* marker = new wxPanel(
        event,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(8, 8)
    );
    marker->SetBackgroundColour(color);

    layout->Add(marker, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 13);
    layout->Add(
        MakeText(event, time, 8, Theme::Muted(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        14
    );
    layout->Add(
        MakePill(event, type, color, 92),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        14
    );
    layout->Add(
        MakeText(event, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        1,
        wxALIGN_CENTER_VERTICAL
    );
    layout->Add(
        MakeText(event, U("查看 ›"), 8, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        14
    );

    event->SetSizer(layout);
    return event;
}

class TimelinePage final : public wxPanel
{
public:
    explicit TimelinePage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P12"),
            U("项目时间线"),
            U("按现实发生时间查看决策、承诺、风险、文件变化和审计事件"),
            U("创建时间切片")
        );

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(MakeInput(this, U("现实发生时间"), 180, true), 0, wxRIGHT, 10);
        toolbar->Add(MakeInput(this, U("2026 年 9 月"), 145, true), 0, wxRIGHT, 10);
        toolbar->Add(MakeInput(this, U("全部类型"), 115, true), 0, wxRIGHT, 10);
        toolbar->Add(MakeInput(this, U("全部人员"), 115, true), 0, wxRIGHT, 10);
        toolbar->Add(MakeButton(this, U("今天"), false, false, 75), 0, wxRIGHT, 10);
        toolbar->AddStretchSpacer();
        toolbar->Add(MakeButton(this, U("日  周  月  季度"), false, false, 155), 0);
        root->Add(toolbar, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        filters->SetMinSize(wxSize(240, -1));
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);
        filterSizer->Add(
            MakeText(filters, U("事件筛选"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> types = {{
            {U("决策事件"), U("8"), Theme::Purple()},
            {U("承诺事件"), U("13"), Theme::Cyan()},
            {U("事实变化"), U("7"), Theme::Green()},
            {U("风险事件"), U("5"), Theme::Red()},
            {U("文件事件"), U("21"), Theme::Blue()},
            {U("冲突事件"), U("6"), Theme::Orange()},
            {U("用户审计"), U("17"), Theme::Muted()}
        }};

        for (const auto& item : types)
        {
            filterSizer->Add(
                MakeProperty(
                    filters,
                    U("✓  ") + std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        filterSizer->Add(
            new wxStaticLine(filters, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        filterSizer->Add(
            MakeText(filters, U("当前范围"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakeWrappedText(
                filters,
                U("2026-09-01 至 2026-09-30\n共 77 个时间事件"),
                9,
                Theme::Muted(),
                195
            ),
            0,
            wxLEFT | wxRIGHT,
            17
        );
        filters->SetSizer(filterSizer);

        auto* timeline = MakeCard(this);
        auto* timelineSizer = new wxBoxSizer(wxVERTICAL);

        timelineSizer->Add(
            MakeText(
                timeline,
                U("2026 年 9 月"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const struct
        {
            const char* date;
            const char* weekday;
            const char* time;
            const char* type;
            const char* title;
            wxColour color;
        } events[] = {
            {
                "9 月 18 日",
                "今天",
                "12:18",
                "文件变化",
                "requirements-v7.pdf 更新为 v7",
                Theme::Blue()
            },
            {
                "",
                "",
                "12:20",
                "冲突发现",
                "交付日期出现两个当前值",
                Theme::Red()
            },
            {
                "",
                "",
                "12:34",
                "证据复核",
                "E-1097 被标记为需要复核",
                Theme::Yellow()
            },
            {
                "9 月 12 日",
                "星期六",
                "10:30",
                "承诺延期",
                "安全复核报告延期至 9 月 25 日",
                Theme::Cyan()
            },
            {
                "",
                "",
                "14:10",
                "风险上升",
                "双轨审计复杂度上升",
                Theme::Red()
            },
            {
                "9 月 9 日",
                "星期三",
                "14:30",
                "决策接受",
                "采用双轨迁移方案",
                Theme::Purple()
            },
            {
                "",
                "",
                "15:05",
                "事实生效",
                "旧协议保留至第四季度",
                Theme::Green()
            }
        };

        for (const auto& event : events)
        {
            if (wxString::FromUTF8(event.date).Length() > 0)
            {
                auto* dateRow = new wxBoxSizer(wxHORIZONTAL);
                dateRow->Add(
                    MakeText(
                        timeline,
                        U(event.date),
                        11,
                        Theme::Text(),
                        wxFONTWEIGHT_BOLD
                    ),
                    0,
                    wxRIGHT,
                    10
                );
                dateRow->Add(
                    MakeText(timeline, U(event.weekday), 8, Theme::Muted()),
                    0,
                    wxALIGN_CENTER_VERTICAL
                );
                timelineSizer->Add(
                    dateRow,
                    0,
                    wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
                    17
                );
            }

            timelineSizer->Add(
                MakeTimelineEvent(
                    timeline,
                    U(event.time),
                    U(event.type),
                    U(event.title),
                    event.color
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        timelineSizer->AddStretchSpacer();
        timelineSizer->Add(
            MakeText(
                timeline,
                U("滚动加载更早事件"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        timeline->SetSizer(timelineSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(timeline, 1, wxEXPAND);
        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);

        SetSizer(root);
    }
};

class HistoricalSlicePage final : public wxPanel
{
public:
    explicit HistoricalSlicePage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P13"),
            U("历史时间切片"),
            U("重建指定时刻可知的项目状态，并与当前状态进行比较"),
            U("导出历史报告")
        );

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(MakeInput(this, U("2026-09-09 16:00"), 205, true), 0, wxRIGHT, 10);
        toolbar->Add(MakeInput(this, U("Asia/Shanghai"), 175, true), 0, wxRIGHT, 10);
        toolbar->Add(
            MakeInput(this, U("排除后来补录的信息"), 220, true),
            0,
            wxRIGHT,
            10
        );
        toolbar->AddStretchSpacer();
        toolbar->Add(MakeButton(this, U("重新构建"), true, false, 120), 0);
        root->Add(toolbar, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* summary = MakeCard(this);
        summary->SetMinSize(wxSize(340, -1));
        auto* summarySizer = new wxBoxSizer(wxVERTICAL);
        summarySizer->Add(
            MakeText(summary, U("当时有效"), 14, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxString, wxColour>, 5> values = {{
            {U("18"), U("事实"), U("旧协议仍处于支持期"), Theme::Green()},
            {U("7"), U("决策"), U("D-0031 当日生效"), Theme::Purple()},
            {U("11"), U("未完成承诺"), U("最近截止日期 09-18"), Theme::Cyan()},
            {U("4"), U("活跃风险"), U("双轨运行风险已登记"), Theme::Red()},
            {U("2"), U("未决问题"), U("合作方切换日期未知"), Theme::Orange()}
        }};

        for (const auto& item : values)
        {
            summarySizer->Add(
                MakeRow(
                    summary,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item),
                    U("有效"),
                    std::get<3>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        summarySizer->AddStretchSpacer();
        summarySizer->Add(
            MakeWrappedText(
                summary,
                U("当时可用证据：127 项\n来源文件：36 个\n最后发现：2026-09-09 15:42"),
                8,
                Theme::Muted(),
                290
            ),
            0,
            wxALL,
            17
        );
        summary->SetSizer(summarySizer);

        auto* state = MakeCard(this);
        auto* stateSizer = new wxBoxSizer(wxVERTICAL);
        auto* stateHeader = new wxBoxSizer(wxHORIZONTAL);
        stateHeader->Add(
            MakeText(state, U("当时状态摘要"), 14, Theme::Text(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        stateHeader->Add(MakePill(state, U("历史视图"), Theme::Blue()), 0);
        stateSizer->Add(stateHeader, 0, wxEXPAND | wxALL, 17);

        auto* narrative = MakeCard(state, Theme::Input());
        auto* narrativeSizer = new wxBoxSizer(wxVERTICAL);
        narrativeSizer->Add(
            MakeWrappedText(
                narrative,
                U(
                    "在 2026 年 9 月 9 日 16:00，团队已经接受双轨迁移方案，"
                    "旧协议计划保留至第四季度。合作方最终切换日期尚未确定，"
                    "安全复核仍未完成。以上内容均可回溯到当时已经存在的证据。"
                ),
                10,
                Theme::Text(),
                500
            ),
            0,
            wxALL,
            16
        );
        narrative->SetSizer(narrativeSizer);
        stateSizer->Add(
            narrative,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        stateSizer->Add(
            MakeText(state, U("关键对象"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        stateSizer->Add(
            MakeRow(
                state,
                U("D-0031"),
                U("采用双轨迁移方案"),
                U("当日 14:30 生效"),
                U("已接受"),
                Theme::Purple(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        stateSizer->Add(
            MakeRow(
                state,
                U("F-0054"),
                U("旧协议保留至第四季度"),
                U("当时当前有效"),
                U("事实"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        stateSizer->Add(
            MakeRow(
                state,
                U("C-0038"),
                U("完成合作方环境验证"),
                U("尚未完成"),
                U("进行中"),
                Theme::Cyan()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        stateSizer->Add(
            MakeRow(
                state,
                U("R-0018"),
                U("双轨运行增加审计复杂度"),
                U("当日已登记"),
                U("活跃"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );

        stateSizer->AddStretchSpacer();
        stateSizer->Add(
            MakeButton(state, U("保存为复盘起点"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        state->SetSizer(stateSizer);

        auto* later = MakeCard(this);
        later->SetMinSize(wxSize(310, -1));
        auto* laterSizer = new wxBoxSizer(wxVERTICAL);
        laterSizer->Add(
            MakeText(later, U("后来才知道"), 14, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );
        laterSizer->Add(
            MakeText(
                later,
                U("目标时刻尚不可知的信息"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        laterSizer->Add(
            MakeRow(
                later,
                U("09-11"),
                U("合作方要求延长支持期"),
                U("目标时间之后出现"),
                U("后来信息"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        laterSizer->Add(
            MakeRow(
                later,
                U("09-12"),
                U("安全复核承诺延期"),
                U("截止日期改为 9 月 25 日"),
                U("后来信息"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        laterSizer->Add(
            MakeRow(
                later,
                U("09-18"),
                U("交付日期改为 11 月 30 日"),
                U("需求文件 v7"),
                U("后来信息"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );

        laterSizer->AddStretchSpacer();
        laterSizer->Add(
            MakeWrappedText(
                later,
                U("知识边界保护已启用。后来出现的信息不会混入当时状态。"),
                9,
                Theme::Blue(),
                260,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            17
        );
        later->SetSizer(laterSizer);

        body->Add(summary, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(state, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(later, 0, wxEXPAND);
        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);

        SetSizer(root);
    }
};

struct GraphNode final
{
    wxPoint position;
    int radius;
    wxString code;
    wxString label;
    wxColour color;
};

class RelationCanvas final : public wxPanel
{
public:
    explicit RelationCanvas(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetBackgroundColour(Theme::Input());
        SetMinSize(wxSize(600, 480));
        Bind(wxEVT_PAINT, &RelationCanvas::OnPaint, this);
    }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(Theme::Input()));
        dc.Clear();

        const wxSize size = GetClientSize();
        const int centerX = size.GetWidth() / 2;
        const int centerY = size.GetHeight() / 2;

        const std::array<GraphNode, 7> nodes = {{
            {{centerX, centerY}, 64, U("D-0031"), U("双轨迁移方案"), Theme::Purple()},
            {{centerX - 205, centerY - 125}, 53, U("E-1042"), U("会议纪要证据"), Theme::Green()},
            {{centerX + 205, centerY - 125}, 53, U("E-1110"), U("合作方反馈"), Theme::Red()},
            {{centerX + 235, centerY + 90}, 55, U("R-0018"), U("审计复杂度"), Theme::Red()},
            {{centerX - 15, centerY + 185}, 54, U("C-0042"), U("安全复核"), Theme::Cyan()},
            {{centerX - 235, centerY + 85}, 54, U("D-0027"), U("旧迁移方案"), Theme::Yellow()},
            {{centerX + 185, centerY + 195}, 51, U("F-0054"), U("保留旧协议"), Theme::Green()}
        }};

        const std::array<std::pair<int, int>, 7> edges = {{
            {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {4, 6}
        }};

        dc.SetPen(wxPen(Theme::BorderStrong(), 2));

        for (const auto& edge : edges)
        {
            dc.DrawLine(
                nodes[edge.first].position,
                nodes[edge.second].position
            );
        }

        for (const auto& node : nodes)
        {
            dc.SetBrush(wxBrush(Theme::Surface2()));
            dc.SetPen(wxPen(node.color, 2));
            dc.DrawCircle(node.position, node.radius);

            dc.SetFont(Theme::Font(9, wxFONTWEIGHT_BOLD));
            dc.SetTextForeground(node.color);

            wxSize codeExtent = dc.GetTextExtent(node.code);
            dc.DrawText(
                node.code,
                node.position.x - codeExtent.x / 2,
                node.position.y - 16
            );

            dc.SetFont(Theme::Font(8, wxFONTWEIGHT_SEMIBOLD));
            dc.SetTextForeground(Theme::Text());

            wxSize labelExtent = dc.GetTextExtent(node.label);
            dc.DrawText(
                node.label,
                node.position.x - labelExtent.x / 2,
                node.position.y + 5
            );
        }
    }
};

class RelationshipBrowserPage final : public wxPanel
{
public:
    explicit RelationshipBrowserPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P14"),
            U("关系浏览器"),
            U("调查支持、反对、替代、依赖、阻塞和影响关系"),
            U("保存关系视图")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* query = MakeCard(this);
        query->SetMinSize(wxSize(270, -1));
        auto* querySizer = new wxBoxSizer(wxVERTICAL);
        querySizer->Add(
            MakeText(query, U("路径查询"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::pair<wxString, wxString>, 5> fields = {{
            {U("起点对象"), U("D-0031 双轨迁移方案")},
            {U("终点对象"), U("不限")},
            {U("关系类型"), U("全部关系")},
            {U("最大深度"), U("3 层")},
            {U("时间范围"), U("全部时间")}
        }};

        for (const auto& field : fields)
        {
            querySizer->Add(
                MakeText(query, field.first, 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
                0,
                wxLEFT | wxRIGHT | wxBOTTOM,
                17
            );
            querySizer->Add(
                MakeInput(query, field.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                17
            );
        }

        querySizer->Add(
            MakeButton(query, U("运行路径查询"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        querySizer->Add(
            new wxStaticLine(query, wxID_ANY),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        querySizer->Add(
            MakeText(query, U("图例"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxColour>, 6> legend = {{
            {U("支持"), Theme::Green()},
            {U("反对"), Theme::Red()},
            {U("替代"), Theme::Yellow()},
            {U("影响"), Theme::Blue()},
            {U("依赖"), Theme::Purple()},
            {U("阻塞"), Theme::Orange()}
        }};

        for (const auto& item : legend)
        {
            querySizer->Add(
                MakePill(query, item.first, item.second, 88),
                0,
                wxLEFT | wxBOTTOM,
                17
            );
        }
        query->SetSizer(querySizer);

        auto* graph = MakeCard(this);
        auto* graphSizer = new wxBoxSizer(wxVERTICAL);
        auto* graphHeader = new wxBoxSizer(wxHORIZONTAL);
        graphHeader->Add(
            MakeText(graph, U("局部关系图"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        graphHeader->Add(
            MakeText(graph, U("图形  路径  表格"), 8, Theme::Muted()),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        graphSizer->Add(graphHeader, 0, wxEXPAND | wxALL, 17);
        graphSizer->Add(
            new RelationCanvas(graph),
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        graphSizer->Add(
            MakeText(
                graph,
                U("已显示 7 个节点、7 条关系 · 双击节点展开 · 滚轮缩放"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        graph->SetSizer(graphSizer);

        auto* inspector = MakeCard(this);
        inspector->SetMinSize(wxSize(300, -1));
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);
        inspectorSizer->Add(
            MakeText(
                inspector,
                U("节点检查器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        inspectorSizer->Add(
            MakePill(inspector, U("决策 D-0031"), Theme::Purple(), 118),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeText(
                inspector,
                U("采用双轨迁移方案"),
                14,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeProperty(inspector, U("入向关系"), U("3"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        inspectorSizer->Add(
            MakeProperty(inspector, U("出向关系"), U("3"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        inspectorSizer->Add(
            MakeProperty(inspector, U("关联证据"), U("4"), Theme::Purple()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("最短路径"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeRow(
                inspector,
                U("D-0031"),
                U("→ 影响 → R-0018"),
                U("双轨方案直接影响审计复杂度"),
                U("长度 1"),
                Theme::Blue(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->AddStretchSpacer();
        inspectorSizer->Add(
            MakeButton(inspector, U("打开完整对象详情"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeButton(inspector, U("以此节点为起点")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspector->SetSizer(inspectorSizer);

        body->Add(query, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(graph, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

}

wxWindow* CreateInvestigationPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P10"))
    {
        return new ConflictCenterPage(parent);
    }

    if (pageCode == U("P11"))
    {
        return new ChangeReviewPage(parent);
    }

    if (pageCode == U("P12"))
    {
        return new TimelinePage(parent);
    }

    if (pageCode == U("P13"))
    {
        return new HistoricalSlicePage(parent);
    }

    if (pageCode == U("P14"))
    {
        return new RelationshipBrowserPage(parent);
    }

    return nullptr;
}

}
