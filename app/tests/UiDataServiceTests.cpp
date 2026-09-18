#include "UiDataService.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-21-test";

    std::error_code error;

    std::filesystem::remove_all(
        base,
        error
    );

    std::filesystem::create_directories(
        base / "source",
        error
    );

    if (error)
    {
        std::cerr << "无法创建测试目录\n";
        return 1;
    }

    {
        std::ofstream file(
            base / "source" / "ui-bridge.md",
            std::ios::binary
        );

        file
            << "# UI Bridge\n\n"
            << "The repository search and backup "
            << "services are connected.\n";
    }

    auto& workspace =
        WorkspaceService::Instance();

    const auto initialize =
        workspace.Initialize(base.u8string());

    if (!initialize.success)
    {
        std::cerr
            << "工作区初始化失败: "
            << initialize.message
            << "\n";

        return 2;
    }

    BackgroundWorkerOptions workerOptions;
    workerOptions.idleInterval =
        std::chrono::milliseconds(20);

    const auto workerStart =
        BackgroundWorker::Instance().Start(
            workspace,
            workerOptions
        );

    if (!workerStart.success)
    {
        std::cerr
            << "后台服务启动失败: "
            << workerStart.message
            << "\n";

        return 3;
    }

    auto& ui = UiDataService::Instance();

    int notificationCount = 0;

    const auto subscription =
        ui.Subscribe(
            [&notificationCount](
                const UiChangeEvent&
            ) {
                ++notificationCount;
            }
        );

    DomainObjectRecord object;
    object.id = "F-UI-BRIDGE";
    object.objectType = "fact";
    object.title = "UI 数据桥接测试";
    object.description =
        "该记录通过 UiDataService 保存";
    object.status = "active";
    object.priority = "normal";
    object.owner = "test";
    object.timePrecision = "exact";

    const auto saveObject =
        ui.SaveObject(object, "test");

    if (!saveObject.success)
    {
        std::cerr
            << "UI 保存对象失败: "
            << saveObject.message
            << "\n";

        return 4;
    }

    const auto objects =
        ui.ListObjects("fact", false, 100);

    bool foundObject = false;

    for (const auto& item : objects)
    {
        if (item.id == object.id)
        {
            foundObject = true;
            break;
        }
    }

    if (!foundObject)
    {
        std::cerr << "UI 对象列表未返回保存记录\n";
        return 5;
    }

    DataSourceRecord source;
    source.id = "SRC-UI-BRIDGE";
    source.name = "UI Bridge Source";
    source.sourceType = "local_folder";
    source.rootPath =
        (base / "source").u8string();
    source.enabled = true;

    const auto saveSource =
        ui.SaveDataSource(source, "test");

    if (!saveSource.success)
    {
        std::cerr
            << "UI 保存数据源失败: "
            << saveSource.message
            << "\n";

        return 6;
    }

    const auto queueScan =
        ui.QueueSourceScan(
            source.id,
            "test"
        );

    if (!queueScan.success)
    {
        std::cerr
            << "UI 提交扫描任务失败: "
            << queueScan.message
            << "\n";

        return 7;
    }

    if (!BackgroundWorker::Instance().WaitUntilIdle(
            std::chrono::seconds(15)
        ))
    {
        std::cerr << "UI 扫描解析任务等待超时\n";
        return 8;
    }

    SearchRequest request;
    request.query = "repository backup";

    const auto search = ui.Search(request);

    if (!search.status.success ||
        search.total != 1)
    {
        std::cerr
            << "UI 搜索失败: "
            << search.status.message
            << "\n";

        return 9;
    }

    const auto dashboard =
        ui.Dashboard();

    if (dashboard.activeObjects < 1 ||
        dashboard.enabledDataSources < 1 ||
        dashboard.indexedFiles < 1 ||
        dashboard.searchDocuments < 1)
    {
        std::cerr
            << "仪表盘统计异常: objects="
            << dashboard.activeObjects
            << " sources="
            << dashboard.enabledDataSources
            << " files="
            << dashboard.indexedFiles
            << " index="
            << dashboard.searchDocuments
            << "\n";

        return 10;
    }

    const auto jobs = ui.ListJobs(100);

    if (jobs.empty())
    {
        std::cerr << "UI 任务列表为空\n";
        return 11;
    }

    BackupCreateOptions backupOptions;
    backupOptions.label = "ui-bridge-test";
    backupOptions.retainLatest = 3;

    const auto backup =
        ui.CreateBackup(backupOptions);

    if (!backup.status.success ||
        !backup.backup.valid)
    {
        std::cerr
            << "UI 创建备份失败: "
            << backup.status.message
            << "\n";

        return 12;
    }

    BackupRecord validated;

    const auto validation =
        ui.ValidateBackup(
            backup.backup.directory,
            &validated
        );

    if (!validation.success ||
        !validated.valid)
    {
        std::cerr << "UI 备份校验失败\n";
        return 13;
    }

    const auto backups = ui.ListBackups();

    if (backups.empty())
    {
        std::cerr << "UI 备份列表为空\n";
        return 14;
    }

    const auto deletion =
        ui.DeleteObject(
            object.id,
            "bridge test",
            "test"
        );

    if (!deletion.success)
    {
        std::cerr << "UI 删除对象失败\n";
        return 15;
    }

    const auto restoration =
        ui.RestoreObject(
            object.id,
            "test"
        );

    if (!restoration.success)
    {
        std::cerr << "UI 恢复对象失败\n";
        return 16;
    }

    if (notificationCount < 5)
    {
        std::cerr
            << "UI 数据变更通知数量不足: "
            << notificationCount
            << "\n";

        return 17;
    }

    ui.Unsubscribe(subscription);

    BackgroundWorker::Instance().Stop();
    workspace.Shutdown();

    std::filesystem::remove_all(
        base,
        error
    );

    std::cout
        << "round-21 UI data service tests passed\n";

    return 0;
}
