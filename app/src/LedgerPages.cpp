#include "LedgerPages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <wx/listbox.h>
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

wxPanel* MakeCard(wxWindow* parent, const wxColour& background = Theme::Surface())
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

wxTextCtrl* MakeSearch(
    wxWindow* parent,
    const wxString& hint,
    int width = -1
)
{
    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(width, 38),
        wxBORDER_NONE | wxTE_PROCESS_ENTER
    );
    input->SetBackgroundColour(Theme::Input());
    input->SetForegroundColour(Theme::Text());
    input->SetFont(Theme::Font(10));
    input->SetHint(hint);
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
    const wxString& action
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
        MakeButton(parent, U("更多操作"), false, false, 108),
        0,
        wxALIGN_BOTTOM | wxRIGHT,
        10
    );
    row->Add(
        MakeButton(parent, action, true, false, 132),
        0,
        wxALIGN_BOTTOM
    );

    root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 26);
}

wxPanel* MakeMetric(
    wxWindow* parent,
    const wxString& value,
    const wxString& title,
    const wxString& detail,
    const wxColour& color
)
{
    auto* panel = MakeCard(parent);
    panel->SetMinSize(wxSize(170, 90));

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* stripe = new wxPanel(
        panel,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(4, -1)
    );
    stripe->SetBackgroundColour(color);

    auto* content = new wxBoxSizer(wxVERTICAL);
    content->Add(
        MakeText(panel, value, 20, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    content->Add(
        MakeText(panel, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    content->Add(MakeText(panel, detail, 8, Theme::Muted()), 0);

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 13);
    root->Add(content, 1, wxALL, 13);
    panel->SetSizer(root);
    return panel;
}

wxPanel* MakeObjectRow(
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

    auto* content = new wxBoxSizer(wxVERTICAL);
    content->Add(
        MakeText(row, code, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    content->Add(
        MakeText(row, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    content->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 12);
    root->Add(content, 1, wxALL, 10);
    root->Add(
        MakePill(row, state, color),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );
    root->Add(
        MakeText(row, U("›"), 15, Theme::Faint(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(root);
    return row;
}

wxPanel* MakePropertyRow(
    wxWindow* parent,
    const wxString& label,
    const wxString& value,
    const wxColour& valueColor = Theme::Text()
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
        MakeText(row, value, 9, valueColor, wxFONTWEIGHT_SEMIBOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );
    row->SetSizer(layout);
    return row;
}

wxPanel* MakeTimelineRow(
    wxWindow* parent,
    const wxString& time,
    const wxString& title,
    const wxString& detail,
    const wxColour& color
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 59));

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* marker = new wxPanel(
        row,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(7, 7)
    );
    marker->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    root->Add(marker, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);
    root->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 9);
    root->Add(
        MakeText(row, time, 8, Theme::Faint()),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );
    row->SetSizer(root);
    return row;
}

class LedgerPage final : public wxPanel
{
public:
    explicit LedgerPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P06"),
            U("项目台账"),
            U("统一管理事实、决策、承诺、风险、问题及其证据状态"),
            U("创建对象")
        );

        auto* metrics = new wxBoxSizer(wxHORIZONTAL);
        const std::array<
            std::tuple<wxString, wxString, wxString, wxColour>,
            5
        > metricData = {{
            {U("18"), U("当前事实"), U("3 项最近变化"), Theme::Green()},
            {U("7"), U("有效决策"), U("1 项被替代"), Theme::Purple()},
            {U("11"), U("未完成承诺"), U("3 项即将到期"), Theme::Cyan()},
            {U("4"), U("活跃风险"), U("1 项高影响"), Theme::Red()},
            {U("2"), U("未决问题"), U("等待外部确认"), Theme::Orange()}
        }};

        for (std::size_t index = 0; index < metricData.size(); ++index)
        {
            const auto& item = metricData[index];
            metrics->Add(
                MakeMetric(
                    this,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item),
                    std::get<3>(item)
                ),
                1,
                index + 1 < metricData.size() ? wxRIGHT : 0,
                index + 1 < metricData.size() ? 11 : 0
            );
        }

        root->Add(metrics, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        filters->SetMinSize(wxSize(220, -1));
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);
        filterSizer->Add(
            MakeText(filters, U("对象类型"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 6> types = {{
            {U("全部对象"), U("42"), Theme::Blue()},
            {U("事实"), U("18"), Theme::Green()},
            {U("决策"), U("7"), Theme::Purple()},
            {U("承诺"), U("11"), Theme::Cyan()},
            {U("风险"), U("4"), Theme::Red()},
            {U("未决问题"), U("2"), Theme::Orange()}
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
                wxALIGN_CENTER_VERTICAL | wxALL,
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
                9
            );
        }

        filterSizer->Add(
            new wxStaticLine(filters, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        filterSizer->Add(
            MakeText(filters, U("快速筛选"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        for (const auto& label : {
            U("需要复核  11"),
            U("没有证据  8"),
            U("本周更新  15"),
            U("存在冲突  6")
        })
        {
            filterSizer->Add(
                MakeText(filters, label, 9, Theme::Muted()),
                0,
                wxLEFT | wxRIGHT | wxBOTTOM,
                17
            );
        }

        filters->SetSizer(filterSizer);

        auto* list = MakeCard(this);
        auto* listSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(
            MakeSearch(list, U("搜索编号、标题或内容"), 315),
            1,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(list, U("状态：全部"), false, false, 108),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(list, U("排序"), false, false, 80),
            0
        );
        listSizer->Add(toolbar, 0, wxEXPAND | wxALL, 16);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } rows[] = {
            {
                "D-0031",
                "采用双轨迁移方案",
                "决策 · 4 项证据 · 更新于今天 12:34",
                "已接受",
                Theme::Purple()
            },
            {
                "F-0054",
                "旧协议保留至第四季度",
                "事实 · 3 项证据 · 来源发生变化",
                "需复核",
                Theme::Yellow()
            },
            {
                "C-0042",
                "完成安全复核报告",
                "承诺 · 负责人周启明 · 截止 9 月 25 日",
                "进行中",
                Theme::Cyan()
            },
            {
                "R-0018",
                "双轨运行增加审计复杂度",
                "风险 · 高影响 · 中等可能性",
                "活跃",
                Theme::Red()
            },
            {
                "Q-0008",
                "合作方最终切换日期",
                "未决问题 · 等待交付委员会确认",
                "未决",
                Theme::Orange()
            },
            {
                "F-0062",
                "最终交付日期为 11 月 30 日",
                "事实 · 1 项证据 · 今天创建",
                "当前有效",
                Theme::Green()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            listSizer->Add(
                MakeObjectRow(
                    list,
                    U(rows[index].code),
                    U(rows[index].title),
                    U(rows[index].detail),
                    U(rows[index].state),
                    rows[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }
        list->SetSizer(listSizer);

        auto* inspector = MakeCard(this);
        inspector->SetMinSize(wxSize(330, -1));
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);

        auto* inspectorHeader = new wxBoxSizer(wxHORIZONTAL);
        inspectorHeader->Add(
            MakeText(
                inspector,
                U("对象预览"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        inspectorHeader->Add(
            MakePill(inspector, U("已接受"), Theme::Purple()),
            0
        );
        inspectorSizer->Add(inspectorHeader, 0, wxEXPAND | wxALL, 17);

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("D-0031"),
                9,
                Theme::Purple(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeText(
                inspector,
                U("采用双轨迁移方案"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeWrappedText(
                inspector,
                U("旧协议继续支持存量合作方，新协议先用于内部流量，并保留紧急回退能力。"),
                9,
                Theme::Muted(),
                285
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 4> properties = {{
            {U("决策时间"), U("2026-09-09 14:30")},
            {U("决策者"), U("交付委员会")},
            {U("支持证据"), U("3 项")},
            {U("反对证据"), U("1 项")}
        }};

        for (const auto& property : properties)
        {
            inspectorSizer->Add(
                MakePropertyRow(
                    inspector,
                    property.first,
                    property.second
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("关联对象"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        inspectorSizer->Add(
            MakeObjectRow(
                inspector,
                U("R-0018"),
                U("双轨审计复杂度"),
                U("受此决策影响"),
                U("影响"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->AddStretchSpacer();
        inspectorSizer->Add(
            MakeButton(inspector, U("打开完整对象详情"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        inspector->SetSizer(inspectorSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(list, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

struct DetailSpec final
{
    wxString code;
    wxString pageTitle;
    wxString pageSubtitle;
    wxString objectCode;
    wxString objectType;
    wxString objectTitle;
    wxString state;
    wxColour accent;
    wxString summary;
    wxString primaryAction;
    std::array<std::pair<wxString, wxString>, 5> properties;
    std::array<std::tuple<wxString, wxString, wxColour>, 3> evidence;
    std::array<std::tuple<wxString, wxString, wxString, wxColour>, 3> related;
    std::array<std::tuple<wxString, wxString, wxString, wxColour>, 3> timeline;
};

class DetailPage final : public wxPanel
{
public:
    DetailPage(wxWindow* parent, const DetailSpec& spec)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            spec.code,
            spec.pageTitle,
            spec.pageSubtitle,
            spec.primaryAction
        );

        auto* identity = MakeCard(this);
        auto* identitySizer = new wxBoxSizer(wxHORIZONTAL);

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                identity,
                spec.objectCode + U(" · ") + spec.objectType,
                9,
                spec.accent,
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        labels->Add(
            MakeText(
                identity,
                spec.objectTitle,
                18,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            8
        );
        labels->Add(
            MakeWrappedText(
                identity,
                spec.summary,
                9,
                Theme::Muted(),
                820
            ),
            0
        );

        identitySizer->Add(labels, 1, wxALL, 18);
        identitySizer->Add(
            MakePill(identity, spec.state, spec.accent, 105),
            0,
            wxALIGN_TOP | wxALL,
            18
        );
        identity->SetSizer(identitySizer);
        root->Add(identity, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* main = MakeCard(this);
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);
        mainSizer->Add(
            MakeText(
                main,
                U("对象信息"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        for (const auto& property : spec.properties)
        {
            mainSizer->Add(
                MakePropertyRow(main, property.first, property.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        mainSizer->Add(
            MakeText(
                main,
                U("证据与依据"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        for (const auto& item : spec.evidence)
        {
            mainSizer->Add(
                MakeObjectRow(
                    main,
                    std::get<0>(item),
                    std::get<1>(item),
                    U("可回溯到原始来源"),
                    std::get<1>(item).Contains(U("反对"))
                        ? U("反对")
                        : U("支持"),
                    std::get<2>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        auto* evidenceActions = new wxBoxSizer(wxHORIZONTAL);
        evidenceActions->Add(
            MakeButton(main, U("关联已有证据")),
            1,
            wxRIGHT,
            10
        );
        evidenceActions->Add(
            MakeButton(main, U("从文件创建证据"), true),
            1
        );
        mainSizer->Add(
            evidenceActions,
            0,
            wxEXPAND | wxALL,
            17
        );
        main->SetSizer(mainSizer);

        auto* relations = MakeCard(this);
        relations->SetMinSize(wxSize(350, -1));
        auto* relationsSizer = new wxBoxSizer(wxVERTICAL);
        relationsSizer->Add(
            MakeText(
                relations,
                U("关系与影响"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        for (const auto& item : spec.related)
        {
            relationsSizer->Add(
                MakeObjectRow(
                    relations,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item),
                    U("关联"),
                    std::get<3>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        relationsSizer->Add(
            MakeButton(relations, U("打开关系浏览器")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        relationsSizer->Add(
            new wxStaticLine(relations, wxID_ANY),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        relationsSizer->Add(
            MakeText(
                relations,
                U("历史记录"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        for (const auto& item : spec.timeline)
        {
            relationsSizer->Add(
                MakeTimelineRow(
                    relations,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item),
                    std::get<3>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        relationsSizer->AddStretchSpacer();
        relationsSizer->Add(
            MakeButton(relations, U("查看完整审计历史"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        relations->SetSizer(relationsSizer);

        body->Add(main, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(relations, 0, wxEXPAND);
        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);

        SetSizer(root);
    }
};

DetailSpec ObjectSpec()
{
    return {
        U("P07"),
        U("对象详情"),
        U("查看结构化对象的字段、证据、关系和完整审计历史"),
        U("F-0054"),
        U("事实"),
        U("旧协议保留至第四季度"),
        U("需要复核"),
        Theme::Yellow(),
        U("旧协议将在 2026 年第四季度内保持可用，最终关闭日期由交付委员会另行确认。"),
        U("保存修改"),
        {{
            {U("主体"), U("旧通信协议")},
            {U("属性"), U("支持期限")},
            {U("归一化值"), U("2026 年第四季度")},
            {U("生效时间"), U("2026-09-09")},
            {U("负责人"), U("交付委员会")}
        }},
        {{
            {U("E-1097"), U("支持 · requirements-v7.pdf"), Theme::Yellow()},
            {U("E-1042"), U("支持 · 会议纪要-0909.md"), Theme::Green()},
            {U("E-0841"), U("反对 · legacy-contract.pdf"), Theme::Red()}
        }},
        {{
            {U("D-0031"), U("采用双轨迁移方案"), U("此事实支持该决策"), Theme::Purple()},
            {U("C-0042"), U("完成安全复核"), U("完成后才能关闭旧协议"), Theme::Cyan()},
            {U("R-0018"), U("双轨审计复杂度"), U("支持期延长增加风险"), Theme::Red()}
        }},
        {{
            {U("12:34"), U("标记需要复核"), U("来源内容发生变化"), Theme::Yellow()},
            {U("09-10"), U("关联证据 E-1097"), U("周启明"), Theme::Green()},
            {U("09-09"), U("创建事实"), U("来源于决策会议"), Theme::Blue()}
        }}
    };
}

DetailSpec DecisionSpec()
{
    return {
        U("P08"),
        U("决策详情"),
        U("查看决策依据、替代方案、反对意见、影响和接受记录"),
        U("D-0031"),
        U("决策"),
        U("采用双轨迁移方案"),
        U("已接受"),
        Theme::Purple(),
        U("旧协议继续支持存量合作方，新协议先用于内部流量，并在验证完成前保留回退通道。"),
        U("更新决策"),
        {{
            {U("决策状态"), U("已接受")},
            {U("决策时间"), U("2026-09-09 14:30")},
            {U("决策者"), U("交付委员会")},
            {U("替代决策"), U("D-0027")},
            {U("复核日期"), U("2026-10-15")}
        }},
        {{
            {U("E-1042"), U("支持 · 委员会会议纪要"), Theme::Green()},
            {U("E-1124"), U("支持 · 协议评审邮件"), Theme::Green()},
            {U("E-1110"), U("反对 · 合作方反馈"), Theme::Red()}
        }},
        {{
            {U("D-0027"), U("直接切换新协议"), U("被当前决策替代"), Theme::Yellow()},
            {U("R-0018"), U("双轨审计复杂度"), U("由此决策引入"), Theme::Red()},
            {U("C-0038"), U("完成环境验证"), U("依赖当前决策"), Theme::Cyan()}
        }},
        {{
            {U("09-18"), U("增加复核标记"), U("证据 E-1097 发生变化"), Theme::Yellow()},
            {U("09-09"), U("决策被接受"), U("交付委员会确认"), Theme::Purple()},
            {U("09-09"), U("创建决策草案"), U("由会议提取结果创建"), Theme::Blue()}
        }}
    };
}

DetailSpec CommitmentSpec()
{
    return {
        U("P09"),
        U("承诺详情"),
        U("管理承诺负责人、截止日期、依赖条件、进度和完成证据"),
        U("C-0042"),
        U("承诺"),
        U("完成安全复核报告"),
        U("进行中"),
        Theme::Cyan(),
        U("在最终交付前完成安全复核报告，覆盖双轨运行、紧急回退和审计记录迁移。"),
        U("更新进度"),
        {{
            {U("负责人"), U("周启明")},
            {U("截止日期"), U("2026-09-25")},
            {U("当前进度"), U("68%")},
            {U("优先级"), U("高")},
            {U("完成证据"), U("尚未提供")}
        }},
        {{
            {U("E-1130"), U("支持 · 安全评审邮件"), Theme::Green()},
            {U("E-1042"), U("支持 · 委员会会议纪要"), Theme::Green()},
            {U("E-1141"), U("反对 · 资源排期说明"), Theme::Red()}
        }},
        {{
            {U("D-0031"), U("采用双轨迁移方案"), U("承诺来源决策"), Theme::Purple()},
            {U("C-0038"), U("完成环境验证"), U("前置依赖"), Theme::Cyan()},
            {U("R-0018"), U("双轨审计复杂度"), U("需要在报告中控制"), Theme::Red()}
        }},
        {{
            {U("09-18"), U("进度更新为 68%"), U("周启明"), Theme::Cyan()},
            {U("09-12"), U("截止日期延期"), U("9 月 18 日调整至 9 月 25 日"), Theme::Yellow()},
            {U("09-09"), U("创建承诺"), U("由决策 D-0031 派生"), Theme::Blue()}
        }}
    };
}

}

wxWindow* CreateLedgerWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P06"))
    {
        return new LedgerPage(parent);
    }

    if (pageCode == U("P07"))
    {
        return new DetailPage(parent, ObjectSpec());
    }

    if (pageCode == U("P08"))
    {
        return new DetailPage(parent, DecisionSpec());
    }

    if (pageCode == U("P09"))
    {
        return new DetailPage(parent, CommitmentSpec());
    }

    return nullptr;
}

}
