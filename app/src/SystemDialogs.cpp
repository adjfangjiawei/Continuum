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
        wxSize(-1, multiline ? 90 : 38),
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

wxPanel* MakeCheckRow(
    wxWindow* parent,
    const wxString& title,
    const wxString& detail,
    const wxString& state,
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
        MakeText(row, state, 8, color, wxFONTWEIGHT_BOLD),
        0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT,
        12
    );

    row->SetSizer(layout);
    return row;
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
    auto* layout = new wxBoxSizer(wxVERTICAL);

    layout->Add(
        MakeText(header, code, 8, accent, wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        5
    );
    layout->Add(
        MakeText(header, title, 17, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxBOTTOM,
        6
    );
    layout->Add(
        MakeWrappedText(header, subtitle, 9, Theme::Muted(), 720),
        0
    );

    header->SetSizer(layout);
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
                145
            ),
            0,
            wxRIGHT,
            10
        );
    }

    actions->AddStretchSpacer();
    actions->Add(
        MakeButton(dialog, wxID_CANCEL, U("取消"), false, false, 100),
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
            155
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

class RestoreSnapshotDialog final : public StyledDialog
{
public:
    explicit RestoreSnapshotDialog(wxWindow* parent)
        : StyledDialog(parent, U("恢复工作区快照"), wxSize(850, 720))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D11"),
            U("恢复工作区快照"),
            U("验证备份内容，并始终将其恢复为新的独立工作区。"),
            Theme::Purple()
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* snapshots = MakeCard(this);
        auto* snapshotSizer = new wxBoxSizer(wxVERTICAL);
        snapshotSizer->Add(
            MakeText(
                snapshots,
                U("选择快照"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString rows;
        rows.Add(U("BAK-020  自动备份 · 今天 12:40"));
        rows.Add(U("BAK-019  修改前安全快照 · 今天 10:05"));
        rows.Add(U("BAK-018  自动备份 · 昨天 20:00"));
        rows.Add(U("BAK-011  协议迁移决策前 · 9 月 9 日"));
        rows.Add(U("BAK-010  自动备份 · 9 月 8 日"));

        auto* list = new wxListBox(
            snapshots,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            rows,
            wxLB_SINGLE | wxBORDER_NONE
        );
        list->SetBackgroundColour(Theme::Surface2());
        list->SetForegroundColour(Theme::Text());
        list->SetFont(Theme::Font(9));
        list->SetSelection(0);

        snapshotSizer->Add(
            list,
            1,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        snapshotSizer->Add(
            MakeButton(
                snapshots,
                wxID_ANY,
                U("选择外部备份文件")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );
        snapshots->SetSizer(snapshotSizer);

        auto* detail = MakeCard(this);
        detail->SetMinSize(wxSize(390, -1));
        auto* detailSizer = new wxBoxSizer(wxVERTICAL);

        auto* detailHeader = new wxBoxSizer(wxHORIZONTAL);
        detailHeader->Add(
            MakeText(
                detail,
                U("备份验证"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            1,
            wxALIGN_CENTER_VERTICAL
        );
        detailHeader->Add(
            MakePill(detail, U("验证通过"), Theme::Green(), 100),
            0
        );
        detailSizer->Add(detailHeader, 0, wxEXPAND | wxALL, 15);

        const std::array<std::tuple<wxString, wxString, wxColour>, 7> info = {{
            {U("备份类型"), U("增量备份"), Theme::Blue()},
            {U("创建时间"), U("2026-09-18 12:40"), Theme::Text()},
            {U("工作区格式"), U("1.0 · 兼容"), Theme::Green()},
            {U("数据库"), U("118 MB · 完整"), Theme::Green()},
            {U("搜索索引"), U("54 MB · 完整"), Theme::Green()},
            {U("内容快照"), U("14 MB · 完整"), Theme::Green()},
            {U("清单验证"), U("238 / 238 通过"), Theme::Green()}
        }};

        for (const auto& item : info)
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
                8
            );
        }

        AddLabel(detail, detailSizer, U("新工作区名称"), true);
        detailSizer->Add(
            MakeInput(detail, U("先锋计划（恢复于 2026-09-18）")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        AddLabel(detail, detailSizer, U("恢复位置"), true);
        detailSizer->Add(
            MakeInput(
                detail,
                U("D:\\UnknownSoftware\\data\\pioneer-restored")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* rebuild = new wxCheckBox(
            detail,
            wxID_ANY,
            U("恢复后重新验证全文索引")
        );
        rebuild->SetValue(true);
        rebuild->SetForegroundColour(Theme::Text());
        rebuild->SetFont(Theme::Font(9));

        detailSizer->Add(
            rebuild,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        detailSizer->AddStretchSpacer();
        detailSizer->Add(
            MakeWrappedText(
                detail,
                U("当前工作区不会被覆盖或修改。"),
                8,
                Theme::Blue(),
                335,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        detail->SetSizer(detailSizer);

        body->Add(snapshots, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(detail, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

        AddActions(
            this,
            root,
            U("恢复为新工作区"),
            U("再次验证")
        );

        Finish(root);
    }
};

class CreateBackupDialog final : public StyledDialog
{
public:
    explicit CreateBackupDialog(wxWindow* parent)
        : StyledDialog(parent, U("创建备份"), wxSize(760, 700))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D12"),
            U("创建工作区备份"),
            U("创建一致性快照，并选择备份范围、目标位置和加密策略。"),
            Theme::Green()
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* settings = MakeCard(this);
        auto* settingSizer = new wxBoxSizer(wxVERTICAL);
        settingSizer->Add(
            MakeText(
                settings,
                U("备份设置"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        AddLabel(settings, settingSizer, U("备份名称"), true);
        settingSizer->Add(
            MakeInput(settings, U("手动备份 · 2026-09-18")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        wxArrayString types;
        types.Add(U("增量备份"));
        types.Add(U("完整备份"));
        types.Add(U("仅数据库快照"));

        AddLabel(settings, settingSizer, U("备份类型"), true);
        settingSizer->Add(
            MakeChoice(settings, types, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(settings, settingSizer, U("目标位置"), true);
        settingSizer->Add(
            MakeInput(
                settings,
                U("D:\\UnknownSoftware\\backups\\Pioneer")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        settingSizer->Add(
            MakeButton(settings, wxID_ANY, U("浏览备份目录")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(settings, settingSizer, U("备注"));
        settingSizer->Add(
            MakeInput(
                settings,
                U("规则调整和批量审查前的安全备份。"),
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        settings->SetSizer(settingSizer);

        auto* scope = MakeCard(this);
        scope->SetMinSize(wxSize(340, -1));
        auto* scopeSizer = new wxBoxSizer(wxVERTICAL);
        scopeSizer->Add(
            MakeText(
                scope,
                U("内容与保护"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        const std::array<std::tuple<wxString, wxString, bool>, 5> options = {{
            {U("工作区数据库"), U("118 MB · 必需"), true},
            {U("全文搜索索引"), U("286 MB"), true},
            {U("证据内容快照"), U("14 MB"), true},
            {U("应用设置"), U("约 48 KB"), true},
            {U("诊断日志"), U("最近 7 天 · 12 MB"), false}
        }};

        for (const auto& option : options)
        {
            auto* row = MakeCard(scope, Theme::Surface2());
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            auto* check = new wxCheckBox(row, wxID_ANY, wxEmptyString);
            check->SetValue(std::get<2>(option));
            check->SetBackgroundColour(Theme::Surface2());

            auto* labels = new wxBoxSizer(wxVERTICAL);
            labels->Add(
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
            labels->Add(
                MakeText(row, std::get<1>(option), 8, Theme::Muted()),
                0
            );

            rowSizer->Add(
                check,
                0,
                wxALIGN_CENTER_VERTICAL | wxALL,
                11
            );
            rowSizer->Add(
                labels,
                1,
                wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT,
                10
            );
            row->SetSizer(rowSizer);

            scopeSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        scopeSizer->Add(
            MakeProperty(
                scope,
                U("备份加密"),
                U("工作区密钥保护"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        scopeSizer->Add(
            MakeProperty(
                scope,
                U("预计大小"),
                U("约 418 MB"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        scopeSizer->Add(
            MakeProperty(
                scope,
                U("可用空间"),
                U("286 GB"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* verify = new wxCheckBox(
            scope,
            wxID_ANY,
            U("创建后立即验证完整性")
        );
        verify->SetValue(true);
        verify->SetForegroundColour(Theme::Text());
        verify->SetFont(Theme::Font(9));
        scopeSizer->Add(
            verify,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        scopeSizer->AddStretchSpacer();
        scopeSizer->Add(
            MakeWrappedText(
                scope,
                U("创建备份时会短暂建立数据库一致性读快照，不会中断阅读操作。"),
                8,
                Theme::Blue(),
                290,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        scope->SetSizer(scopeSizer);

        body->Add(settings, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(scope, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

        AddActions(
            this,
            root,
            U("创建备份"),
            U("计算精确大小")
        );

        Finish(root);
    }
};

class MigrateWorkspaceDialog final : public StyledDialog
{
public:
    explicit MigrateWorkspaceDialog(wxWindow* parent)
        : StyledDialog(parent, U("迁移工作区"), wxSize(850, 740))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D13"),
            U("迁移工作区"),
            U("将工作区复制到新目录或设备，并验证数据库、索引和密钥可用性。"),
            Theme::Blue()
        );

        auto* steps = MakeCard(this);
        auto* stepSizer = new wxBoxSizer(wxHORIZONTAL);

        const std::array<std::tuple<wxString, wxString, wxColour>, 4> stepData = {{
            {U("1"), U("选择目标"), Theme::Blue()},
            {U("2"), U("迁移内容"), Theme::Muted()},
            {U("3"), U("密钥保护"), Theme::Muted()},
            {U("4"), U("验证切换"), Theme::Muted()}
        }};

        for (std::size_t index = 0; index < stepData.size(); ++index)
        {
            auto* step = MakeCard(
                steps,
                index == 0 ? Theme::Surface3() : Theme::Surface2()
            );
            auto* layout = new wxBoxSizer(wxVERTICAL);
            layout->Add(
                MakeText(
                    step,
                    std::get<0>(stepData[index]),
                    12,
                    std::get<2>(stepData[index]),
                    wxFONTWEIGHT_BOLD
                ),
                0,
                wxALIGN_CENTER | wxTOP | wxBOTTOM,
                9
            );
            layout->Add(
                MakeText(
                    step,
                    std::get<1>(stepData[index]),
                    8,
                    index == 0 ? Theme::Text() : Theme::Muted(),
                    wxFONTWEIGHT_SEMIBOLD
                ),
                0,
                wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM,
                10
            );
            step->SetSizer(layout);

            stepSizer->Add(
                step,
                1,
                index + 1 < stepData.size() ? wxRIGHT : 0,
                index + 1 < stepData.size() ? 9 : 0
            );
        }

        steps->SetSizer(stepSizer);
        root->Add(
            steps,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* target = MakeCard(this);
        auto* targetSizer = new wxBoxSizer(wxVERTICAL);
        targetSizer->Add(
            MakeText(
                target,
                U("迁移目标"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        wxArrayString modes;
        modes.Add(U("迁移到本机新目录"));
        modes.Add(U("创建可移动迁移包"));
        modes.Add(U("迁移到其他 Windows 用户"));

        AddLabel(target, targetSizer, U("迁移方式"), true);
        targetSizer->Add(
            MakeChoice(target, modes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(target, targetSizer, U("当前位置"));
        targetSizer->Add(
            MakeInput(
                target,
                U("D:\\UnknownSoftware\\data\\pioneer"),
                false,
                true
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        AddLabel(target, targetSizer, U("目标位置"), true);
        targetSizer->Add(
            MakeInput(
                target,
                U("E:\\ContinuumWorkspaces\\pioneer")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        targetSizer->Add(
            MakeButton(target, wxID_ANY, U("浏览目标目录")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        targetSizer->Add(
            MakeProperty(target, U("目标文件系统"), U("NTFS"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        targetSizer->Add(
            MakeProperty(target, U("可用空间"), U("412 GB"), Theme::Green()),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );
        target->SetSizer(targetSizer);

        auto* plan = MakeCard(this);
        plan->SetMinSize(wxSize(370, -1));
        auto* planSizer = new wxBoxSizer(wxVERTICAL);
        planSizer->Add(
            MakeText(
                plan,
                U("迁移计划"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        planSizer->Add(
            MakeCheckRow(
                plan,
                U("工作区数据库"),
                U("118 MB · SQLCipher"),
                U("包含"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        planSizer->Add(
            MakeCheckRow(
                plan,
                U("全文搜索索引"),
                U("286 MB · 可重新生成"),
                U("包含"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        planSizer->Add(
            MakeCheckRow(
                plan,
                U("内容快照"),
                U("14 MB · 证据恢复所需"),
                U("包含"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        planSizer->Add(
            MakeCheckRow(
                plan,
                U("数据源路径"),
                U("4 个路径将在迁移后重新验证"),
                U("需检查"),
                Theme::Yellow()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        wxArrayString keyModes;
        keyModes.Add(U("使用当前 Windows 用户重新保护密钥"));
        keyModes.Add(U("使用恢复口令保护迁移包"));

        AddLabel(plan, planSizer, U("目标密钥保护"), true);
        planSizer->Add(
            MakeChoice(plan, keyModes, 0),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* keepSource = new wxCheckBox(
            plan,
            wxID_ANY,
            U("迁移成功后保留原工作区")
        );
        keepSource->SetValue(true);
        keepSource->SetForegroundColour(Theme::Text());
        keepSource->SetFont(Theme::Font(9));

        planSizer->Add(
            keepSource,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            12
        );

        auto* verifyTarget = new wxCheckBox(
            plan,
            wxID_ANY,
            U("完成后执行完整验证")
        );
        verifyTarget->SetValue(true);
        verifyTarget->SetForegroundColour(Theme::Text());
        verifyTarget->SetFont(Theme::Font(9));

        planSizer->Add(
            verifyTarget,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        planSizer->AddStretchSpacer();
        planSizer->Add(
            MakeWrappedText(
                plan,
                U("迁移采用复制后验证策略；只有验证通过后才允许切换默认工作区。"),
                8,
                Theme::Blue(),
                320,
                wxFONTWEIGHT_SEMIBOLD
            ),
            0,
            wxALL,
            15
        );
        plan->SetSizer(planSizer);

        body->Add(target, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(plan, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxALL, 18);

        AddActions(
            this,
            root,
            U("开始迁移"),
            U("验证目标位置")
        );

        Finish(root);
    }
};

class ExportDiagnosticsDialog final : public StyledDialog
{
public:
    explicit ExportDiagnosticsDialog(wxWindow* parent)
        : StyledDialog(parent, U("导出诊断包"), wxSize(780, 710))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D14"),
            U("导出诊断包"),
            U("创建用于离线排障的脱敏诊断包，不包含业务正文或解密密钥。"),
            Theme::Cyan()
        );

        auto* body = new wxBoxSizer(wxHORIZONTAL);

        auto* content = MakeCard(this);
        auto* contentSizer = new wxBoxSizer(wxVERTICAL);
        contentSizer->Add(
            MakeText(
                content,
                U("诊断内容"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        const std::array<std::tuple<wxString, wxString, bool>, 6> options = {{
            {U("应用与系统信息"), U("版本、平台和组件"), true},
            {U("诊断检查结果"), U("最近完整诊断"), true},
            {U("应用日志"), U("最近 7 天"), true},
            {U("任务失败摘要"), U("不包含文件正文"), true},
            {U("配置摘要"), U("自动移除路径和用户名"), true},
            {U("数据库结构统计"), U("仅表数量和完整性结果"), false}
        }};

        for (const auto& option : options)
        {
            auto* row = MakeCard(content, Theme::Surface2());
            auto* rowSizer = new wxBoxSizer(wxHORIZONTAL);

            auto* check = new wxCheckBox(row, wxID_ANY, wxEmptyString);
            check->SetValue(std::get<2>(option));
            check->SetBackgroundColour(Theme::Surface2());

            auto* labels = new wxBoxSizer(wxVERTICAL);
            labels->Add(
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
            labels->Add(
                MakeText(row, std::get<1>(option), 8, Theme::Muted()),
                0
            );

            rowSizer->Add(
                check,
                0,
                wxALIGN_CENTER_VERTICAL | wxALL,
                11
            );
            rowSizer->Add(
                labels,
                1,
                wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxRIGHT,
                10
            );
            row->SetSizer(rowSizer);

            contentSizer->Add(
                row,
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }
        content->SetSizer(contentSizer);

        auto* security = MakeCard(this);
        security->SetMinSize(wxSize(345, -1));
        auto* securitySizer = new wxBoxSizer(wxVERTICAL);
        securitySizer->Add(
            MakeText(
                security,
                U("脱敏与输出"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        securitySizer->Add(
            MakeProperty(
                security,
                U("业务正文"),
                U("不包含"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        securitySizer->Add(
            MakeProperty(
                security,
                U("数据库密钥"),
                U("不包含"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        securitySizer->Add(
            MakeProperty(
                security,
                U("绝对路径"),
                U("自动脱敏"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        securitySizer->Add(
            MakeProperty(
                security,
                U("用户名与邮件"),
                U("自动脱敏"),
                Theme::Blue()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        AddLabel(security, securitySizer, U("输出文件"), true);
        securitySizer->Add(
            MakeInput(
                security,
                U("D:\\Exports\\continuum-diagnostics-20260918.zip")
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            10
        );
        securitySizer->Add(
            MakeButton(security, wxID_ANY, U("选择输出位置")),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        securitySizer->Add(
            MakeProperty(
                security,
                U("预计大小"),
                U("8.4 MB"),
                Theme::Cyan()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            8
        );
        securitySizer->Add(
            MakeProperty(
                security,
                U("完整性清单"),
                U("SHA-256"),
                Theme::Green()
            ),
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* preview = MakeCard(security, Theme::Surface2());
        auto* previewSizer = new wxBoxSizer(wxVERTICAL);
        previewSizer->Add(
            MakeText(
                preview,
                U("隐私预检查通过"),
                9,
                Theme::Green(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            6
        );
        previewSizer->Add(
            MakeWrappedText(
                preview,
                U("检测到并计划替换 18 个路径、1 个用户名和 3 个邮件地址。"),
                8,
                Theme::Muted(),
                285
            ),
            0
        );
        preview->SetSizer(previewSizer);
        securitySizer->Add(
            preview,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        securitySizer->AddStretchSpacer();
        securitySizer->Add(
            MakeButton(security, wxID_ANY, U("预览文件清单")),
            0,
            wxEXPAND | wxALL,
            15
        );
        security->SetSizer(securitySizer);

        body->Add(content, 1, wxEXPAND | wxRIGHT, 12);
        body->Add(security, 0, wxEXPAND);

        root->Add(body, 1, wxEXPAND | wxLEFT | wxRIGHT, 18);

        AddActions(
            this,
            root,
            U("导出诊断包"),
            U("重新运行隐私检查")
        );

        Finish(root);
    }
};

class DangerousOperationDialog final : public StyledDialog
{
public:
    explicit DangerousOperationDialog(wxWindow* parent)
        : StyledDialog(parent, U("确认危险操作"), wxSize(660, 650))
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        AddHeader(
            this,
            root,
            U("D15"),
            U("确认危险操作"),
            U("此操作会永久删除回收站内容，完成后无法通过应用恢复。"),
            Theme::Red()
        );

        auto* warning = MakeCard(this, Theme::Surface2());
        auto* warningSizer = new wxBoxSizer(wxHORIZONTAL);

        warningSizer->Add(
            MakeText(
                warning,
                U("!"),
                24,
                Theme::Red(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALIGN_TOP | wxRIGHT,
            15
        );

        auto* warningLabels = new wxBoxSizer(wxVERTICAL);
        warningLabels->Add(
            MakeText(
                warning,
                U("即将清空整个回收站"),
                14,
                Theme::Red(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxBOTTOM,
            7
        );
        warningLabels->Add(
            MakeWrappedText(
                warning,
                U(
                    "14 个已删除项目将被永久清理。审计记录仍会保留，"
                    "但业务对象和未被其他对象引用的内容快照将无法恢复。"
                ),
                9,
                Theme::Muted(),
                500
            ),
            0
        );

        warningSizer->Add(warningLabels, 1);
        warning->SetSizer(warningSizer);

        root->Add(
            warning,
            0,
            wxEXPAND | wxLEFT | wxRIGHT,
            18
        );

        auto* impact = MakeCard(this);
        auto* impactSizer = new wxBoxSizer(wxVERTICAL);
        impactSizer->Add(
            MakeText(
                impact,
                U("影响摘要"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );

        const std::array<std::tuple<wxString, wxString, wxColour>, 6> values = {{
            {U("业务对象"), U("8 项"), Theme::Red()},
            {U("证据记录"), U("3 项"), Theme::Yellow()},
            {U("文件记录"), U("1 项"), Theme::Blue()},
            {U("标签与实体"), U("2 项"), Theme::Purple()},
            {U("可释放空间"), U("28.6 MB"), Theme::Green()},
            {U("审计记录"), U("永久保留"), Theme::Green()}
        }};

        for (const auto& value : values)
        {
            impactSizer->Add(
                MakeProperty(
                    impact,
                    std::get<0>(value),
                    std::get<1>(value),
                    std::get<2>(value)
                ),
                0,
                wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                8
            );
        }

        auto* backup = new wxCheckBox(
            impact,
            wxID_ANY,
            U("执行前创建安全快照")
        );
        backup->SetValue(true);
        backup->SetForegroundColour(Theme::Text());
        backup->SetFont(Theme::Font(9));

        impactSizer->Add(
            backup,
            0,
            wxLEFT | wxRIGHT | wxTOP | wxBOTTOM,
            15
        );

        impact->SetSizer(impactSizer);
        root->Add(
            impact,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxTOP,
            18
        );

        auto* confirmation = MakeCard(this);
        auto* confirmationSizer = new wxBoxSizer(wxVERTICAL);

        confirmationSizer->Add(
            MakeText(
                confirmation,
                U("输入确认短语"),
                11,
                Theme::Text(),
                wxFONTWEIGHT_BOLD
            ),
            0,
            wxALL,
            15
        );
        confirmationSizer->Add(
            MakeWrappedText(
                confirmation,
                U("请输入“永久清空回收站”以启用确认按钮。"),
                9,
                Theme::Muted(),
                560
            ),
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        auto* phrase = MakeInput(confirmation);
        phrase->SetHint(U("永久清空回收站"));

        confirmationSizer->Add(
            phrase,
            0,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            14
        );

        auto* acknowledge = new wxCheckBox(
            confirmation,
            wxID_ANY,
            U("我理解该操作不可撤销")
        );
        acknowledge->SetValue(false);
        acknowledge->SetForegroundColour(Theme::Text());
        acknowledge->SetFont(Theme::Font(9));

        confirmationSizer->Add(
            acknowledge,
            0,
            wxLEFT | wxRIGHT | wxBOTTOM,
            15
        );

        confirmation->SetSizer(confirmationSizer);
        root->Add(
            confirmation,
            1,
            wxEXPAND | wxALL,
            18
        );

        AddActions(
            this,
            root,
            U("永久清空"),
            U("导出删除清单"),
            true
        );

        Finish(root);
    }
};

}

wxDialog* CreateSystemDialog(
    wxWindow* parent,
    const wxString& dialogCode
)
{
    if (dialogCode == U("D11"))
    {
        return new RestoreSnapshotDialog(parent);
    }

    if (dialogCode == U("D12"))
    {
        return new CreateBackupDialog(parent);
    }

    if (dialogCode == U("D13"))
    {
        return new MigrateWorkspaceDialog(parent);
    }

    if (dialogCode == U("D14"))
    {
        return new ExportDiagnosticsDialog(parent);
    }

    if (dialogCode == U("D15"))
    {
        return new DangerousOperationDialog(parent);
    }

    return nullptr;
}

}
