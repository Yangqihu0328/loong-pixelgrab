# Changelog

本文件记录 PixelGrab 各版本的主要变更。

---

## v1.3.0

### Highlights

- **工程质量全面提升**：错误处理标准化、输入验证加固、依赖可选化、测试扩展、API 文档配置、安全加固。
- **开发体验升级**：C++ RAII Wrapper、性能基准测试框架、CI/CD 自动化流水线。

### New Features

- **OCR/翻译依赖可选化**：新增 `PIXELGRAB_ENABLE_OCR`（默认 ON）和 `PIXELGRAB_ENABLE_TRANSLATE`（默认 ON）CMake 选项。禁用时编译 stub 后端，库体积更小且无需安装 Tesseract 或 WinHTTP 依赖。
- **Doxygen API 文档**：新增 `docs/Doxyfile` 配置，可通过 `cd docs && doxygen Doxyfile` 生成完整的 HTML API 参考文档。
- **C++ Header-only Wrapper (`pixelgrab.hpp`)**：零依赖的 C++17 RAII 封装层，提供 `Context`、`Image`、`ImageView`、`Annotation`、`PinWindow` 五个核心类，自动管理资源生命周期（构造即创建、析构即释放），失败操作以 `pixelgrab::Error` 异常报告错误码和消息。同时封装颜色工具函数 `to_hsv`/`to_rgb`/`to_hex`/`from_hex`。
- **性能基准测试框架**：新增 `pixelgrab_bench` 可执行目标，包含屏幕截图、区域截图、取色、放大镜、颜色转换、窗口枚举、Context 创建/销毁等 9 项基准测试，输出 avg/min/max 毫秒耗时。通过 `cmake --build build --target pixelgrab_bench` 构建。
- **CI/CD GitHub Actions 流水线**：
  - `ci.yml`：push/PR 触发，Windows (MSVC) + macOS (AppleClang) + Linux (GCC) 三平台矩阵构建与测试，支持 OCR/Translate ON/OFF 组合。
  - `release.yml`：tag 推送触发，三平台自动打包并创建 GitHub Release。

### Improvements

- **错误处理标准化**：所有 API 失败路径现在均通过 `pixelgrab_get_last_error()` / `pixelgrab_get_last_error_message()` 报告详细错误信息。Annotation、Pin Window、Clipboard 相关 API 补全了此前缺失的错误状态设置。
- **输入验证加固**：Annotation 形状要求正值尺寸，Pencil 限制最大点数（100,000），Mosaic/Blur 要求正值区域和参数，Magnifier 增加 radius 上限校验（[1, 500]）及输出尺寸溢出保护（16384 像素上限），Image 创建增加维度溢出保护（256 MB 上限）。
- **消除魔法数字**：将硬编码的默认值提取为命名常量——录屏默认帧率、码率、音频采样率、HTTP 超时、Pin 窗口默认尺寸、合成器等待时间等。
- **线程安全文档**：在 `pixelgrab.h` 公共头文件中添加完整的线程安全说明，明确各 handle 类型的并发访问规则和推荐使用模式。
- **API 生命周期文档**：为返回指针的 API 补充了明确的生命周期说明。
- **安全加固**：OCR tessdata 路径增加路径穿越防护，BGRA→灰度转换增加维度溢出检查，放大镜输出增加尺寸上限保护。
- **测试扩展**：新增 `test_ocr.cpp`（7 个测试）、`test_translate.cpp`（11 个测试）、`test_validation.cpp`（18 个测试），覆盖 OCR/翻译 API 和 Phase 1 新增验证逻辑。测试总数由 203 → 239。

---

## v1.2.0

### Highlights

- **OCR + 翻译**：集成 Tesseract 5 跨平台 OCR 引擎 + 百度翻译 API，截图后一键识别文字并翻译。

### New Features

