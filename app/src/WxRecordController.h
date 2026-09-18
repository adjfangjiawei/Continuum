#pragma once

#include "UiDataService.h"

#include <string>
#include <vector>

#include <wx/event.h>
#include <wx/listctrl.h>
#include <wx/window.h>

class wxButton;
class wxChoice;
class wxComboBox;
class wxListCtrl;
class wxStaticText;
class wxTextCtrl;

namespace continuum
{

/*
 * 业务对象页面控件名称：
 *
 * objects.list
 * objects.id
 * objects.type
 * objects.title
 * objects.description
 * objects.status
 * objects.priority
 * objects.owner
 * objects.time_precision
 * objects.new
 * objects.save
 * objects.delete
 * objects.restore
 * objects.status_message
 *
 * 证据审查页面控件名称：
 *
 * evidence.review_list
 * evidence.verify
 * evidence.reject
 * evidence.needs_review
 * evidence.status_message
 */
class WxRecordController final : public wxEvtHandler
{
public:
    static WxRecordController& Instance();

    WxRecordController(
        const WxRecordController&
    ) = delete;

    WxRecordController& operator=(
        const WxRecordController&
    ) = delete;

    UiOperationResult Attach(wxWindow* root);
    void Detach();

    bool IsAttached() const;

    void RefreshAll();
    void RefreshObjects();
    void RefreshEvidence();

private:
    WxRecordController();
    ~WxRecordController() override;

    wxWindow* FindControl(
        const std::string& name
    ) const;

    template<typename T>
    T* FindAs(const std::string& name) const
    {
        return dynamic_cast<T*>(
            FindControl(name)
        );
    }

    void BindCommands();

    std::string ReadValue(
        const std::string& name
    ) const;

    void WriteValue(
        const std::string& name,
        const std::string& value
    );

    void SetStatus(
        const std::string& name,
        const UiOperationResult& result
    );

    std::string SelectedId(
        const std::string& listName
    ) const;

    DomainObjectRecord* FindCachedObject(
        const std::string& id
    );

    static std::string GenerateObjectId();

    void ClearObjectForm();
    void LoadObjectForm(
        const DomainObjectRecord& record
    );

    void OnUiChange(wxThreadEvent& event);
    void OnObjectSelected(
        wxListEvent& event
    );
    void OnNewObject(wxCommandEvent& event);
    void OnSaveObject(wxCommandEvent& event);
    void OnDeleteObject(wxCommandEvent& event);
    void OnRestoreObject(wxCommandEvent& event);
    void OnVerifyEvidence(wxCommandEvent& event);
    void OnRejectEvidence(wxCommandEvent& event);
    void OnMarkEvidenceForReview(
        wxCommandEvent& event
    );

    wxWindow* root_;
    bool commandsBound_;
    std::vector<DomainObjectRecord> objectCache_;
};

}
