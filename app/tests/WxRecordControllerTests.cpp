#include "UiDataService.h"

#include <filesystem>
#include <iostream>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-23-test";

    std::error_code error;

    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base, error);

    if (error)
    {
        std::cerr << "无法创建测试目录\n";
        return 1;
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

    auto& ui = UiDataService::Instance();

    DomainObjectRecord record;
    record.id = "OBJ-ROUND-23";
    record.objectType = "fact";
    record.title = "对象控制器测试";
    record.description =
        "验证 UI 对象保存、删除与恢复";
    record.status = "active";
    record.priority = "normal";
    record.owner = "test";
    record.timePrecision = "exact";

    const auto save =
        ui.SaveObject(record, "test");

    if (!save.success)
    {
        std::cerr
            << "保存对象失败: "
            << save.message
            << "\n";
        return 3;
    }

    const auto objects =
        ui.ListObjects(
            "fact",
            false,
            100
        );

    bool found = false;

    for (const auto& object : objects)
    {
        if (object.id == record.id)
        {
            found = true;
            break;
        }
    }

    if (!found)
    {
        std::cerr << "对象列表没有返回保存记录\n";
        return 4;
    }

    const auto deletion =
        ui.DeleteObject(
            record.id,
            "round-23-test",
            "test"
        );

    if (!deletion.success)
    {
        std::cerr << "对象软删除失败\n";
        return 5;
    }

    const auto restoration =
        ui.RestoreObject(
            record.id,
            "test"
        );

    if (!restoration.success)
    {
        std::cerr << "对象恢复失败\n";
        return 6;
    }

    if (!workspace.Objects().FindById(record.id))
    {
        std::cerr << "恢复后对象不存在\n";
        return 7;
    }

    workspace.Shutdown();
    std::filesystem::remove_all(base, error);

    std::cout
        << "round-23 record controller service tests passed\n";

    return 0;
}
