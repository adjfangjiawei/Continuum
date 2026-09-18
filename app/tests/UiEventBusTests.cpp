#include "UiDataService.h"

#include <iostream>
#include <string>

int main()
{
    using namespace continuum;

    /*
     * 本测试不创建 GUI 窗口，只验证事件负载类型和
     * UiDataService 订阅机制能被 wxWidgets 绑定层使用。
     */
    auto& service = UiDataService::Instance();

    int received = 0;
    UiChangeType lastType =
        UiChangeType::Workspace;
    std::string lastMessage;

    const auto token = service.Subscribe(
        [&](const UiChangeEvent& event) {
            ++received;
            lastType = event.type;
            lastMessage = event.message;
        }
    );

    if (token == 0)
    {
        std::cerr << "无法注册 UI 事件监听器\n";
        return 1;
    }

    service.Refresh(
        UiChangeType::Dashboard,
        "refresh-dashboard"
    );

    if (received != 1 ||
        lastType != UiChangeType::Dashboard ||
        lastMessage != "refresh-dashboard")
    {
        std::cerr << "UI 事件负载传递异常\n";
        return 2;
    }

    service.Unsubscribe(token);

    service.Refresh(
        UiChangeType::Jobs,
        "must-not-arrive"
    );

    if (received != 1)
    {
        std::cerr << "取消订阅后仍收到事件\n";
        return 3;
    }

    std::cout
        << "round-22 UI event subscription tests passed\n";

    return 0;
}
