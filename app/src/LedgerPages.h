#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateLedgerWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
