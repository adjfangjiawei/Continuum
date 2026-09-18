#include "EvidencePages.h"

#include "Theme.h"

#include <array>
#include <tuple>

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/panel.h>
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

    if (danger)
    {
        button->SetBackgroundColour(Theme::Red());
    }
    else
    {
        button->SetBackgroundColour(
            primary ? Theme::Blue() : Theme::Surface2()
        );
    }

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
    auto* search = new wxTextCtrl(
        parent,
        wxID_ANY,
        wxEmptyString,
        wxDefaultPosition,
        wxSize(width, 38),
        wxBORDER_NONE | wxTE_PROCESS_ENTER
    );

    search->SetBackgroundColour(Theme::Input());
    search->SetForegroundColour(Theme::Text());
    search->SetFont(Theme::Font(10));
    search->SetHint(hint);
    return search;
}

wxPanel* MakeCard(wxWindow* parent)
{
    auto* card = new wxPanel(parent, wxID_ANY);
    Theme::Apply(card, Theme::Surface());
    return card;
}

wxPanel* MakeSubCard(wxWindow* parent)
{
    auto* card = new wxPanel(parent, wxID_ANY);
    Theme::Apply(card, Theme::Surface2());
    return card;
}

wxPanel* MakeStatusPill(
    wxWindow* parent,
    const wxString& label,
    const wxColour& color,
    int width = 92
)
{
    auto* panel = new wxPanel(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(width, 25)
    );
    Theme::Apply(panel, Theme::Surface3());

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

wxPanel* MakeMetric(
    wxWindow* parent,
    const wxString& value,
    const wxString& title,
    const wxString& detail,
    const wxColour& color
)
{
    auto* card = MakeCard(parent);
    card->SetMinSize(wxSize(170, 91));

    auto* root = new wxBoxSizer(wxHORIZONTAL);

    auto* stripe = new wxPanel(
        card,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(4, -1)
    );
    stripe->SetBackgroundColour(color);

    auto* content = new wxBoxSizer(wxVERTICAL);
    content->Add(
        MakeText(card, value, 20, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    content->Add(
        MakeText(card, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    content->Add(
        MakeText(card, detail, 8, Theme::Muted()),
        0
    );

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 13);
    root->Add(content, 1, wxALL, 13);
    card->SetSizer(root);
    return card;
}

wxPanel* MakeQueueRow(
    wxWindow* parent,
    const wxString& code,
    const wxString& title,
    const wxString& detail,
    const wxString& state,
    const wxColour& color,
    bool selected = false
)
{
    auto* row = new wxPanel(parent, wxID_ANY);
    Theme::Apply(
        row,
        selected ? Theme::Surface3() : Theme::Surface2()
    );
    row->SetMinSize(wxSize(-1, 82));

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
        4
    );
    content->Add(
        MakeText(row, title, 10, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        4
    );
    content->Add(
        MakeText(row, detail, 8, Theme::Muted()),
        0
    );

    root->Add(stripe, 0, wxEXPAND | wxRIGHT, 12);
    root->Add(content, 1, wxALL, 11);
    root->Add(
        MakeStatusPill(row, state, color),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(root);
    return row;
}

void AddPageHeading(
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
    labels->Add(
        MakeText(parent, subtitle, 10, Theme::Muted()),
        0
    );

    row->Add(labels, 1, wxEXPAND);
    row->Add(
        MakeButton(parent, U("刷新"), false, false, 88),
        0,
        wxALIGN_BOTTOM | wxRIGHT,
        10
    );
    row->Add(
        MakeButton(parent, primaryAction, true, false, 132),
        0,
        wxALIGN_BOTTOM
    );

    root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 26);
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

class ReviewInboxPage final : public wxPanel
{
public:
    explicit ReviewInboxPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddPageHeading(
            this,
            root,
            U("P02"),
            U("审查收件箱"),
            U("处理自动提取结果、来源变化和需要人工确认的项目对象"),
            U("批量审查")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("24"), U("待审查"), U("今天新增 8 项"), Theme::Blue()},
                {U("7"), U("高优先级"), U("包含 2 项冲突"), Theme::Red()},
                {U("11"), U("需要证据"), U("缺少来源确认"), Theme::Yellow()},
                {U("38"), U("今天完成"), U("确认率 92%"), Theme::Green()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* filters = MakeCard(this);
        filters->SetMinSize(wxSize(215, -1));
        auto* filterSizer = new wxBoxSizer(wxVERTICAL);

        filterSizer->Add(
            MakeText(filters, U("审查队列"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        const struct
        {
            const char* name;
            const char* count;
            wxColour color;
        } categories[] = {
            {"全部待审查", "24", Theme::Blue()},
            {"自动提取", "9", Theme::Purple()},
            {"来源变化", "5", Theme::Orange()},
            {"证据不足", "6", Theme::Yellow()},
            {"可能冲突", "4", Theme::Red()}
        };

        for (const auto& category : categories)
        {
            auto* row = MakeSubCard(filters);
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            rowSizer->Add(
                MakeText(
                    row,
                    U(category.name),
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                1,
                wxALIGN_CENTER_VERTICAL | wxALL,
                10
            );
            rowSizer->Add(
                MakeText(
                    row,
                    U(category.count),
                    9,
                    category.color,
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                12
            );

            row->SetSizer(rowSizer);
            filterSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
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
            MakeStatusPill(filters, U("高严重度"), Theme::Red(), 105),
            0,
            wxLEFT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakeStatusPill(filters, U("中严重度"), Theme::Yellow(), 105),
            0,
            wxLEFT | wxBOTTOM,
            17
        );
        filterSizer->Add(
            MakeStatusPill(filters, U("低严重度"), Theme::Blue(), 105),
            0,
            wxLEFT,
            17
        );

        filters->SetSizer(filterSizer);

        auto* queue = MakeCard(this);
        queue->SetMinSize(wxSize(455, -1));
        auto* queueSizer = new wxBoxSizer(wxVERTICAL);

        auto* queueToolbar = new wxBoxSizer(wxHORIZONTAL);
        queueToolbar->Add(
            MakeSearch(queue, U("搜索待审查内容"), 265),
            1,
            wxRIGHT,
            10
        );
        queueToolbar->Add(
            MakeButton(queue, U("排序"), false, false, 80),
            0
        );
        queueSizer->Add(queueToolbar, 0, wxEXPAND | wxALL, 16);

        queueSizer->Add(
            MakeQueueRow(
                queue,
                U("R-0241"),
                U("交付日期被识别为 11 月 30 日"),
                U("requirements-v7.pdf · 第 8 页"),
                U("高优先级"),
                Theme::Red(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeQueueRow(
                queue,
                U("R-0240"),
                U("旧协议支持期发生变化"),
                U("内容由确定日期改为第四季度"),
                U("来源变化"),
                Theme::Orange()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeQueueRow(
                queue,
                U("R-0238"),
                U("提取承诺：完成安全复核"),
                U("负责人可能为周启明"),
                U("需确认"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeQueueRow(
                queue,
                U("R-0236"),
                U("识别风险：双轨审计复杂度"),
                U("会议纪要-0909.md · 第 51 行"),
                U("自动提取"),
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        queueSizer->Add(
            MakeQueueRow(
                queue,
                U("R-0234"),
                U("事实缺少明确的生效时间"),
                U("需要补充日期精度"),
                U("证据不足"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );

        queue->SetSizer(queueSizer);

        auto* inspector = MakeCard(this);
        inspector->SetMinSize(wxSize(355, -1));
        auto* inspectorSizer = new wxBoxSizer(wxVERTICAL);

        auto* inspectorHeader = new wxBoxSizer(wxHORIZONTAL);
        inspectorHeader->Add(
            MakeText(
                inspector,
                U("审查详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        inspectorHeader->Add(
            MakeStatusPill(
                inspector,
                U("高优先级"),
                Theme::Red(),
                105
            ),
            0
        );
        inspectorSizer->Add(inspectorHeader, 0, wxEXPAND | wxALL, 17);

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("交付日期被识别为 11 月 30 日"),
                14,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* quote = MakeSubCard(inspector);
        auto* quoteSizer = new wxBoxSizer(wxVERTICAL);
        quoteSizer->Add(
            MakeText(quote, U("来源片段"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxBOTTOM,
            8
        );
        quoteSizer->Add(
            MakeWrappedText(
                quote,
                U("最终交付日期调整为 2026 年 11 月 30 日，旧计划日期不再适用。"),
                10,
                Theme::Text(),
                285,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0
        );
        quote->SetSizer(quoteSizer);
        inspectorSizer->Add(
            quote,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspectorSizer->Add(
            MakeText(
                inspector,
                U("结构化字段"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const std::array<std::pair<wxString, wxString>, 4> fields = {{
            {U("对象类型"), U("事实")},
            {U("属性"), U("交付日期")},
            {U("归一化值"), U("2026-11-30")},
            {U("置信度"), U("96%")}
        }};

        for (const auto& field : fields)
        {
            auto* fieldRow = MakeSubCard(inspector);
            auto* fieldSizer = new wxBoxSizer(wxHORIZONTAL);
            fieldSizer->Add(
                MakeText(fieldRow, field.first, 8, Theme::Muted()),
                1,
                wxALL,
                10
            );
            fieldSizer->Add(
                MakeText(
                    fieldRow,
                    field.second,
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                10
            );
            fieldRow->SetSizer(fieldSizer);
            inspectorSizer->Add(
                fieldRow,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                9
            );
        }

        inspectorSizer->AddStretchSpacer();
        inspectorSizer->Add(
            MakeButton(inspector, U("拒绝"), false, true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeButton(inspector, U("编辑后接受")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        inspectorSizer->Add(
            MakeButton(inspector, U("接受并创建事实"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        inspector->SetSizer(inspectorSizer);

        body->Add(filters, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(queue, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(inspector, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class LibraryPage final : public wxPanel
{
public:
    explicit LibraryPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddPageHeading(
            this,
            root,
            U("P03"),
            U("资料库"),
            U("浏览数据源、文件、内容块、历史版本和证据引用"),
            U("添加数据源")
        );

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(
            MakeSearch(this, U("搜索文件名、路径或文档内容"), 460),
            1,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(this, U("类型：全部"), false, false, 112),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(this, U("状态：全部"), false, false, 112),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(this, U("重新扫描"), false, false, 105),
            0
        );

        root->Add(toolbar, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* sourcePanel = MakeCard(this);
        sourcePanel->SetMinSize(wxSize(245, -1));
        auto* sourceSizer = new wxBoxSizer(wxVERTICAL);

        sourceSizer->Add(
            MakeText(
                sourcePanel,
                U("数据源与目录"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        wxArrayString sources;
        sources.Add(U("▾  研发仓库"));
        sources.Add(U("    ▾  docs"));
        sources.Add(U("        requirements"));
        sources.Add(U("        migration"));
        sources.Add(U("    ▸  source"));
        sources.Add(U("▾  项目会议资料"));
        sources.Add(U("    2026-09"));
        sources.Add(U("▾  邮件归档"));
        sources.Add(U("    合作方"));
        sources.Add(U("无法访问的数据源  1"));

        auto* sourceList = new wxListBox(
            sourcePanel,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            sources,
            wxLB_SINGLE | wxBORDER_NONE
        );
        sourceList->SetBackgroundColour(Theme::Surface());
        sourceList->SetForegroundColour(Theme::Muted());
        sourceList->SetFont(Theme::Font(9));
        sourceList->SetSelection(2);

        sourceSizer->Add(
            sourceList,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );
        sourceSizer->Add(
            MakeButton(sourcePanel, U("管理数据源")),
            0,
            wxEXPAND | wxALL,
            16
        );
        sourcePanel->SetSizer(sourceSizer);

        auto* filesPanel = MakeCard(this);
        filesPanel->SetMinSize(wxSize(570, -1));
        auto* filesSizer = new wxBoxSizer(wxVERTICAL);

        auto* listHeader = new wxBoxSizer(wxHORIZONTAL);
        listHeader->Add(
            MakeText(
                filesPanel,
                U("文件"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        listHeader->Add(
            MakeText(filesPanel, U("显示 6 / 2,418"), 8, Theme::Muted()),
            0,
            wxALIGN_CENTER_VERTICAL
        );
        filesSizer->Add(listHeader, 0, wxEXPAND | wxALL, 17);

        const struct
        {
            const char* name;
            const char* detail;
            const char* state;
            wxColour color;
        } files[] = {
            {
                "requirements-v7.pdf",
                "PDF · 18 页 · 更新于今天 12:18",
                "已变化",
                Theme::Orange()
            },
            {
                "交付计划.xlsx",
                "Excel · 4 个工作表 · 昨天 18:20",
                "已索引",
                Theme::Green()
            },
            {
                "会议纪要-0909.md",
                "Markdown · 328 行 · 9 月 9 日",
                "已索引",
                Theme::Green()
            },
            {
                "协议评审.eml",
                "电子邮件 · 3 个附件 · 9 月 8 日",
                "有证据",
                Theme::Blue()
            },
            {
                "迁移架构图.png",
                "PNG · 2480 × 1440 · OCR 完成",
                "OCR",
                Theme::Purple()
            },
            {
                "legacy-contract.pdf",
                "PDF · 源路径当前不可访问",
                "失联",
                Theme::Red()
            }
        };

        for (std::size_t index = 0; index < 6; ++index)
        {
            filesSizer->Add(
                MakeQueueRow(
                    filesPanel,
                    wxString::Format(U("FILE-%03d"), static_cast<int>(index + 1)),
                    U(files[index].name),
                    U(files[index].detail),
                    U(files[index].state),
                    files[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        filesPanel->SetSizer(filesSizer);

        auto* preview = MakeCard(this);
        preview->SetMinSize(wxSize(360, -1));
        auto* previewSizer = new wxBoxSizer(wxVERTICAL);

        previewSizer->Add(
            MakeText(
                preview,
                U("文件检查器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        previewSizer->Add(
            MakeText(
                preview,
                U("requirements-v7.pdf"),
                14,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        previewSizer->Add(
            MakeStatusPill(preview, U("内容已变化"), Theme::Orange(), 110),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* previewText = new wxTextCtrl(
            preview,
            wxID_ANY,
            U(
                "第 18 页预览\n\n"
                "旧协议保留至 2026 年第四季度结束，"
                "具体关闭日期由交付委员会另行确认。\n\n"
                "在切换工作完成之前，必须保留紧急回退通道。\n\n"
                "当前版本：v7\n"
                "上一版本：v6\n"
                "检测到 3 处业务相关变化。"
            ),
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );
        previewText->SetBackgroundColour(Theme::Input());
        previewText->SetForegroundColour(Theme::Text());
        previewText->SetFont(Theme::Font(10));

        previewSizer->Add(
            previewText,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* stats = MakeSubCard(preview);
        auto* statsSizer = new wxBoxSizer(wxVERTICAL);
        statsSizer->Add(
            MakeText(stats, U("内容块  86"), 9, Theme::Text()),
            0,
            wxBOTTOM,
            6
        );
        statsSizer->Add(
            MakeText(stats, U("关联证据  12"), 9, Theme::Text()),
            0,
            wxBOTTOM,
            6
        );
        statsSizer->Add(
            MakeText(stats, U("受影响对象  4"), 9, Theme::Yellow()),
            0
        );
        stats->SetSizer(statsSizer);
        previewSizer->Add(
            stats,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        previewSizer->Add(
            MakeButton(preview, U("打开文件阅读器"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        preview->SetSizer(previewSizer);

        body->Add(sourcePanel, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(filesPanel, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(preview, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class FileReaderPage final : public wxPanel
{
public:
    explicit FileReaderPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddPageHeading(
            this,
            root,
            U("P04"),
            U("文件阅读器"),
            U("阅读源文档、定位内容块并创建带锚点的证据"),
            U("创建证据")
        );

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(
            MakeButton(this, U("‹ 返回资料库"), false, false, 120),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeSearch(this, U("在当前文档中搜索"), 330),
            1,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(this, U("第 18 / 18 页"), false, false, 115),
            0,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(this, U("缩放 110%"), false, false, 105),
            0
        );
        root->Add(toolbar, 0, wxEXPAND | wxALL, 26);

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* outline = MakeCard(this);
        outline->SetMinSize(wxSize(230, -1));
        auto* outlineSizer = new wxBoxSizer(wxVERTICAL);

        outlineSizer->Add(
            MakeText(outline, U("文档结构"), 12, Theme::Text(), wxFONTWEIGHT_BOLD),
            0,
            wxALL,
            17
        );

        wxArrayString sections;
        sections.Add(U("1. 项目背景"));
        sections.Add(U("2. 交付目标"));
        sections.Add(U("3. 系统边界"));
        sections.Add(U("4. 迁移策略"));
        sections.Add(U("5. 双轨运行"));
        sections.Add(U("6. 风险与控制"));
        sections.Add(U("7. 交付日期"));
        sections.Add(U("8. 旧协议退出"));
        sections.Add(U("9. 验收条件"));
        sections.Add(U("附录 A 术语"));

        auto* outlineList = new wxListBox(
            outline,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            sections,
            wxLB_SINGLE | wxBORDER_NONE
        );
        outlineList->SetBackgroundColour(Theme::Surface());
        outlineList->SetForegroundColour(Theme::Muted());
        outlineList->SetFont(Theme::Font(9));
        outlineList->SetSelection(7);

        outlineSizer->Add(
            outlineList,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        outlineSizer->Add(
            MakeText(outline, U("页面缩略图"), 9, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* thumbnail = MakeSubCard(outline);
        thumbnail->SetMinSize(wxSize(-1, 130));
        auto* thumbnailSizer = new wxBoxSizer(wxVERTICAL);
        thumbnailSizer->Add(
            MakeText(
                thumbnail,
                U("第 18 页"),
                9,
                Theme::Blue(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            12
        );
        thumbnailSizer->Add(
            MakeWrappedText(
                thumbnail,
                U("旧协议支持范围与退出条件"),
                8,
                Theme::Muted(),
                165
            ),
            0,
            wxLEFT | wxRIGHT,
            12
        );
        thumbnail->SetSizer(thumbnailSizer);
        outlineSizer->Add(
            thumbnail,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        outline->SetSizer(outlineSizer);

        auto* documentPanel = MakeCard(this);
        auto* documentSizer = new wxBoxSizer(wxVERTICAL);

        auto* documentHeader = new wxBoxSizer(wxHORIZONTAL);
        documentHeader->Add(
            MakeText(
                documentPanel,
                U("requirements-v7.pdf"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        documentHeader->Add(
            MakeStatusPill(
                documentPanel,
                U("版本 v7"),
                Theme::Blue(),
                88
            ),
            0
        );
        documentSizer->Add(
            documentHeader,
            0,
            wxEXPAND | wxALL,
            17
        );

        auto* page = new wxTextCtrl(
            documentPanel,
            wxID_ANY,
            U(
                "8. 旧协议退出与回退要求\n\n"
                "8.1 支持期限\n\n"
                "旧协议保留至 2026 年第四季度结束，具体关闭日期由"
                "交付委员会另行确认。在合作方切换工作完成之前，"
                "不得关闭旧接口。\n\n"
                "8.2 紧急回退\n\n"
                "在新协议完成连续十四天稳定运行之前，必须保留紧急"
                "回退通道。回退操作需要安全负责人和交付负责人共同批准。\n\n"
                "8.3 验收约束\n\n"
                "最终交付日期调整为 2026 年 11 月 30 日。安全复核、"
                "合作方环境验证和审计记录迁移必须在最终交付前完成。\n\n"
                "所选内容块 B-1882\n"
                "锚点：第 18 页 · 段落 12 · 字符 640—781"
            ),
            wxDefaultPosition,
            wxDefaultSize,
            wxTE_MULTILINE | wxTE_READONLY | wxBORDER_NONE
        );
        page->SetBackgroundColour(wxColour(238, 235, 226));
        page->SetForegroundColour(wxColour(38, 45, 55));
        page->SetFont(Theme::Font(11));

        documentSizer->Add(
            page,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            30
        );
        documentPanel->SetSizer(documentSizer);

        auto* evidence = MakeCard(this);
        evidence->SetMinSize(wxSize(355, -1));
        auto* evidenceSizer = new wxBoxSizer(wxVERTICAL);

        evidenceSizer->Add(
            MakeText(
                evidence,
                U("证据创建器"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );
        evidenceSizer->Add(
            MakeStatusPill(evidence, U("已选择文本"), Theme::Green(), 105),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* selection = MakeSubCard(evidence);
        auto* selectionSizer = new wxBoxSizer(wxVERTICAL);
        selectionSizer->Add(
            MakeText(selection, U("选中原文"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxBOTTOM,
            8
        );
        selectionSizer->Add(
            MakeWrappedText(
                selection,
                U("旧协议保留至 2026 年第四季度结束，具体关闭日期由交付委员会另行确认。"),
                10,
                Theme::Text(),
                285,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0
        );
        selection->SetSizer(selectionSizer);
        evidenceSizer->Add(
            selection,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        evidenceSizer->Add(
            MakeText(evidence, U("证据角色"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        evidenceSizer->Add(
            MakeButton(evidence, U("支持当前事实  ▾")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        evidenceSizer->Add(
            MakeText(evidence, U("关联对象"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        evidenceSizer->Add(
            MakeQueueRow(
                evidence,
                U("F-0054"),
                U("旧协议保留至第四季度"),
                U("当前有效事实"),
                U("已关联"),
                Theme::Green(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        evidenceSizer->Add(
            MakeText(evidence, U("来源锚点"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        evidenceSizer->Add(
            MakeWrappedText(
                evidence,
                U("第 18 页 · 段落 12 · 内容块 B-1882\nSHA-256 内容指纹将在保存时计算"),
                8,
                Theme::Muted(),
                300
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        evidenceSizer->AddStretchSpacer();
        evidenceSizer->Add(
            MakeButton(evidence, U("复制带来源引用的文本")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        evidenceSizer->Add(
            MakeButton(evidence, U("创建证据并关联"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        evidence->SetSizer(evidenceSizer);

        body->Add(outline, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(documentPanel, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(evidence, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

class EvidenceInspectorPage final : public wxPanel
{
public:
    explicit EvidenceInspectorPage(wxWindow* parent)
        : wxPanel(parent, wxID_ANY)
    {
        Theme::Apply(this, Theme::Window());

        auto* root = new wxBoxSizer(wxVERTICAL);

        AddPageHeading(
            this,
            root,
            U("P05"),
            U("证据检查器"),
            U("验证来源锚点、内容指纹、历史版本和业务对象引用"),
            U("重新验证")
        );

        AddMetrics(
            this,
            root,
            {{
                {U("127"), U("全部证据"), U("来自 36 个文件"), Theme::Blue()},
                {U("119"), U("锚点正常"), U("可精确重新定位"), Theme::Green()},
                {U("5"), U("需要复核"), U("来源内容已变化"), Theme::Yellow()},
                {U("3"), U("来源失联"), U("保留历史快照"), Theme::Red()}
            }}
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* evidenceList = MakeCard(this);
        evidenceList->SetMinSize(wxSize(415, -1));
        auto* listSizer = new wxBoxSizer(wxVERTICAL);

        auto* toolbar = new wxBoxSizer(wxHORIZONTAL);
        toolbar->Add(
            MakeSearch(evidenceList, U("搜索证据编号或内容"), 245),
            1,
            wxRIGHT,
            10
        );
        toolbar->Add(
            MakeButton(evidenceList, U("状态"), false, false, 78),
            0
        );
        listSizer->Add(toolbar, 0, wxEXPAND | wxALL, 16);

        const struct
        {
            const char* code;
            const char* title;
            const char* source;
            const char* state;
            wxColour color;
        } evidenceRows[] = {
            {
                "E-1097",
                "旧协议保留至第四季度",
                "requirements-v7.pdf · 第 18 页",
                "需复核",
                Theme::Yellow()
            },
            {
                "E-1042",
                "委员会接受双轨迁移方案",
                "会议纪要-0909.md · 第 42 行",
                "正常",
                Theme::Green()
            },
            {
                "E-1124",
                "必须保留紧急回退通道",
                "协议评审.eml · 正文第 6 段",
                "正常",
                Theme::Green()
            },
            {
                "E-0841",
                "旧协议原定 10 月关闭",
                "legacy-contract.pdf · 第 12 页",
                "失联",
                Theme::Red()
            },
            {
                "E-1130",
                "安全复核截止 9 月 25 日",
                "安全评审.eml · 正文第 4 段",
                "正常",
                Theme::Green()
            }
        };

        for (std::size_t index = 0; index < 5; ++index)
        {
            listSizer->Add(
                MakeQueueRow(
                    evidenceList,
                    U(evidenceRows[index].code),
                    U(evidenceRows[index].title),
                    U(evidenceRows[index].source),
                    U(evidenceRows[index].state),
                    evidenceRows[index].color,
                    index == 0
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
        }

        evidenceList->SetSizer(listSizer);

        auto* verification = MakeCard(this);
        auto* verificationSizer = new wxBoxSizer(wxVERTICAL);

        auto* header = new wxBoxSizer(wxHORIZONTAL);
        header->Add(
            MakeText(
                verification,
                U("E-1097"),
                9,
                Theme::Blue(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            12
        );
        header->Add(
            MakeText(
                verification,
                U("证据验证详情"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        header->Add(
            MakeStatusPill(
                verification,
                U("需要复核"),
                Theme::Yellow(),
                105
            ),
            0
        );
        verificationSizer->Add(header, 0, wxEXPAND | wxALL, 17);

        auto* quote = MakeSubCard(verification);
        auto* quoteSizer = new wxBoxSizer(wxVERTICAL);
        quoteSizer->Add(
            MakeText(quote, U("保存的证据文本"), 8, Theme::Faint(), wxFONTWEIGHT_BOLD),
            0,
            wxBOTTOM,
            9
        );
        quoteSizer->Add(
            MakeWrappedText(
                quote,
                U("旧协议保留至 2026 年 10 月 31 日，之后关闭旧接口并撤销回退通道。"),
                11,
                Theme::Text(),
                500,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0
        );
        quote->SetSizer(quoteSizer);
        verificationSizer->Add(
            quote,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        verificationSizer->Add(
            MakeText(
                verification,
                U("当前来源内容"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        auto* current = MakeSubCard(verification);
        auto* currentSizer = new wxBoxSizer(wxVERTICAL);
        currentSizer->Add(
            MakeWrappedText(
                current,
                U("旧协议保留至 2026 年第四季度结束，具体关闭日期由交付委员会另行确认，在此之前必须保留紧急回退通道。"),
                11,
                Theme::Text(),
                500,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0
        );
        current->SetSizer(currentSizer);
        verificationSizer->Add(
            current,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        verificationSizer->Add(
            MakeText(
                verification,
                U("验证检查"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        const struct
        {
            const char* title;
            const char* value;
            wxColour color;
        } checks[] = {
            {"源文件可访问", "通过", Theme::Green()},
            {"历史版本快照", "v6 已保留", Theme::Green()},
            {"内容指纹", "与当前版本不同", Theme::Yellow()},
            {"精确锚点", "原段落已被修改", Theme::Yellow()},
            {"相似内容定位", "候选相似度 78%", Theme::Blue()}
        };

        for (const auto& check : checks)
        {
            auto* checkRow = MakeSubCard(verification);
            auto* checkSizer = new wxBoxSizer(wxHORIZONTAL);
            checkSizer->Add(
                MakeText(checkRow, U(check.title), 9, Theme::Muted()),
                1,
                wxALL,
                10
            );
            checkSizer->Add(
                MakeText(
                    checkRow,
                    U(check.value),
                    9,
                    check.color,
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER_VERTICAL | wxRIGHT,
                10
            );
            checkRow->SetSizer(checkSizer);
            verificationSizer->Add(
                checkRow,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        verificationSizer->AddStretchSpacer();

        auto* actions = new wxBoxSizer(wxHORIZONTAL);
        actions->Add(
            MakeButton(verification, U("保留旧证据")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(verification, U("标记为失效")),
            1,
            wxRIGHT,
            10
        );
        actions->Add(
            MakeButton(verification, U("重新锚定"), true),
            1
        );
        verificationSizer->Add(
            actions,
            0,
            wxEXPAND | wxALL,
            17
        );

        verification->SetSizer(verificationSizer);

        auto* references = MakeCard(this);
        references->SetMinSize(wxSize(320, -1));
        auto* referenceSizer = new wxBoxSizer(wxVERTICAL);

        referenceSizer->Add(
            MakeText(
                references,
                U("引用与影响"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            17
        );

        referenceSizer->Add(
            MakeQueueRow(
                references,
                U("F-0054"),
                U("旧协议保留至第四季度"),
                U("当前有效事实"),
                U("支持"),
                Theme::Green(),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        referenceSizer->Add(
            MakeQueueRow(
                references,
                U("D-0031"),
                U("采用双轨迁移方案"),
                U("已接受决策"),
                U("依据"),
                Theme::Purple()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        referenceSizer->Add(
            MakeQueueRow(
                references,
                U("R-0018"),
                U("双轨审计复杂度"),
                U("活跃风险"),
                U("影响"),
                Theme::Red()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        referenceSizer->Add(
            new wxStaticLine(references, wxID_ANY),
            0,
            wxEXPAND | wxALL,
            17
        );
        referenceSizer->Add(
            MakeText(
                references,
                U("审计记录"),
                9,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        referenceSizer->Add(
            MakeWrappedText(
                references,
                U(
                    "创建：2026-09-10 09:12\n"
                    "创建者：周启明\n"
                    "来源版本：v6\n"
                    "最后验证：今天 12:31\n"
                    "状态变更：正常 → 需要复核"
                ),
                8,
                Theme::Muted(),
                270
            ),
            0,
            wxLEFT | wxRIGHT,
            17
        );

        referenceSizer->AddStretchSpacer();
        referenceSizer->Add(
            MakeButton(references, U("打开来源版本")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );
        referenceSizer->Add(
            MakeButton(references, U("查看完整审计历史"), true),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            17
        );

        references->SetSizer(referenceSizer);

        body->Add(evidenceList, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(verification, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(references, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 26);
        SetSizer(root);
    }
};

}

wxWindow* CreateEvidenceWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
)
{
    if (pageCode == U("P02"))
    {
        return new ReviewInboxPage(parent);
    }

    if (pageCode == U("P03"))
    {
        return new LibraryPage(parent);
    }

    if (pageCode == U("P04"))
    {
        return new FileReaderPage(parent);
    }

    if (pageCode == U("P05"))
    {
        return new EvidenceInspectorPage(parent);
    }

    return nullptr;
}

}
