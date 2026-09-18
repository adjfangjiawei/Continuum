#include "WxUiBinder.h"

#include "SecurityService.h"
#include "UiEventBus.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

#include <sqlite3.h>

#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace continuum
{
namespace
{

std::string Lower(std::string value)
{
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char character) {
            return static_cast<char>(
                std::tolower(character)
            );
        }
    );

    return value;
}

std::string Utf8(const wxString& value)
{
    const wxScopedCharBuffer buffer =
        value.ToUTF8();

    return buffer.data() == nullptr
        ? std::string()
        : std::string(buffer.data());
}

wxString Wx(const std::string& value)
{
    return wxString::FromUTF8(value.c_str());
}

wxWindow* FindRecursively(
    wxWindow* parent,
    const std::string& normalizedName
)
{
    if (parent == nullptr)
    {
        return nullptr;
    }

    if (Lower(Utf8(parent->GetName())) ==
        normalizedName)
    {
        return parent;
    }

    for (wxWindow* child : parent->GetChildren())
    {
        if (auto* found = FindRecursively(
                child,
                normalizedName
            ))
        {
            return found;
        }
    }

    return nullptr;
}

void PrepareList(
    wxListCtrl* list,
    const std::vector<wxString>& headings,
    const std::vector<int>& widths
)
{
    if (list == nullptr)
    {
        return;
    }

    list->Freeze();
    list->ClearAll();

    for (std::size_t index = 0;
         index < headings.size();
         ++index)
    {
        list->InsertColumn(
            static_cast<long>(index),
            headings[index]
        );

        if (index < widths.size())
        {
            list->SetColumnWidth(
                static_cast<int>(index),
                widths[index]
            );
        }
    }

    list->Thaw();
}

}

WxUiBinder& WxUiBinder::Instance()
{
    static WxUiBinder instance;
    return instance;
}

WxUiBinder::WxUiBinder()
    : root_(nullptr),
      refreshTimer_(this),
      commandsBound_(false)
{
    Bind(
        wxEVT_TIMER,
        &WxUiBinder::OnTimer,
        this
    );
}

WxUiBinder::~WxUiBinder()
{
    Detach();
}

UiOperationResult WxUiBinder::Attach(
    wxWindow* root
)
{
    if (root == nullptr)
    {
        return {
            false,
            SQLITE_MISUSE,
            "无法绑定空的 wxWidgets 根窗口"
        };
    }

    Detach();
    root_ = root;

    const auto initialize =
        UiDataService::Instance().Initialize();

    if (!initialize.success)
    {
        root_ = nullptr;
        return initialize;
    }

    root_->Bind(
        wxEVT_CONTINUUM_UI_CHANGE,
        &WxUiBinder::OnUiChange,
        this
    );

    root_->Bind(
        wxEVT_DESTROY,
        &WxUiBinder::OnRootDestroyed,
        this
    );

    UiEventBus::Instance().Start(root_);

    BindCommands();
    RefreshAll();

    /*
     * 数据变化由 UiEventBus 立即驱动刷新。定时器只负责兜底同步，
     * 不应每两秒执行十余条统计 SQL 并持续与后台解析线程争锁。
     */
    refreshTimer_.Start(10000);

    return UiOperationResult::Ok(
        "wxWidgets 页面数据绑定已启动"
    );
}

void WxUiBinder::Detach()
{
    refreshTimer_.Stop();
    UiEventBus::Instance().Stop();

    if (root_ != nullptr)
    {
        root_->Unbind(
            wxEVT_CONTINUUM_UI_CHANGE,
            &WxUiBinder::OnUiChange,
            this
        );

        root_->Unbind(
            wxEVT_DESTROY,
            &WxUiBinder::OnRootDestroyed,
            this
        );
    }

    root_ = nullptr;
    commandsBound_ = false;
    lastSearchQuery_.clear();
}

bool WxUiBinder::IsAttached() const
{
    return root_ != nullptr;
}

wxWindow* WxUiBinder::FindControl(
    const std::string& bindingName
) const
{
    return FindRecursively(
        root_,
        Lower(bindingName)
    );
}

void WxUiBinder::SetText(
    const std::string& bindingName,
    const std::string& value
)
{
    wxWindow* control =
        FindControl(bindingName);

    if (auto* label =
            dynamic_cast<wxStaticText*>(control))
    {
        label->SetLabel(Wx(value));
        return;
    }

    if (auto* text =
            dynamic_cast<wxTextCtrl*>(control))
    {
        if (text->GetValue() != Wx(value))
        {
            text->ChangeValue(Wx(value));
        }
    }
}

