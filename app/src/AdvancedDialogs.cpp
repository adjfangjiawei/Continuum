#include "AdvancedDialogs.h"

#include "SystemDialogs.h"

#include "Theme.h"

#include <array>
#include <tuple>
#include <utility>

#include <wx/arrstr.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/gauge.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
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
    wxWindowID id,
    const wxString& label,
    bool primary = false,
    bool danger = false,
    int width = -1
)
{
    auto* button = new wxButton(
        parent,
        id,
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
    bool multiline = false,
    bool readOnly = false,
    long extraStyle = 0
)
{
    long style = wxBORDER_NONE | extraStyle;

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
        wxSize(-1, multiline ? 88 : 38),
        style
    );
    input->SetBackgroundColour(Theme::Input());
    input->SetForegroundColour(Theme::Text());
    input->SetFont(Theme::Font(9));
    return input;
}

wxChoice* MakeChoice(
    wxWindow* parent,
    const wxArrayString& values,
    int selection = 0
)
{
    auto* choice = new wxChoice(
        parent,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(-1, 38),
        values
    );
    choice->SetBackgroundColour(Theme::Input());
    choice->SetForegroundColour(Theme::Text());
    choice->SetFont(Theme::Font(9));

    if (!values.IsEmpty())
    {
        choice->SetSelection(selection);
    }

    return choice;
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
        MakeText(
            row,
            value,
            9,
            valueColor,
            wxFONTWEIGHT_SEMIBOLD
        ),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(layout);
    return row;
}

