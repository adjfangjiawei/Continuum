#pragma once

#include "Pages.h"

#include <vector>

#include <wx/frame.h>

class wxBoxSizer;
class wxButton;
class wxPanel;
class wxSimplebook;
class wxStaticText;
class wxTextCtrl;

namespace continuum
{

class MainFrame final : public wxFrame
{
public:
    MainFrame();

private:
    struct NavigationEntry final
    {
        PageDescriptor page;
        wxButton* button = nullptr;
        int controlId = wxID_NONE;
    };

    void BuildInterface();
    wxPanel* BuildHeader(wxWindow* parent);
    wxPanel* BuildSidebar(wxWindow* parent);
    wxPanel* BuildStatusBar(wxWindow* parent);

    void AddNavigationSection(
        wxWindow* parent,
        wxBoxSizer* sizer,
        const wxString& title,
        const std::vector<PageDescriptor>& pages
    );

    void AddNavigationEntry(
        wxWindow* parent,
        wxBoxSizer* sizer,
        const PageDescriptor& page
    );

    void NavigateTo(std::size_t index);
    void UpdateNavigationStyles();
    void ShowCommandPalette();
    void BindKeyboardShortcuts();

    wxSimplebook* pageBook_ = nullptr;
    wxStaticText* workspaceTitle_ = nullptr;
    wxTextCtrl* globalSearch_ = nullptr;

    std::vector<NavigationEntry> navigation_;
    std::size_t selectedIndex_ = 0;
};

}