void WxUiBinder::SetStatus(
    const std::string& bindingName,
    const UiOperationResult& result
)
{
    SetText(
        bindingName,
        result.success
            ? (
                result.message.empty()
                    ? "操作成功"
                    : result.message
            )
            : (
                "操作失败：" +
                result.message
            )
    );
}

void WxUiBinder::BindCommands()
{
    if (commandsBound_ || root_ == nullptr)
    {
        return;
    }

    if (auto* query =
            FindAs<wxTextCtrl>("search.query"))
    {
        query->Unbind(
            wxEVT_TEXT_ENTER,
            &WxUiBinder::OnSearch,
            this
        );
        query->Bind(
            wxEVT_TEXT_ENTER,
            &WxUiBinder::OnSearch,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("search.execute"))
    {
        button->Unbind(
            wxEVT_BUTTON,
            &WxUiBinder::OnSearch,
            this
        );
        button->Bind(
            wxEVT_BUTTON,
            &WxUiBinder::OnSearch,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("sources.scan"))
    {
        button->Unbind(
            wxEVT_BUTTON,
            &WxUiBinder::OnScanSource,
            this
        );
        button->Bind(
            wxEVT_BUTTON,
            &WxUiBinder::OnScanSource,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("jobs.cancel"))
    {
        button->Unbind(
            wxEVT_BUTTON,
            &WxUiBinder::OnCancelJob,
            this
        );
        button->Bind(
            wxEVT_BUTTON,
            &WxUiBinder::OnCancelJob,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("jobs.retry"))
    {
        button->Unbind(
            wxEVT_BUTTON,
            &WxUiBinder::OnRetryJob,
            this
        );
        button->Bind(
            wxEVT_BUTTON,
            &WxUiBinder::OnRetryJob,
            this
        );
    }

    if (auto* button =
            FindAs<wxButton>("backups.create"))
    {
        button->Unbind(
            wxEVT_BUTTON,
            &WxUiBinder::OnCreateBackup,
            this
        );
        button->Bind(
            wxEVT_BUTTON,
            &WxUiBinder::OnCreateBackup,
            this
        );
    }

    commandsBound_ = true;
}

void WxUiBinder::RefreshAll()
{
    if (root_ == nullptr)
    {
        return;
    }

    /*
     * 页面可能在绑定器启动后才延迟创建。
     * 每次完整刷新重新尝试命令绑定。
     */
    commandsBound_ = false;
    BindCommands();

    RefreshDashboard();
    RefreshDataSources();
    RefreshJobs();
    RefreshBackups();

    if (!lastSearchQuery_.empty())
    {
        RefreshSearch();
    }
}

void WxUiBinder::RefreshDashboard()
{
    if (root_ == nullptr)
    {
        return;
    }

    const auto snapshot =
        UiDataService::Instance().Dashboard();

    SetText(
        "dashboard.active_objects",
        std::to_string(snapshot.activeObjects)
    );

    SetText(
        "dashboard.evidence_review",
        std::to_string(
            snapshot.evidenceNeedsReview
        )
    );

    SetText(
        "dashboard.sources",
        std::to_string(
            snapshot.enabledDataSources
        )
    );

    SetText(
        "dashboard.files",
        std::to_string(snapshot.indexedFiles)
    );

    SetText(
        "dashboard.jobs",
        std::to_string(snapshot.queuedJobs)
    );

    if (FindControl("dashboard.backups") != nullptr)
    {
        SetText(
            "dashboard.backups",
            std::to_string(
                UiDataService::Instance()
                    .BackupCount()
            )
        );
    }

    const auto security =
        UiDataService::Instance().Security();

    const std::string securityText =
        security.sqlCipherAvailable
            ? (
                "SQLCipher " +
                security.sqlCipherVersion
            )
            : "普通 SQLite（未加密）";

    SetText(
        "dashboard.security",
        securityText
    );

    SetText(
        "app.security_status",
        securityText
    );

    SetText(
        "app.workspace_status",
        snapshot.workspaceDirectory.empty()
            ? "工作区不可用"
            : snapshot.workspaceDirectory
    );

    SetText(
        "app.database_status",
        snapshot.databasePath.empty()
            ? "数据库不可用"
            : "●  数据库已连接"
    );

    SetText(
        "app.jobs_status",
        "后台任务 " +
            std::to_string(snapshot.queuedJobs)
    );

    const std::string workerText =
        snapshot.backgroundWorkerRunning
            ? (
                snapshot.lastError.empty()
                    ? "后台服务正在运行"
                    : "后台服务错误：" +
                        snapshot.lastError
            )
            : "后台服务未运行";

    SetText(
        "dashboard.worker_status",
        workerText
    );

    SetText(
        "dashboard.search_status",
        "全文索引文档 " +
            std::to_string(
                snapshot.searchDocuments
            ) +
            "，等待索引 " +
            std::to_string(
                snapshot.pendingSearchItems
            )
    );

    SetText(
        "app.index_status",
        snapshot.lastError.empty()
            ? (
                "●  已索引 " +
                std::to_string(
                    snapshot.searchDocuments
                ) +
                "，等待 " +
                std::to_string(
                    snapshot.pendingSearchItems
                )
            )
            : "索引/后台错误：" +
                snapshot.lastError
    );
}

void WxUiBinder::RefreshSearch()
{
    auto* list =
        FindAs<wxListCtrl>("search.results");

    if (list == nullptr ||
        lastSearchQuery_.empty())
    {
        return;
    }

    SearchRequest request;
    request.query = lastSearchQuery_;

    const auto response =
        UiDataService::Instance().Search(request);

    PrepareList(
        list,
        {
            "文件",
            "标题",
            "内容",
            "相关度"
        },
        {
            180,
            220,
            520,
            90
        }
    );

    list->Freeze();

    for (const auto& item : response.hits)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(item.fileId)
        );

        list->SetItem(
            row,
            1,
            Wx(item.title)
        );

        list->SetItem(
            row,
            2,
            Wx(item.snippet)
        );

        std::ostringstream score;
        score << item.score;

        list->SetItem(
            row,
            3,
            Wx(score.str())
        );
    }

    list->Thaw();

    SetText(
        "search.status",
        response.status.success
            ? (
                "找到 " +
                std::to_string(response.total) +
                " 条结果"
            )
            : response.status.message
    );
}

void WxUiBinder::RefreshDataSources()
{
    auto* list =
        FindAs<wxListCtrl>("sources.list");

    if (list == nullptr)
    {
        return;
    }

    const auto sources =
        UiDataService::Instance()
            .ListDataSources(true, 500);

    PrepareList(
        list,
        {
            "编号",
            "名称",
            "类型",
            "路径",
            "状态"
        },
        {
            170,
            180,
            120,
            420,
            90
        }
    );

    list->Freeze();

    for (const auto& source : sources)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(source.id)
        );

        list->SetItem(row, 1, Wx(source.name));
        list->SetItem(
            row,
            2,
            Wx(source.sourceType)
        );
        list->SetItem(
            row,
            3,
            Wx(source.rootPath)
        );
        list->SetItem(
            row,
            4,
            source.enabled ? "已启用" : "已停用"
        );
    }

    list->Thaw();
}

