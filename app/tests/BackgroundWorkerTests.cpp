#include "BackgroundWorker.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-17-test";

    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(
        base / "source"
    );

    {
        std::ofstream file(
            base / "source" / "background.md",
            std::ios::binary
        );

        file
            << "# Background Processing\n\n"
            << "The asynchronous worker completed "
            << "the migration indexing task.\n";
    }

    auto& workspace = WorkspaceService::Instance();

    const auto initialize =
        workspace.Initialize(base.u8string());

    if (!initialize.success)
    {
        std::cerr
            << "工作区初始化失败: "
            << initialize.message
            << "\n";
        return 1;
    }

    DataSourceRecord source;
    source.id = "SRC-BACKGROUND";
    source.name = "Background Test";
    source.sourceType = "local_folder";
    source.rootPath =
        (base / "source").u8string();

    if (!workspace.DataSources().Save(
            source,
            "test"
        ).success)
    {
        std::cerr << "保存测试数据源失败\n";
        return 2;
    }

    JobRepository jobs(
        workspace.GetDatabase()
    );

    JobRecord scanJob;
    scanJob.id = "JOB-SCAN-BACKGROUND";
    scanJob.jobType = "scan_source";
    scanJob.state = "queued";
    scanJob.payload =
        "{\"source_id\":\"SRC-BACKGROUND\"}";

    if (!jobs.Enqueue(
            scanJob,
            "test"
        ).success)
    {
        std::cerr << "扫描任务入队失败\n";
        return 3;
    }

    BackgroundWorkerOptions options;
    options.maximumAttempts = 2;
    options.indexBatchSize = 20;
    options.idleInterval =
        std::chrono::milliseconds(20);

    auto& worker = BackgroundWorker::Instance();

    const auto start =
        worker.Start(workspace, options);

    if (!start.success)
    {
        std::cerr
            << "后台线程启动失败: "
            << start.message
            << "\n";
        return 4;
    }

    worker.Wake();

    if (!worker.WaitUntilIdle(
            std::chrono::seconds(15)
        ))
    {
        std::cerr << "后台任务等待超时\n";
        worker.Stop();
        return 5;
    }

    const auto file =
        workspace.Files().FindByPath(
            "SRC-BACKGROUND",
            "background.md"
        );

    if (!file)
    {
        std::cerr << "后台扫描未保存文件\n";
        worker.Stop();
        return 6;
    }

    if (file->parseState != "parsed")
    {
        std::cerr
            << "后台解析状态异常: "
            << file->parseState
            << "\n";
        worker.Stop();
        return 7;
    }

    SearchService search(
        workspace.GetDatabase()
    );

    SearchRequest request;
    request.query = "asynchronous migration";

    const auto response =
        search.Search(request);

    if (!response.status.success ||
        response.total != 1)
    {
        std::cerr << "后台索引搜索失败\n";
        worker.Stop();
        return 8;
    }

    JobRecord cancelJob;
    cancelJob.id = "JOB-CANCEL-TEST";
    cancelJob.jobType = "unknown_test";
    cancelJob.state = "queued";
    cancelJob.payload = "{}";

    if (!jobs.Enqueue(
            cancelJob,
            "test"
        ).success)
    {
        std::cerr << "取消测试任务入队失败\n";
        worker.Stop();
        return 9;
    }

    const auto cancel =
        worker.RequestCancel(
            cancelJob.id,
            "test"
        );

    if (!cancel.success)
    {
        std::cerr
            << "任务取消失败: "
            << cancel.message
            << "\n";
        worker.Stop();
        return 10;
    }

    worker.Wake();

    if (!worker.WaitUntilIdle(
            std::chrono::seconds(5)
        ))
    {
        std::cerr << "取消任务等待超时\n";
        worker.Stop();
        return 11;
    }

    {
        std::lock_guard<std::recursive_mutex> lock(
            workspace.GetDatabase().Mutex()
        );

        sqlite3* handle =
            workspace.GetDatabase().Handle();

        sqlite3_stmt* statement = nullptr;

        if (sqlite3_prepare_v2(
                handle,
                "SELECT state FROM jobs "
                "WHERE id='JOB-CANCEL-TEST';",
                -1,
                &statement,
                nullptr
            ) != SQLITE_OK)
        {
            std::cerr << "无法检查取消状态\n";
            worker.Stop();
            return 12;
        }

        std::string state;

        if (sqlite3_step(statement) == SQLITE_ROW)
        {
            const auto* value =
                sqlite3_column_text(statement, 0);

            if (value != nullptr)
            {
                state =
                    reinterpret_cast<const char*>(
                        value
                    );
            }
        }

        sqlite3_finalize(statement);

        if (state != "cancelled")
        {
            std::cerr
                << "取消状态异常: "
                << state
                << "\n";
            worker.Stop();
            return 13;
        }
    }

    const auto status = worker.Status();

    if (status.completedJobs < 2 ||
        status.indexedDocuments < 1)
    {
        std::cerr
            << "后台统计异常: completed="
            << status.completedJobs
            << " indexed="
            << status.indexedDocuments
            << "\n";
        worker.Stop();
        return 14;
    }

    worker.Stop();

    if (worker.IsRunning())
    {
        std::cerr << "后台线程未安全停止\n";
        return 15;
    }

    workspace.Shutdown();
    std::filesystem::remove_all(base, error);

    std::cout
        << "round-17 background worker tests passed\n";

    return 0;
}
