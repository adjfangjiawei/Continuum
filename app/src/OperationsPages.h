#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateOperationsPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