void WxUiBinder::RefreshJobs()
{
    auto* list =
        FindAs<wxListCtrl>("jobs.list");

    if (list == nullptr)
    {
        return;
    }

    const std::string selectedId =
        SelectedListId("jobs.list");

    const auto jobs =
        UiDataService::Instance().ListJobs(500);

    PrepareList(
        list,
        {
            "编号",
            "类型",
            "状态",
            "进度",
            "尝试",
            "错误"
        },
        {
            210,
            130,
            100,
            80,
            70,
            420
        }
    );

    list->Freeze();

    for (const auto& job : jobs)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(job.id)
        );

        list->SetItem(row, 1, Wx(job.type));
        list->SetItem(row, 2, Wx(job.state));
        list->SetItem(
            row,
            3,
            Wx(std::to_string(job.progress) + "%")
        );
        list->SetItem(
            row,
            4,
            Wx(std::to_string(job.attempts))
        );
        list->SetItem(
            row,
            5,
            Wx(job.errorMessage)
        );

        if (!selectedId.empty() &&
            job.id == selectedId)
        {
            list->SetItemState(
                row,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED |
                    wxLIST_STATE_FOCUSED
            );
            list->EnsureVisible(row);
        }
    }

    list->Thaw();
}

void WxUiBinder::RefreshBackups()
{
    auto* list =
        FindAs<wxListCtrl>("backups.list");

    if (list == nullptr)
    {
        return;
    }

    const auto backups =
        UiDataService::Instance().ListBackups();

    PrepareList(
        list,
        {
            "编号",
            "创建时间",
            "大小",
            "版本",
            "校验"
        },
        {
            280,
            170,
            110,
            80,
            220
        }
    );

    list->Freeze();

    for (const auto& backup : backups)
    {
        const long row = list->InsertItem(
            list->GetItemCount(),
            Wx(backup.id)
        );

        list->SetItem(
            row,
            1,
            Wx(backup.createdAt)
        );
        list->SetItem(
            row,
            2,
            Wx(std::to_string(
                backup.databaseSize
            ))
        );
        list->SetItem(
            row,
            3,
            Wx(std::to_string(
                backup.schemaVersion
            ))
        );
        list->SetItem(
            row,
            4,
            Wx(
                backup.valid
                    ? "有效"
                    : backup.validationMessage
            )
        );
    }

    list->Thaw();
}

