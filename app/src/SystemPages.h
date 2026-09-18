#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateSystemPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
