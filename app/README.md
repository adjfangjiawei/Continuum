# 续证 Continuum 桌面程序

这是根据 `design/ui/round-01` 至 `round-06` 页面设计创建的 C++ 桌面应用工程。

## 当前实现阶段

程序实现第 1 轮已完成：

- CMake 工程
- wxWidgets 应用入口
- 深色主题
- 主窗口
- 顶部搜索区域
- 可滚动侧边导航
- P01-P17 页面路由
- M01-M12 页面路由
- 项目概览初步实现
- 通用页面布局
- 状态栏
- Ctrl+K 命令面板

其余页面目前已经具有可运行的导航入口和通用内容容器，后续轮次会逐页替换为对应设计中的专用组件与交互。

## 依赖

- CMake 3.24 或更高版本
- C++20 编译器
- wxWidgets 3.2
- Ninja 或 Visual Studio
- Windows 推荐使用 clang-cl 或 MSVC

## 使用 vcpkg 安装 wxWidgets

在 Windows PowerShell 中执行：

    vcpkg install wxwidgets:x64-windows

配置工程：

    cmake -S D:\UnknownSoftware\app       -B D:\UnknownSoftware\app\build\windows-debug    -G Ninja   -DCMAKE_BUILD_TYPE=Debug   -DCMAKE_TOOLCHAIN_FILE=D:\windowsvcpkg\vcpkg\scripts\buildsystems\vcpkg.cmake

构建：

    cmake --build D:\UnknownSoftware\app\build\windows-debug --parallel

程序位置通常为：

    D:\UnknownSoftware\app\build\windows-debug\Continuum.exe

## 设计文件

设计文件继续保留在：

    D:\UnknownSoftware\design\ui

代码生成不会删除或移动设计文件。
