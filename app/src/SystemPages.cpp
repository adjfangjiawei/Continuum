#include "SystemPages.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>

#include <wx/button.h>
#include <wx/gauge.h>
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
        wxSize(width, multiline ? 90 : 38),
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

wxGauge* MakeProgress(wxWindow* parent, int value)
{
    auto* gauge = new wxGauge(
        parent,
        wxID_ANY,
        100,
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

wxPanel* MakeToggle(
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
            U("管理工作区加密、密钥保护、锁定策略和敏感数据处理"),
            U("运行安全检查"),
            U("保存安全设置")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("正常"), U("安全状态"), U("未发现高风险问题"), Theme::Green()},
                {U("AES-256"), U("数据库加密"), U("SQLCipher 页面加密"), Theme::Blue()},
                {U("DPAPI"), U("密钥保护"), U("当前 Windows 用户"), Theme::Purple()},
                {U("3"), U("受保护备份"), U("最近验证于今天"), Theme::Cyan()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* sections = MakeCard(this);
        sections->SetMinSize(wxSize(245, -1));
        auto* sectionSizer = new wxBoxSizer(wxVERTICAL);

        sectionSizer->Add(
            MakeText(
                sections,
                U("安全分类"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> groups = {{
            {U("加密概况"), U("正常"), Theme::Green()},
            {U("密钥管理"), U("DPAPI"), Theme::Purple()},
            {U("自动锁定"), U("15 分钟"), Theme::Blue()},
            {U("敏感内容"), U("3 条规则"), Theme::Yellow()},
            {U("剪贴板保护"), U("已启用"), Theme::Green()},
            {U("导出安全"), U("强制验证"), Theme::Cyan()},
            {U("安全事件"), U("0"), Theme::Green()}
        }};

        for (std::size_t index = 0; index < groups.size(); ++index)
        {
            auto* row = MakeCard(
                sections,
                index == 0 ? Theme::Surface3() : Theme::Surface2()
            );
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            rowSizer->Add(
                MakeText(
                    row,
                    std::get<0>(groups[index]),
                    9,
                    index == 0 ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                1,
                wxALL,
                11
            );
            rowSizer->Add(
                MakeText(
                    row,
                    std::get<1>(groups[index]),
                    8,
                    std::get<2>(groups[index]),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                11
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
            MakeButton(sections, U("查看安全事件")),
            0,
            wxEXPAND | wxALL,
            17
        );
        sections->SetSizer(sectionSizer);

        auto* settings = MakeCard(this);
        auto* settingsSizer = new wxBoxSizer(wxVERTICAL);

        settingsSizer->Add(
            MakeText(
                settings,
                U("工作区加密"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            18
        );

        auto* status = MakeCard(settings, Theme::Surface2());
        auto* statusSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* statusLabels = new wxBoxSizer(wxVERTICAL);
        statusLabels->Add(
            MakeText(
                status,
                U("数据库已加密"),
                11,
                Theme::Green(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            5
        );
        statusLabels->Add(
            MakeText(
                status,
                U("SQLCipher AES-256 · 页面大小 4096 字节"),
                8,
                Theme::Muted()
            ),
            0
        );

        statusSizer->Add(statusLabels, 1, wxALL, 14);
        statusSizer->Add(
            MakePill(status, U("保护正常"), Theme::Green(), 100),
            0,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            14
        );
        status->SetSizer(statusSizer);
        settingsSizer->Add(
            status,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 6> properties = {{
            {U("数据库算法"), U("AES-256-CBC"), Theme::Green()},
            {U("密钥派生"), U("PBKDF2-HMAC-SHA512"), Theme::Blue()},
            {U("密钥保护"), U("Windows DPAPI"), Theme::Purple()},
            {U("密钥作用域"), U("当前 Windows 用户"), Theme::Text()},
            {U("完整性算法"), U("HMAC-SHA512"), Theme::Green()},
            {U("最后轮换"), U("2026-08-18"), Theme::Text()}
        }};

        for (const auto& property : properties)
        {
            settingsSizer->Add(
                MakeProperty(
                    settings,
                    std::get<0>(property),
                    std::get<1>(property),
                    std::get<2>(property)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
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
                U("访问与锁定"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settingsSizer->Add(
            MakeProperty(settings, U("自动锁定"), U("闲置 15 分钟")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("恢复验证"), U("Windows 身份")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        settingsSizer->Add(
            MakeProperty(settings, U("失败尝试限制"), U("5 次")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settingsSizer->Add(
            MakeToggle(
                settings,
                U("最小化时锁定工作区"),
                U("进入后台后清除已解密的页面缓存"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeToggle(
                settings,
                U("剪贴板自动清理"),
                U("复制敏感内容后 60 秒清空剪贴板"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        settingsSizer->Add(
            MakeToggle(
                settings,
                U("允许未加密导出"),
                U("关闭后所有导出必须使用口令或安全目录"),
                false
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        settings->SetSizer(settingsSizer);

        auto* actions = MakeCard(this);
        actions->SetMinSize(wxSize(355, -1));
        auto* actionSizer = new wxBoxSizer(wxVERTICAL);

        actionSizer->Add(
            MakeText(
                actions,
                U("密钥与恢复"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        actionSizer->Add(
            MakeWrappedText(
                actions,
                U(
                    "工作区主密钥由当前 Windows 用户的 DPAPI 凭据保护。"
                    "恢复密钥可用于迁移到新设备，但不会自动上传。"
                ),
                9,
                Theme::Muted(),
                305
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        actionSizer->Add(
            MakeProperty(actions, U("恢复密钥"), U("已创建"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        actionSizer->Add(
            MakeProperty(actions, U("创建时间"), U("2026-08-18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        actionSizer->Add(
            MakeProperty(actions, U("最后验证"), U("今天 12:56"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        actionSizer->Add(
            MakeButton(actions, U("验证恢复密钥")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        actionSizer->Add(
            MakeButton(actions, U("导出恢复密钥"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        actionSizer->Add(
            new wxStaticLine(actions, wxID_ANY),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        actionSizer->Add(
            MakeText(
                actions,
                U("危险操作"),
                9,
                Theme::Red(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        actionSizer->Add(
            MakeWrappedText(
                actions,
                U("轮换主密钥会重写整个数据库，执行前将自动创建安全快照。"),
                8,
                Theme::Muted(),
                305
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        actionSizer->AddStretchSpacer();
        actionSizer->Add(
            MakeButton(actions, U("轮换工作区主密钥"), false, true),
            0,
            wxEXPAND | wxALL,
            17
        );
        actions->SetSizer(actionSizer);

        body->Add(sections, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(settings, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(actions, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class AuditLogPage final : public wxPanel
{
public:
    explicit AuditLogPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M10"),
            U("审计日志"),
            U("查询用户操作、对象变化、系统任务和安全事件"),
            U("验证日志链"),
            U("导出审计报告")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("1,284"), U("本月事件"), U("今天新增 47 项"), Theme::Blue()},
                {U("100%"), U("日志链完整"), U("哈希验证已通过"), Theme::Green()},
                {U("6"), U("关键变更"), U("需要保留说明"), Theme::Yellow()},
                {U("0"), U("安全异常"), U("最近 30 天"), Theme::Purple()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        filters->SetMinSize(wxSize(245, -1));
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);

        filterSizer->Add(
            MakeText(
                filters,
                U("事件筛选"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> types = {{
            {U("全部事件"), U("1,284"), Theme::Blue()},
            {U("对象变化"), U("642"), Theme::Green()},
            {U("证据操作"), U("218"), Theme::Cyan()},
            {U("文件与数据源"), U("176"), Theme::Purple()},
            {U("系统任务"), U("141"), Theme::Blue()},
            {U("设置变化"), U("92"), Theme::Yellow()},
            {U("安全事件"), U("15"), Theme::Red()}
        }};

        for (const auto& type : types)
        {
            filterSizer->Add(
                MakeProperty(
                    filters,
                    std::get<0>(type),
                    std::get<1>(type),
                    std::get<2>(type)
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
            MakeText(
                filters,
                U("时间范围"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakeProperty(filters, U("开始"), U("2026-09-01")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        filterSizer->Add(
            MakeProperty(filters, U("结束"), U("2026-09-18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        filterSizer->AddStretchSpacer();
        filterSizer->Add(
            MakeButton(filters, U("重置筛选")),
            0,
            wxEXPAND | wxALL,
            17
        );
        filters->SetSizer(filterSizer);

        auto* events = MakeCard(this);
        auto* eventSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        auto* search = MakeInput(events, wxEmptyString, 320);
        search->SetHint(U("搜索对象编号、用户或事件说明"));

        toolbar->Add(search, 1, wxRIGHT, 10);
        toolbar->Add(
            MakeButton(events, U("用户：全部"), false, false, 105),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(events, U("严重度"), false, false, 95),
            0
        );
        eventSizer->Add(toolbar, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* code;
            const char* title;
            const char* detail;
            const char* state;
            wxColour color;
        } rows[] = {
            {
                "AUD-1284",
                "事实 F-0054 被标记为需要复核",
                "今天 12:34 · 周启明 · 来源内容发生变化",
                "对象变化",
                Theme::Yellow()
            },
            {
                "AUD-1283",
                "完成数据源 SRC-001 增量扫描",
                "今天 12:31 · 系统任务 · 发现 3 个变化",
                "系统任务",
                Theme::Blue()
            },
            {
                "AUD-1282",
                "证据 E-1097 内容指纹验证失败",
                "今天 12:20 · 系统验证 · 已创建复核项",
                "证据操作",
                Theme::Cyan()
            },
            {
                "AUD-1281",
                "requirements-v7.pdf 更新为版本 v7",
                "今天 12:18 · 文件监视器 · SHA-256 已记录",
                "文件变化",
                Theme::Purple()
            },
            {
                "AUD-1279",
                "规则 RULE-018 发现日期冲突",
                "今天 12:16 · 规则引擎 · 创建 CF-0061",
                "规则命中",
                Theme::Red()
            },
            {
                "AUD-1274",
                "承诺 C-0042 进度更新为 68%",
                "今天 10:42 · 周启明 · 原值 55%",
                "对象变化",
                Theme::Green()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            eventSizer->Add(
                MakeRow(
                    events,
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

        eventSizer->AddStretchSpacer();
        eventSizer->Add(
            MakeText(
                events,
                U("显示 6 / 1,284 · 审计日志按事件时间倒序排列"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        events->SetSizer(eventSizer);

        auto* detail = MakeCard(this);
        detail->SetMinSize(wxSize(390, -1));
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);

        auto* detailHeader = new wxBoxSizer(wxHORIZONTAL);
        detailHeader->Add(
            MakeText(
                detail,
                U("事件详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        detailHeader->Add(
            MakePill(detail, U("日志链正常"), Theme::Green(), 110),
            0
        );
        detailSizer->Add(detailHeader, 0, wxEXPAND | wxALL, 17);

        detailSizer->Add(
            MakeText(
                detail,
                U("事实 F-0054 被标记为需要复核"),
                13,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 7> details = {{
            {U("事件编号"), U("AUD-1284")},
            {U("事件时间"), U("2026-09-18 12:34:18")},
            {U("操作者"), U("周启明")},
            {U("操作来源"), U("证据检查器")},
            {U("对象类型"), U("事实")},
            {U("对象编号"), U("F-0054")},
            {U("客户端会话"), U("SESSION-041")}
        }};

        for (const auto& item : details)
        {
            detailSizer->Add(
                MakeProperty(detail, item.first, item.second),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        detailSizer->Add(
            MakeText(
                detail,
                U("变化内容"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        detailSizer->Add(
            MakeInput(
                detail,
                U(
                    "evidence_state:\n"
                    "  before: verified\n"
                    "  after:  needs_review\n"
                    "reason: source_content_changed"
                ),
                -1,
                true,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        detailSizer->Add(
            MakeProperty(
                detail,
                U("事件哈希"),
                U("7f19…a20e"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        detailSizer->Add(
            MakeProperty(
                detail,
                U("前序哈希"),
                U("91d4…443b"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        detailSizer->AddStretchSpacer();
        detailSizer->Add(
            MakeButton(detail, U("打开关联对象"), true),
            0,
            wxEXPAND | wxALL,
            17
        );
        detail->SetSizer(detailSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(events, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

wxPanel* MakeCheckRow(
    wxWindow* parent,
    const wxString& title,
    const wxString& detail,
    const wxString& result,
    const wxColour& color
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 62));

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
    labels->Add(MakeText(row, detail, 8, Theme::Muted()), 0);

    layout->Add(marker, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 12);
    layout->Add(labels, 1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM, 10);
    layout->Add(
        MakeText(row, result, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(layout);
    return row;
}

class DiagnosticsPage final : public wxPanel
{
public:
    explicit DiagnosticsPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);
        AddHeading(
            this,
            root,
            U("M11"),
            U("诊断中心"),
            U("检查数据库、索引、文件访问、解析器和运行环境"),
            U("导出诊断包"),
            U("运行完整诊断")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("正常"), U("总体状态"), U("核心服务可用"), Theme::Green()},
                {U("9 / 10"), U("检查通过"), U("1 项建议处理"), Theme::Yellow()},
                {U("118 MB"), U("数据库"), U("完整性检查通过"), Theme::Purple()},
                {U("286 MB"), U("搜索索引"), U("最后更新 12:52"), Theme::Blue()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* checks = MakeCard(this);
        auto* checkSizer = new wxBoxSizer(wxVERTICAL);

        auto* checkHeader = new wxBoxSizer(wxHORIZONTAL);
        checkHeader->Add(
            MakeText(
                checks,
                U("系统检查"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        checkHeader->Add(
            MakePill(checks, U("9 项通过"), Theme::Green()),
            0
        );
        checkSizer->Add(checkHeader, 0, wxEXPAND | wxALL, 17);

        const std::array<
            std::tuple<wxString, wxString, wxString, wxColour>,
            8
        > checkData = {{
            {
                U("工作区数据库"),
                U("SQLite quick_check 返回 ok"),
                U("通过"),
                Theme::Green()
            },
            {
                U("数据库加密"),
                U("SQLCipher 密钥和 HMAC 验证正常"),
                U("通过"),
                Theme::Green()
            },
            {
                U("全文搜索索引"),
                U("索引段、词典和文档数量一致"),
                U("通过"),
                Theme::Green()
            },
            {
                U("证据内容指纹"),
                U("127 项证据完成一致性检查"),
                U("通过"),
                Theme::Green()
            },
            {
                U("数据源访问"),
                U("法律合同数据源当前不可访问"),
                U("警告"),
                Theme::Yellow()
            },
            {
                U("文件解析器"),
                U("PDF、Office、邮件和 Markdown 解析器已加载"),
                U("通过"),
                Theme::Green()
            },
            {
                U("后台任务队列"),
                U("无失联任务，无锁等待"),
                U("通过"),
                Theme::Green()
            },
            {
                U("备份完整性"),
                U("最近 20 个备份均可读取"),
                U("通过"),
                Theme::Green()
            }
        }};

        for (const auto& check : checkData)
        {
            checkSizer->Add(
                MakeCheckRow(
                    checks,
                    std::get<0>(check),
                    std::get<1>(check),
                    std::get<2>(check),
                    std::get<3>(check)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        checkSizer->AddStretchSpacer();
        checkSizer->Add(
            MakeText(
                checks,
                U("上次完整诊断：今天 12:56 · 用时 8.4 秒"),
                8,
                Theme::Faint()
            ),
            0,
            wxALL,
            17
        );
        checks->SetSizer(checkSizer);

        auto* environment = MakeCard(this);
        environment->SetMinSize(wxSize(390, -1));
        auto* environmentSizer = new wxBoxSizer(wxVERTICAL);

        environmentSizer->Add(
            MakeText(
                environment,
                U("运行环境"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 9> values = {{
            {U("应用版本"), U("0.8.0-dev"), Theme::Blue()},
            {U("操作系统"), U("Windows 11 x64"), Theme::Text()},
            {U("wxWidgets"), U("3.2.x Unicode"), Theme::Text()},
            {U("SQLite"), U("3.x + FTS5"), Theme::Purple()},
            {U("数据库加密"), U("SQLCipher"), Theme::Green()},
            {U("CPU 架构"), U("x86-64"), Theme::Text()},
            {U("逻辑处理器"), U("16"), Theme::Cyan()},
            {U("可用内存"), U("18.4 GB"), Theme::Cyan()},
            {U("可用磁盘"), U("286 GB"), Theme::Green()}
        }};

        for (const auto& value : values)
        {
            environmentSizer->Add(
                MakeProperty(
                    environment,
                    std::get<0>(value),
                    std::get<1>(value),
                    std::get<2>(value)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        environmentSizer->Add(
            new wxStaticLine(environment, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        environmentSizer->Add(
            MakeText(
                environment,
                U("资源使用"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        environmentSizer->Add(
            MakeText(environment, U("工作集内存  412 MB"), 8, Theme::Muted()),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        environmentSizer->Add(
            MakeProgress(environment, 34),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        environmentSizer->Add(
            MakeText(environment, U("任务队列负载  18%"), 8, Theme::Muted()),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        environmentSizer->Add(
            MakeProgress(environment, 18),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        environmentSizer->AddStretchSpacer();
        environmentSizer->Add(
            MakeButton(environment, U("打开日志目录")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        environmentSizer->Add(
            MakeButton(environment, U("复制环境摘要"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        environment->SetSizer(environmentSizer);

        auto* issue = MakeCard(this);
        issue->SetMinSize(wxSize(330, -1));
        auto* issueSizer = new wxBoxSizer(wxVERTICAL);

        auto* issueHeader = new wxBoxSizer(wxHORIZONTAL);
        issueHeader->Add(
            MakeText(
                issue,
                U("建议处理"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        issueHeader->Add(
            MakePill(issue, U("1 项警告"), Theme::Yellow(), 100),
            0
        );
        issueSizer->Add(issueHeader, 0, wxEXPAND | wxALL, 17);

        issueSizer->Add(
            MakeRow(
                issue,
                U("SRC-004"),
                U("法律合同数据源不可访问"),
                U("E:\\Legal\\contracts"),
                U("访问异常"),
                Theme::Yellow(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        issueSizer->Add(
            MakeWrappedText(
                issue,
                U(
                    "该数据源最后一次成功访问是在 2026-09-15。"
                    "已有内容快照和证据不会丢失，但无法检测新的文件变化。"
                ),
                9,
                Theme::Muted(),
                280
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        issueSizer->Add(
            MakeProperty(issue, U("受影响文件"), U("315"), Theme::Yellow()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        issueSizer->Add(
            MakeProperty(issue, U("关联证据"), U("3"), Theme::Yellow()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        issueSizer->Add(
            MakeProperty(issue, U("历史快照"), U("完整"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        issueSizer->AddStretchSpacer();
        issueSizer->Add(
            MakeButton(issue, U("重新测试路径")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        issueSizer->Add(
            MakeButton(issue, U("打开数据源设置"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        issue->SetSizer(issueSizer);

        body->Add(checks, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(environment, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(issue, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
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
            U("查看版本、组件、数据原则和许可证信息"),
            U("检查更新"),
            U("复制版本信息")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* product = MakeCard(this);
        product->SetMinSize(wxSize(420, -1));
        auto* productSizer = new wxBoxSizer(wxVERTICAL);

        auto* logo = MakeCard(product, Theme::Surface3());
        logo->SetMinSize(wxSize(92, 92));
        auto* logoSizer = new wxBoxSizer(wxVERTICAL);
        logoSizer->AddStretchSpacer();
        logoSizer->Add(
            MakeText(
                logo,
                U("续"),
                30,
                Theme::Blue(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_CENTER
        );
        logoSizer->AddStretchSpacer();
        logo->SetSizer(logoSizer);

        productSizer->Add(
            logo,
            0,
            wxALIGN_CENTER | wxTOP | wxBOTTOM,
            28
        );
        productSizer->Add(
            MakeText(
                product,
                U("续证 Continuum"),
                23,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_CENTER | wxBOTTOM,
            8
        );
        productSizer->Add(
            MakeText(
                product,
                U("离线优先的项目证据与决策连续性工作台"),
                10,
                Theme::Muted()
            ),
            0,
            wxALIGN_CENTER | wxBOTTOM,
            18
        );
        productSizer->Add(
            MakePill(product, U("版本 0.8.0-dev"), Theme::Blue(), 126),
            0,
            wxALIGN_CENTER | wxBOTTOM,
            22
        );

        auto* description = MakeCard(product, Theme::Surface2());
        auto* descriptionSizer = new wxBoxSizer(wxVERTICAL);
        descriptionSizer->Add(
            MakeWrappedText(
                description,
                U(
                    "续证帮助团队把来源文件、证据、事实、决策、承诺、"
                    "风险和历史状态组织为可追溯的连续记录。"
                    "工作区默认保存在本地，不依赖云端服务。"
                ),
                10,
                Theme::Text(),
                350
            ),
            0,
            wxALL,
            16
        );
        description->SetSizer(descriptionSizer);
        productSizer->Add(
            description,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            28
        );

        productSizer->Add(
            MakeText(
                product,
                U("核心原则"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        for (const auto& principle : {
            U("●  来源优先：每项关键结论都应能回到原始证据"),
            U("●  时间诚实：区分发生时间、获知时间和记录时间"),
            U("●  离线优先：没有网络时仍可完成主要工作"),
            U("●  可恢复：备份、审计和历史版本默认可验证")
        })
        {
            productSizer->Add(
                MakeWrappedText(
                    product,
                    principle,
                    9,
                    Theme::Muted(),
                    360
                ),
                0,
                wxLEFT | wxRIGHT | wxBOTTOM,
                14
            );
        }

        productSizer->AddStretchSpacer();
        productSizer->Add(
            MakeText(
                product,
                U("© 2026 Continuum Contributors"),
                8,
                Theme::Faint()
            ),
            0,
            wxALIGN_CENTER | wxALL,
            18
        );
        product->SetSizer(productSizer);

        auto* components = MakeCard(this);
        auto* componentSizer = new wxBoxSizer(wxVERTICAL);

        componentSizer->Add(
            MakeText(
                components,
                U("版本与组件"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            18
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 9> versions = {{
            {U("应用版本"), U("0.8.0-dev"), Theme::Blue()},
            {U("构建通道"), U("Development"), Theme::Yellow()},
            {U("构建日期"), U("2026-09-18"), Theme::Text()},
            {U("wxWidgets"), U("3.2.x"), Theme::Purple()},
            {U("C++ 标准"), U("C++17"), Theme::Cyan()},
            {U("SQLite"), U("3.x + FTS5"), Theme::Green()},
            {U("SQLCipher"), U("4.x"), Theme::Green()},
            {U("工作区格式"), U("1.0"), Theme::Blue()},
            {U("导出格式"), U("1.0"), Theme::Blue()}
        }};

        for (const auto& version : versions)
        {
            componentSizer->Add(
                MakeProperty(
                    components,
                    std::get<0>(version),
                    std::get<1>(version),
                    std::get<2>(version)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        componentSizer->Add(
            new wxStaticLine(components, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            18
        );
        componentSizer->Add(
            MakeText(
                components,
                U("构建标识"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        componentSizer->Add(
            MakeInput(
                components,
                U(
                    "continuum/0.8.0-dev\n"
                    "workspace-format/1.0\n"
                    "platform/windows-x86_64\n"
                    "channel/development"
                ),
                -1,
                true,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        componentSizer->AddStretchSpacer();
        componentSizer->Add(
            MakeButton(components, U("查看第三方组件")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        componentSizer->Add(
            MakeButton(components, U("查看变更记录"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        components->SetSizer(componentSizer);

        auto* legal = MakeCard(this);
        legal->SetMinSize(wxSize(365, -1));
        auto* legalSizer = new wxBoxSizer(wxVERTICAL);

        legalSizer->Add(
            MakeText(
                legal,
                U("许可证与数据"),
                15,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            18
        );
        legalSizer->Add(
            MakeProperty(legal, U("应用许可证"), U("内部开发版本")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        legalSizer->Add(
            MakeProperty(legal, U("联网要求"), U("无"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        legalSizer->Add(
            MakeProperty(legal, U("遥测"), U("不发送"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        legalSizer->Add(
            MakeProperty(legal, U("默认数据位置"), U("本地工作区")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        auto* privacy = MakeCard(legal, Theme::Surface2());
        auto* privacySizer = new wxBoxSizer(wxVERTICAL);
        privacySizer->Add(
            MakeText(
                privacy,
                U("数据说明"),
                9,
                Theme::Blue(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        privacySizer->Add(
            MakeWrappedText(
                privacy,
                U(
                    "源文件保留在用户选择的位置。工作区保存索引、"
                    "结构化对象、审计记录和必要的证据内容快照。"
                    "删除工作区不会自动删除源文件。"
                ),
                8,
                Theme::Muted(),
                300
            ),
            0
        );
        privacy->SetSizer(privacySizer);
        legalSizer->Add(
            privacy,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        legalSizer->Add(
            MakeText(
                legal,
                U("帮助与支持"),
                10,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        legalSizer->Add(
            MakeProperty(legal, U("用户手册"), U("本地帮助中心"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        legalSizer->Add(
            MakeProperty(legal, U("诊断"), U("M11 诊断中心"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        legalSizer->Add(
            MakeProperty(legal, U("问题反馈"), U("导出诊断包")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );

        legalSizer->AddStretchSpacer();
        legalSizer->Add(
            MakeButton(legal, U("打开本地帮助")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        legalSizer->Add(
            MakeButton(legal, U("第三方许可证"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            18
        );
        legal->SetSizer(legalSizer);

        body->Add(product, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(components, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(legal, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 26);
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
