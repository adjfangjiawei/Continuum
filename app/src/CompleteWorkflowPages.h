#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateCompleteWorkflowPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
