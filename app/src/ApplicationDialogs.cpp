#include "ApplicationDialogs.h"

#include "AdvancedDialogs.h"

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

void AddDialogHeader(
    wxDialog* dialog,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle,
    const wxColour& accent = Theme::Blue()
)
{
    auto* header = MakeCard(dialog);
    auto* headerSizer = new wxBoxSizer(wxHORIZONTAL);

    auto* labels = new wxBoxSizer(wxVERTICAL);
    labels->Add(
        MakeText(header, code, 8, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    labels->Add(
        MakeText(header, title, 17, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        6
    );
    labels->Add(
        MakeWrappedText(header, subtitle, 9, Theme::Muted(), 620),
        0
    );

    headerSizer->Add(labels, 1, wxALL, 18);
    header->SetSizer(headerSizer);

    root->Add(
        header,
        0,
        wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
        18
    );
}

void AddFieldLabel(
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

void AddDialogActions(
    wxDialog* dialog,
    wxBoxSizer* root,
    const wxString& confirmLabel,
    bool danger = false,
    const wxString& secondaryLabel = wxEmptyString
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
                130
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
            145
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

class CreateObjectDialog final : public StyledDialog
{
public:
    explicit CreateObjectDialog(wxWindow* parent)
        : StyledDialog(parent, U("创建业务对象"), wxSize(760, 720))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddDialogHeader(
            this,
            root,
            U("D01"),
            U("创建业务对象"),
            U("创建事实、决策、承诺、风险或未决问题，并设置初始时间语义。")
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* form = MakeCard(this);
        auto* formSizer = new wxBoxSizer(wxVERTICAL);

        formSizer->Add(
            MakeText(
                form,
                U("基本信息"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );

        AddFieldLabel(form, formSizer, U("对象类型"), true);

        wxArrayString objectTypes;
        objectTypes.Add(U("事实"));
        objectTypes.Add(U("决策"));
        objectTypes.Add(U("承诺"));
        objectTypes.Add(U("风险"));
        objectTypes.Add(U("未决问题"));

        formSizer->Add(
            MakeChoice(form, objectTypes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        AddFieldLabel(form, formSizer, U("标题"), true);
        auto* title = MakeInput(
            form,
            U("旧协议保留至第四季度")
        );
        title->SetHint(U("输入简洁、可搜索的对象标题"));
        formSizer->Add(
            title,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        AddFieldLabel(form, formSizer, U("说明"));
        formSizer->Add(
            MakeInput(
                form,
                U(
                    "旧协议将在合作方完成切换前保持可用，"
                    "具体关闭日期由交付委员会确认。"
                ),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* twoColumns = new wxBoxSizer(wxHORIZONTAL);

        auto* leftFields = MakeCard(form, Theme::Surface2());
        auto* leftSizer = new wxBoxSizer(wxVERTICAL);
        AddFieldLabel(leftFields, leftSizer, U("负责人"));

        wxArrayString owners;
        owners.Add(U("未指定"));
        owners.Add(U("周启明"));
        owners.Add(U("林致远"));
        owners.Add(U("交付委员会"));

        leftSizer->Add(
            MakeChoice(leftFields, owners, 1),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        leftFields->SetSizer(leftSizer);

        auto* rightFields = MakeCard(form, Theme::Surface2());
        auto* rightSizer = new wxBoxSizer(wxVERTICAL);
        AddFieldLabel(rightFields, rightSizer, U("优先级"));

        wxArrayString priorities;
        priorities.Add(U("普通"));
        priorities.Add(U("高"));
        priorities.Add(U("紧急"));
        priorities.Add(U("低"));

        rightSizer->Add(
            MakeChoice(rightFields, priorities, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        rightFields->SetSizer(rightSizer);

        twoColumns->Add(leftFields, 1, wxRIGHT, 10);
        twoColumns->Add(rightFields, 1);

        formSizer->Add(
            twoColumns,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        AddFieldLabel(form, formSizer, U("标签"));
        auto* tags = MakeInput(form, U("协议迁移, 交付"));
        tags->SetHint(U("使用逗号分隔多个标签"));
        formSizer->Add(
            tags,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        form->SetSizer(formSizer);

        auto* temporal = MakeCard(this);
        temporal->SetMinSize(wxSize(280, -1));
        auto* temporalSizer = new wxBoxSizer(wxVERTICAL);

        temporalSizer->Add(
            MakeText(
                temporal,
                U("时间语义"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            16
        );
        temporalSizer->Add(
            MakeWrappedText(
                temporal,
                U("区分对象何时发生、团队何时获知以及记录何时创建。"),
                8,
                Theme::Muted(),
                235
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        AddFieldLabel(temporal, temporalSizer, U("发生时间"));
        temporalSizer->Add(
            MakeInput(temporal, U("2026-09-18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddFieldLabel(temporal, temporalSizer, U("获知时间"));
        temporalSizer->Add(
            MakeInput(temporal, U("2026-09-18 12:18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddFieldLabel(temporal, temporalSizer, U("时间精度"));

        wxArrayString precisions;
        precisions.Add(U("精确日期"));
        precisions.Add(U("月份"));
        precisions.Add(U("季度"));
        precisions.Add(U("估计时间"));
        precisions.Add(U("未知"));

        temporalSizer->Add(
            MakeChoice(temporal, precisions, 2),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        temporalSizer->Add(
            new wxStaticLine(temporal, wxID_ANY),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        auto* evidenceCheck = new wxCheckBox(
            temporal,
            wxID_ANY,
            U("创建后立即关联证据")
        );
        evidenceCheck->SetValue(true);
        evidenceCheck->SetForegroundColour(Theme::Text());
        evidenceCheck->SetFont(Theme::Font(9));

        temporalSizer->Add(
            evidenceCheck,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        temporalSizer->Add(
            MakePill(
                temporal,
                U("将创建 F-0063"),
                Theme::Green(),
                130
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            16
        );

        temporalSizer->AddStretchSpacer();
        temporalSizer->Add(
            MakeWrappedText(
                temporal,
                U("所有初始字段及后续修改都会写入审计日志。"),
                8,
                Theme::Blue(),
                235,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            16
        );
        temporal->SetSizer(temporalSizer);

        body->Add(form, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(temporal, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 18);

        AddDialogActions(
            this,
            root,
            U("创建对象"),
            false,
            U("创建并继续")
        );

        Finish(root);
    }
};

class LinkEvidenceDialog final : public StyledDialog
{
public:
    explicit LinkEvidenceDialog(wxWindow* parent)
        : StyledDialog(parent, U("关联证据"), wxSize(850, 680))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddDialogHeader(
            this,
            root,
            U("D02"),
            U("关联证据"),
            U("选择已有证据并指定其对当前对象的支持、反对或上下文作用。"),
            Theme::Cyan()
        );

        auto* target = MakeCard(this);
        auto* targetSizer = new wxBoxSizer(wxHORIZONTAL);
        targetSizer->Add(
            MakePill(target, U("事实 F-0054"), Theme::Green(), 112),
            0,
            wxALIGN_CENTER_VERTICAL | wxALL,
            14
        );

        auto* targetLabels = new wxBoxSizer(wxVERTICAL);
        targetLabels->Add(
            MakeText(
                target,
                U("旧协议保留至第四季度"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        targetLabels->Add(
            MakeText(
                target,
                U("当前已关联 3 项证据"),
                8,
                Theme::Muted()
            ),
            0
        );
        targetSizer->Add(
            targetLabels,
            1,
            wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,
            14
        );
        target->SetSizer(targetSizer);

        root->Add(
            target,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* browser = MakeCard(this);
        auto* browserSizer = new wxBoxSizer(wxVERTICAL);

        auto* search = MakeInput(browser);
        search->SetHint(U("搜索证据编号、原文或来源文件"));
        browserSizer->Add(
            search,
            0,
            wxEXPAND | wxALL,
            15
        );

        wxArrayString evidenceRows;
        evidenceRows.Add(
            U("E-1097  requirements-v7.pdf · 第 18 页")
        );
        evidenceRows.Add(
            U("E-1042  会议纪要-0909.md · 第 42 行")
        );
        evidenceRows.Add(
            U("E-1124  协议评审.eml · 正文第 6 段")
        );
        evidenceRows.Add(
            U("E-0841  legacy-contract.pdf · 第 12 页")
        );
        evidenceRows.Add(
            U("E-1130  安全评审.eml · 正文第 4 段")
        );

        auto* evidenceList = new wxListBox(
            browser,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            evidenceRows,
            wxLB_SINGLE | wxBORDER_NONE
        );
        evidenceList->SetBackgroundColour(Theme::Surface2());
        evidenceList->SetForegroundColour(Theme::Text());
        evidenceList->SetFont(Theme::Font(9));
        evidenceList->SetSelection(0);

        browserSizer->Add(
            evidenceList,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        browserSizer->Add(
            MakeText(
                browser,
                U("显示 5 / 127 项证据"),
                8,
                Theme::Faint()
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        browser->SetSizer(browserSizer);

        auto* detail = MakeCard(this);
        detail->SetMinSize(wxSize(350, -1));
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);

        auto* detailHeader = new wxBoxSizer(wxHORIZONTAL);
        detailHeader->Add(
            MakeText(
                detail,
                U("证据预览"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        detailHeader->Add(
            MakePill(detail, U("需要复核"), Theme::Yellow(), 102),
            0
        );
        detailSizer->Add(detailHeader, 0, wxEXPAND | wxALL, 15);

        auto* quote = MakeCard(detail, Theme::Surface2());
        auto* quoteSizer = new wxBoxSizer(wxVERTICAL);
        quoteSizer->Add(
            MakeText(
                quote,
                U("E-1097 · 保存原文"),
                8,
                Theme::Faint(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            8
        );
        quoteSizer->Add(
            MakeWrappedText(
                quote,
                U(
                    "旧协议保留至 2026 年第四季度结束，"
                    "具体关闭日期由交付委员会另行确认。"
                ),
                10,
                Theme::Text(),
                290,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0
        );
        quote->SetSizer(quoteSizer);
        detailSizer->Add(
            quote,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        detailSizer->Add(
            MakeProperty(
                detail,
                U("来源"),
                U("requirements-v7.pdf")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        detailSizer->Add(
            MakeProperty(
                detail,
                U("锚点"),
                U("第 18 页 · 段落 12")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        detailSizer->Add(
            MakeProperty(
                detail,
                U("指纹状态"),
                U("来源已变化"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddFieldLabel(detail, detailSizer, U("证据角色"), true);

        wxArrayString roles;
        roles.Add(U("支持"));
        roles.Add(U("反对"));
        roles.Add(U("上下文"));
        roles.Add(U("替代依据"));
        roles.Add(U("完成证明"));

        detailSizer->Add(
            MakeChoice(detail, roles, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddFieldLabel(detail, detailSizer, U("关联说明"));
        auto* note = MakeInput(
            detail,
            U("该证据支持旧协议继续保留，但最终日期仍待确认。"),
            true
        );
        detailSizer->Add(
            note,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        detailSizer->AddStretchSpacer();

        auto* allowReview = new wxCheckBox(
            detail,
            wxID_ANY,
            U("允许关联需要复核的证据")
        );
        allowReview->SetValue(true);
        allowReview->SetForegroundColour(Theme::Text());
        allowReview->SetFont(Theme::Font(9));

        detailSizer->Add(
            allowReview,
            0,
            wxALL,
            15
        );
        detail->SetSizer(detailSizer);

        body->Add(browser, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 18);

        AddDialogActions(
            this,
            root,
            U("关联所选证据"),
            false,
            U("打开来源")
        );

        Finish(root);
    }
};

wxPanel* MakeConflictClaim(
    wxWindow* parent,
    const wxString& heading,
    const wxString& value,
    const wxString& source,
    const wxString& evidenceCode,
    const wxColour& color
)
{
    auto* card = MakeCard(parent, Theme::Surface2());
    auto* cardSizer = new wxBoxSizer(wxVERTICAL);

    auto* headingRow = new wxBoxSizer(wxHORIZONTAL);
    headingRow->Add(
        MakeText(card, heading, 9, color, wxFONTWEIGHT_BOLD),
        1,
        wxALIGN_CENTER_VERTICAL
    );
    headingRow->Add(
        MakePill(card, evidenceCode, color, 82),
        0
    );

    cardSizer->Add(headingRow, 0, wxEXPAND | wxALL, 14);
    cardSizer->Add(
        MakeText(
            card,
            value,
            16,
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
            source,
            8,
            Theme::Muted(),
            285
        ),
        0,
        wxLEFT | wxRIGHT | wxBOTTOM,
        14
    );

    card->SetSizer(cardSizer);
    return card;
}

class ResolveConflictDialog final : public StyledDialog
{
public:
    explicit ResolveConflictDialog(wxWindow* parent)
        : StyledDialog(parent, U("冲突裁决"), wxSize(900, 750))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddDialogHeader(
            this,
            root,
            U("D03"),
            U("冲突裁决"),
            U("比较相反主张、证据质量和时间顺序，并记录可审计的处理结论。"),
            Theme::Red()
        );

        auto* conflict = MakeCard(this);
        auto* conflictSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* labels = new wxBoxSizer(wxVERTICAL);
        labels->Add(
            MakeText(
                conflict,
                U("CF-0061 · 交付日期存在两个当前值"),
                12,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            5
        );
        labels->Add(
            MakeText(
                conflict,
                U("同一项目的最终交付日期不能同时保持两个当前值"),
                8,
                Theme::Muted()
            ),
            0
        );

        conflictSizer->Add(labels, 1, wxALL, 15);
        conflictSizer->Add(
            MakePill(conflict, U("高严重度"), Theme::Red(), 105),
            0,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            15
        );
        conflict->SetSizer(conflictSizer);

        root->Add(
            conflict,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* claims = new wxBoxSizer(wxHORIZONTAL);
        claims->Add(
            MakeConflictClaim(
                this,
                U("主张 A"),
                U("2026 年 10 月 15 日"),
                U(
                    "交付计划.xlsx · 里程碑表 · 版本 v5\n"
                    "记录时间：2026-09-02"
                ),
                U("E-1018"),
                Theme::Red()
            ),
            1,
            wxRIGHT,
            12
        );
        claims->Add(
            MakeConflictClaim(
                this,
                U("主张 B"),
                U("2026 年 11 月 30 日"),
                U(
                    "requirements-v7.pdf · 第 8 页 · 版本 v7\n"
                    "记录时间：2026-09-18"
                ),
                U("E-1144"),
                Theme::Blue()
            ),
            1
        );

        root->Add(
            claims,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* decision = MakeCard(this);
        auto* decisionSizer = new wxBoxSizer(wxVERTICAL);

        decisionSizer->Add(
            MakeText(
                decision,
                U("处理结论"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString outcomes;
        outcomes.Add(U("主张 B 为当前有效值"));
        outcomes.Add(U("主张 A 为当前有效值"));
        outcomes.Add(U("两个值适用于不同时间范围"));
        outcomes.Add(U("两个值适用于不同范围"));
        outcomes.Add(U("信息不足，暂不处理"));
        outcomes.Add(U("冲突为误报"));

        auto* outcome = new wxRadioBox(
            decision,
            wxID_ANY,
            U("选择裁决结果"),
            wxDefaultPosition,
            wxDefaultSize,
            outcomes,
            1,
            wxRA_SPECIFY_COLS
        );
        outcome->SetSelection(0);
        outcome->SetBackgroundColour(Theme::Surface());
        outcome->SetForegroundColour(Theme::Text());
        outcome->SetFont(Theme::Font(9));

        decisionSizer->Add(
            outcome,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* fields = new wxBoxSizer(wxHORIZONTAL);

        auto* effective = MakeCard(decision, Theme::Surface2());
        auto* effectiveSizer = new wxBoxSizer(wxVERTICAL);
        AddFieldLabel(effective, effectiveSizer, U("确认的当前值"));
        effectiveSizer->Add(
            MakeInput(effective, U("2026-11-30")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        effective->SetSizer(effectiveSizer);

        auto* superseded = MakeCard(decision, Theme::Surface2());
        auto* supersededSizer = new wxBoxSizer(wxVERTICAL);
        AddFieldLabel(superseded, supersededSizer, U("旧值处理"));
        wxArrayString oldValueActions;
        oldValueActions.Add(U("标记为被替代"));
        oldValueActions.Add(U("标记为无效"));
        oldValueActions.Add(U("保留为历史值"));
        supersededSizer->Add(
            MakeChoice(superseded, oldValueActions, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );
        superseded->SetSizer(supersededSizer);

        fields->Add(effective, 1, wxRIGHT, 10);
        fields->Add(superseded, 1);

        decisionSizer->Add(
            fields,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddFieldLabel(decision, decisionSizer, U("裁决理由"), true);
        decisionSizer->Add(
            MakeInput(
                decision,
                U(
                    "需求文件 v7 是较新的正式来源，并明确使用“最终交付日期”"
                    "描述 11 月 30 日；旧里程碑值保留为历史计划。"
                ),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        decisionSizer->Add(
            MakeProperty(
                decision,
                U("预计影响"),
                U("更新 1 项事实 · 关闭 1 项冲突 · 保留 2 项证据"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        decision->SetSizer(decisionSizer);
        root->Add(
            decision,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddDialogActions(
            this,
            root,
            U("确认裁决"),
            false,
            U("保存为草稿")
        );

        Finish(root);
    }
};

class DeleteConfirmationDialog final : public StyledDialog
{
public:
    explicit DeleteConfirmationDialog(wxWindow* parent)
        : StyledDialog(parent, U("确认删除"), wxSize(680, 650))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddDialogHeader(
            this,
            root,
            U("D04"),
            U("确认删除"),
            U("对象将进入回收站。请先检查引用影响和恢复能力。"),
            Theme::Red()
        );

        auto* object = MakeCard(this);
        auto* objectSizer = new wxBoxSizer(wxHORIZONTAL);
        objectSizer->Add(
            MakePill(object, U("决策 D-0027"), Theme::Purple(), 120),
            0,
            wxALIGN_CENTER_VERTICAL | wxALL,
            15
        );

        auto* objectLabels = new wxBoxSizer(wxVERTICAL);
        objectLabels->Add(
            MakeText(
                object,
                U("直接切换新协议"),
                13,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            4
        );
        objectLabels->Add(
            MakeText(
                object,
                U("当前状态：已被 D-0031 替代"),
                8,
                Theme::Muted()
            ),
            0
        );

        objectSizer->Add(
            objectLabels,
            1,
            wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,
            15
        );
        object->SetSizer(objectSizer);

        root->Add(
            object,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* impact = MakeCard(this);
        auto* impactSizer = new wxBoxSizer(wxVERTICAL);

        impactSizer->Add(
            MakeText(
                impact,
                U("引用影响"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        impactSizer->Add(
            MakeProperty(
                impact,
                U("当前对象引用"),
                U("0"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(
                impact,
                U("历史对象引用"),
                U("2"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(
                impact,
                U("关联证据"),
                U("3"),
                Theme::Cyan()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        impactSizer->Add(
            MakeProperty(
                impact,
                U("审计事件"),
                U("14 · 永久保留"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* warning = MakeCard(impact, Theme::Surface2());
        auto* warningSizer = new wxBoxSizer(wxHORIZONTAL);
        warningSizer->Add(
            MakeText(
                warning,
                U("●"),
                10,
                Theme::Yellow(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_TOP | wxRIGHT,
            10
        );
        warningSizer->Add(
            MakeWrappedText(
                warning,
                U(
                    "历史时间切片仍会显示该决策；当前视图中的直接入口将被隐藏。"
                    "关联证据不会随对象删除。"
                ),
                9,
                Theme::Muted(),
                520
            ),
            1
        );
        warning->SetSizer(warningSizer);
        impactSizer->Add(
            warning,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        impact->SetSizer(impactSizer);
        root->Add(
            impact,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* options = MakeCard(this);
        auto* optionSizer = new wxBoxSizer(wxVERTICAL);

        AddFieldLabel(options, optionSizer, U("删除原因"), true);

        wxArrayString reasons;
        reasons.Add(U("对象已被替代"));
        reasons.Add(U("重复对象"));
        reasons.Add(U("错误创建"));
        reasons.Add(U("内容不再适用"));
        reasons.Add(U("其他"));

        optionSizer->Add(
            MakeChoice(options, reasons, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddFieldLabel(options, optionSizer, U("备注"));
        optionSizer->Add(
            MakeInput(
                options,
                U("已由 D-0031 双轨迁移方案正式替代。"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* acknowledge = new wxCheckBox(
            options,
            wxID_ANY,
            U("我已检查上述引用影响")
        );
        acknowledge->SetValue(false);
        acknowledge->SetForegroundColour(Theme::Text());
        acknowledge->SetFont(Theme::Font(9));

        optionSizer->Add(
            acknowledge,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        options->SetSizer(optionSizer);
        root->Add(
            options,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddDialogActions(
            this,
            root,
            U("移到回收站"),
            true,
            U("打开关系浏览器")
        );

        Finish(root);
    }
};

class UnlockWorkspaceDialog final : public StyledDialog
{
public:
    explicit UnlockWorkspaceDialog(wxWindow* parent)
        : StyledDialog(parent, U("解锁工作区"), wxSize(560, 570))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddDialogHeader(
            this,
            root,
            U("D05"),
            U("解锁工作区"),
            U("验证当前 Windows 身份或输入恢复口令以解锁本地加密工作区。"),
            Theme::Purple()
        );

        auto* identity = MakeCard(this);
        auto* identitySizer = new wxBoxSizer(wxVERTICAL);

        identitySizer->Add(
            MakeText(
                identity,
                U("先锋计划"),
                16,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_CENTER | wxTOP | wxBOTTOM,
            18
        );
        identitySizer->Add(
            MakePill(
                identity,
                U("SQLCipher 已加密"),
                Theme::Green(),
                130
            ),
            0,
            wxALIGN_CENTER | wxBOTTOM,
            18
        );
        identitySizer->Add(
            MakeText(
                identity,
                U("D:\\UnknownSoftware\\data\\pioneer"),
                8,
                Theme::Muted()
            ),
            0,
            wxALIGN_CENTER | wxBOTTOM,
            18
        );
        identity->SetSizer(identitySizer);

        root->Add(
            identity,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* authentication = MakeCard(this);
        auto* authenticationSizer = new wxBoxSizer(wxVERTICAL);

        authenticationSizer->Add(
            MakeText(
                authentication,
                U("验证方式"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString methods;
        methods.Add(U("Windows 身份验证"));
        methods.Add(U("恢复口令"));

        authenticationSizer->Add(
            MakeChoice(authentication, methods, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        authenticationSizer->Add(
            MakeProperty(
                authentication,
                U("当前 Windows 用户"),
                U("DESKTOP\\qiming"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        authenticationSizer->Add(
            MakeProperty(
                authentication,
                U("密钥保护"),
                U("Windows DPAPI"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddFieldLabel(
            authentication,
            authenticationSizer,
            U("恢复口令")
        );

        auto* password = MakeInput(
            authentication,
            wxEmptyString,
            false,
            false,
            wxTE_PASSWORD
        );
        password->SetHint(
            U("仅在 Windows 身份验证不可用时需要")
        );

        authenticationSizer->Add(
            password,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* remember = new wxCheckBox(
            authentication,
            wxID_ANY,
            U("本次应用会话保持解锁")
        );
        remember->SetValue(true);
        remember->SetForegroundColour(Theme::Text());
        remember->SetFont(Theme::Font(9));

        authenticationSizer->Add(
            remember,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* securityNote = MakeCard(
            authentication,
            Theme::Surface2()
        );
        auto* noteSizer = new wxBoxSizer(wxHORIZONTAL);
        noteSizer->Add(
            MakeText(
                securityNote,
                U("●"),
                9,
                Theme::Green(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_TOP | wxRIGHT,
            10
        );
        noteSizer->Add(
            MakeWrappedText(
                securityNote,
                U(
                    "解锁操作完全在本机执行。口令不会写入日志，"
                    "主密钥不会以明文保存到磁盘。"
                ),
                8,
                Theme::Muted(),
                420
            ),
            1
        );
        securityNote->SetSizer(noteSizer);

        authenticationSizer->Add(
            securityNote,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        authentication->SetSizer(authenticationSizer);
        root->Add(
            authentication,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddDialogActions(
            this,
            root,
            U("解锁工作区"),
            false,
            U("选择其他工作区")
        );

        Finish(root);
    }
};

}

wxDialog* CreateApplicationDialog(
    wxWindow* parent,
    const wxString& dialogCode
)
{
    if (dialogCode == U("D01"))
    {
        return new CreateObjectDialog(parent);
    }

    if (dialogCode == U("D02"))
    {
        return new LinkEvidenceDialog(parent);
    }

    if (dialogCode == U("D03"))
    {
        return new ResolveConflictDialog(parent);
    }

    if (dialogCode == U("D04"))
    {
        return new DeleteConfirmationDialog(parent);
    }

    if (dialogCode == U("D05"))
    {
        return new UnlockWorkspaceDialog(parent);
    }


    if (auto* advancedDialog = CreateAdvancedDialog(
            parent,
            dialogCode
        ))
    {
        return advancedDialog;
    }

    return nullptr;
}

}
