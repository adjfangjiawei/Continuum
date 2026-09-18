#include "App.h"

#include "MainFrame.h"
#include "StartCenter.h"
#include "Theme.h" 

#include <wx/image.h>
#include <wx/msgdlg.h>

#include "WorkspaceService.h"
#include "BackgroundWorker.h"
#include "UiDataService.h"
#include "WxUiBinder.h"
#include "WxRecordController.h"

namespace continuum
{

bool ContinuumApp::OnInit()
{
    // wxWidgets 必须先完成基础初始化。否则后续使用标准路径、
    // 图像处理器或窗口对象时可能处于未初始化状态。
    if (!wxApp::OnInit())
    {
        return false;
    }

    SetAppName(U("续证 Continuum"));
    SetVendorName(U("Continuum Project"));

    wxInitAllImageHandlers();

    /*
     * 启动时不得静默创建或打开默认工作区。
     * 用户必须在 P01 中明确创建、打开、导入或恢复工作区。
     */
    StartCenterDialog startCenter(nullptr);

    if (startCenter.ShowModal() != wxID_OK ||
        !WorkspaceService::Instance().IsInitialized())
    {
        WorkspaceService::Instance().Shutdown();
        return false;
    }

    /*
     * 只有工作区完整打开后，才允许构造依赖数据库的主窗口。
     */
    auto* frame = new MainFrame();
    SetTopWindow(frame);

    // 在窗口显示前启动后台服务，避免显示无法工作的工作区窗口。
    const auto backgroundWorkerStatus =
        BackgroundWorker::Instance().Start(
            WorkspaceService::Instance()
        );

    if (!backgroundWorkerStatus.success)
    {
        wxMessageBox(
            U("工作区已经打开，但后台处理服务无法启动。\n\n") +
                wxString::FromUTF8(
                    backgroundWorkerStatus.message.c_str()
                ),
            U("续证启动失败"),
            wxOK | wxICON_ERROR,
            frame
        );

        SetTopWindow(nullptr);
        delete frame;
        WorkspaceService::Instance().Shutdown();
        return false;
    }

    frame->Show(true);
    frame->Raise();

    // CONTINUUM_WX_UI_BINDER_STARTUP
    CallAfter([]() {
        wxWindow* root =
            wxTheApp != nullptr
                ? wxTheApp->GetTopWindow()
                : nullptr;

        if (root != nullptr)
        {
            WxUiBinder::Instance().Attach(root);
        }
    });

    // CONTINUUM_RECORD_CONTROLLER_STARTUP
    CallAfter([]() {
        wxWindow* root =
            wxTheApp != nullptr
                ? wxTheApp->GetTopWindow()
                : nullptr;

        if (root != nullptr)
        {
            WxRecordController::Instance().Attach(root);
        }
    });

    return true;
}

int ContinuumApp::OnExit()
{
    /*
     * 先停止所有 UI 数据订阅和定时刷新，避免后台线程停止及
     * 数据库关闭期间继续投递或处理界面更新事件。
     *
     * MainFrame 析构函数也会执行 Detach；这些操作应当保持
     * 幂等，因此这里作为应用级生命周期保险再次调用。
     */
    WxRecordController::Instance().Detach();
    WxUiBinder::Instance().Detach();

    /*
     * 等待后台任务线程完全退出后才能关闭它所使用的数据库。
     */
    BackgroundWorker::Instance().Stop();
    WorkspaceService::Instance().Shutdown();

    return wxApp::OnExit();
}

}
