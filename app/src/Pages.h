#pragma once

#include <wx/panel.h>
#include <wx/string.h>

namespace continuum
{

struct PageDescriptor final
{
    wxString code;
    wxString title;
    wxString subtitle;
    wxString category;
};

class OverviewPage final : public wxPanel
{
public:
    explicit OverviewPage(wxWindow* parent);

private:
    wxPanel* CreateStatCard(
        wxWindow* parent,
        const wxString& value,
        const wxString& label,
        const wxString& detail,
        const wxColour& accent
    );

    wxPanel* CreateActivityRow(
        wxWindow* parent,
        const wxString& time,
        const wxString& type,
        const wxString& message,
        const wxColour& accent
    );
};

class ModulePage final : public wxPanel
{
public:
    ModulePage(wxWindow* parent, const PageDescriptor& descriptor);

private:
    wxPanel* CreateMetric(
        wxWindow* parent,
        const wxString& value,
        const wxString& title,
        const wxString& description,
        const wxColour& accent
    );

    wxPanel* CreateListRow(
        wxWindow* parent,
        const wxString& identifier,
        const wxString& title,
        const wxString& detail,
        const wxString& state,
        const wxColour& accent
    );
};

wxWindow* CreatePage(wxWindow* parent, const PageDescriptor& descriptor);

}
