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
    ~MainFrame() override;

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
    bool NavigateToCode(const wxString& code);
    void UpdateNavigationStyles();
    void ShowCommandPalette();
    void RunGlobalSearch();
    void BindKeyboardShortcuts();

    wxSimplebook* pageBook_ = nullptr;
    wxStaticText* workspaceTitle_ = nullptr;
    wxTextCtrl* globalSearch_ = nullptr;

    std::vector<NavigationEntry> navigation_;
    // 与 navigation_ 一一对应；页面仅在用户第一次访问时创建。
    std::vector<wxWindow*> pages_;
    std::size_t selectedIndex_ = 0;
};

}
