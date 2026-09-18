#include "DeliveryPages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>

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
        wxSize(width, multiline ? 80 : 38),
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

wxPanel* MakeChapterRow(
    wxWindow* parent,
    const wxString& title,
    const wxString& detail,
    bool enabled,
    bool selected = false
)
{
    auto* row = MakeCard(
        parent,
        selected ? Theme::Surface3() : Theme::Surface2()
    );
    row->SetMinSize(wxSize(-1, 50));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    layout->Add(
        MakeText(row, U("≡"), 11, Theme::Faint(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT,
        11
    );
    layout->Add(
        MakeText(
            row,
            enabled ? U("☑") : U("☐"),
            11,
            enabled ? Theme::Blue() : Theme::Muted(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        10
    );

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        3
    );
    labels->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    layout->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 8);
    layout->Add(
        MakeText(row, U("•••"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        11
    );

    row->SetSizer(layout);
    return row;
}

class ReportPreviewPanel final : public wxPanel
{
public:
    explicit ReportPreviewPanel(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        SetBackgroundColour(wxColour(238, 235, 226));

        auto* root = new wxBoxSizer(wxVERTICAL);
        root->Add(
            MakeText(
                this,
                U("续证 CONTINUUM"),
                8,
                wxColour(65, 91, 138),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            28
        );
        root->Add(
            MakeText(
                this,
                U("有效决策"),
                20,
                wxColour(32, 41, 54),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            28
        );
        root->Add(
            MakeText(
                this,
                U("先锋计划交接包 · 当前状态"),
                8,
                wxColour(95, 105, 118)
            ),
            0,
            wxLEFT | wxRIGHT | wxTOP,
            28
        );

        auto* rule = new wxStaticLine(this, wxID_ANY);
        rule->SetForegroundColour(wxColour(196, 191, 181));
        root->Add(rule, 0, wxEXPAND | wxALL, 28);

        root->Add(
            MakeText(
                this,
                U("D-0031  采用双轨迁移方案"),
                12,
                wxColour(32, 41, 54),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );
        root->Add(
            MakeText(
                this,
                U("状态：已接受 · 决策时间：2026-09-09"),
                8,
                wxColour(95, 105, 118)
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );
        root->Add(
            MakeWrappedText(
                this,
                U(
                    "旧协议保留至第四季度，新协议先用于内部流量。"
                    "合作方切换时间由交付委员会另行确认。"
                ),
                10,
                wxColour(42, 51, 64),
                390
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );

        auto* reason = new wxPanel(this, wxID_ANY);
        reason->SetBackgroundColour(wxColour(220, 226, 236));

        auto* reasonSizer = new wxBoxSizer(wxVERTICAL);
        reasonSizer->Add(
            MakeText(
                reason,
                U("决策依据"),
                8,
                wxColour(69, 83, 103),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        reasonSizer->Add(
            MakeWrappedText(
                reason,
                U("一次性迁移无法满足兼容与回退要求。关联 3 项支持证据、1 项反对证据。"),
                8,
                wxColour(42, 51, 64),
                350
            ),
            0
        );
        reason->SetSizer(reasonSizer);
        root->Add(reason, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 28);

        root->Add(
            MakeText(
                this,
                U("证据引用"),
                10,
                wxColour(32, 41, 54),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );

        for (const auto& reference : {
            U("[E-1042] 会议纪要-0909.md，第 42—43 行"),
            U("[E-1097] requirements-v7.pdf，第 18 页"),
            U("[E-1124] 协议评审.eml，正文第 6 段")
        })
        {
            root->Add(
                MakeText(
                    this,
                    reference,
                    8,
                    wxColour(49, 94, 168)
                ),
                0,
                wxLEFT | wxRIGHT | wxBOTTOM,
                28
            );
        }

        root->AddStretchSpacer();
        root->Add(
            MakeText(
                this,
                U("3"),
                8,
                wxColour(95, 105, 118)
            ),
            0,
            wxALIGN_RIGHT | wxALL,
            20
        );

        SetSizer(root);
    }
};

class HandoverCapsulePage final : public wxPanel
{
public:
    explicit HandoverCapsulePage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P15"),
            U("交接胶囊编辑器"),
            U("组织可验证、可脱敏、可离线阅读的项目交接包"),
            U("保存为草稿"),
            U("预览并验证")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* steps = MakeCard(this);
        steps->SetMinSize(wxSize(220, -1));
        auto* stepsSizer = new wxBoxSizer(wxVERTICAL);
        stepsSizer->Add(
            MakeText(steps, U("胶囊配置"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, bool>, 5> stepData = {{
            {U("1"), U("用途与范围"), true},
            {U("2"), U("选择内容"), true},
            {U("3"), U("章节编排"), true},
            {U("4"), U("隐私与附件"), false},
            {U("5"), U("验证与导出"), false}
        }};

        for (const auto& step : stepData)
        {
            auto* row = MakeCard(steps, Theme::Surface2());
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            rowSizer->Add(
                MakeText(
                    row,
                    std::get<2>(step) ? U("✓") : std::get<0>(step),
                    10,
                    std::get<2>(step) ? Theme::Green() : Theme::Muted(),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT,
                12
            );
            rowSizer->Add(
                MakeText(
                    row,
                    std::get<1>(step),
                    9,
                    std::get<2>(step) ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                1,
                wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,
                12
            );

            row->SetSizer(rowSizer);
            stepsSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        stepsSizer->Add(
            new wxStaticLine(steps, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            16
        );
        stepsSizer->Add(
            MakeText(steps, U("胶囊信息"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        stepsSizer->Add(
            MakeProperty(steps, U("名称"), U("先锋计划交接包")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        stepsSizer->Add(
            MakeProperty(steps, U("时间范围"), U("当前状态")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        stepsSizer->Add(
            MakeProperty(steps, U("用途"), U("项目交接"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        stepsSizer->Add(
            MakeText(steps, U("完成度 68%"), 8, Theme::Blue(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* progress = new wxGauge(
            steps,
            wxID_ANY,
            100,
            wxDefaultPosition,
            wxSize(-1, 10),
            wxGA_HORIZONTAL
        );
        progress->SetValue(68);
        progress->SetForegroundColour(Theme::Blue());
        progress->SetBackgroundColour(Theme::Surface3());
        stepsSizer->Add(
            progress,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        steps->SetSizer(stepsSizer);

        auto* chapters = MakeCard(this);
        chapters->SetMinSize(wxSize(300, -1));
        auto* chapterSizer = new wxBoxSizer(wxVERTICAL);

        auto* chapterHeader = new wxBoxSizer(wxHORIZONTAL);
        chapterHeader->Add(
            MakeText(
                chapters,
                U("章节结构"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        chapterHeader->Add(
            MakeButton(chapters, U("添加章节"), false, false, 90),
            0
        );
        chapterSizer->Add(chapterHeader, 0, wxEXPAND | wxALL, 17);

        const std::array<std::tuple<wxString, wxString, bool>, 10> chapterData = {{
            {U("项目概要"), U("自动摘要"), true},
            {U("当前关键事实"), U("18 项"), true},
            {U("有效决策"), U("7 项"), true},
            {U("未完成承诺"), U("11 项"), true},
            {U("活跃风险"), U("4 项"), true},
            {U("未决问题"), U("2 项"), true},
            {U("冲突与复核"), U("6 项"), true},
            {U("推荐阅读顺序"), U("手工编排"), false},
            {U("证据索引"), U("127 项"), true},
            {U("完整性清单"), U("自动生成"), true}
        }};

        for (std::size_t index = 0; index < chapterData.size(); ++index)
        {
            chapterSizer->Add(
                MakeChapterRow(
                    chapters,
                    std::get<0>(chapterData[index]),
                    std::get<1>(chapterData[index]),
                    std::get<2>(chapterData[index]),
                    index == 2
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                7
            );
        }

        chapterSizer->AddStretchSpacer();
        chapterSizer->Add(
            MakeText(
                chapters,
                U("拖动章节可调整离线报告顺序"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        chapters->SetSizer(chapterSizer);

        auto* preview = MakeCard(this);
        auto* previewSizer = new wxBoxSizer(wxVERTICAL);

        auto* previewHeader = new wxBoxSizer(wxHORIZONTAL);
        previewHeader->Add(
            MakeText(
                preview,
                U("报告预览"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        previewHeader->Add(
            MakeText(preview, U("页面 3 / 18"), 8, Theme::Muted()),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        previewSizer->Add(previewHeader, 0, wxEXPAND | wxALL, 17);

        previewSizer->Add(
            new ReportPreviewPanel(preview),
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );
        preview->SetSizer(previewSizer);

        auto* settings = MakeCard(this);
        settings->SetMinSize(wxSize(250, -1));
        auto* settingsSizer = new wxBoxSizer(wxVERTICAL);
        settingsSizer->Add(
            MakeText(
                settings,
                U("章节设置"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        settingsSizer->Add(
            MakeText(
                settings,
                U("有效决策"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        for (const auto& property : {
            std::pair<wxString, wxString>(U("包含规则"), U("当前有效决策")),
            std::pair<wxString, wxString>(U("排序方式"), U("重要程度")),
            std::pair<wxString, wxString>(U("支持证据"), U("包含")),
            std::pair<wxString, wxString>(U("反对证据"), U("包含")),
            std::pair<wxString, wxString>(U("关系摘要"), U("显示")),
            std::pair<wxString, wxString>(U("完整历史"), U("不显示"))
        })
        {
            settingsSizer->Add(
                MakeProperty(settings, property.first, property.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        settingsSizer->Add(
            new wxStaticLine(settings, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        settingsSizer->Add(
            MakeText(
                settings,
                U("本章节统计"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        settingsSizer->Add(
            MakeText(settings, U("7 项有效决策"), 9, Theme::Text()),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        settingsSizer->Add(
            MakeText(settings, U("24 项关联证据"), 9, Theme::Text()),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        settingsSizer->Add(
            MakeText(settings, U("2 项需要复核"), 9, Theme::Yellow()),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        settingsSizer->AddStretchSpacer();
        settingsSizer->Add(
            MakeButton(settings, U("编辑人工说明")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        settingsSizer->Add(
            MakeButton(settings, U("检查本章节"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        settings->SetSizer(settingsSizer);

        body->Add(steps, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(chapters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(preview, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(settings, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

wxPanel* MakeValidationRow(
    wxWindow* parent,
    const wxString& title,
    const wxString& result,
    const wxColour& color
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 58));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    auto* marker = new wxPanel(
        row,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(8, 8)
    );
    marker->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    labels->Add(
        MakeText(row, result, 8, color, wxFONTWEIGHT_BOLD),
        0
    );

    layout->Add(marker, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);
    layout->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 9);
    layout->Add(
        MakeText(row, U("›"), 14, Theme::Faint()),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(layout);
    return row;
}

class ExportValidationPage final : public wxPanel
{
public:
    explicit ExportValidationPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P16"),
            U("导出预览与验证"),
            U("检查引用、附件、脱敏策略和离线包完整性"),
            U("返回编辑器"),
            U("开始导出")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* settings = MakeCard(this);
        settings->SetMinSize(wxSize(280, -1));
        auto* settingsSizer = new wxBoxSizer(wxVERTICAL);
        settingsSizer->Add(
            MakeText(settings, U("导出设置"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const std::array<std::pair<wxString, wxString>, 5> fields = {{
            {U("格式"), U("独立 HTML 报告")},
            {U("输出位置"), U("D:\\Exports\\Pioneer")},
            {U("原始资料"), U("包含选定文件")},
            {U("证据截图"), U("包含")},
            {U("历史版本"), U("不包含")}
        }};

        for (const auto& field : fields)
        {
            settingsSizer->Add(
                MakeProperty(settings, field.first, field.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        settingsSizer->Add(
            new wxStaticLine(settings, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        settingsSizer->Add(
            MakeText(settings, U("安全"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        settingsSizer->Add(
            MakeProperty(settings, U("导出口令"), U("已启用"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("移除绝对路径"), U("已启用"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("敏感内容脱敏"), U("已启用"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        settingsSizer->Add(
            MakeText(settings, U("预计大小"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        settingsSizer->Add(
            MakeText(settings, U("186 MB"), 20, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeText(
                settings,
                U("报告 4.2 MB · 附件 181.8 MB"),
                8,
                Theme::Muted()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        settingsSizer->AddStretchSpacer();
        settingsSizer->Add(
            MakeButton(settings, U("重新计算大小")),
            0,
            wxEXPAND | wxALL,
            17
        );
        settings->SetSizer(settingsSizer);

        auto* preview = MakeCard(this);
        auto* previewSizer = new wxBoxSizer(wxVERTICAL);

        auto* previewHeader = new wxBoxSizer(wxHORIZONTAL);
        previewHeader->Add(
            MakeText(
                preview,
                U("离线报告预览"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        previewHeader->Add(
            MakeText(
                preview,
                U("18 页 · 10 个章节"),
                8,
                Theme::Muted()
            ),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        previewSizer->Add(previewHeader, 0, wxEXPAND | wxALL, 17);
        previewSizer->Add(
            new ReportPreviewPanel(preview),
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );
        previewSizer->Add(
            MakeText(
                preview,
                U("所有内部链接和证据锚点均在导出后重新检查"),
                8,
                Theme::Faint()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        preview->SetSizer(previewSizer);

        auto* validation = MakeCard(this);
        validation->SetMinSize(wxSize(350, -1));
        auto* validationSizer = new wxBoxSizer(wxVERTICAL);

        auto* validationHeader = new wxBoxSizer(wxHORIZONTAL);
        validationHeader->Add(
            MakeText(
                validation,
                U("导出验证"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        validationHeader->Add(
            MakePill(validation, U("可以导出"), Theme::Green(), 100),
            0
        );
        validationSizer->Add(validationHeader, 0, wxEXPAND | wxALL, 17);

        const std::array<std::tuple<wxString, wxString, wxColour>, 6> checks = {{
            {U("报告内部链接"), U("146 / 146 通过"), Theme::Green()},
            {U("证据引用"), U("127 / 127 通过"), Theme::Green()},
            {U("附件可访问性"), U("35 / 36 通过"), Theme::Yellow()},
            {U("文件内容指纹"), U("36 / 36 通过"), Theme::Green()},
            {U("敏感路径检查"), U("已移除 18 项"), Theme::Blue()},
            {U("高严重度冲突"), U("仍有 2 项"), Theme::Red()}
        }};

        for (const auto& check : checks)
        {
            validationSizer->Add(
                MakeValidationRow(
                    validation,
                    std::get<0>(check),
                    std::get<1>(check),
                    std::get<2>(check)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        auto* warning = MakeCard(validation, Theme::Surface2());
        auto* warningSizer = new wxBoxSizer(wxVERTICAL);
        warningSizer->Add(
            MakeText(
                warning,
                U("1 个附件不可访问"),
                9,
                Theme::Yellow(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        warningSizer->Add(
            MakeWrappedText(
                warning,
                U("legacy-contract.pdf 的源路径失联，报告将保留引用但不会包含该附件。"),
                8,
                Theme::Muted(),
                290
            ),
            0
        );
        warning->SetSizer(warningSizer);
        validationSizer->Add(
            warning,
            0,
            wxEXPAND | wxALL,
            17
        );

        validationSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(validation, U("详细清单")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(validation, U("忽略并继续"), true),
            1
        );
        validationSizer->Add(actions, 0, wxEXPAND | wxALL, 17);
        validation->SetSizer(validationSizer);

        body->Add(settings, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(preview, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(validation, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

class RecycleBinPage final : public wxPanel
{
public:
    explicit RecycleBinPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("P17"),
            U("回收站"),
            U("恢复或彻底清理已软删除的文件记录、证据和业务对象"),
            U("恢复所选"),
            U("清空回收站")
        );

        auto* content = MakeCard(this);
        auto* contentSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(content, wxEmptyString, 360);
        search->SetHint(U("搜索已删除内容"));

        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(content, U("类型：全部"), false, false, 108),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(content, U("删除时间"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(content, U("删除原因"), false, false, 105),
            0
        );
        toolbar->AddStretchSpacer();
        toolbar->Add(
            MakeText(
                content,
                U("共 14 项 · 28.6 MB"),
                8,
                Theme::Muted()
            ),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        contentSizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        auto* header = MakeCard(content, Theme::Surface3());
        auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);
        headerSizer->Add(
            MakeText(header, U("对象"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            3,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        headerSizer->Add(
            MakeText(header, U("类型"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        headerSizer->Add(
            MakeText(header, U("删除时间"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        headerSizer->Add(
            MakeText(header, U("删除原因"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            2,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        headerSizer->Add(
            MakeText(header, U("引用影响"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        headerSizer->Add(
            MakeText(header, U("操作"), 9, Theme::Muted(), wxFONTWEIGHT_BOLD),
            1,
            wxALIGN_CENTER_VERTICAL | wxALL,
            12
        );
        header->SetSizer(headerSizer);
        contentSizer->Add(
            header,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            17
        );

        const std::array<
            std::tuple<
                wxString,
                wxString,
                wxString,
                wxString,
                wxString,
                wxString,
                wxColour
            >,
            7
        > rows = {{
            {
                U("E-0984"),
                U("旧版协议范围证据"),
                U("证据"),
                U("今天 11:22"),
                U("合并重复证据"),
                U("2 项对象引用"),
                Theme::Green()
            },
            {
                U("D-0011"),
                U("直接切换新协议"),
                U("决策"),
                U("昨天 18:10"),
                U("方案已废弃"),
                U("无当前引用"),
                Theme::Purple()
            },
            {
                U("C-0029"),
                U("完成旧网关停机"),
                U("承诺"),
                U("昨天 16:45"),
                U("误创建"),
                U("1 项历史引用"),
                Theme::Cyan()
            },
            {
                U("FILE-077"),
                U("old-protocol.md"),
                U("文件记录"),
                U("9 月 16 日"),
                U("源文件已删除"),
                U("3 项证据引用"),
                Theme::Blue()
            },
            {
                U("R-0007"),
                U("供应商退出风险"),
                U("风险"),
                U("9 月 14 日"),
                U("与 R-0014 合并"),
                U("审计历史保留"),
                Theme::Red()
            },
            {
                U("F-0032"),
                U("交付日期为 10 月 1 日"),
                U("事实"),
                U("9 月 12 日"),
                U("错误录入"),
                U("无引用"),
                Theme::Green()
            },
            {
                U("TAG-019"),
                U("临时讨论"),
                U("标签"),
                U("9 月 10 日"),
                U("未使用标签"),
                U("无引用"),
                Theme::Orange()
            }
        }};

        for (std::size_t index = 0; index < rows.size(); ++index)
        {
            auto* row = MakeCard(
                content,
                index % 2 == 0 ? Theme::Surface() : Theme::Surface2()
            );
            row->SetMinSize(wxSize(-1, 63));

            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            auto* object = new wxBoxSizer(wxVERTICAL);
            object->Add(
                MakeText(
                    row,
                    std::get<0>(rows[index]),
                    8,
                    Theme::Blue(),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxBOTTOM,
                4
            );
            object->Add(
                MakeText(
                    row,
                    std::get<1>(rows[index]),
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0
            );

            rowSizer->Add(object, 3, wxALIGN_CENTER_VERTICAL | wxALL, 12);
            rowSizer->Add(
                MakePill(
                    row,
                    std::get<2>(rows[index]),
                    std::get<6>(rows[index])
                ),
                1,
                wxALIGN_CENTER_VERTICAL | wxALL,
                8
            );
            rowSizer->Add(
                MakeText(row, std::get<3>(rows[index]), 8, Theme::Muted()),
                1,
                wxALIGN_CENTER_VERTICAL | wxALL,
                12
            );
            rowSizer->Add(
                MakeText(row, std::get<4>(rows[index]), 8, Theme::Text()),
                2,
                wxALIGN_CENTER_VERTICAL | wxALL,
                12
            );

            const wxColour impactColor =
                std::get<5>(rows[index]).Contains(U("引用"))
                && !std::get<5>(rows[index]).Contains(U("无"))
                ? Theme::Yellow()
                : Theme::Muted();

            rowSizer->Add(
                MakeText(
                    row,
                    std::get<5>(rows[index]),
                    8,
                    impactColor,
                    wxFONTWEIGHT_SEMIBOLD
                ),
                1,
                wxALIGN_CENTER_VERTICAL | wxALL,
                12
            );
            rowSizer->Add(
                MakeText(
                    row,
                    U("恢复  •••"),
                    8,
                    Theme::Blue(),
                    wxFONTWEIGHT_BOLD
                ),
                1,
                wxALIGN_CENTER_VERTICAL | wxALL,
                12
            );

            row->SetSizer(rowSizer);
            contentSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT,
                17
            );
        }

        contentSizer->AddStretchSpacer();

        auto* warning = MakeCard(content, Theme::Surface2());
        auto* warningSizer = new wxBoxSizer(wxHORIZONTAL);
        warningSizer->Add(
            MakeText(
                warning,
                U("●"),
                9,
                Theme::Red(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT,
            12
        );
        warningSizer->Add(
            MakeWrappedText(
                warning,
                U(
                    "彻底删除不可撤销；数据库物理页可能在维护压缩前继续存在。"
                    "加密工作区仍由工作区密钥保护。"
                ),
                8,
                Theme::Muted(),
                900
            ),
            1,
            wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,
            12
        );
        warning->SetSizer(warningSizer);

        contentSizer->Add(
            warning,
            0,
            wxEXPAND | wxALL,
            17
        );

        auto* footer = new wxBoxSizer(wxHORIZONTAL);
        footer->Add(
            MakeText(content, U("已选择 0 项"), 8, Theme::Faint()),
            0
        );
        footer->AddStretchSpacer();
        footer->Add(
            MakeText(
                content,
                U("自动清理策略：从不自动清理"),
                8,
                Theme::Faint()
            ),
            0
        );
        contentSizer->Add(
            footer,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        content->SetSizer(contentSizer);
        root->Add(content, 1, wxEXPAND | wxALL, 26);
        SetSizer(root);
    }
};

}

wxWindow* CreateDeliveryPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P15"))
    {
        return new HandoverCapsulePage(parent);
    }

    if (pageCode == U("P16"))
    {
        return new ExportValidationPage(parent);
    }

    if (pageCode == U("P17"))
    {
        return new RecycleBinPage(parent);
    }

    return nullptr;
}

}
