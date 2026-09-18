#include "OperationsPages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>
#include <vector>

#include <wx/button.h>
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

wxGauge* MakeProgress(
    wxWindow* parent,
    int value,
    int range = 100
)
{
    auto* gauge = new wxGauge(
        parent,
        wxID_ANY,
        range,
        wxDefaultPosition,
        wxSize(-1, 9),
        wxGA_HORIZONTAL
    );
    gauge->SetValue(value);
    gauge->SetForegroundColour(Theme::Blue());
    gauge->SetBackgroundColour(Theme::Surface3());
    return gauge;
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
            U("管理文件来源、扫描范围、监视状态、索引覆盖和访问异常"),
            U("重新扫描全部"),
            U("添加数据源")
        );

        auto* metrics = new wxBoxSizer(wxHORIZONTAL);
        const std::array<
            std::tuple<wxString, wxString, wxString, wxColour>,
            4
        > metricData = {{
            {U("4"), U("数据源"), U("3 个当前可用"), Theme::Blue()},
            {U("12,846"), U("发现文件"), U("12,531 个受支持"), Theme::Green()},
            {U("3"), U("访问异常"), U("等待用户处理"), Theme::Yellow()},
            {U("418 MB"), U("工作区占用"), U("不含用户源文件"), Theme::Purple()}
        }};

        for (std::size_t index = 0; index < metricData.size(); ++index)
        {
            const auto& metric = metricData[index];
            metrics->Add(
                MakeMetric(
                    this,
                    std::get<0>(metric),
                    std::get<1>(metric),
                    std::get<2>(metric),
                    std::get<3>(metric)
                ),
                1,
                index + 1 < metricData.size() ? wxRIGHT : 0,
                index + 1 < metricData.size() ? 12 : 0
            );
        }
        root->Add(metrics, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* groups = MakeCard(this);
        groups->SetMinSize(wxSize(230, -1));
        auto* groupSizer = new wxBoxSizer(wxVERTICAL);
        groupSizer->Add(
            MakeText(groups, U("数据源分组"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 5> categories = {{
            {U("全部数据源"), U("4"), Theme::Blue()},
            {U("文件夹"), U("2"), Theme::Green()},
            {U("Git 仓库"), U("1"), Theme::Purple()},
            {U("邮件归档"), U("1"), Theme::Cyan()},
            {U("存在异常"), U("1"), Theme::Red()}
        }};

        for (std::size_t index = 0; index < categories.size(); ++index)
        {
            groupSizer->Add(
                MakeProperty(
                    groups,
                    std::get<0>(categories[index]),
                    std::get<1>(categories[index]),
                    std::get<2>(categories[index])
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        groupSizer->Add(
            new wxStaticLine(groups, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        groupSizer->Add(
            MakeText(groups, U("监视状态"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        groupSizer->Add(
            MakeProperty(groups, U("实时监视"), U("3"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        groupSizer->Add(
            MakeProperty(groups, U("手动扫描"), U("1"), Theme::Muted()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        groupSizer->AddStretchSpacer();
        groupSizer->Add(
            MakeButton(groups, U("导入数据源定义")),
            0,
            wxEXPAND | wxALL,
            17
        );
        groups->SetSizer(groupSizer);

        auto* sourceList = MakeCard(this);
        auto* sourceListSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(sourceList, wxEmptyString, 310);
        search->SetHint(U("搜索名称、路径或类型"));
        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(sourceList, U("类型：全部"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(sourceList, U("状态：全部"), false, false, 105),
            0
        );
        sourceListSizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } sources[] = {
            {
                "SRC-001",
                "研发仓库",
                "D:\\Pioneer\\repository · Git 仓库 · 5,302 个文件",
                "监视正常",
                Theme::Green()
            },
            {
                "SRC-002",
                "项目文档",
                "D:\\Pioneer\\docs · 文件夹 · 6,718 个文件",
                "扫描完成",
                Theme::Green()
            },
            {
                "SRC-003",
                "邮件归档",
                "D:\\Pioneer\\mail · 826 个邮件文件",
                "监视正常",
                Theme::Cyan()
            },
            {
                "SRC-004",
                "法律合同",
                "E:\\Legal\\contracts · 最后可访问于 9 月 15 日",
                "无法访问",
                Theme::Red()
            }
        };

        for (std::size_t index = 0; index < 4; ++index)
        {
            sourceListSizer->Add(
                MakeRow(
                    sourceList,
                    U(sources[index].code),
                    U(sources[index].title),
                    U(sources[index].detail),
                    U(sources[index].state),
                    sources[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        sourceListSizer->AddStretchSpacer();
        sourceListSizer->Add(
            MakeText(
                sourceList,
                U("源文件始终保留在用户目录中，工作区只保存索引、元数据和被引用内容快照。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        sourceList->SetSizer(sourceListSizer);

        auto* inspector = MakeCard(this);
        inspector->SetMinSize(wxSize(350, -1));
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);

        auto* inspectorHeader = new wxBoxSizer(wxHORIZONTAL);
        inspectorHeader->Add(
            MakeText(
                inspector,
                U("数据源详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        inspectorHeader->Add(
            MakePill(inspector, U("监视正常"), Theme::Green(), 100),
            0
        );
        inspectorSizer->Add(inspectorHeader, 0, wxEXPAND | wxALL, 17);

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("研发仓库"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 7> properties = {{
            {U("类型"), U("Git 仓库")},
            {U("根目录"), U("D:\\Pioneer\\repository")},
            {U("分支"), U("main")},
            {U("文件数量"), U("5,302")},
            {U("已索引"), U("5,287")},
            {U("排除"), U("15")},
            {U("最后扫描"), U("今天 12:49")}
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
                U("索引覆盖"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        inspectorSizer->Add(
            MakeProgress(inspector, 99),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeText(
                inspector,
                U("99.7% · 15 个文件被过滤规则排除"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(inspector, U("暂停监视")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(inspector, U("编辑设置"), true),
            1
        );
        inspectorSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        inspector->SetSizer(inspectorSizer);

        body->Add(groups, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(sourceList, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
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
            U("预扫描目录并配置导入、监视、OCR 和版本策略"),
            U("取消"),
            U("开始导入")
        );

        auto* steps = MakeCard(this);
        auto* stepSizer = new wxBoxSizer(wxHORIZONTAL);

        const std::array<std::tuple<wxString, wxString, wxColour>, 5> stepData = {{
            {U("✓"), U("选择类型"), Theme::Green()},
            {U("✓"), U("位置与范围"), Theme::Green()},
            {U("3"), U("解析策略"), Theme::Blue()},
            {U("4"), U("预扫描"), Theme::Muted()},
            {U("5"), U("确认添加"), Theme::Muted()}
        }};

        for (std::size_t index = 0; index < stepData.size(); ++index)
        {
            auto* step = MakeCard(
                steps,
                index == 2 ? Theme::Surface3() : Theme::Surface2()
            );
            auto* itemSizer = new wxBoxSizer(wxVERTICAL);
            itemSizer->Add(
                MakeText(
                    step,
                    std::get<0>(stepData[index]),
                    12,
                    std::get<2>(stepData[index]),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER | wxTOP | wxBOTTOM,
                10
            );
            itemSizer->Add(
                MakeText(
                    step,
                    std::get<1>(stepData[index]),
                    9,
                    index == 2 ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
            step->SetSizer(itemSizer);

            stepSizer->Add(
                step,
                1,
                index + 1 < stepData.size() ? wxRIGHT : 0,
                index + 1 < stepData.size() ? 10 : 0
            );
        }

        steps->SetSizer(stepSizer);
        root->Add(steps, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* settings = MakeCard(this);
        auto* settingsSizer = new wxBoxSizer(wxVERTICAL);
        settingsSizer->Add(
            MakeText(
                settings,
                U("解析与索引策略"),
                15,
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
                U("数据源：D:\\Pioneer\\docs · 预计 12,846 个文件"),
                9,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settingsSizer->Add(
            MakeText(
                settings,
                U("文件处理"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        const std::array<std::tuple<wxString, wxString, bool>, 4> options = {{
            {
                U("递归扫描子目录"),
                U("导入所有下级目录中的受支持文件"),
                true
            },
            {
                U("持续监视文件变化"),
                U("源文件变化后自动执行增量扫描"),
                true
            },
            {
                U("保留文档历史版本"),
                U("保存被业务对象引用过的内容版本"),
                true
            },
            {
                U("解析隐藏文件"),
                U("默认跳过 Windows 隐藏文件"),
                false
            }
        }};

        for (const auto& option : options)
        {
            auto* optionRow = MakeCard(settings, Theme::Surface2());
            auto* optionSizer = new wxBoxSizer(wxHORIZONTAL);
            optionSizer->Add(
                MakeText(
                    optionRow,
                    std::get<2>(option) ? U("●") : U("○"),
                    11,
                    std::get<2>(option) ? Theme::Blue() : Theme::Muted(),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT,
                13
            );

            auto* labels = new wxBoxSizer(wxVERTICAL);
            labels->Add(
                MakeText(
                    optionRow,
                    std::get<0>(option),
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxBOTTOM,
                4
            );
            labels->Add(
                MakeText(
                    optionRow,
                    std::get<1>(option),
                    8,
                    Theme::Muted()
                ),
                0
            );

            optionSizer->Add(labels, 1, wxALL, 10);
            optionRow->SetSizer(optionSizer);
            settingsSizer->Add(
                optionRow,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
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
                U("内容提取"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        auto* fieldGrid = new wxBoxSizer(wxHORIZONTAL);

        const std::array<std::pair<wxString, wxString>, 3> fields = {{
            {U("默认文本编码"), U("自动检测")},
            {U("默认时区"), U("Asia/Shanghai")},
            {U("单文件大小上限"), U("256 MB")}
        }};

        for (std::size_t index = 0; index < fields.size(); ++index)
        {
            auto* field = MakeCard(settings, Theme::Surface2());
            auto* fieldSizer = new wxBoxSizer(wxVERTICAL);
            fieldSizer->Add(
                MakeText(field, fields[index].first, 8, Theme::Faint()),
                0,
                wxBOTTOM,
                5
            );
            fieldSizer->Add(
                MakeText(
                    field,
                    fields[index].second,
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0
            );
            field->SetSizer(fieldSizer);
            fieldGrid->Add(
                field,
                1,
                index + 1 < fields.size() ? wxRIGHT : 0,
                index + 1 < fields.size() ? 10 : 0
            );
        }

        settingsSizer->Add(
            fieldGrid,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        settingsSizer->Add(
            MakeProperty(settings, U("扫描文档 OCR"), U("启用"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeProperty(
                settings,
                U("OCR 语言"),
                U("简体中文 + 英文")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeProperty(
                settings,
                U("资源策略"),
                U("空闲时执行重任务")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        settingsSizer->AddStretchSpacer();
        settingsSizer->Add(
            MakeWrappedText(
                settings,
                U("Office 文档解析不会执行宏、脚本、嵌入程序或远程资源。"),
                8,
                Theme::Yellow(),
                680,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            18
        );
        settings->SetSizer(settingsSizer);

        auto* preview = MakeCard(this);
        preview->SetMinSize(wxSize(370, -1));
        auto* previewSizer = new wxBoxSizer(wxVERTICAL);

        auto* previewHeader = new wxBoxSizer(wxHORIZONTAL);
        previewHeader->Add(
            MakeText(
                preview,
                U("导入预览"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        previewHeader->Add(
            MakePill(preview, U("预扫描完成"), Theme::Green(), 110),
            0
        );
        previewSizer->Add(previewHeader, 0, wxEXPAND | wxALL, 18);

        const std::array<std::tuple<wxString, wxString, wxColour>, 5> stats = {{
            {U("发现文件"), U("12,846"), Theme::Blue()},
            {U("支持格式"), U("12,531"), Theme::Green()},
            {U("将被排除"), U("287"), Theme::Muted()},
            {U("需要 OCR"), U("86"), Theme::Purple()},
            {U("存在异常"), U("3"), Theme::Yellow()}
        }};

        for (const auto& stat : stats)
        {
            previewSizer->Add(
                MakeProperty(
                    preview,
                    std::get<0>(stat),
                    std::get<1>(stat),
                    std::get<2>(stat)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        previewSizer->Add(
            new wxStaticLine(preview, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            18
        );
        previewSizer->Add(
            MakeText(
                preview,
                U("预计资源"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        previewSizer->Add(
            MakeProperty(preview, U("首次索引时间"), U("约 8—14 分钟")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        previewSizer->Add(
            MakeProperty(preview, U("工作区新增占用"), U("约 418 MB")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        previewSizer->Add(
            MakeProperty(preview, U("OCR 后台任务"), U("86 项")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        auto* warning = MakeCard(preview, Theme::Surface2());
        auto* warningSizer = new wxBoxSizer(wxVERTICAL);
        warningSizer->Add(
            MakeText(
                warning,
                U("3 个文件权限不足"),
                9,
                Theme::Yellow(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            6
        );
        warningSizer->Add(
            MakeWrappedText(
                warning,
                U("这些文件将被跳过并记录诊断，添加数据源后可单独重试。"),
                8,
                Theme::Muted(),
                300
            ),
            0
        );
        warning->SetSizer(warningSizer);
        previewSizer->Add(
            warning,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        previewSizer->AddStretchSpacer();
        previewSizer->Add(
            MakeButton(preview, U("查看完整预扫描清单")),
            0,
            wxEXPAND | wxALL,
            18
        );
        preview->SetSizer(previewSizer);

        body->Add(settings, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(preview, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

wxPanel* MakeTaskRow(
    wxWindow* parent,
    const wxString& title,
    const wxString& type,
    const wxString& detail,
    const wxString& state,
    int progress,
    const wxColour& color,
    bool selected = false
)
{
    auto* row = MakeCard(
        parent,
        selected ? Theme::Surface3() : Theme::Surface2()
    );
    row->SetMinSize(wxSize(-1, 82));

    auto* root = new wxBoxSizer(wxHORIZONTAL);
    auto* marker = new wxPanel(
        row,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(selected ? 5 : 3, -1)
    );
    marker->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(
            row,
            type + U(" · ") + detail,
            8,
            Theme::Muted()
        ),
        0,
        wxBOTTOM,
        7
    );

    auto* gauge = MakeProgress(row, progress);
    labels->Add(gauge, 0, wxEXPAND);

    root->Add(marker, 0, wxEXPAND | wxRIGHT, 12);
    root->Add(labels, 1, wxALL, 11);
    root->Add(
        MakePill(row, state, color),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(root);
    return row;
}

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
            U("后台任务中心"),
            U("查看、暂停、取消和重试扫描、解析、OCR、索引及导出任务"),
            U("暂停全部"),
            U("新建扫描任务")
        );

        auto* metrics = new wxBoxSizer(wxHORIZONTAL);
        const std::array<
            std::tuple<wxString, wxString, wxString, wxColour>,
            4
        > metricData = {{
            {U("1"), U("运行中"), U("扫描研发仓库"), Theme::Blue()},
            {U("4"), U("排队中"), U("等待前置任务"), Theme::Purple()},
            {U("38"), U("今天完成"), U("平均耗时 42 秒"), Theme::Green()},
            {U("2"), U("需要处理"), U("文件级错误"), Theme::Red()}
        }};

        for (std::size_t index = 0; index < metricData.size(); ++index)
        {
            const auto& metric = metricData[index];
            metrics->Add(
                MakeMetric(
                    this,
                    std::get<0>(metric),
                    std::get<1>(metric),
                    std::get<2>(metric),
                    std::get<3>(metric)
                ),
                1,
                index + 1 < metricData.size() ? wxRIGHT : 0,
                index + 1 < metricData.size() ? 12 : 0
            );
        }
        root->Add(metrics, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* queue = MakeCard(this);
        auto* queueSizer = new wxBoxSizer(wxVERTICAL);

        auto* queueHeader = new wxBoxSizer(wxHORIZONTAL);
        queueHeader->Add(
            MakeText(
                queue,
                U("任务队列"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        queueHeader->Add(
            MakeText(
                queue,
                U("活动任务 5 · 已完成 38 · 失败 2"),
                8,
                Theme::Muted()
            ),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        queueSizer->Add(queueHeader, 0, wxEXPAND | wxALL, 17);

        queueSizer->Add(
            MakeTaskRow(
                queue,
                U("扫描并解析研发仓库"),
                U("文件扫描"),
                U("3,615 / 5,302 · protocol.cpp"),
                U("运行中"),
                68,
                Theme::Blue(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeTaskRow(
                queue,
                U("识别会议白板图像"),
                U("OCR"),
                U("0 / 24 · 等待扫描完成"),
                U("排队中"),
                0,
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeTaskRow(
                queue,
                U("重建全文搜索索引"),
                U("索引"),
                U("0 / 1,284,903 · 优先级普通"),
                U("排队中"),
                0,
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeTaskRow(
                queue,
                U("检测事实与日期冲突"),
                U("冲突检测"),
                U("0 / 168 · 依赖索引任务"),
                U("排队中"),
                0,
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeTaskRow(
                queue,
                U("创建自动工作区快照"),
                U("备份"),
                U("0 / 1 · 计划任务"),
                U("排队中"),
                0,
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );

        queueSizer->AddStretchSpacer();
        queueSizer->Add(
            MakeText(
                queue,
                U("任务记录默认保留 30 天，不包含原始文档正文。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        queue->SetSizer(queueSizer);

        auto* detail = MakeCard(this);
        detail->SetMinSize(wxSize(365, -1));
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);

        auto* detailHeader = new wxBoxSizer(wxHORIZONTAL);
        detailHeader->Add(
            MakeText(
                detail,
                U("任务详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        detailHeader->Add(
            MakePill(detail, U("运行中"), Theme::Blue()),
            0
        );
        detailSizer->Add(detailHeader, 0, wxEXPAND | wxALL, 17);

        detailSizer->Add(
            MakeText(
                detail,
                U("扫描并解析研发仓库"),
                14,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        detailSizer->Add(
            MakeText(
                detail,
                U("任务 ID：JOB-20260918-041"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        detailSizer->Add(
            MakeProgress(detail, 68),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        detailSizer->Add(
            MakeProperty(detail, U("进度"), U("68% · 3,615 / 5,302"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );

        const std::array<std::pair<wxString, wxString>, 6> details = {{
            {U("当前文件"), U("protocol.cpp")},
            {U("已运行"), U("00:03:42")},
            {U("预计剩余"), U("约 00:01:45")},
            {U("平均速度"), U("16.2 文件/秒")},
            {U("错误"), U("2 个文件已跳过")},
            {U("优先级"), U("高")}
        }};

        for (std::size_t index = 0; index < details.size(); ++index)
        {
            detailSizer->Add(
                MakeProperty(
                    detail,
                    details[index].first,
                    details[index].second,
                    index == 4 ? Theme::Yellow() : Theme::Text()
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        detailSizer->Add(
            new wxStaticLine(detail, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        detailSizer->Add(
            MakeText(
                detail,
                U("资源预算"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        detailSizer->Add(
            MakeProperty(detail, U("CPU"), U("42%"), Theme::Cyan()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        detailSizer->Add(
            MakeProperty(detail, U("内存"), U("318 MB"), Theme::Purple()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        detailSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(detail, U("暂停")),
            1,
            wxRIGHT,
            8
        );
        actions->Add(
            MakeButton(detail, U("降低优先级")),
            1,
            wxRIGHT,
            8
        );
        actions->Add(
            MakeButton(detail, U("取消"), false, true),
            1
        );
        detailSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        detail->SetSizer(detailSizer);

        body->Add(queue, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
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
            U("工作区快照与备份"),
            U("创建、验证并恢复独立工作区备份"),
            U("验证全部"),
            U("创建快照")
        );

        auto* summary = MakeCard(this);
        auto* summarySizer = new wxBoxSizer(wxHORIZONTAL);

        auto* automatic = new wxBoxSizer(wxVERTICAL);
        automatic->Add(
            MakeText(
                summary,
                U("自动备份已启用"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            6
        );
        automatic->Add(
            MakeWrappedText(
                summary,
                U("每天 20:00 创建增量备份，保留最近 14 个日备份和 6 个周备份。"),
                8,
                Theme::Muted(),
                420
            ),
            0,
            wxBOTTOM,
            5
        );
        automatic->Add(
            MakeText(
                summary,
                U("D:\\UnknownSoftware\\backups\\Pioneer"),
                8,
                Theme::Blue()
            ),
            0
        );
        summarySizer->Add(automatic, 2, wxALL, 17);

        const std::array<std::tuple<wxString, wxString, wxColour>, 4> stats = {{
            {U("最近备份"), U("今天 12:40"), Theme::Green()},
            {U("备份总量"), U("4.8 GB"), Theme::Blue()},
            {U("已验证"), U("20 / 20"), Theme::Green()},
            {U("下次备份"), U("今天 20:00"), Theme::Purple()}
        }};

        for (const auto& stat : stats)
        {
            auto* statCard = MakeCard(summary, Theme::Surface2());
            auto* statSizer = new wxBoxSizer(wxVERTICAL);
            statSizer->Add(
                MakeText(statCard, std::get<0>(stat), 8, Theme::Muted()),
                0,
                wxBOTTOM,
                6
            );
            statSizer->Add(
                MakeText(
                    statCard,
                    std::get<1>(stat),
                    11,
                    std::get<2>(stat),
                    wxFONTWEIGHT_BOLD
                ),
                0
            );
            statCard->SetSizer(statSizer);
            summarySizer->Add(statCard, 1, wxALL, 12);
        }

        summary->SetSizer(summarySizer);
        root->Add(summary, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* history = MakeCard(this);
        auto* historySizer = new wxBoxSizer(wxVERTICAL);

        auto* historyHeader = new wxBoxSizer(wxHORIZONTAL);
        historyHeader->Add(
            MakeText(
                history,
                U("快照与备份历史"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        historyHeader->Add(
            MakeText(history, U("共 20 个备份"), 8, Theme::Muted()),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        historySizer->Add(historyHeader, 0, wxEXPAND | wxALL, 17);

        auto* searchRow = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(history, wxEmptyString, 310);
        search->SetHint(U("按名称、时间或备注搜索"));
        searchRow->Add(search, 1, wxRIGHT, 10);
        searchRow->Add(
            MakeButton(history, U("类型：全部"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        searchRow->Add(
            MakeButton(history, U("状态：正常"), false, false, 105),
            0
        );
        historySizer->Add(
            searchRow,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } backups[] = {
            {
                "BAK-020",
                "自动备份 · 2026-09-18 12:40",
                "增量 · 186 MB · 今天 12:40",
                "已验证",
                Theme::Green()
            },
            {
                "BAK-019",
                "修改前安全快照",
                "手动快照 · 412 MB · 今天 10:05",
                "已验证",
                Theme::Green()
            },
            {
                "BAK-018",
                "自动备份 · 2026-09-17",
                "增量 · 142 MB · 昨天 20:00",
                "已验证",
                Theme::Green()
            },
            {
                "BAK-011",
                "协议迁移决策前",
                "里程碑 · 1.1 GB · 9 月 9 日",
                "已验证",
                Theme::Green()
            },
            {
                "BAK-010",
                "自动备份 · 2026-09-08",
                "完整 · 1.0 GB · 9 月 8 日",
                "需复核",
                Theme::Yellow()
            }
        };

        for (std::size_t index = 0; index < 5; ++index)
        {
            historySizer->Add(
                MakeRow(
                    history,
                    U(backups[index].code),
                    U(backups[index].title),
                    U(backups[index].detail),
                    U(backups[index].state),
                    backups[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }
        historySizer->AddStretchSpacer();
        historySizer->Add(
            MakeText(
                history,
                U("选择备份后可在右侧查看内容和恢复影响。"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        history->SetSizer(historySizer);

        auto* detail = MakeCard(this);
        detail->SetMinSize(wxSize(410, -1));
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);

        auto* detailHeader = new wxBoxSizer(wxHORIZONTAL);
        detailHeader->Add(
            MakeText(
                detail,
                U("备份详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        detailHeader->Add(
            MakePill(detail, U("已验证"), Theme::Green()),
            0
        );
        detailSizer->Add(detailHeader, 0, wxEXPAND | wxALL, 17);

        detailSizer->Add(
            MakeText(
                detail,
                U("自动备份 · 2026-09-18 12:40"),
                13,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        detailSizer->Add(
            MakeText(
                detail,
                U("增量备份 · 格式版本 1.0"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 5> details = {{
            {U("工作区数据库"), U("正常 · 118 MB"), Theme::Green()},
            {U("搜索索引"), U("正常 · 54 MB"), Theme::Green()},
            {U("内容快照"), U("正常 · 14 MB"), Theme::Green()},
            {U("加密状态"), U("SQLCipher + DPAPI"), Theme::Blue()},
            {U("完整性清单"), U("238 项全部通过"), Theme::Green()}
        }};

        for (const auto& item : details)
        {
            detailSizer->Add(
                MakeProperty(
                    detail,
                    std::get<0>(item),
                    std::get<1>(item),
                    std::get<2>(item)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        detailSizer->Add(
            new wxStaticLine(detail, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );

        auto* policy = MakeCard(detail, Theme::Surface2());
        auto* policySizer = new wxBoxSizer(wxVERTICAL);
        policySizer->Add(
            MakeText(
                policy,
                U("恢复策略"),
                9,
                Theme::Blue(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        policySizer->Add(
            MakeWrappedText(
                policy,
                U("始终恢复为新工作区，不覆盖当前项目。"),
                8,
                Theme::Text(),
                330
            ),
            0
        );
        policy->SetSizer(policySizer);
        detailSizer->Add(
            policy,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        detailSizer->AddStretchSpacer();

        detailSizer->Add(
            MakeButton(detail, U("再次验证")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        detailSizer->Add(
            MakeButton(detail, U("导出独立备份")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        detailSizer->Add(
            MakeButton(detail, U("恢复为新工作区"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        detail->SetSizer(detailSizer);

        body->Add(history, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
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
