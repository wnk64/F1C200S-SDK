# Windows LVGL 构建与验证（2026-09-30）

## 2026-09-30 独立候选

新候选位于 `D:/AITools/build/wiliwili_optimized_20260930/wiliwili_lvgl.exe`，不覆盖常用路径或正在运行的窗口。原生条带只更新对应 SDL 纹理区域；正常实例不再保留额外整帧，隐藏审计仍保留截图副本。浅色主题改为绘制前设置对象调色板，不再按精确色值修改已抗锯齿的字体、封面或二维码。FPS 指标统计实际 UI 呈现次数，而非主循环次数。

评论每个视频最多保存 24 条，作者最多 96 字节、正文最多 1,024 字节，按 UTF-8 边界裁剪。追加响应同时校验导航代次、视频 BVID、索引和分页；失败不推进页码，空页/到达上限停止继续加载。HTTP GET/POST 共享有界接收逻辑，查询失败、读取失败、已宣布字节未读完或结束确认失败不再被当作完整响应。

32 位与 RGB565 Release、CTest、软件/GPU 隐藏 UI 与真实 SDL 纹理读回校验均通过；最新候选还通过虚构 QR/登录/Dynamic 及 H.264 本地回归，RGB565 JPEG/WebP 封面对齐专项通过。测试不读取真实 Cookie，不发送全局输入，不启动实际板卡；测试进程退出时只关闭自建实例。详细结果与候选 SHA256 见源码根目录变更记录的 2026-09-30 条目。

按照后面的构建环境设置后可重复执行：

```powershell
cmake -S D:/AITools/workspaces/wiliwili_source/windows_lvgl -B D:/AITools/build/wiliwili_optimized_20260930 -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build D:/AITools/build/wiliwili_optimized_20260930 --parallel 6
ctest --test-dir D:/AITools/build/wiliwili_optimized_20260930 --output-on-failure
& ./windows_lvgl/ui_optimization_regression.ps1 -Iterations 3 -SoftwareRendering
```

独立 RGB565 宿主验证可在另一个构建目录配置 `-DWILIWILI_WINDOWS_COLOR_DEPTH=16`，再运行同一单元和隐藏 UI 回归。此选项仅改变 Windows 宿主颜色深度，仍使用 FreeType 和 512 KiB LVGL 池；不是 ARM/Buildroot 编译或真实板卡验收。新的刷新和封面契约见 `docs/PLATFORM_BUILDROOT_PORT.md`。

## 本机环境

- 工具实际安装于 `D:/AI工具/msys64`；`D:/AITools` 是同一目录的 ASCII junction。
- `D:/AITools/workspaces/wiliwili_source` 指向当前源码目录，不是源码副本或新 worktree。
- 独立构建目录为 `D:/AITools/build/wiliwili_ascii`，不复用搬迁前的缓存。
- 所需 MSYS2 mingw64 包：gcc、cmake、ninja、pkgconf、SDL2、SDL2_image、freetype。

中文路径会导致 pkg-config 输出编码和 GNU 汇编/链接路径失败。仅给构建目录起 ASCII 名称不够；工具和源码也要通过 ASCII 链接访问。GNU 链接器还需要以下进程级环境变量。

## PowerShell 构建

```powershell
$env:Path = 'D:\AITools\msys64\mingw64\bin;' + $env:Path
$env:GCC_EXEC_PREFIX = 'D:/AITools/msys64/mingw64/lib/gcc/'
$env:COMPILER_PATH = 'D:/AITools/msys64/mingw64/bin;D:/AITools/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.2.0'
$env:LIBRARY_PATH = 'D:/AITools/msys64/mingw64/lib;D:/AITools/msys64/mingw64/lib/gcc/x86_64-w64-mingw32/16.2.0'
cmake -S D:/AITools/workspaces/wiliwili_source/windows_lvgl -B D:/AITools/build/wiliwili_ascii -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build D:/AITools/build/wiliwili_ascii --parallel 6
```

