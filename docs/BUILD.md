# 编译构建指南

本文档详细说明如何在各平台上编译 loong-pixelgrab 库。

## 目录

- [环境要求](#环境要求)
- [快速编译](#快速编译)
- [CMake 选项](#cmake-选项)
- [Windows 编译](#windows-编译)
- [macOS 编译](#macos-编译)
- [Linux 编译](#linux-编译)
- [构建产物](#构建产物)
- [在你的项目中集成](#在你的项目中集成)
- [常见问题](#常见问题)

---

## 环境要求

### 通用要求

| 工具 | 最低版本 | 说明 |
|------|---------|------|
| CMake | 3.16 | 构建系统 |
| C++ 编译器 | C++17 支持 | MSVC 2019+, GCC 8+, Clang 8+ |
| Git | 2.x | FetchContent 拉取依赖 |
| 网络连接 | — | 首次构建需要下载 spdlog 等依赖 |

### 自动获取的依赖

以下依赖通过 CMake FetchContent 自动下载，**无需手动安装**：

| 依赖 | 版本 | 用途 |
|------|------|------|
| [spdlog](https://github.com/gabime/spdlog) | v1.15.1 | 日志系统 (始终构建) |
| [Google Test](https://github.com/google/googletest) | v1.15.2 | 单元测试 (可选, 仅测试模式) |

### 平台系统库

loong-pixelgrab 使用各平台原生系统库，**无需额外安装第三方库**：

#### Windows

自动链接的系统库：
- `user32`, `gdi32` — 基础窗口和图形
- `gdiplus` — 标注绘制 (GDI+)
- `uiautomationcore`, `ole32`, `oleaut32` — UI 元素检测 (COM)
- `shlwapi` — Shell 工具函数

#### macOS

自动链接的框架：
- `CoreGraphics`, `AppKit` — 截图与窗口管理
- `ScreenCaptureKit` (可选) — 现代截图 API
- `ApplicationServices` — Accessibility API

#### Linux

需要安装的开发包：

```bash
# Debian/Ubuntu
sudo apt install libx11-dev libxext-dev libcairo2-dev libpango1.0-dev

# Fedora/RHEL
sudo dnf install libX11-devel libXext-devel cairo-devel pango-devel

# Arch Linux
sudo pacman -S libx11 libxext cairo pango
```

可选 (UI 元素检测)：

```bash
# Debian/Ubuntu
sudo apt install libatspi2.0-dev libdbus-1-dev

# Fedora/RHEL
sudo dnf install at-spi2-core-devel dbus-devel
```

---

## 快速编译

适用于所有平台的最简编译步骤：

```bash
# 1. 进入项目目录
cd loong-pixelgrab

# 2. 生成构建文件 (Release 模式)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# 3. 编译
cmake --build build --config Release
```

编译成功后，动态库和示例程序位于 `build/` 目录中。

---

## CMake 选项

| 选项 | 默认值 | 描述 |
|------|--------|------|
| `CMAKE_BUILD_TYPE` | `Release` | 构建类型: `Debug`, `Release`, `RelWithDebInfo`, `MinSizeRel` |
| `PIXELGRAB_BUILD_EXAMPLES` | `ON` | 是否编译示例程序 |
| `PIXELGRAB_BUILD_TESTS` | `OFF` | 是否编译单元测试 (启用后自动下载 Google Test) |

### 使用示例

```bash
# Release 模式 + 示例 + 测试
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPIXELGRAB_BUILD_EXAMPLES=ON \
  -DPIXELGRAB_BUILD_TESTS=ON

# 仅编译库 (无示例无测试)
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DPIXELGRAB_BUILD_EXAMPLES=OFF

# Debug 模式 (含调试信息)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
```

---

## Windows 编译

### 使用 Visual Studio (推荐)

#### 方法 1: CMake 命令行

```powershell
# 使用 Visual Studio 2022 生成器
cmake -B build -G "Visual Studio 17 2022" -A x64

# 编译 Release
cmake --build build --config Release

# 编译 Debug
cmake --build build --config Debug
```

#### 方法 2: Visual Studio 直接打开

1. 打开 Visual Studio 2022
2. 选择 **文件** → **打开** → **CMake...**
3. 选择项目根目录的 `CMakeLists.txt`
4. 在工具栏选择 `Release` 或 `Debug` 配置
5. 按 `Ctrl+Shift+B` 编译

#### 方法 3: Developer Command Prompt

```powershell
# 打开 "Developer PowerShell for VS 2022"
cd loong-pixelgrab
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### 编译器选项

MSVC 编译时自动启用：
- `/utf-8` — UTF-8 源码和执行字符集
- `/W4` — 高警告级别
- `/WX` — 将警告视为错误

### 构建产物位置

```
build/
├── Release/
│   ├── pixelgrab.dll          # 动态库
│   ├── pixelgrab.lib          # 导入库
│   ├── basic_capture.exe      # 示例程序
│   └── PixelGrab.exe          # 截图/标注/贴图工具
└── Debug/
    ├── pixelgrab.dll
    ├── pixelgrab.lib
    ├── pixelgrab.pdb          # 调试符号
    ├── basic_capture.exe
    └── PixelGrab.exe
```

---

## macOS 编译

### 使用 Xcode 命令行工具

```bash
# 确保安装了 Xcode 命令行工具
xcode-select --install

# 编译
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 使用 Xcode IDE

```bash
# 生成 Xcode 项目
cmake -B build -G Xcode
open build/loong-pixelgrab.xcodeproj
```

### 通用二进制 (Universal Binary)

```bash
# 同时支持 arm64 和 x86_64
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build
```

### 构建产物位置

```
build/
├── libpixelgrab.dylib         # 动态库
├── basic_capture              # 示例程序
└── PixelGrab                  # 截图/标注/贴图工具
```

---

## Linux 编译

### 安装依赖

```bash
# Debian/Ubuntu
sudo apt update
sudo apt install build-essential cmake git \
  libx11-dev libxext-dev libcairo2-dev libpango1.0-dev

# Fedora
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake git libX11-devel libXext-devel cairo-devel pango-devel
```

### 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### 使用 Ninja (更快)

```bash
sudo apt install ninja-build  # 或 sudo dnf install ninja-build

cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### 构建产物位置

```
build/
├── libpixelgrab.so            # 动态库
├── basic_capture              # 示例程序
└── PixelGrab                  # 截图/标注/贴图工具
```

---

## 构建产物

编译成功后的产物汇总：

| 平台 | 动态库 | 导入库/符号链接 |
|------|--------|----------------|
| Windows | `pixelgrab.dll` | `pixelgrab.lib` |
| macOS | `libpixelgrab.dylib` | — |
| Linux | `libpixelgrab.so` | — |

### 运行示例

```bash
# Windows (在 build/Release/ 目录中)
.\PixelGrab.exe

# macOS / Linux (在 build/ 目录中)
./PixelGrab
```

### 运行测试

```bash
# 首先启用测试编译
cmake -B build -DPIXELGRAB_BUILD_TESTS=ON
cmake --build build --config Release

# 运行测试
cd build
ctest --output-on-failure -C Release
```

---

## 在你的项目中集成

### 方式 1: CMake add_subdirectory

将 loong-pixelgrab 放入你的项目目录 (如 `third_party/loong-pixelgrab/`)：

```cmake
# 你的 CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(my_app LANGUAGES CXX)

# 不需要编译示例和测试
set(PIXELGRAB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PIXELGRAB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(third_party/loong-pixelgrab)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE pixelgrab)
```

### 方式 2: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
  pixelgrab
  GIT_REPOSITORY <repository-url>
  GIT_TAG        v1.0.0
  GIT_SHALLOW    TRUE
)
set(PIXELGRAB_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PIXELGRAB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(pixelgrab)

target_link_libraries(my_app PRIVATE pixelgrab)
```

### 方式 3: 预编译库

1. 编译 loong-pixelgrab 得到动态库和头文件
2. 将以下文件复制到你的项目中：
   - `include/pixelgrab/pixelgrab.h` — 头文件
   - `pixelgrab.dll` + `pixelgrab.lib` (Windows)
   - `libpixelgrab.dylib` (macOS)
   - `libpixelgrab.so` (Linux)
3. 在你的构建系统中链接

```cmake
# 手动指定路径
add_executable(my_app main.cpp)
target_include_directories(my_app PRIVATE path/to/pixelgrab/include)
target_link_libraries(my_app PRIVATE path/to/pixelgrab/lib/pixelgrab)
```

### 头文件使用

只需包含一个头文件即可使用全部 76 个 API：

```c
#include "pixelgrab/pixelgrab.h"
```

C 和 C++ 均可直接使用 — API 使用 `extern "C"` 导出。

---

## 常见问题

### Q: 首次编译很慢？

首次编译时 CMake FetchContent 需要从 GitHub 下载 spdlog (和可选的 Google Test)。后续编译会使用缓存，速度正常。

如果网络受限，可手动下载依赖源码放入 `build/_deps/` 目录。

### Q: Windows 上找不到编译器？

确保安装了 Visual Studio 2019 或更高版本，并勾选了 **"C++ 桌面开发"** 工作负载。推荐使用 "Developer PowerShell for VS" 或从 Visual Studio 内直接打开 CMake 项目。

### Q: Linux 上缺少头文件？

确保安装了所有必要的开发包：

```bash
sudo apt install libx11-dev libxext-dev libcairo2-dev libpango1.0-dev
```

### Q: 如何只编译库而不编译示例？

```bash
cmake -B build -DPIXELGRAB_BUILD_EXAMPLES=OFF
```

### Q: 如何编译静态库？

当前版本仅支持动态库 (SHARED)。如需静态库支持，请提交 Issue。

### Q: 运行时找不到 DLL？

Windows 上需要确保 `pixelgrab.dll` 与可执行文件在同一目录，或位于系统 PATH 中。

Linux 上可以设置 `LD_LIBRARY_PATH`：

```bash
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:path/to/lib
./my_app
```

macOS 上使用 `DYLD_LIBRARY_PATH`：

```bash
export DYLD_LIBRARY_PATH=$DYLD_LIBRARY_PATH:path/to/lib
./my_app
```

### Q: 如何启用调试日志？

在代码中设置日志级别：

```c
pixelgrab_set_log_level(kPixelGrabLogDebug);  // 或 kPixelGrabLogTrace
```

也可以注册自定义日志回调：

```c
void my_log(PixelGrabLogLevel level, const char* msg, void* userdata) {
    printf("[pixelgrab] %s\n", msg);
}

pixelgrab_set_log_callback(my_log, NULL);
```
