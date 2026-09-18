#include "BackupService.h"

#include <filesystem>
#include <fstream>
#include <iostream>

int main()
{
    using namespace continuum;

    const auto base =
        std::filesystem::temp_directory_path() /
        "continuum-round-18-test";

    std::error_code error;

    std::filesystem::remove_all(
        base,
        error
    );

    std::filesystem::create_directories(
        base,
        error
    );

    auto& workspace =
        WorkspaceService::Instance();

    const auto initialize =
        workspace.Initialize(
            base.u8string()
        );

    if (!initialize.success)
    {
        std::cerr
            << "工作区初始化失败: "
            << initialize.message
            << "\n";

        return 1;
    }

    DomainObjectRecord original;
    original.id = "F-BACKUP-ORIGINAL";
    original.objectType = "fact";
    original.title = "备份前对象";
    original.description =
        "该对象应在恢复后存在";
    original.status = "active";
    original.priority = "normal";
    original.timePrecision = "exact";

    const auto saveOriginal =
        workspace.Objects().Save(
            original,
            "test"
        );

    if (!saveOriginal.success)
    {
        std::cerr
            << "保存原始对象失败: "
            << saveOriginal.message
            << "\n";

        return 2;
    }

    BackupService backups(workspace);

    BackupCreateOptions createOptions;
    createOptions.label = "automated-test";
    createOptions.retainLatest = 5;

    const auto created =
        backups.CreateBackup(createOptions);

    if (!created.status.success)
    {
        std::cerr
            << "创建备份失败: "
            << created.status.message
            << "\n";

        return 3;
    }

    if (!created.backup.valid ||
        created.backup.databaseSha256.size() != 64)
    {
        std::cerr << "备份元数据异常\n";
        return 4;
    }

    BackupRecord validated;

    const auto validation =
        backups.ValidateBackup(
            created.backup.directory,
            &validated
        );

    if (!validation.success ||
        !validated.valid)
    {
        std::cerr
            << "备份校验失败: "
            << validation.message
            << "\n";

        return 5;
    }

    DomainObjectRecord mutation;
    mutation.id = "F-AFTER-BACKUP";
    mutation.objectType = "fact";
    mutation.title = "备份后对象";
    mutation.description =
        "该对象恢复后不应存在";
    mutation.status = "active";
    mutation.priority = "normal";
    mutation.timePrecision = "exact";

    const auto saveMutation =
        workspace.Objects().Save(
            mutation,
            "test"
        );

    if (!saveMutation.success)
    {
        std::cerr << "保存变更对象失败\n";
        return 6;
    }

    if (!workspace.Objects().FindById(
            mutation.id
        ))
    {
        std::cerr << "变更对象保存后不存在\n";
        return 7;
    }

    BackupRestoreOptions restoreOptions;
    restoreOptions.createRollbackCopy = true;
    restoreOptions.restartBackgroundWorker = false;

    const auto restore =
        backups.RestoreBackup(
            created.backup.directory,
            restoreOptions
        );

    if (!restore.success)
    {
        std::cerr
            << "恢复备份失败: "
            << restore.message
            << "\n";

        return 8;
    }

    if (!workspace.Objects().FindById(
            original.id
        ))
    {
        std::cerr
            << "恢复后原始对象不存在\n";

        return 9;
    }

    if (workspace.Objects().FindById(
            mutation.id
        ))
    {
        std::cerr
            << "恢复后仍存在备份之后的对象\n";

        return 10;
    }

    const auto integrity =
        workspace.GetDatabase().CheckIntegrity();

    if (!integrity.success)
    {
        std::cerr
            << "恢复后数据库检查失败: "
            << integrity.message
            << "\n";

        return 11;
    }

    const auto listed =
        backups.ListBackups();

    if (listed.empty() ||
        listed.front().databaseSha256.empty())
    {
        std::cerr << "备份列表异常\n";
        return 12;
    }

    const auto prune =
        backups.PruneBackups(1);

    if (!prune.success)
    {
        std::cerr
            << "清理备份失败: "
            << prune.message
            << "\n";

        return 13;
    }

    workspace.Shutdown();

    std::filesystem::remove_all(
        base,
        error
    );

    std::cout
        << "round-18 backup and restore tests passed\n";

    return 0;
}
