#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateEvidenceWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