wxPanel* MakeResultRow(
    wxWindow* parent,
    const wxString& code,
    const wxString& title,
    const wxString& detail,
    const wxString& state,
    const wxColour& color
)
{
    auto* row = MakeCard(parent, Theme::Surface2());
    row->SetMinSize(wxSize(-1, 68));

    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* marker = new wxPanel(
        row,
        wxID_ANY,
        wxDefaultPosition,
        wxSize(4, -1)
    );
    marker->SetBackgroundColour(color);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(row, code, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        3
    );
    labels->Add(
        MakeText(row, title, 9, Theme::Text(), wxFONTWEIGHT_SEMIBOLD),
        0,
        wxBOTTOM,
        3
    );
    labels->Add(
        MakeText(row, detail, 8, Theme::Muted()),
        0
    );

    layout->Add(marker, 0, wxEXPAND | wxRIGHT, 11);
    layout->Add(labels, 1, wxALL, 10);
    layout->Add(
        MakePill(row, state, color),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        11
    );

    row->SetSizer(layout);
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

void AddHeader(
    wxDialog* dialog,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle,
    const wxColour& accent
)
{
    auto* header = MakeCard(dialog);
    auto* headerSizer = new wxBoxSizer(wxVERTICAL);

    headerSizer->Add(
        MakeText(header, code, 8, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    headerSizer->Add(
        MakeText(header, title, 17, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        6
    );
    headerSizer->Add(
        MakeWrappedText(header, subtitle, 9, Theme::Muted(), 720),
        0
    );

    header->SetSizer(headerSizer);
    root->Add(header, 0, wxEXPAND | wxALL, 18);
}

void AddLabel(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& label,
    bool required = false
)
{
    root->Add(
        MakeText(
            parent,
            required ? label + U("  *") : label,
            8,
            required ? Theme::Text() : Theme::Faint(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
        4
    );
}

void AddActions(
    wxDialog* dialog,
    wxBoxSizer* root,
    const wxString& confirmLabel,
    const wxString& secondaryLabel = wxEmptyString,
    bool danger = false
)
{
    root->Add(
        new wxStaticLine(dialog, wxID_ANY),
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        18
    );

    auto* actions = new wxBoxSizer(wxHORIZONTAL);

    if (!secondaryLabel.IsEmpty())
    {
        actions->Add(
            MakeButton(
                dialog,
                wxID_APPLY,
                secondaryLabel,
                false,
                false,
                140
            ),
            0,
            wxRIGHT,
            10
        );
    }

    actions->AddStretchSpacer();
    actions->Add(
        MakeButton(
            dialog,
            wxID_CANCEL,
            U("取消"),
            false,
            false,
            100
        ),
        0,
        wxRIGHT,
        10
    );
    actions->Add(
        MakeButton(
            dialog,
            wxID_OK,
            confirmLabel,
            !danger,
            danger,
            150
        ),
        0
    );

    root->Add(actions, 0, wxEXPAND | wxALL, 18);
}

class StyledDialog : public wxDialog
{
public:
    StyledDialog(
        wxWindow* parent,
        const wxString& title,
        const wxSize& size
    )
        : wxDialog(
            parent,
            wxID_ANY,
            title,
            wxDefaultPosition,
            size,
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
        )
    {
        Theme::Apply(this, Theme::Window());
        SetMinSize(size);
    }

protected:
    void Finish(wxBoxSizer* root)
    {
        SetSizer(root);
        Layout();
        CentreOnParent();
    }
};

class SelectDataSourceDialog final : public StyledDialog
{
public:
    explicit SelectDataSourceDialog(wxWindow* parent)
        : StyledDialog(parent, U("选择数据源"), wxSize(820, 690))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D06"),
            U("选择数据源与路径"),
            U("选择已有数据源或指定新的本地路径，并在继续前验证读取权限。"),
            Theme::Blue()
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* sources = MakeCard(this);
        auto* sourceSizer = new wxBoxSizer(wxVERTICAL);

        sourceSizer->Add(
            MakeText(
                sources,
                U("已有数据源"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        auto* search = MakeInput(sources);
        search->SetHint(U("搜索数据源名称或路径"));
        sourceSizer->Add(
            search,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        wxArrayString sourceRows;
        sourceRows.Add(U("研发仓库  ·  D:\\Pioneer\\repository"));
        sourceRows.Add(U("项目文档  ·  D:\\Pioneer\\docs"));
        sourceRows.Add(U("邮件归档  ·  D:\\Pioneer\\mail"));
        sourceRows.Add(U("法律合同  ·  E:\\Legal\\contracts"));

        auto* list = new wxListBox(
            sources,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            sourceRows,
            wxLB_SINGLE | wxBORDER_NONE
        );
        list->SetBackgroundColour(Theme::Surface2());
        list->SetForegroundColour(Theme::Text());
        list->SetFont(Theme::Font(9));
        list->SetSelection(1);

        sourceSizer->Add(
            list,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        sourceSizer->Add(
            MakeText(
                sources,
                U("4 个数据源 · 其中 1 个当前不可访问"),
                8,
                Theme::Faint()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        sources->SetSizer(sourceSizer);

        auto* selection = MakeCard(this);
        selection->SetMinSize(wxSize(360, -1));
        auto* selectionSizer = new wxBoxSizer(wxVERTICAL);

        auto* selectionHeader = new wxBoxSizer(wxHORIZONTAL);
        selectionHeader->Add(
            MakeText(
                selection,
                U("路径确认"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        selectionHeader->Add(
            MakePill(selection, U("路径可访问"), Theme::Green(), 105),
            0
        );
        selectionSizer->Add(
            selectionHeader,
            0,
            wxEXPAND | wxALL,
            15
        );

        AddLabel(selection, selectionSizer, U("数据源名称"), true);
        selectionSizer->Add(
            MakeInput(selection, U("项目文档")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddLabel(selection, selectionSizer, U("根目录"), true);
        selectionSizer->Add(
            MakeInput(selection, U("D:\\Pioneer\\docs")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        selectionSizer->Add(
            MakeButton(
                selection,
                wxID_ANY,
                U("浏览本地目录"),
                false,
                false
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        wxArrayString sourceTypes;
        sourceTypes.Add(U("本地文件夹"));
        sourceTypes.Add(U("Git 仓库"));
        sourceTypes.Add(U("邮件归档"));
        sourceTypes.Add(U("单个文件"));

        AddLabel(selection, selectionSizer, U("数据源类型"));
        selectionSizer->Add(
            MakeChoice(selection, sourceTypes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        selectionSizer->Add(
            MakeProperty(
                selection,
                U("读取权限"),
                U("正常"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        selectionSizer->Add(
            MakeProperty(
                selection,
                U("发现文件"),
                U("6,718")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        selectionSizer->Add(
            MakeProperty(
                selection,
                U("支持格式"),
                U("6,602"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* recursive = new wxCheckBox(
            selection,
            wxID_ANY,
            U("包含所有子目录")
        );
        recursive->SetValue(true);
        recursive->SetForegroundColour(Theme::Text());
        recursive->SetFont(Theme::Font(9));

        selectionSizer->Add(
            recursive,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        selectionSizer->AddStretchSpacer();
        selectionSizer->Add(
            MakeWrappedText(
                selection,
                U("应用只读取选定路径；不会移动或修改源文件。"),
                8,
                Theme::Blue(),
                310,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        selection->SetSizer(selectionSizer);

        body->Add(sources, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(selection, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        AddActions(
            this,
            root,
            U("使用此数据源"),
            U("测试路径")
        );

        Finish(root);
    }
};

class EditTemporalDialog final : public StyledDialog
{
public:
    explicit EditTemporalDialog(wxWindow* parent)
        : StyledDialog(parent, U("编辑时间语义"), wxSize(720, 720))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D07"),
            U("编辑时间语义"),
            U("设置对象的发生时间、有效区间、获知时间和时间精度。"),
            Theme::Cyan()
        );

        auto* object = MakeCard(this);
        auto* objectSizer = new wxBoxSizer(wxHORIZONTAL);
        objectSizer->Add(
            MakePill(object, U("事实 F-0054"), Theme::Green(), 112),
            0,
            wxALIGN_CENTER_VERTICAL | wxALL,
            14
        );

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                object,
                U("旧协议保留至第四季度"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        labels->Add(
            MakeText(
                object,
                U("当前时间状态：有效"),
                8,
                Theme::Muted()
            ),
            0
        );

        objectSizer->Add(
            labels,
            1,
            wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,
            14
        );
        object->SetSizer(objectSizer);
        root->Add(
            object,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* fields = MakeCard(this);
        auto* fieldSizer = new wxBoxSizer(wxVERTICAL);
        fieldSizer->Add(
            MakeText(
                fields,
                U("现实时间"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        AddLabel(fields, fieldSizer, U("发生时间"), true);
        fieldSizer->Add(
            MakeInput(fields, U("2026-09-09 14:30")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(fields, fieldSizer, U("有效开始"));
        fieldSizer->Add(
            MakeInput(fields, U("2026-09-09")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(fields, fieldSizer, U("有效结束"));
        auto* validEnd = MakeInput(fields);
        validEnd->SetHint(U("未结束时保持为空"));
        fieldSizer->Add(
            validEnd,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        wxArrayString precisionValues;
        precisionValues.Add(U("精确到分钟"));
        precisionValues.Add(U("精确日期"));
        precisionValues.Add(U("月份"));
        precisionValues.Add(U("季度"));
        precisionValues.Add(U("年份"));
        precisionValues.Add(U("估计时间"));
        precisionValues.Add(U("未知"));

        AddLabel(fields, fieldSizer, U("时间精度"));
        fieldSizer->Add(
            MakeChoice(fields, precisionValues, 3),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* estimated = new wxCheckBox(
            fields,
            wxID_ANY,
            U("该时间为估计值")
        );
        estimated->SetValue(false);
        estimated->SetForegroundColour(Theme::Text());
        estimated->SetFont(Theme::Font(9));
        fieldSizer->Add(
            estimated,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        fields->SetSizer(fieldSizer);

        auto* knowledge = MakeCard(this);
        knowledge->SetMinSize(wxSize(305, -1));
        auto* knowledgeSizer = new wxBoxSizer(wxVERTICAL);
        knowledgeSizer->Add(
            MakeText(
                knowledge,
                U("知识时间"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        AddLabel(knowledge, knowledgeSizer, U("首次获知时间"), true);
        knowledgeSizer->Add(
            MakeInput(knowledge, U("2026-09-09 15:05")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(knowledge, knowledgeSizer, U("记录创建时间"));
        knowledgeSizer->Add(
            MakeInput(
                knowledge,
                U("2026-09-09 15:08"),
                false,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(knowledge, knowledgeSizer, U("时间依据"));
        wxArrayString basis;
        basis.Add(U("来源文件明确记载"));
        basis.Add(U("会议发生时间"));
        basis.Add(U("邮件发送时间"));
        basis.Add(U("用户手工确认"));
        basis.Add(U("系统估算"));

        knowledgeSizer->Add(
            MakeChoice(knowledge, basis, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        knowledgeSizer->Add(
            MakeProperty(
                knowledge,
                U("来源证据"),
                U("E-1042"),
                Theme::Cyan()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        knowledgeSizer->Add(
            MakeProperty(
                knowledge,
                U("历史切片影响"),
                U("3 个切片"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        knowledgeSizer->AddStretchSpacer();
        knowledgeSizer->Add(
            MakeWrappedText(
                knowledge,
                U(
                    "修改获知时间可能改变历史时间切片中的可见性，"
                    "但不会修改原始证据时间戳。"
                ),
                8,
                Theme::Yellow(),
                255,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        knowledge->SetSizer(knowledgeSizer);

        body->Add(fields, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(knowledge, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddActions(
            this,
            root,
            U("保存时间语义"),
            U("预览历史影响")
        );

        Finish(root);
    }
};

wxPanel* MakeEntityCard(
    wxWindow* parent,
    const wxString& code,
    const wxString& name,
    const wxString& detail,
    const wxColour& color,
    bool primary
)
{
    auto* card = MakeCard(
        parent,
        primary ? Theme::Surface3() : Theme::Surface2()
    );
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* header = new wxBoxSizer(wxHORIZONTAL);
    header->Add(
        MakeText(card, code, 8, color, wxFONTWEIGHT_BOLD),
        1,
        wxALIGN_CENTER_VERTICAL
    );
    header->Add(
        MakePill(
            card,
            primary ? U("保留") : U("合并"),
            color,
            82
        ),
        0
    );

    cardSizer->Add(header, 0, wxEXPAND | wxALL, 14);
    cardSizer->Add(
        MakeText(
            card,
            name,
            15,
            Theme::Text(),
            wxFONTWEIGHT_BOLD
        ),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );
    cardSizer->Add(
        MakeWrappedText(
            card,
            detail,
            8,
            Theme::Muted(),
            275
        ),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );

    card->SetSizer(cardSizer);
    return card;
}

class MergeEntitiesDialog final : public StyledDialog
{
public:
    explicit MergeEntitiesDialog(wxWindow* parent)
        : StyledDialog(parent, U("合并重复实体"), wxSize(860, 730))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D08"),
            U("合并重复实体"),
            U("选择保留实体、合并别名和引用，并在提交前检查关系变化。"),
            Theme::Purple()
        );

        auto* comparison = new wxBoxSizer(wxHORIZONTAL);
        comparison->Add(
            MakeEntityCard(
                this,
                U("PERSON-018"),
                U("周启明"),
                U(
                    "规范名称 · qiming@example.local\n"
                    "31 个对象引用 · 3 个别名"
                ),
                Theme::Cyan(),
                true
            ),
            1,
            wxRIGHT,
            12
        );
        comparison->Add(
            MakeEntityCard(
                this,
                U("PERSON-041"),
                U("Qiming Zhou"),
                U(
                    "自动识别实体 · 未提供电子邮件\n"
                    "7 个对象引用 · 1 个别名"
                ),
                Theme::Yellow(),
                false
            ),
            1
        );

        root->Add(
            comparison,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* merge = MakeCard(this);
        auto* mergeSizer = new wxBoxSizer(wxVERTICAL);
        mergeSizer->Add(
            MakeText(
                merge,
                U("合并设置"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString targets;
        targets.Add(U("保留 PERSON-018 · 周启明"));
        targets.Add(U("保留 PERSON-041 · Qiming Zhou"));

        AddLabel(merge, mergeSizer, U("主实体"), true);
        mergeSizer->Add(
            MakeChoice(merge, targets, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(merge, mergeSizer, U("规范名称"), true);
        mergeSizer->Add(
            MakeInput(merge, U("周启明")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(merge, mergeSizer, U("合并后的别名"));
        mergeSizer->Add(
            MakeInput(
                merge,
                U("Qiming Zhou, Qiming, 周工"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* moveRelations = new wxCheckBox(
            merge,
            wxID_ANY,
            U("迁移全部对象关系和证据引用")
        );
        moveRelations->SetValue(true);
        moveRelations->SetForegroundColour(Theme::Text());
        moveRelations->SetFont(Theme::Font(9));
        mergeSizer->Add(
            moveRelations,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* keepRedirect = new wxCheckBox(
            merge,
            wxID_ANY,
            U("为旧实体编号保留重定向")
        );
        keepRedirect->SetValue(true);
        keepRedirect->SetForegroundColour(Theme::Text());
        keepRedirect->SetFont(Theme::Font(9));
        mergeSizer->Add(
            keepRedirect,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        merge->SetSizer(mergeSizer);

        auto* impact = MakeCard(this);
        impact->SetMinSize(wxSize(350, -1));
        auto* impactSizer = new wxBoxSizer(wxVERTICAL);
        impactSizer->Add(
            MakeText(
                impact,
                U("合并影响"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        impactSizer->Add(
            MakeProperty(impact, U("对象引用"), U("38"), Theme::Cyan()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(impact, U("证据引用"), U("12"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(impact, U("保存的查询"), U("2"), Theme::Blue()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(impact, U("规则引用"), U("1"), Theme::Purple()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(impact, U("冲突"), U("0"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        impactSizer->Add(
            MakeResultRow(
                impact,
                U("PREVIEW"),
                U("PERSON-041 将进入回收站"),
                U("旧编号继续重定向到 PERSON-018"),
                U("可恢复"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        impactSizer->AddStretchSpacer();
        impactSizer->Add(
            MakeWrappedText(
                impact,
                U(
                    "合并不会删除审计历史；所有引用迁移都会记录为"
                    "独立审计事件。"
                ),
                8,
                Theme::Blue(),
                300,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        impact->SetSizer(impactSizer);

        body->Add(merge, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(impact, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddActions(
            this,
            root,
            U("合并实体"),
            U("交换主实体")
        );

        Finish(root);
    }
};

class TestRuleDialog final : public StyledDialog
{
public:
    explicit TestRuleDialog(wxWindow* parent)
        : StyledDialog(parent, U("测试规则"), wxSize(900, 750))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D09"),
            U("测试规则"),
            U("在历史对象或当前工作区样本上运行规则，不修改正式数据。"),
            Theme::Green()
        );

        auto* rule = MakeCard(this);
        auto* ruleSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                rule,
                U("RULE-018 · 同一主体的当前日期值冲突"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        labels->Add(
            MakeText(
                rule,
                U("冲突检测 · 已启用 · 最近运行今天 12:16"),
                8,
                Theme::Muted()
            ),
            0
        );

        ruleSizer->Add(labels, 1, wxALL, 14);
        ruleSizer->Add(
            MakePill(rule, U("测试模式"), Theme::Green(), 100),
            0,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            14
        );
        rule->SetSizer(ruleSizer);
        root->Add(
            rule,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* settings = MakeCard(this);
        settings->SetMinSize(wxSize(340, -1));
        auto* settingSizer = new wxBoxSizer(wxVERTICAL);
        settingSizer->Add(
            MakeText(
                settings,
                U("测试范围"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString scopes;
        scopes.Add(U("当前工作区对象"));
        scopes.Add(U("选定历史快照"));
        scopes.Add(U("固定测试数据集"));
        scopes.Add(U("单个对象"));

        AddLabel(settings, settingSizer, U("数据范围"), true);
        settingSizer->Add(
            MakeChoice(settings, scopes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        wxArrayString objectTypes;
        objectTypes.Add(U("事实"));
        objectTypes.Add(U("全部对象"));
        objectTypes.Add(U("决策"));
        objectTypes.Add(U("承诺"));

        AddLabel(settings, settingSizer, U("对象类型"));
        settingSizer->Add(
            MakeChoice(settings, objectTypes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(settings, settingSizer, U("时间范围"));
        settingSizer->Add(
            MakeInput(settings, U("2026-09-01 至 2026-09-18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(settings, settingSizer, U("最大样本数"));
        settingSizer->Add(
            MakeInput(settings, U("500")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* includeDeleted = new wxCheckBox(
            settings,
            wxID_ANY,
            U("包含回收站中的历史对象")
        );
        includeDeleted->SetValue(false);
        includeDeleted->SetForegroundColour(Theme::Text());
        includeDeleted->SetFont(Theme::Font(9));
        settingSizer->Add(
            includeDeleted,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* trace = new wxCheckBox(
            settings,
            wxID_ANY,
            U("记录详细条件求值跟踪")
        );
        trace->SetValue(true);
        trace->SetForegroundColour(Theme::Text());
        trace->SetFont(Theme::Font(9));
        settingSizer->Add(
            trace,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        settingSizer->Add(
            MakeButton(
                settings,
                wxID_ANY,
                U("运行测试"),
                true,
                false
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        settingSizer->Add(
            MakeProgress(settings, 100),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        settingSizer->Add(
            MakeText(
                settings,
                U("测试完成 · 用时 0.84 秒"),
                8,
                Theme::Green(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        settings->SetSizer(settingSizer);

        auto* results = MakeCard(this);
        auto* resultSizer = new wxBoxSizer(wxVERTICAL);

        auto* resultHeader = new wxBoxSizer(wxHORIZONTAL);
        resultHeader->Add(
            MakeText(
                results,
                U("测试结果"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        resultHeader->Add(
            MakePill(results, U("6 项命中"), Theme::Yellow(), 100),
            0
        );
        resultSizer->Add(resultHeader, 0, wxEXPAND | wxALL, 15);

        auto* metrics = new wxBoxSizer(wxHORIZONTAL);

        const std::array<std::tuple<wxString, wxString, wxColour>, 4> values = {{
            {U("168"), U("已检查"), Theme::Blue()},
            {U("6"), U("规则命中"), Theme::Yellow()},
            {U("0"), U("执行错误"), Theme::Green()},
            {U("94%"), U("预期一致"), Theme::Purple()}
        }};

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            auto* card = MakeCard(results, Theme::Surface2());
            auto* cardSizer = new wxBoxSizer(wxVERTICAL);
            cardSizer->Add(
                MakeText(
                    card,
                    std::get<0>(values[index]),
                    15,
                    std::get<2>(values[index]),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxBOTTOM,
                4
            );
            cardSizer->Add(
                MakeText(
                    card,
                    std::get<1>(values[index]),
                    8,
                    Theme::Muted()
                ),
                0
            );
            card->SetSizer(cardSizer);

            metrics->Add(
                card,
                1,
                index + 1 < values.size() ? wxRIGHT : 0,
                index + 1 < values.size() ? 8 : 0
            );
        }

        resultSizer->Add(
            metrics,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        resultSizer->Add(
            MakeResultRow(
                results,
                U("CF-PREVIEW-01"),
                U("交付日期存在两个当前值"),
                U("F-0062 与 F-0041 · 置信度 98%"),
                U("预期命中"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        resultSizer->Add(
            MakeResultRow(
                results,
                U("CF-PREVIEW-02"),
                U("旧协议退出时间不一致"),
                U("F-0054 与 F-0049 · 置信度 91%"),
                U("预期命中"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );
        resultSizer->Add(
            MakeResultRow(
                results,
                U("CF-PREVIEW-05"),
                U("环境验证完成日期"),
                U("日期属于不同环境范围"),
                U("可能误报"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            9
        );

        resultSizer->AddStretchSpacer();
        resultSizer->Add(
            MakeWrappedText(
                results,
                U("测试模式不会创建正式冲突、修改对象或写入业务审计事件。"),
                8,
                Theme::Blue(),
                450,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        results->SetSizer(resultSizer);

        body->Add(settings, 0, wxEXPAND | wxRIGHT, 12);
        body->Add(results, 1, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddActions(
            this,
            root,
            U("应用测试配置"),
            U("导出测试结果")
        );

        Finish(root);
    }
};

class ExportSecurityDialog final : public StyledDialog
{
public:
    explicit ExportSecurityDialog(wxWindow* parent)
        : StyledDialog(parent, U("导出安全设置"), wxSize(760, 750))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D10"),
            U("导出安全设置"),
            U("设置导出口令、脱敏策略、附件范围和离线包完整性保护。"),
            Theme::Yellow()
        );

        auto* package = MakeCard(this);
        auto* packageSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                package,
                U("先锋计划交接包"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        labels->Add(
            MakeText(
                package,
                U("独立 HTML · 18 页 · 预计 186 MB"),
                8,
                Theme::Muted()
            ),
            0
        );

        packageSizer->Add(labels, 1, wxALL, 14);
        packageSizer->Add(
            MakePill(package, U("等待安全确认"), Theme::Yellow(), 125),
            0,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            14
        );
        package->SetSizer(packageSizer);

        root->Add(
            package,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* protection = MakeCard(this);
        auto* protectionSizer = new wxBoxSizer(wxVERTICAL);
        protectionSizer->Add(
            MakeText(
                protection,
                U("包保护"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString modes;
        modes.Add(U("口令加密离线包"));
        modes.Add(U("仅导出到受信任目录"));
        modes.Add(U("不加密，仅生成完整性清单"));

        AddLabel(protection, protectionSizer, U("保护方式"), true);
        protectionSizer->Add(
            MakeChoice(protection, modes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(protection, protectionSizer, U("导出口令"), true);
        auto* password = MakeInput(
            protection,
            wxEmptyString,
            false,
            false,
            wxTE_PASSWORD
        );
        password->SetHint(U("至少 12 个字符"));
        protectionSizer->Add(
            password,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );

        AddLabel(protection, protectionSizer, U("确认口令"), true);
        protectionSizer->Add(
            MakeInput(
                protection,
                wxEmptyString,
                false,
                false,
                wxTE_PASSWORD
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        protectionSizer->Add(
            MakeProperty(
                protection,
                U("加密算法"),
                U("AES-256-GCM"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        protectionSizer->Add(
            MakeProperty(
                protection,
                U("口令派生"),
                U("Argon2id"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* manifest = new wxCheckBox(
            protection,
            wxID_ANY,
            U("生成 SHA-256 完整性清单")
        );
        manifest->SetValue(true);
        manifest->SetForegroundColour(Theme::Text());
        manifest->SetFont(Theme::Font(9));
        protectionSizer->Add(
            manifest,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* expiry = new wxCheckBox(
            protection,
            wxID_ANY,
            U("设置报告访问期限")
        );
        expiry->SetValue(false);
        expiry->SetForegroundColour(Theme::Text());
        expiry->SetFont(Theme::Font(9));
        protectionSizer->Add(
            expiry,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        protection->SetSizer(protectionSizer);

        auto* privacy = MakeCard(this);
        privacy->SetMinSize(wxSize(340, -1));
        auto* privacySizer = new wxBoxSizer(wxVERTICAL);
        privacySizer->Add(
            MakeText(
                privacy,
                U("隐私与附件"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        const std::array<std::tuple<wxString, wxString, bool>, 5> options = {{
            {
                U("移除绝对文件路径"),
                U("替换为数据源名称和相对路径"),
                true
            },
            {
                U("脱敏电子邮件地址"),
                U("保留实体名称，隐藏地址正文"),
                true
            },
            {
                U("移除 Windows 用户名"),
                U("从日志和路径中删除用户名"),
                true
            },
            {
                U("包含原始附件"),
                U("包含 35 个当前可访问文件"),
                true
            },
            {
                U("包含历史文件版本"),
                U("可能额外增加 624 MB"),
                false
            }
        }};

        for (const auto& option : options)
        {
            auto* row = MakeCard(privacy, Theme::Surface2());
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            auto* check = new wxCheckBox(
                row,
                wxID_ANY,
                wxEmptyString
            );
            check->SetValue(std::get<2>(option));
            check->SetBackgroundColour(Theme::Surface2());

            auto* rowLabels = new wxBoxSizer(wxVERTICAL);
            rowLabels->Add(
                MakeText(
                    row,
                    std::get<0>(option),
                    9,
                    Theme::Text(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxBOTTOM,
                4
            );
            rowLabels->Add(
                MakeText(
                    row,
                    std::get<1>(option),
                    8,
                    Theme::Muted()
                ),
                0
            );

            rowSizer->Add(
                check,
                0,
                wxALIGN_CENTER_VERTICAL | wxALL,
                11
            );
            rowSizer->Add(
                rowLabels,
                1,
                wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT,
                10
            );
            row->SetSizer(rowSizer);

            privacySizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        privacySizer->Add(
            MakeProperty(
                privacy,
                U("脱敏规则"),
                U("3 条已启用"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        privacySizer->Add(
            MakeProperty(
                privacy,
                U("不可访问附件"),
                U("1 个将跳过"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        privacy->SetSizer(privacySizer);

        body->Add(protection, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(privacy, 0, wxEXPAND);

        root->Add(
            body,
            1,
            wxEXPAND | wxALL,
            18
        );

        auto* validation = MakeCard(this, Theme::Surface2());
        auto* validationSizer = new wxBoxSizer(wxHORIZONTAL);
        validationSizer->Add(
            MakeText(
                validation,
                U("●"),
                10,
                Theme::Green(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_TOP | wxRIGHT,
            10
        );
        validationSizer->Add(
            MakeWrappedText(
                validation,
                U(
                    "安全预检查通过：未发现绝对路径泄露；"
                    "1 个不可访问附件将在清单中标记为缺失。"
                ),
                9,
                Theme::Muted(),
                620
            ),
            1
        );
        validation->SetSizer(validationSizer);

        root->Add(
            validation,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        AddActions(
            this,
            root,
            U("确认安全设置"),
            U("重新运行预检查")
        );

        Finish(root);
    }
};

}

wxDialog* CreateAdvancedDialog(
    wxWindow* parent,
    const wxString& dialogCode
)
{
    if (dialogCode == U("D06"))
    {
        return new SelectDataSourceDialog(parent);
    }

    if (dialogCode == U("D07"))
    {
        return new EditTemporalDialog(parent);
    }

    if (dialogCode == U("D08"))
    {
        return new MergeEntitiesDialog(parent);
    }

    if (dialogCode == U("D09"))
    {
        return new TestRuleDialog(parent);
    }

    if (dialogCode == U("D10"))
    {
        return new ExportSecurityDialog(parent);
    }


    if (auto* systemDialog = CreateSystemDialog(
            parent,
            dialogCode
        ))
    {
        return systemDialog;
    }

    return nullptr;
}

}
