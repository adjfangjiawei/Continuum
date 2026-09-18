#include "App.h"

#include "MainFrame.h"
#include "Theme.h"

#include <wx/image.h>
#include "WorkspaceService.h"
#include "BackgroundWorker.h"

namespace continuum
{

bool ContinuumApp::OnInit()
{

    // CONTINUUM_WORKSPACE_STARTUP
    const auto workspaceStatus =
        continuum::WorkspaceService::Instance().InitializeDefault();

    if (!workspaceStatus.success)
    {
        return false;
    }

    if (!wxApp::OnInit())
    {
        return false;
    }

    wxInitAllImageHandlers();

    SetAppName(U("续证 Continuum"));
    SetVendorName(U("Continuum Project"));

    auto* frame = new MainFrame();
    frame->Show(true);
    frame->Raise();

    
    // CONTINUUM_BACKGROUND_WORKER_STARTUP
    const auto backgroundWorkerStatus =
        continuum::BackgroundWorker::Instance().Start(
            continuum::WorkspaceService::Instance()
        );

    if (!backgroundWorkerStatus.success)
    {
        continuum::WorkspaceService::Instance().Shutdown();
        return false;
    }

return true;
}

int ContinuumApp::OnExit()
{
    return wxApp::OnExit();
}

}
