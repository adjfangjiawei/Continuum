#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateInvestigationPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