std::string WxUiBinder::SelectedListId(
    const std::string& bindingName
) const
{
    auto* list =
        FindAs<wxListCtrl>(bindingName);

    if (list == nullptr)
    {
        return std::string();
    }

    const long selected = list->GetNextItem(
        -1,
        wxLIST_NEXT_ALL,
        wxLIST_STATE_SELECTED
    );

    if (selected < 0)
    {
        return std::string();
    }

    return Utf8(
        list->GetItemText(selected, 0)
    );
}

void WxUiBinder::OnUiChange(
    wxThreadEvent& event
)
{
    const auto change =
        event.GetPayload<UiChangeEvent>();

    switch (change.type)
    {
    case UiChangeType::Dashboard:
    case UiChangeType::Workspace:
    case UiChangeType::Security:
        RefreshDashboard();
        break;

    case UiChangeType::Search:
        break;

    case UiChangeType::DataSources:
    case UiChangeType::Files:
        RefreshDataSources();
        RefreshDashboard();
        break;

    case UiChangeType::Jobs:
        RefreshJobs();
        RefreshDashboard();
        break;

    case UiChangeType::Backups:
        RefreshBackups();
        RefreshDashboard();
        break;

    case UiChangeType::Objects:
    case UiChangeType::Evidence:
        RefreshDashboard();
        break;
    }
}

void WxUiBinder::OnRootDestroyed(
    wxWindowDestroyEvent& event
)
{
    /*
     * wxEVT_DESTROY 也可能由子窗口向上传播。
     * 只有实际销毁对象就是当前根窗口时才解除绑定状态。
     */
    if (event.GetEventObject() == root_)
    {
        refreshTimer_.Stop();
        UiEventBus::Instance().Stop();

        root_ = nullptr;
        commandsBound_ = false;
        lastSearchQuery_.clear();
    }

    event.Skip();
}

void WxUiBinder::OnTimer(wxTimerEvent&)
{
    if (root_ == nullptr ||
        root_->IsBeingDeleted())
    {
        Detach();
        return;
    }

    RefreshDashboard();
    RefreshJobs();
}

void WxUiBinder::OnSearch(wxCommandEvent&)
{
    auto* query =
        FindAs<wxTextCtrl>("search.query");

    if (query == nullptr)
    {
        return;
    }

    lastSearchQuery_ = Utf8(
        query->GetValue()
    );

    if (lastSearchQuery_.empty())
    {
        SetText(
            "search.status",
            "请输入搜索内容"
        );
        return;
    }

    RefreshSearch();
}

void WxUiBinder::OnScanSource(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedListId("sources.list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一个数据源。",
            "扫描数据源",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .QueueSourceScan(id);

    if (!result.success)
    {
        wxMessageBox(
            Wx(result.message),
            "扫描任务提交失败",
            wxOK | wxICON_ERROR,
            root_
        );
    }

    RefreshJobs();
}

void WxUiBinder::OnCancelJob(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedListId("jobs.list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一个任务。",
            "取消任务",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .CancelJob(id);

    if (!result.success)
    {
        wxMessageBox(
            Wx(result.message),
            "取消任务失败",
            wxOK | wxICON_ERROR,
            root_
        );
    }

    RefreshJobs();
}

void WxUiBinder::OnRetryJob(
    wxCommandEvent&
)
{
    const std::string id =
        SelectedListId("jobs.list");

    if (id.empty())
    {
        wxMessageBox(
            "请先选择一个任务。",
            "重试任务",
            wxOK | wxICON_INFORMATION,
            root_
        );
        return;
    }

    const auto result =
        UiDataService::Instance()
            .RetryJob(id);

    if (!result.success)
    {
        wxMessageBox(
            Wx(result.message),
            "重试任务失败",
            wxOK | wxICON_ERROR,
            root_
        );
    }

    RefreshJobs();
}

void WxUiBinder::OnCreateBackup(
    wxCommandEvent&
)
{
    BackupCreateOptions options;
    options.label = "manual";
    options.retainLatest = 20;

    const auto result =
        UiDataService::Instance()
            .CreateBackup(options);

    SetStatus(
        "backups.status",
        UiOperationResult::FromStorage(
            result.status
        )
    );

    if (!result.status.success)
    {
        wxMessageBox(
            Wx(result.status.message),
            "创建备份失败",
            wxOK | wxICON_ERROR,
            root_
        );
    }

    RefreshBackups();
    RefreshDashboard();
}

}
