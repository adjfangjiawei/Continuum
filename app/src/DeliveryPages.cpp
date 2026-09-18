#include "DeliveryPages.h"

#include "Theme.h"
#include "UiDataService.h"
#include "WorkspaceService.h"

#include <string>
#include <vector>

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace continuum
{
namespace
{

wxString Wx(const std::string& value)
{
    return wxString::FromUTF8(value.c_str());
}

std::string Utf8(const wxString& value)
{
    const wxScopedCharBuffer buffer = value.ToUTF8();

    return buffer.data() == nullptr
        ? std::string()
        : std::string(buffer.data());
}

wxStaticText* MakeText(
    wxWindow* parent,
    const wxString& value,
    int size,
    const wxColour& color,
    wxFontWeight weight = wxFONTWEIGHT_NORMAL
)
{
    auto* text = new wxStaticText(parent, wxID_ANY, value);
    text->SetForegroundColour(color);
    text->SetFont(Theme::Font(size, weight));
    return text;
}

void AddHeading(
    wxWindow* parent,
    wxBoxSizer* root,
    const wxString& code,
    const wxString& title,
    const wxString& subtitle
)
{
    root->Add(
        MakeText(parent, code, 9, Theme::Blue(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
    root->Add(
        MakeText(parent, title, 22, Theme::Text(), wxFONTWEIGHT_BOLD),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
    root->Add(
        MakeText(parent, subtitle, 10, Theme::Muted()),
        0,
        wxLEFT | wxRIGHT | wxTOP,
        26
    );
}



}

wxWindow* CreateDeliveryPage(
    wxWindow* parent,
    const wxString& pageCode
)
{

    return nullptr;
}

}
