#include "GovernancePages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/gauge.h>
#include <wx/listbox.h>
#include <wx/panel.h>
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
    const wxString& value = wxEmptyString,
    int width = -1,
    bool multiline = false,
    bool readOnly = false
)
{
    long style = wxBORDER_NONE;

    if (multiline)
    {
        style |= wxTE_MULTILINE;
    }

    if (readOnly)
    {
        style |= wxTE_READONLY;
    }

    auto* input = new wxTextCtrl(
        parent,
        wxID_ANY,
        value,
        wxDefaultPosition,
        wxSize(width, multiline ? 88 : 38),
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

wxPanel* MakeProperty(
    wxWindow* parent,
    const wxString& label,
    const wxString& value,
    const wxColour& color = Theme::Text()
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 43));

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

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle,
    const wxString& secondaryAction,
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
        MakeButton(parent, secondaryAction, false, false, 125),
        0,
        wxALIGN_BOTTOM | wxRIGHT,
        10
    );
    row->Add(
        MakeButton(parent, primaryAction, true, false, 145),
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
    auto* card = MakeCard(parent);
    card->SetMinSize(wxSize(170, 88));

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* stripe = new wxPanel(
        card,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(4, -1)
    );
    stripe->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(card, value, 19, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    labels->Add(
        MakeText(card, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(MakeText(card, detail, 8, Theme::Muted()), 0);

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 13);
    root->Add(labels, 1, wxALL, 12);
    card->SetSizer(root);
    return card;
}

void AddMetrics(
    wxWindow* parent,
    wxBoxSizer* root,
    const std::array<
        std::tuple<wxString, wxString, wxString, wxColour>,
        4
    >& values
)
{
    auto* metrics = new wxBoxSizer(wxHORIZONTAL);

    for (std::size_t index = 0; index < values.size(); ++index)
    {
        const auto& item = values[index];
        metrics->Add(
            MakeMetric(
                parent,
                std::get<0>(item),
                std::get<1>(item),
                std::get<2>(item),
                std::get<3>(item)
            ),
            1,
            index + 1 < values.size() ? wxRIGHT : 0,
            index + 1 < values.size() ? 12 : 0
        );
    }

    root->Add(metrics, 0, wxEXPAND | wxALL, 26);
}

class TagsEntitiesPage final : public wxPanel
{
public:
    explicit TagsEntitiesPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M05"),
            U("标签与实体管理"),
            U("管理标签词表、人员、组织、系统和自动识别实体"),
            U("合并重复项"),
            U("创建标签")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("38"), U("标签"), U("用于 246 个对象"), Theme::Blue()},
                {U("64"), U("人员实体"), U("7 个待确认"), Theme::Cyan()},
                {U("22"), U("组织实体"), U("2 组可能重复"), Theme::Purple()},
                {U("17"), U("系统实体"), U("3 个缺少别名"), Theme::Green()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* taxonomy = MakeCard(this);
        taxonomy->SetMinSize(wxSize(245, -1));
        auto* taxonomySizer = new wxBoxSizer(wxVERTICAL);
        taxonomySizer->Add(
            MakeText(
                taxonomy,
                U("词表与实体类型"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> groups = {{
            {U("全部项目"), U("141"), Theme::Blue()},
            {U("项目标签"), U("38"), Theme::Blue()},
            {U("人员"), U("64"), Theme::Cyan()},
            {U("组织"), U("22"), Theme::Purple()},
            {U("系统与组件"), U("17"), Theme::Green()},
            {U("地点"), U("6"), Theme::Yellow()},
            {U("待确认实体"), U("9"), Theme::Red()}
        }};

        for (std::size_t index = 0; index < groups.size(); ++index)
        {
            taxonomySizer->Add(
                MakeProperty(
                    taxonomy,
                    std::get<0>(groups[index]),
                    std::get<1>(groups[index]),
                    std::get<2>(groups[index])
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        taxonomySizer->Add(
            new wxStaticLine(taxonomy, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        taxonomySizer->Add(
            MakeText(
                taxonomy,
                U("标签组"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        for (const auto& label : {
            U("阶段"),
            U("工作流"),
            U("产品域"),
            U("严重程度"),
            U("交付批次")
        })
        {
            taxonomySizer->Add(
                MakeText(taxonomy, U("•  ") + label, 9, Theme::Muted()),
                0,
                wxLEFT | wxRIGHT | wxBOTTOM,
                15
            );
        }

        taxonomySizer->AddStretchSpacer();
        taxonomySizer->Add(
            MakeButton(taxonomy, U("管理词表层级")),
            0,
            wxEXPAND | wxALL,
            17
        );
        taxonomy->SetSizer(taxonomySizer);

        auto* items = MakeCard(this);
        auto* itemsSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(items, wxEmptyString, 330);
        search->SetHint(U("搜索标签、实体名称或别名"));
        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(items, U("类型：全部"), false, false, 108),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(items, U("使用次数"), false, false, 102),
            0
        );
        itemsSizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } rows[] = {
            {
                "TAG-004",
                "协议迁移",
                "项目标签 · 使用于 42 个对象",
                "已确认",
                Theme::Blue()
            },
            {
                "PERSON-018",
                "周启明",
                "人员 · 3 个别名 · 关联 31 个对象",
                "已确认",
                Theme::Cyan()
            },
            {
                "ORG-007",
                "交付委员会",
                "组织 · 关联 18 个决策与承诺",
                "已确认",
                Theme::Purple()
            },
            {
                "SYS-003",
                "旧通信协议",
                "系统 · 别名 Legacy Protocol",
                "已确认",
                Theme::Green()
            },
            {
                "PERSON-041",
                "Qiming Zhou",
                "可能与 PERSON-018 为同一人员",
                "可能重复",
                Theme::Yellow()
            },
            {
                "ORG-019",
                "合作方技术团队",
                "自动识别 · 缺少正式组织名称",
                "待确认",
                Theme::Red()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            itemsSizer->Add(
                MakeRow(
                    items,
                    U(rows[index].code),
                    U(rows[index].title),
                    U(rows[index].detail),
                    U(rows[index].state),
                    rows[index].color,
                    index == 1
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        itemsSizer->AddStretchSpacer();
        itemsSizer->Add(
            MakeText(
                items,
                U("显示 6 / 141 · 自动识别实体必须经过确认后才用于规则匹配。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        items->SetSizer(itemsSizer);

        auto* inspector = MakeCard(this);
        inspector->SetMinSize(wxSize(355, -1));
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);

        auto* inspectorHeader = new wxBoxSizer(wxHORIZONTAL);
        inspectorHeader->Add(
            MakeText(
                inspector,
                U("实体详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        inspectorHeader->Add(
            MakePill(inspector, U("已确认"), Theme::Cyan()),
            0
        );
        inspectorSizer->Add(inspectorHeader, 0, wxEXPAND | wxALL, 17);

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("周启明"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 6> properties = {{
            {U("实体类型"), U("人员")},
            {U("规范名称"), U("周启明")},
            {U("英文名称"), U("Qiming Zhou")},
            {U("电子邮件"), U("qiming@example.local")},
            {U("所属组织"), U("先锋计划组")},
            {U("引用次数"), U("31")}
        }};

        for (const auto& property : properties)
        {
            inspectorSizer->Add(
                MakeProperty(inspector, property.first, property.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("其他别名"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        inspectorSizer->Add(
            MakeProperty(inspector, U("别名 1"), U("Qiming")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        inspectorSizer->Add(
            MakeProperty(inspector, U("别名 2"), U("周工")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->Add(
            MakeRow(
                inspector,
                U("PERSON-041"),
                U("Qiming Zhou"),
                U("系统判断相似度 96%"),
                U("建议合并"),
                Theme::Yellow(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(inspector, U("合并实体")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(inspector, U("保存修改"), true),
            1
        );
        inspectorSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        inspector->SetSizer(inspectorSizer);

        body->Add(taxonomy, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(items, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class SavedQueriesPage final : public wxPanel
{
public:
    explicit SavedQueriesPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M06"),
            U("保存的查询"),
            U("创建可复用的搜索、过滤器、调查视图和定期审查入口"),
            U("导入查询"),
            U("创建查询")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("16"), U("保存的查询"), U("5 个固定到导航"), Theme::Blue()},
                {U("6"), U("台账查询"), U("覆盖全部对象类型"), Theme::Green()},
                {U("5"), U("调查查询"), U("关系与历史分析"), Theme::Purple()},
                {U("3"), U("计划运行"), U("下次运行今天 18:00"), Theme::Cyan()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* groups = MakeCard(this);
        groups->SetMinSize(wxSize(230, -1));
        auto* groupSizer = new wxBoxSizer(wxVERTICAL);
        groupSizer->Add(
            MakeText(
                groups,
                U("查询分组"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 6> categories = {{
            {U("全部查询"), U("16"), Theme::Blue()},
            {U("我的查询"), U("9"), Theme::Cyan()},
            {U("工作区共享"), U("7"), Theme::Purple()},
            {U("固定到导航"), U("5"), Theme::Green()},
            {U("计划运行"), U("3"), Theme::Yellow()},
            {U("存在错误"), U("1"), Theme::Red()}
        }};

        for (const auto& category : categories)
        {
            groupSizer->Add(
                MakeProperty(
                    groups,
                    std::get<0>(category),
                    std::get<1>(category),
                    std::get<2>(category)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        groupSizer->AddStretchSpacer();
        groupSizer->Add(
            MakeButton(groups, U("管理查询分组")),
            0,
            wxEXPAND | wxALL,
            17
        );
        groups->SetSizer(groupSizer);

        auto* queries = MakeCard(this);
        auto* querySizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(queries, wxEmptyString, 300);
        search->SetHint(U("搜索查询名称或说明"));
        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(queries, U("范围：全部"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(queries, U("最近运行"), false, false, 105),
            0
        );
        querySizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } queryRows[] = {
            {
                "QUERY-016",
                "本周需要处理的高风险事项",
                "台账 · 8 个结果 · 今天 12:58",
                "已固定",
                Theme::Red()
            },
            {
                "QUERY-014",
                "没有有效证据的当前事实",
                "证据审查 · 6 个结果 · 每天 18:00",
                "计划运行",
                Theme::Yellow()
            },
            {
                "QUERY-012",
                "未来七天到期的承诺",
                "台账 · 3 个结果 · 今天 09:00",
                "已固定",
                Theme::Cyan()
            },
            {
                "QUERY-009",
                "受 D-0031 影响的所有对象",
                "关系查询 · 深度 3 · 12 个结果",
                "调查",
                Theme::Purple()
            },
            {
                "QUERY-006",
                "最近变化且需要复核的证据",
                "变化审查 · 5 个结果",
                "共享",
                Theme::Blue()
            },
            {
                "QUERY-003",
                "已删除来源的活动对象",
                "查询字段已被规则更新移除",
                "需要修复",
                Theme::Red()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            querySizer->Add(
                MakeRow(
                    queries,
                    U(queryRows[index].code),
                    U(queryRows[index].title),
                    U(queryRows[index].detail),
                    U(queryRows[index].state),
                    queryRows[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        querySizer->AddStretchSpacer();
        querySizer->Add(
            MakeText(
                queries,
                U("查询只保存条件和显示设置，不复制结果数据。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        queries->SetSizer(querySizer);

        auto* editor = MakeCard(this);
        editor->SetMinSize(wxSize(390, -1));
        auto* editorSizer = new wxBoxSizer(wxVERTICAL);

        auto* editorHeader = new wxBoxSizer(wxHORIZONTAL);
        editorHeader->Add(
            MakeText(
                editor,
                U("查询编辑器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        editorHeader->Add(
            MakePill(editor, U("8 个结果"), Theme::Blue()),
            0
        );
        editorSizer->Add(editorHeader, 0, wxEXPAND | wxALL, 17);

        editorSizer->Add(
            MakeInput(editor, U("本周需要处理的高风险事项")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        editorSizer->Add(
            MakeText(
                editor,
                U("查询条件"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 5> filters = {{
            {U("对象类型"), U("风险、承诺、冲突")},
            {U("严重程度"), U("高")},
            {U("状态"), U("不等于已完成")},
            {U("更新时间"), U("本周")},
            {U("证据状态"), U("任意")}
        }};

        for (const auto& filter : filters)
        {
            editorSizer->Add(
                MakeProperty(editor, filter.first, filter.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        editorSizer->Add(
            MakeButton(editor, U("+ 添加条件")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        editorSizer->Add(
            MakeText(
                editor,
                U("显示与运行"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        editorSizer->Add(
            MakeProperty(editor, U("排序"), U("严重程度降序")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        editorSizer->Add(
            MakeProperty(editor, U("固定到导航"), U("是"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        editorSizer->Add(
            MakeProperty(editor, U("计划运行"), U("不自动运行")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        editorSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(editor, U("运行查询")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(editor, U("保存查询"), true),
            1
        );
        editorSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        editor->SetSizer(editorSizer);

        body->Add(groups, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(queries, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(editor, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class RulesManagementPage final : public wxPanel
{
public:
    explicit RulesManagementPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M07"),
            U("规则管理"),
            U("管理提取、归一化、冲突检测、验证和隐私规则"),
            U("导入规则包"),
            U("创建规则")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("34"), U("已启用规则"), U("覆盖 8 类对象"), Theme::Green()},
                {U("5"), U("草稿规则"), U("尚未参与处理"), Theme::Blue()},
                {U("2"), U("规则错误"), U("最近运行失败"), Theme::Red()},
                {U("1,248"), U("今日命中"), U("92% 自动通过"), Theme::Purple()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* categories = MakeCard(this);
        categories->SetMinSize(wxSize(230, -1));
        auto* categorySizer = new wxBoxSizer(wxVERTICAL);
        categorySizer->Add(
            MakeText(
                categories,
                U("规则类型"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> types = {{
            {U("全部规则"), U("41"), Theme::Blue()},
            {U("内容提取"), U("12"), Theme::Purple()},
            {U("字段归一化"), U("7"), Theme::Cyan()},
            {U("冲突检测"), U("8"), Theme::Red()},
            {U("证据验证"), U("6"), Theme::Green()},
            {U("隐私与脱敏"), U("5"), Theme::Yellow()},
            {U("自动化动作"), U("3"), Theme::Orange()}
        }};

        for (const auto& type : types)
        {
            categorySizer->Add(
                MakeProperty(
                    categories,
                    std::get<0>(type),
                    std::get<1>(type),
                    std::get<2>(type)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        categorySizer->Add(
            new wxStaticLine(categories, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        categorySizer->Add(
            MakeText(
                categories,
                U("运行状态"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        categorySizer->Add(
            MakeProperty(categories, U("启用"), U("34"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        categorySizer->Add(
            MakeProperty(categories, U("停用"), U("5"), Theme::Muted()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        categorySizer->Add(
            MakeProperty(categories, U("错误"), U("2"), Theme::Red()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        categories->SetSizer(categorySizer);

        auto* rules = MakeCard(this);
        auto* ruleSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(rules, wxEmptyString, 300);
        search->SetHint(U("搜索规则名称或编号"));
        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(rules, U("类型：全部"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(rules, U("状态：全部"), false, false, 105),
            0
        );
        ruleSizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } rows[] = {
            {
                "RULE-018",
                "同一主体的当前日期值冲突",
                "冲突检测 · 今日命中 6 次",
                "已启用",
                Theme::Green()
            },
            {
                "RULE-027",
                "中文日期表达归一化",
                "字段归一化 · 今日处理 418 次",
                "已启用",
                Theme::Cyan()
            },
            {
                "RULE-031",
                "证据来源指纹变化",
                "证据验证 · 今日命中 5 次",
                "已启用",
                Theme::Green()
            },
            {
                "RULE-034",
                "导出时移除绝对路径",
                "隐私与脱敏 · 今日处理 3 次",
                "已启用",
                Theme::Yellow()
            },
            {
                "RULE-038",
                "自动识别承诺负责人",
                "内容提取 · 测试集准确率 89%",
                "草稿",
                Theme::Blue()
            },
            {
                "RULE-041",
                "已删除决策引用检测",
                "字段 decision_state 已更名",
                "运行错误",
                Theme::Red()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            ruleSizer->Add(
                MakeRow(
                    rules,
                    U(rows[index].code),
                    U(rows[index].title),
                    U(rows[index].detail),
                    U(rows[index].state),
                    rows[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }
        ruleSizer->AddStretchSpacer();
        ruleSizer->Add(
            MakeText(
                rules,
                U("规则变更会写入审计日志；发布前可在历史快照上测试。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        rules->SetSizer(ruleSizer);

        auto* editor = MakeCard(this);
        editor->SetMinSize(wxSize(415, -1));
        auto* editorSizer = new wxBoxSizer(wxVERTICAL);

        auto* editorHeader = new wxBoxSizer(wxHORIZONTAL);
        editorHeader->Add(
            MakeText(
                editor,
                U("规则编辑器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        editorHeader->Add(
            MakePill(editor, U("已启用"), Theme::Green()),
            0
        );
        editorSizer->Add(editorHeader, 0, wxEXPAND | wxALL, 17);

        editorSizer->Add(
            MakeInput(editor, U("同一主体的当前日期值冲突")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        editorSizer->Add(
            MakeInput(
                editor,
                U("检测同一主体和属性同时存在多个当前日期值。"),
                -1,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 4> metadata = {{
            {U("规则类型"), U("冲突检测")},
            {U("适用对象"), U("事实")},
            {U("严重程度"), U("高")},
            {U("执行阶段"), U("对象更新后")}
        }};

        for (const auto& item : metadata)
        {
            editorSizer->Add(
                MakeProperty(editor, item.first, item.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        editorSizer->Add(
            MakeText(
                editor,
                U("条件表达式"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        editorSizer->Add(
            MakeInput(
                editor,
                U(
                    "object.type == \"fact\"\n"
                    "and object.temporal_state == \"current\"\n"
                    "and count_distinct(object.normalized_value) > 1"
                ),
                -1,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        editorSizer->Add(
            MakeProperty(editor, U("测试集"), U("128 个历史对象")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        editorSizer->Add(
            MakeProperty(editor, U("测试结果"), U("6 命中 · 0 错误"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        editorSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(editor, U("运行测试")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(editor, U("保存并发布"), true),
            1
        );
        editorSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        editor->SetSizer(editorSizer);

        body->Add(categories, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(rules, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(editor, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

wxPanel* MakeSettingToggle(
    wxWindow* parent,
    const wxString& title,
    const wxString& detail,
    bool enabled
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 62));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    layout->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxALL, 11);
    layout->Add(
        MakePill(
            row,
            enabled ? U("已启用") : U("已关闭"),
            enabled ? Theme::Green() : Theme::Muted(),
            88
        ),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );
    row->SetSizer(layout);
    return row;
}

class WorkspaceSettingsPage final : public wxPanel
{
public:
    explicit WorkspaceSettingsPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M08"),
            U("工作区设置"),
            U("配置工作区标识、时间语义、安全、存储和处理策略"),
            U("恢复默认设置"),
            U("保存设置")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* sections = MakeCard(this);
        sections->SetMinSize(wxSize(245, -1));
        auto* sectionSizer = new wxBoxSizer(wxVERTICAL);
        sectionSizer->Add(
            MakeText(
                sections,
                U("设置分类"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 8> groups = {{
            {U("常规"), U("名称与区域"), Theme::Blue()},
            {U("时间与日期"), U("时区与精度"), Theme::Cyan()},
            {U("安全与加密"), U("已启用"), Theme::Green()},
            {U("存储"), U("418 MB"), Theme::Purple()},
            {U("解析与 OCR"), U("中文 + 英文"), Theme::Yellow()},
            {U("后台任务"), U("自动调度"), Theme::Blue()},
            {U("审计与保留"), U("无限期"), Theme::Green()},
            {U("高级"), U("开发者选项"), Theme::Red()}
        }};

        for (std::size_t index = 0; index < groups.size(); ++index)
        {
            auto* row = MakeCard(
                sections,
                index == 0 ? Theme::Surface3() : Theme::Surface2()
            );
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            auto* labels = new wxBoxSizer(wxVERTICAL);
            labels->Add(
                MakeText(
                    row,
                    std::get<0>(groups[index]),
                    9,
                    index == 0 ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxBOTTOM,
                3
            );
            labels->Add(
                MakeText(
                    row,
                    std::get<1>(groups[index]),
                    8,
                    std::get<2>(groups[index])
                ),
                0
            );

            rowSizer->Add(labels, 1, wxALL, 10);
            rowSizer->Add(
                MakeText(row, U("›"), 13, Theme::Faint()),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                12
            );
            row->SetSizer(rowSizer);
            sectionSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        sectionSizer->AddStretchSpacer();
        sectionSizer->Add(
            MakeText(
                sections,
                U("设置变更将记录到工作区审计历史。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        sections->SetSizer(sectionSizer);

        auto* settings = MakeCard(this);
        auto* settingsSizer = new wxBoxSizer(wxVERTICAL);
        settingsSizer->Add(
            MakeText(
                settings,
                U("常规设置"),
                16,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            18
        );
        settingsSizer->Add(
            MakeText(
                settings,
                U("工作区标识"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        const std::array<std::pair<wxString, wxString>, 4> identity = {{
            {U("工作区名称"), U("先锋计划")},
            {U("工作区编号"), U("WS-PIONEER-2026")},
            {U("负责人"), U("周启明")},
            {U("默认语言"), U("简体中文")}
        }};

        for (const auto& item : identity)
        {
            auto* field = MakeCard(settings, Theme::Surface2());
            auto* fieldSizer = new wxBoxSizer(wxVERTICAL);
            fieldSizer->Add(
                MakeText(field, item.first, 8, Theme::Muted()),
                0,
                wxBOTTOM,
                6
            );
            auto* input = MakeInput(field, item.second);
            fieldSizer->Add(input, 0, wxEXPAND);
            field->SetSizer(fieldSizer);
            settingsSizer->Add(
                field,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        settingsSizer->Add(
            new wxStaticLine(settings, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            18
        );
        settingsSizer->Add(
            MakeText(
                settings,
                U("区域与时间"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        settingsSizer->Add(
            MakeProperty(settings, U("默认时区"), U("Asia/Shanghai")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("日期格式"), U("YYYY-MM-DD")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("周起始日"), U("星期一")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settingsSizer->Add(
            MakeText(
                settings,
                U("行为"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        settingsSizer->Add(
            MakeSettingToggle(
                settings,
                U("启动时打开上次页面"),
                U("保留最后一次导航位置和筛选条件"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeSettingToggle(
                settings,
                U("删除前要求确认"),
                U("软删除业务对象前显示影响摘要"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeSettingToggle(
                settings,
                U("发送匿名使用统计"),
                U("当前应用为离线优先，不发送遥测"),
                false
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settingsSizer->AddStretchSpacer();
        settingsSizer->Add(
            MakeWrappedText(
                settings,
                U("工作区名称和默认语言不会修改既有对象内容；时区变更会触发时间显示重新计算。"),
                8,
                Theme::Yellow(),
                700,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            18
        );
        settings->SetSizer(settingsSizer);

        auto* summary = MakeCard(this);
        summary->SetMinSize(wxSize(345, -1));
        auto* summarySizer = new wxBoxSizer(wxVERTICAL);
        summarySizer->Add(
            MakeText(
                summary,
                U("工作区状态"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        summarySizer->Add(
            MakePill(summary, U("安全状态正常"), Theme::Green(), 120),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> state = {{
            {U("数据库"), U("SQLCipher 加密"), Theme::Green()},
            {U("密钥保护"), U("Windows DPAPI"), Theme::Green()},
            {U("工作区位置"), U("D:\\UnknownSoftware\\data"), Theme::Blue()},
            {U("数据库大小"), U("118 MB"), Theme::Purple()},
            {U("搜索索引"), U("286 MB"), Theme::Purple()},
            {U("内容快照"), U("14 MB"), Theme::Purple()},
            {U("最后完整性检查"), U("今天 12:55"), Theme::Green()}
        }};

        for (const auto& item : state)
        {
            summarySizer->Add(
                MakeProperty(
                    summary,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        summarySizer->Add(
            new wxStaticLine(summary, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        summarySizer->Add(
            MakeText(
                summary,
                U("待保存变更"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        summarySizer->Add(
            MakeText(
                summary,
                U("当前没有未保存的设置变更"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        summarySizer->AddStretchSpacer();
        summarySizer->Add(
            MakeButton(summary, U("运行完整性检查")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        summarySizer->Add(
            MakeButton(summary, U("打开工作区目录")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        summary->SetSizer(summarySizer);

        body->Add(sections, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(settings, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(summary, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

}

wxWindow* CreateGovernancePage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("M05"))
    {
        return new TagsEntitiesPage(parent);
    }

    if (pageCode == U("M06"))
    {
        return new SavedQueriesPage(parent);
    }

    if (pageCode == U("M07"))
    {
        return new RulesManagementPage(parent);
    }

    if (pageCode == U("M08"))
    {
        return new WorkspaceSettingsPage(parent);
    }

    return nullptr;
}

}
