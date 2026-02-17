# PixelGrab 发布流程文档

本文档描述 PixelGrab 的版本管理机制、构建打包流程和发布到 GitHub 的完整步骤。

---

## 目录

- [版本管理架构](#版本管理架构)
- [环境准备](#环境准备)
- [脚本一览](#脚本一览)
- [日常开发：手动构建](#日常开发手动构建)
- [版本号管理](#版本号管理)
- [一键发布](#一键发布)
- [手动发布（逐步执行）](#手动发布逐步执行)
- [自动更新检测](#自动更新检测)
- [打包产物说明](#打包产物说明)
- [版本号规范](#版本号规范)
- [常见问题](#常见问题)

---

## 版本管理架构

项目采用**单一数据源**策略——版本号只需在根 `CMakeLists.txt` 中维护，构建时自动传播到所有位置：

```
CMakeLists.txt (VERSION 1.2.0)
    │
    ├──→ configure_file → build/include/pixelgrab/version.h  (自动生成)
    │        │
    │        ├──→ include/pixelgrab/pixelgrab.h  (#include version.h)
    │        │        └──→ pixelgrab_version_string() API
    │        │
    │        └──→ examples/resources/app.rc  (#include version.h)
    │                 └──→ EXE 文件属性中的版本号
    │
    ├──→ CPack 变量 → NSIS 安装包 / ZIP 包版本号
    │
    └──→ update_checker.cpp → 与 GitHub Releases 比对版本
```

**关键文件**：

| 文件 | 作用 |
|------|------|
| `CMakeLists.txt` | 唯一的版本号来源 (`project(... VERSION x.y.z)`) |
| `include/pixelgrab/version.h.in` | 版本头文件模板 |
| `build/include/pixelgrab/version.h` | 构建时自动生成的版本头（不要手动编辑） |

---

## 环境准备

### 必需工具

| 工具 | 用途 | 安装 |
|------|------|------|
| CMake >= 3.16 | 构建系统 | https://cmake.org/download/ |
| Visual Studio 2022+ | C++ 编译器 (MSVC) | https://visualstudio.microsoft.com/ |
| PowerShell 5.1+ | 发布脚本内部使用（无需配置执行策略） | Windows 自带 |
| GitHub CLI (`gh`) | 创建 GitHub Release 并上传产物 | https://cli.github.com/ |

### 可选工具

| 工具 | 用途 | 安装 |
|------|------|------|
| NSIS | 生成 Windows 安装程序 (.exe) | https://nsis.sourceforge.io/ |
| Git | 版本控制（发布脚本可在无 git 环境下运行） | https://git-scm.com/ |

> 如未安装 NSIS，打包脚本会自动回退为仅生成 ZIP 包。

---

## 脚本一览

所有脚本位于 `scripts/` 目录下：

| 脚本 | 用途 | 说明 |
|------|------|------|
| `scripts/release.py` | **一键发布（跨平台）** | 版本升级 → 构建打包 → 发布到 GitHub（推荐） |
| `scripts/bump_version.bat` | 版本号管理 (Windows) | 修改 `CMakeLists.txt` 中的版本号 |
| `scripts/release.bat` | 一键发布 (Windows) | Windows 专用 bat 版本 |
| `scripts/build_and_package.bat` | 构建打包 (Windows) | CMake 配置 → 编译 → 安装 → CPack 打包 |

---

## 日常开发：手动构建

### 方式一：使用打包脚本（推荐）

```bat
:: Release 构建 + 打包
.\scripts\build_and_package.bat

:: Debug 构建（不打包）
.\scripts\build_and_package.bat Debug

:: Release 构建 + 打包（显式指定）
.\scripts\build_and_package.bat Release

:: 清理构建目录
.\scripts\build_and_package.bat clean
```

### 方式二：直接使用 CMake

```bat
:: 配置
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DPIXELGRAB_BUILD_EXAMPLES=ON

:: 编译
cmake --build build --config Release --parallel

:: 安装到 staging 目录
cmake --install build --config Release --prefix build/install

:: 打包（NSIS 安装包 + ZIP 便携包）
cd build && cpack -C Release -G NSIS && cpack -C Release -G ZIP
```

### 构建产物位置

```
build/
├── bin/Release/
│   ├── PixelGrab.exe          ← 可执行文件
│   └── pixelgrab.dll          ← 动态库
├── lib/Release/
│   └── pixelgrab.lib          ← 导入库
├── include/pixelgrab/
│   └── version.h              ← 自动生成的版本头
└── packages/
    ├── PixelGrab-1.1.0-win64.exe  ← NSIS 安装包
    └── PixelGrab-1.1.0-win64.zip  ← 便携 ZIP 包
```

---

## 版本号管理

### 使用 bump_version.bat 脚本

```bat
:: Bug 修复版本（patch）: 1.0.0 → 1.0.1
.\scripts\bump_version.bat patch

:: 功能更新版本（minor）: 1.0.0 → 1.1.0
.\scripts\bump_version.bat minor

:: 重大版本（major）: 1.0.0 → 2.0.0
.\scripts\bump_version.bat major

:: 指定具体版本号
.\scripts\bump_version.bat 2.1.0
```

脚本只修改 `CMakeLists.txt` 中的 `VERSION` 字段。重新构建后，版本号自动传播到：
- `version.h` (C/C++ 宏定义)
- `app.rc` (EXE 文件属性)
- CPack 安装包文件名和元数据
- 运行时 `pixelgrab_version_string()` 返回值

### 手动修改

也可以直接编辑 `CMakeLists.txt` 第 3 行：

```cmake
project(loong-pixelgrab
  VERSION 1.2.0          ← 修改此处
  DESCRIPTION "Cross-platform screen capture library"
  LANGUAGES C CXX
)
```

---

## 一键发布

### 使用 release.bat

`release.bat` 自动执行完整的发布流程，**同时支持有 git 和无 git 两种环境**。

#### 常用命令

```bat
:: 发布 patch 版本（最常用）: 1.1.0 → 1.1.1
.\scripts\release.bat patch

:: 发布 minor 版本: 1.1.0 → 1.2.0
.\scripts\release.bat minor

:: 发布 major 版本: 1.1.0 → 2.0.0
.\scripts\release.bat major

:: 发布指定版本号
.\scripts\release.bat 2.0.0

:: 使用自定义 Release Notes
.\scripts\release.bat patch --notes build\release-notes.md

:: 跳过构建（使用已有的打包产物）
.\scripts\release.bat patch --skip-build

:: 预览模式（不实际执行，只显示将要做的操作）
.\scripts\release.bat patch --dry-run
```

#### 参数说明

| 参数 | 类型 | 说明 |
|------|------|------|
| 第一个参数 | `major`/`minor`/`patch`/版本号 | 递增版本号的哪一部分，或直接指定如 `2.0.0` |
| `--skip-build` | 开关 | 跳过构建步骤，使用 `build/packages/` 中已有的安装包 |
| `--notes` | 路径 | 自定义 Release Notes 的 Markdown 文件路径 |
| `--dry-run` | 开关 | 预览模式，显示所有操作但不执行 |

#### 执行流程

脚本根据是否存在本地 git 仓库自动选择流程：

**有 git 仓库时（6 步）：**

```
预检查 → 版本升级 → 构建打包 → Git Commit + Tag + Push → 生成 Release Notes → 创建 GitHub Release
```

| 步骤 | 操作 | 说明 |
|------|------|------|
| 预检查 | 环境验证 | 检查 `gh` 是否安装并已登录；检查工作区是否干净 |
| 1 | 版本升级 | 调用 `bump_version.bat` 修改 `CMakeLists.txt` |
| 2 | 构建打包 | 自动终止运行中的 PixelGrab 进程，调用 `build_and_package.bat Release` |
| 3 | Git Commit & Tag | `git commit` + `git tag -a vx.y.z` |
| 4 | Git Push | 推送代码和标签到远程仓库 |
| 5 | 生成 Release Notes | 从 git log 自动生成，或使用 `--notes` 指定文件 |
| 6 | GitHub Release | `gh release create` 上传安装包和 ZIP |

**无 git 仓库时（4 步）：**

```
预检查 → 版本升级 → 构建打包 → 生成 Release Notes → 创建 GitHub Release
```

| 步骤 | 操作 | 说明 |
|------|------|------|
| 预检查 | 环境验证 | 检查 `gh` 是否安装并已登录 |
| 1 | 版本升级 | 调用 `bump_version.bat` 修改 `CMakeLists.txt` |
| 2 | 构建打包 | 自动终止运行中的 PixelGrab 进程，调用 `build_and_package.bat Release` |
| 3 | 生成 Release Notes | 使用 `--notes` 指定文件，或自动生成默认内容 |
| 4 | GitHub Release | `gh release create --repo ... --target main` 上传安装包和 ZIP |

> 无 git 时，GitHub 仓库地址从 `CMakeLists.txt` 中的 `PIXELGRAB_GITHUB_REPO` 自动读取。

#### 智能特性

- **自动终止进程**：构建前自动检测并终止正在运行的 `PixelGrab.exe`，避免 LINK 锁文件错误
- **已有 Release 处理**：如果目标 tag 的 Release 已存在，自动删除并重新创建
- **包文件智能匹配**：优先匹配当前版本的包文件，找不到时回退匹配所有 `PixelGrab-*.exe/zip`
- **仓库地址自动读取**：从 `CMakeLists.txt` 中 `PIXELGRAB_GITHUB_REPO` 读取，无需硬编码

#### 前置条件

1. **GitHub CLI 已登录**：运行 `gh auth login` 完成授权
2. **（可选）本地 git 仓库**：如有则自动提交和推送；没有也能直接发布到 GitHub

---

## 手动发布（逐步执行）

如果不使用一键脚本，可以手动执行每个步骤：

### 有 git 仓库

```bat
:: 1. 递增版本号
.\scripts\bump_version.bat patch
:: 假设新版本为 1.1.1

:: 2. 构建并打包
.\scripts\build_and_package.bat Release

:: 3. 提交版本变更
git add CMakeLists.txt
git commit -m "chore: bump version to 1.1.1"

:: 4. 打标签
git tag -a v1.1.1 -m "Release 1.1.1"

:: 5. 推送到远程
git push
git push --tags

:: 6. 创建 GitHub Release（附带安装包）
gh release create v1.1.1 ^
    --title "PixelGrab 1.1.1" ^
    --notes-file build\release-notes.md ^
    build\packages\PixelGrab-1.1.1-win64.exe ^
    build\packages\PixelGrab-1.1.1-win64.zip
```

### 无 git 仓库

```bat
:: 1. 递增版本号
.\scripts\bump_version.bat patch

:: 2. 构建并打包
.\scripts\build_and_package.bat Release

:: 3. 准备 Release Notes（可选，写入 Markdown 文件）
:: 编辑 build\release-notes.md

:: 4. 直接发布到 GitHub（需指定 --repo 和 --target）
gh release create v1.1.1 ^
    --repo Yangqihu0328/loong-pixelgrab ^
    --target main ^
    --title "PixelGrab 1.1.1" ^
    --notes-file build\release-notes.md ^
    build\packages\PixelGrab-1.1.1-win64.exe ^
    build\packages\PixelGrab-1.1.1-win64.zip
```

---

## 自动更新检测

PixelGrab 内置了基于 GitHub Releases API 的自动更新检测功能。

### 工作原理

1. **启动时自动检查**：应用启动后在后台线程查询 GitHub API
2. **手动检查**：系统托盘右键菜单 → "检查更新"
3. **关于对话框**：点击"是"按钮检查更新

### 检测流程

```
PixelGrab 启动
    │
    └──→ 后台线程: GET https://api.github.com/repos/{owner}/{repo}/releases/latest
            │
            ├── 解析 tag_name (如 "v1.2.0")
            ├── 与当前版本 (PIXELGRAB_VERSION_STRING) 比较
            │
            ├── 有新版本 → 弹窗提示，点击"是"打开浏览器下载
            └── 已是最新 → 静默（手动检查时显示提示）
```

### 配置 GitHub 仓库

在 `CMakeLists.txt` 中设置：

```cmake
set(PIXELGRAB_GITHUB_REPO "Yangqihu0328/loong-pixelgrab" CACHE STRING
    "GitHub owner/repo for update checking")
```

或在构建时覆盖：

```bat
cmake -S . -B build -DPIXELGRAB_GITHUB_REPO="your-org/your-repo"
```

> 此变量同时被 `release.bat` 读取，用作 `gh release create --repo` 的值。

### 网络与代理

更新检测使用 Windows WinHTTP API，默认行为：

1. **自动检测系统代理**：优先使用 `WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY`，自动读取系统代理设置（包括 IE/系统代理、PAC 脚本等）
2. **回退机制**：若自动检测不可用（如旧版 Windows），回退为 `WINHTTP_ACCESS_TYPE_DEFAULT_PROXY`
3. **无代理环境**：直连 `api.github.com`，若网络可达则正常工作；若不可达则静默失败，不影响应用正常使用

> **注意**：在完全无法访问 GitHub 的网络环境中（如受限内网且无代理），自动更新检测将不会生效，但不影响应用其他功能。

### 发布新版本后用户体验

1. 用户运行旧版 PixelGrab
2. 启动时检测到 GitHub 上有新版本
3. 弹窗提示："发现新版本 v1.2.0！点击"是"打开下载页面。"
4. 用户点击"是" → 浏览器打开 GitHub Release 下载页面
5. 用户下载新版安装包，运行安装覆盖旧版

---

## 打包产物说明

### Windows

| 格式 | 文件名示例 | 说明 |
|------|-----------|------|
| NSIS 安装包 | `PixelGrab-1.1.0-win64.exe` | 标准 Windows 安装程序，含开始菜单/桌面快捷方式、卸载程序 |
| ZIP 便携包 | `PixelGrab-1.1.0-win64.zip` | 免安装便携版，解压即用 |

### 安装包内容

```
PixelGrab/
├── bin/
│   ├── PixelGrab.exe      ← 主程序
│   └── pixelgrab.dll      ← 核心动态库
└── Uninstall.exe          ← 卸载程序 (仅 NSIS)
```

### NSIS 安装包特性

- 开始菜单快捷方式
- 桌面快捷方式
- "添加/删除程序"中可卸载
- 安装前自动卸载旧版本
- 安装完成后可选自动启动应用程序

---

## 版本号规范

项目遵循 [语义化版本 (Semantic Versioning)](https://semver.org/lang/zh-CN/) 规范：

```
MAJOR.MINOR.PATCH
  │      │     │
  │      │     └── 修订号：向后兼容的 Bug 修复
  │      └──────── 次版本号：向后兼容的功能新增
  └─────────────── 主版本号：不兼容的 API 变更
```

### 何时递增

| 场景 | 递增 | 示例 |
|------|------|------|
| Bug 修复、性能优化 | `patch` | 1.0.0 → 1.0.1 |
| 新增功能（不破坏现有 API） | `minor` | 1.0.0 → 1.1.0 |
| API 变更、重大架构调整 | `major` | 1.0.0 → 2.0.0 |

### Git 标签规范

- 标签格式：`v{MAJOR}.{MINOR}.{PATCH}`（如 `v1.2.0`）
- 标签为 annotated tag（带注释），消息为 `Release x.y.z`

### Commit 消息规范

发布脚本的 Changelog 生成器根据 commit 消息前缀自动分类：

| 前缀 | 分类 | 示例 |
|------|------|------|
| `feat:` | 新功能 | `feat: add blur annotation tool` |
| `fix:` | Bug 修复 | `fix: correct DPI scaling on 4K displays` |
| `docs:` | 文档 | `docs: update build guide` |
| `refactor:` | 重构 | `refactor: simplify capture pipeline` |
| `chore:` | 杂项（自动忽略） | `chore: bump version to 1.2.0` |

---

## 常见问题

### Q: 修改版本号后忘记重新构建，EXE 版本号不对？

版本号通过 CMake `configure_file` 在配置阶段生成。只要运行 `cmake --build` 就会自动检测 `CMakeLists.txt` 变更并重新配置。

### Q: NSIS 未安装，如何生成安装包？

`build_and_package.bat` 会自动回退为仅生成 ZIP 包。如需 NSIS 安装程序，从 https://nsis.sourceforge.io/ 下载安装，确保 `makensis.exe` 在 PATH 中。

### Q: 构建时报 LNK1104 无法打开 exe/dll 文件？

通常是因为 PixelGrab 正在运行，文件被锁定。解决方法：

- **使用 `release.bat`**：脚本会自动终止运行中的进程
- **手动终止**：`taskkill /f /im PixelGrab.exe`

### Q: 如何修改 GitHub 仓库地址？

两种方式：
1. 修改 `CMakeLists.txt` 中的 `PIXELGRAB_GITHUB_REPO` 默认值
2. 构建时传参：`cmake -DPIXELGRAB_GITHUB_REPO="org/repo" ...`

> `release.bat` 会自动从 `CMakeLists.txt` 读取此值。

### Q: release.bat 提示 "gh is not installed"？

安装 GitHub CLI：https://cli.github.com/，安装后运行 `gh auth login` 完成授权。

### Q: release.bat 提示 "GitHub CLI is not authenticated"？

运行 `gh auth login`，按提示完成 GitHub 账号授权。验证状态：`gh auth status`。

### Q: 没有 git 仓库能发布吗？

可以。`release.bat` 自动检测 `.git` 目录是否存在：
- **有 git**：完整流程（commit → tag → push → release），共 6 步
- **无 git**：简化流程（bump → build → release），共 4 步，通过 `--repo` 和 `--target main` 直接发布

### Q: 发布时目标 tag 已存在怎么办？

`release.bat` 会自动检测并删除已有的同名 Release，然后重新创建。

### Q: 如何只构建不发布？

```bat
:: 只构建打包，不推送到 GitHub
.\scripts\build_and_package.bat Release
```

### Q: 如何回退版本号？

```bat
.\scripts\bump_version.bat 1.0.0
```

### Q: 自动更新检查没有反应 / 检测不到新版本？

可能原因及排查步骤：

1. **网络不通**：确认能访问 `https://api.github.com`。如使用代理，确保系统代理已正确配置（控制面板 → Internet 选项 → 连接 → 局域网设置）
2. **仓库地址错误**：确认 `CMakeLists.txt` 中 `PIXELGRAB_GITHUB_REPO` 设置为正确的 `owner/repo`（当前为 `Yangqihu0328/loong-pixelgrab`）。如在构建时通过 `-D` 参数覆盖过，检查 `build/CMakeCache.txt` 中缓存值是否正确
3. **CMake 缓存残留**：如果修改了 `PIXELGRAB_GITHUB_REPO`，需清理缓存或重新配置：
   ```bat
   cmake -S . -B build -DPIXELGRAB_GITHUB_REPO="Yangqihu0328/loong-pixelgrab"
   ```
4. **GitHub Release 的 tag 格式**：Release 的 tag 必须为 `v{MAJOR}.{MINOR}.{PATCH}` 格式（如 `v1.0.1`），否则版本比较会失败

### Q: NSIS 安装完成后应用程序没有自动启动？

安装向导最后一页有"运行 PixelGrab"复选框（默认勾选）。如果勾选了但仍未启动，检查 `CMakeLists.txt` 中的 `CPACK_NSIS_MUI_FINISHPAGE_RUN` 是否设置为 `"PixelGrab.exe"`（不要包含 `bin\\` 前缀，NSIS 模板会自动添加）。
