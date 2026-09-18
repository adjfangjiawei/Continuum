#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateGovernancePage(
    wxWindow* parent,
    const wxString& pageCode
);

}