- **OCR 文字识别（Tesseract 5）**：集成 Tesseract 5.5.2 开源 OCR 引擎，替代原有的 Windows.Media.Ocr (WinRT) 实现。支持中文简体（chi_sim）和英文（eng）识别，使用 tessdata_best 高精度模型（~28MB）。BGRA→灰度手动转换，无需 OpenCV 依赖。tessdata 路径自动检测（exe 同级目录 / TESSDATA_PREFIX 环境变量）。单一 `TesseractOcrBackend` 全平台通用，替代原先 3 个平台特定实现（win/mac/linux）。
- **翻译功能（百度翻译 API）**：OCR 识别后一键翻译，自动检测文本语言方向（CJK→英文 / 其他→中文）。基于模板方法模式实现 `TranslateBackend` 抽象层，平台子类只需实现 `HttpPost()` 和 `ComputeMD5()`。Windows 平台使用 WinHTTP + Wincrypt 实现。翻译结果自动复制到剪贴板。配置文件 `pixelgrab.cfg` 管理百度翻译 API 密钥。新增 3 个 C API：`pixelgrab_translate_set_config`、`pixelgrab_translate_is_supported`、`pixelgrab_translate_text`。

### Improvements

- **翻译错误诊断增强**：翻译失败时 MessageBox 显示具体错误原因（百度 API 错误码、HTTP 状态码、WinHTTP 网络错误等），不再只显示笼统的"翻译失败"。错误链路从 `TranslateBackend` → `pixelgrab_translate_text` → UI 完整传递。
- **打包脚本适配 OCR/翻译**：CMake install 规则新增 20 个 Tesseract/Leptonica 第三方 DLL、tessdata 模型目录、`pixelgrab.cfg.example` 翻译配置模板的安装，确保 NSIS 安装包和 ZIP 分发包可直接运行。
- **错误码扩展**：新增 `kPixelGrabErrorTranslateFailed` 错误码，精确报告翻译操作失败原因。

---

## v1.1.0

### Highlights

- **项目开源**：loong-pixelgrab 完整源码正式开源，包含跨平台截图库（Windows / macOS / Linux）、标注引擎、录屏模块及示例桌面应用。

### New Features

- **像素放大镜 API**：新增 `pixelgrab_get_magnifier()` 接口，以指定坐标为中心采样并放大像素区域（支持 2–32 倍），采用最近邻缩放算法，适用于颜色拾取器等精确取色场景。
- **颜色拾取器增强**：拾色窗口新增 160×160 像素放大镜实时预览，中心像素高亮（黑框 + 白框双层标识），2×2 网格线辅助定位；支持 Shift 键切换 RGB / HEX 显示格式，Ctrl+C 一键复制颜色值。
- **文字水印**：新增 `pixelgrab_watermark_apply_text()` 接口，支持自定义字体、大小、颜色（ARGB）、6 种位置预设（四角 / 居中 / 自定义坐标）、边距与旋转角度，跨平台实现（Windows GDI+ / macOS CoreText / Linux Cairo+Pango）。
- **图片水印**：新增 `pixelgrab_watermark_apply_image()` 接口，支持将 Logo 或签名图片叠加到截图上，可控制叠加位置与透明度（Alpha 混合）。
- **颜色格式转换工具**：新增 RGB ↔ HSV 互转（`pixelgrab_color_rgb_to_hsv` / `pixelgrab_color_hsv_to_rgb`）、RGB ↔ HEX 互转（`pixelgrab_color_to_hex` / `pixelgrab_color_from_hex`，支持 #RGB、#RRGGBB、#RRGGBBAA 格式）。

### Improvements

- **录屏水印双轨架构**：系统水印与用户自定义水印独立配置，互不干扰；用户水印以黑字白边样式、-25° 旋转角度、5 个实例随机入场飘移循环显示，GPU 与 CPU 双渲染路径均支持。
- **水印平台支持检测**：新增 `pixelgrab_watermark_is_supported()` 接口，运行时查询当前平台是否支持水印渲染。
- **错误码扩展**：新增 `kPixelGrabErrorWatermarkFailed` 错误码，精确报告水印操作失败原因。