升级 GCC 后应同步更新环境变量中的版本目录。

## 运行

扫码登录：从普通 Windows 窗口进入 Mine，点击 Show QR，使用哔哩哔哩手机 App 扫码并在手机上确认；也可直接启动 `wiliwili_lvgl.exe --qr-login`。2026-09-27 修复二维码显示却没有启动轮询的问题：轮询密钥在交给 UI 后原先被 `std::move` 清空；现保留本地轮询密钥并显示获取二维码的 HTTP/API 错误。扫码后底部应显示 waiting for scan 或 confirm on phone，不要对同一二维码反复扫码；遇到生成失败或过期，回到 Mine 重新点击 Show QR 获取新码。不要公开保存在用户目录中的 Cookie 文件或二维码。

扫码修复版起初位于 `D:/AITools/build/wiliwili_qr_fix/wiliwili_lvgl.exe`。2026-09-27 登录后导航保护已同步重新构建到常用路径 `D:/AITools/build/wiliwili_ascii/wiliwili_lvgl.exe`；自行关闭旧窗口再启动新的进程即可，不能靠刷新当前窗口热更新。用户此前确认真实扫码成功；这次导航保护仍只通过隔离虚构账户验证，没有读取或修改真实 Cookie，也未声称所有线上账户页面已验收。

已登录 Mine/Dynamic/Inbox 页面原先将“返回空列表或失败”当作每次重绘都应立即重试，可能持续产生后台请求和重绘。现在每次进入/切换选项卡仅自动拉取一次；结果为空时显示空状态，点击相应导航或选项卡可以重试。`windows_lvgl/qr_login_regression.ps1` 用固定虚构 Cookie 和离线响应验证扫码状态、空数据稳定、Home/Dynamic/Mine/Inbox/Settings 导航；另用 `ui_regression.ps1` 两轮验证未登录布局、播放器返回与封面对齐。证据在构建目录 `wiliwili_qr_fix/qr_all_pages_validated_20260927/` 与 `wiliwili_qr_fix/ui_logged_in_fix_20260927/`。

```powershell
& D:/AITools/build/wiliwili_ascii/wiliwili_lvgl.exe --board-preview
& D:/AITools/build/wiliwili_ascii/wiliwili_lvgl.exe --board-settings
```

800x480 是 Windows 横屏预览的逻辑与物理客户区，不代表板端 480x800 已移植。全屏渲染使用 SDL 逻辑画布映射显示和鼠标坐标。截图程序也必须使用 DPI-aware 线程获取窗口尺寸，否则 175% DPI 下会误裁切。

## 后台自动回归

H.264 输出接口与独立媒体回归见 `docs/H264_OUTPUT_API.md`；构建同时生成 `wiliwili_h264_output_test.exe`，可通过 `windows_lvgl/h264_regression.ps1` 后台验证，区别于下面的 UI 样例回归。

播放内存采样工具为 `windows_lvgl/h264_memory_audit.ps1`；组件实测、软件/GPU 后端区别及板端预算边界见 `docs/PLAYBACK_MEMORY_AUDIT.md`。`WILIWILI_UI_AUDIT_SOFTWARE=1` 仅对隐藏实例选择软件 SDL 渲染，不改变正常窗口。审计元数据附带 LVGL 当前堆、采样最高堆和碎片率，不能将它们作为总应用 RAM。

```powershell
& ./windows_lvgl/ui_regression.ps1 -Iterations 10 -OutputDirectory D:/AITools/build/wiliwili_ascii/release_regression_20260926
```

从源码根目录运行。脚本生成离线 JSON 与封面样例，通过 `ui_audit.ps1` 启动隐藏实例并向其专用消息通道注入 SDL 输入，读取 LVGL 帧缓冲截图。无需移动全局鼠标、发送全局按键或激活窗口。配置、日志、截图和 `results.json` 仅写入指定构建输出目录；退出时关闭测试实例。

