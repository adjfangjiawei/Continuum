#pragma once

#include <wx/string.h>

class wxWindow;

namespace continuum
{

wxWindow* CreateDeliveryPage(
    wxWindow* parent,
    const wxString& pageCode
);

}
