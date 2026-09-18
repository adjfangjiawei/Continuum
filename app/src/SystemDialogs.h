#pragma once

#include <wx/string.h>

class wxDialog;
class wxWindow;

namespace continuum
{

wxDialog* CreateSystemDialog(
    wxWindow* parent,
    const wxString& dialogCode
);

}
