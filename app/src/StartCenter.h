
#pragma once

#include <wx/dialog.h>
#include <wx/listbox.h>
#include <wx/stattext.h>

namespace continuum
{

class StartCenterDialog final : public wxDialog
{
public:
    explicit StartCenterDialog(wxWindow* parent);

    const wxString& OpenedWorkspace() const;

private:
    void BuildInterface();
    void RefreshRecent();
    void CreateWorkspace();
    void OpenWorkspace();
    void OpenRecent();
    void RestoreBackupCopy();
    void ImportWorkspacePackage();

    bool CompleteOpen(const wxString& directory);
    void AddRecent(const wxString& directory);

    wxListBox* recentList_ = nullptr;
    wxStaticText* status_ = nullptr;
    wxString openedWorkspace_;
};

}
