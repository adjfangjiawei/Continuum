#pragma once

#include <wx/string.h>

class wxDialog;
class wxWindow;

namespace continuum
{

wxDialog* CreateAdvancedDialog(
    wxWindow* parent,
    const wxString& dialogCode
);

}
