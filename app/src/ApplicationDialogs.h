#pragma once

#include <wx/string.h>

class wxDialog;
class wxWindow;

namespace continuum
{

wxDialog* CreateApplicationDialog(
    wxWindow* parent,
    const wxString& dialogCode
);

}