- 仅 `WILIWILI_UI_AUDIT=1` 启用测试通道，必须设置独立 `WILIWILI_UI_AUDIT_DIR`；样例模式由 `WILIWILI_UI_AUDIT_FIXTURES` 指定。
- 样例模式未知 GET 使用空样例，不回退到真实网络；测试模式禁止 POST、打开外部网址和外部播放器。
- 回归检查：详情/播放器返回与首页头部恢复、搜索滚轮、重叠分页去重后的四张新增封面、晚到请求不能覆盖 Settings；设置选项卡与未登录导航保存截图供检查。
- 封面颜色断言允许每通道最多 7 的量化差异，并排除已复现的旧错误封面；不以像素完全相等误判图片解码量化。
- 紧凑设置页固定选项卡、仅内容纵向滚动，Playback 与 Interface 的下部控件可访问。回归新增滚动/回顶部/切页复位断言；Other 页检查所有行与单行代理输入框无需滚动即可完整显示。UI scale、Keymap、Theme 按钮明确标注用途。
- 2026-09-26 布局优化最终 Release 连续 10 轮通过，包括新增设置滚动与无溢出断言；截图和日志位于 `D:/AITools/build/wiliwili_ascii/layout_acceptance_20260926/`。独立隐藏实例另验证底部 A/B 切换按钮可点击且仅写隔离配置，切换后可恢复。

## 当前验证边界

- 修复首页预览封面缺失：已知 `i0/i1/i2.hdslb.com` 图片请求沿用上游 wiliwili 的 `@256w_144h_1c.jpg` 缩图路径，不再直接下载被低内存解码尺寸限制拒绝的大原图。保留 128 KiB 下载限额与原图解码保护；离线 CDN URL/封面颜色专项与超大 PNG 拒绝回归通过。不同图片 CDN、真实在线请求仍需人工验收。

- Release 已成功构建；去掉 MinGW PATH 后启动进程仍存活，但独立运行尚未逐页验收。
- 175% DPI 下实际客户区已核对为 800x480；已捕获推荐首页与中文缩略图。
- 隐藏实例已可靠触发卡片点击；RelWithDebInfo 离线连续 10 轮详情/播放器返回及搜索滚轮、分页封面对齐、晚到请求和分区导航检查通过。离线样例不是实际视频播放、音画同步或完整在线 API 验收。
- 2026-09-26 续测：Release 重新构建成功，同样 10 轮回归全部通过；证据位于 `D:/AITools/build/wiliwili_ascii/release_regression_20260926/`。设置页和分页底部截图已人工检查；分区导航仅证明动作后实例存活及截图成功，不代表各分区业务内容全部正确。
- 板端配置预处理已核对：RGB565、192 KiB 内存池、禁用 FreeType。不代表板端链接或实机运行成功。
- 当前 Windows 使用单个 16 行原生绘制条带：800x480 时 32 位为 51,200 字节、16 位为 25,600 字节；正常实例已移除共享完整 ARGB 呈现缓存，隐藏审计副本、缩略图和平台库仍须单独计入总内存。
- 同一 LVGL 刷新周期只在最后一个块呈现一次，每个块只上传对应纹理区域，不再重复上传全屏。
- 推荐/搜索列表上限为 32 条；去重后视频与封面同步追加，换页前失效图片缓存，失败解码释放压缩数据。此上限限制累计数据，不代表总内存已有板端验收。
- 实测截图位于构建目录下 `audit_20260926/`，不纳入源码。无扫码登录、账户写操作或板端部署。
- 在线搜索曾触发服务端限流，已停止重复在线请求；当前回归使用离线样例。隐藏帧缓冲截图不代替原生窗口全屏/DPI 验收，登录、账户接口、真实播放及板端 RAM/FPS 仍需单独验证。
