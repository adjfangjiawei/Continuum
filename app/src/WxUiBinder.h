#pragma once

#include "UiDataService.h"

#include <string>
#include <vector>

#include <wx/event.h>
#include <wx/timer.h>
#include <wx/window.h>

class wxButton;
class wxListCtrl;
class wxStaticText;
class wxTextCtrl;

namespace continuum
{

/*
 * 控件绑定名称约定：
 *
 * dashboard.active_objects
 * dashboard.evidence_review
 * dashboard.sources
 * dashboard.files
 * dashboard.jobs
 * dashboard.backups
 * dashboard.security
 *
 * search.query
 * search.execute
 * search.results
 * search.status
 *
 * sources.list
 * sources.scan
 *
 * jobs.list
 * jobs.cancel
 * jobs.retry
 *
 * backups.list
 * backups.create
 * backups.status
 *
 * 控件名称通过 wxWindow::SetName() 设置。
 * 未找到对应控件时绑定器会安全跳过。
 */
class WxUiBinder final : public wxEvtHandler
{
public:
    static WxUiBinder& Instance();

    WxUiBinder(const WxUiBinder&) = delete;
    WxUiBinder& operator=(const WxUiBinder&) = delete;

    UiOperationResult Attach(wxWindow* root);
    void Detach();

    bool IsAttached() const;

    void RefreshAll();
    void RefreshDashboard();
    void RefreshSearch();
    void RefreshDataSources();
    void RefreshJobs();
    void RefreshBackups();

private:
    WxUiBinder();
    ~WxUiBinder() override;

    wxWindow* FindControl(
        const std::string& bindingName
    ) const;

    template<typename T>
    T* FindAs(
        const std::string& bindingName
    ) const
    {
        return dynamic_cast<T*>(
            FindControl(bindingName)
        );
    }

    void BindCommands();

    void OnUiChange(wxThreadEvent& event);
    void OnTimer(wxTimerEvent& event);
    void OnRootDestroyed(wxWindowDestroyEvent& event);
    void OnSearch(wxCommandEvent& event);
    void OnScanSource(wxCommandEvent& event);
    void OnCancelJob(wxCommandEvent& event);
    void OnRetryJob(wxCommandEvent& event);
    void OnCreateBackup(wxCommandEvent& event);

    void SetText(
        const std::string& bindingName,
        const std::string& value
    );

    void SetStatus(
        const std::string& bindingName,
        const UiOperationResult& result
    );

    std::string SelectedListId(
        const std::string& bindingName
    ) const;

    wxWindow* root_;
    wxTimer refreshTimer_;
    bool commandsBound_;
    std::string lastSearchQuery_;
};

}
