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


wxWindow* CreatePage(wxWindow* parent, const PageDescriptor& descriptor);

}