### Bug Fixes

- **修复菜单截图与 F1 快捷键行为不一致**：从系统托盘右键菜单点击"截图"现在与 F1 快捷键行为完全一致，均显示顶部工具栏（截图/录屏/OCR）并进入选区模式。
- **防止独立录屏期间误触发截图**：补齐录屏保护条件（`!g_standalone_recording`），防止在独立录屏过程中误触截图操作。
---

## v1.0.7

### Bug Fixes

- **修复菜单截图与 F1 快捷键行为不一致**：修复从系统托盘右键菜单点击"截图"时直接进入选区模式而不显示 F1 工具栏的问题。现在菜单截图与 F1 快捷键行为完全一致，均会显示顶部工具栏（截图/录屏/OCR）并进入选区模式，支持在工具栏中切换模式。同时补齐了录屏中保护条件（`!g_standalone_recording`），防止在独立录屏期间误触发截图。

---

## v1.0.6

### New Features

- **录屏自定义水印**：录屏设置对话框新增"自定义水印"分组，可启用并输入自定义水印文字。水印以黑字白边样式、-25° 旋转角度显示在录屏画面中，5 个实例随机入场、从右上向左下慢速飘移，循环环绕。系统水印保持不变。水印设置通过注册表持久化保存。
- **"关于"对话框展示联系与赞赏二维码**：系统托盘右键菜单"关于"从简单的 MessageBox 升级为自定义 GDI+ 对话框，居中展示联系二维码。

### Improvements

- **录屏水印双轨架构**：系统水印与用户水印独立配置，互不干扰。GPU 与 CPU 双渲染路径均支持。
- **关于对话框保留检查更新功能**：新对话框底部提供"检查更新"和"关闭"按钮。

---

## v1.0.5

### Bug Fixes

- **安装程序在文件复制前终止运行中的进程**：将 `taskkill` 从安装后移至安装前，确保在覆盖文件之前进程已被终止，解决安装时因程序正在运行导致文件被占用无法覆盖的问题。
- **安装时先卸载旧版本再安装**：完整流程为：检测旧版本 → 卸载 → 安装前 kill → 复制新文件。

---

## v1.0.4

### Bug Fixes

- **安装程序自动终止运行中的进程**：NSIS 安装包在安装和卸载时自动执行 `taskkill` 终止正在运行的 PixelGrab.exe。
- **构建脚本自动终止运行中的进程**：`build_and_package.bat` 在构建前自动检测并终止正在运行的 PixelGrab.exe。
- **录屏绿框显示时机修正**：选定录屏区域后绿色边框立即显示在录制设置对话框阶段。

---

## v1.0.3

### New Features

- **标注模式下支持拖拽调整选区大小**：选定区域进入标注模式后，边框显示 8 个拖拽控制点，可随时拖拽调整选区范围。
- **统一选区与标注流程**：点击窗口选取或拖拽绘制选区后直接进入标注模式，无需额外确认步骤。

### Improvements

- **拖拽控制点视觉反馈**：选区/标注边框上绘制 8 个白底黑边方形控制点。
- **动态调整光标**：鼠标悬停到控制点时自动切换为对应方向的调整大小光标。
- **拖拽实时预览**：拖拽调整区域大小时实时显示新区域边框。

### Bug Fixes

- **选区操作不再穿透到背景程序**：通过低级键盘钩子拦截 Enter/Escape 按键。
- **标注模式四周暗色遮罩缺失**：标注模式下现在全程保持四周暗色半透明遮罩。
- **调整大小后全屏闪烁**：通过离屏移动画布窗口和控制 DWM 合成顺序实现无闪烁切换。
- **录屏绿框选区后消失**：选区完成后绿框立即显示并贯穿整个设置与倒计时阶段。

---

## v1.0.2

### Bug Fixes

- **F1 选区模式下收到更新提醒导致鼠标和键盘失效**：在选区/标注/录屏等模式下自动延迟更新弹窗，退出后再显示。

---

## v1.0.0

- 初始发布
