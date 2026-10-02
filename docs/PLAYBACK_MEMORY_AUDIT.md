# 播放内存审计（2026-09-26）

## 测量方法与边界

`windows_lvgl/h264_memory_audit.ps1` 为可重复后台工具，使用 Windows `GetProcessMemoryInfo` 采集当前工作集、私有提交及系统记录的进程峰值，约每 100 毫秒采样。输出 `memory_summary.json`、`memory_samples.json`、`ui_stages.json`。样例为 800x480/25 fps/300 帧的本地 H.264 MP4；生成器和 PowerShell 审计宿主不计入被测进程。

三个测试场景分开运行：

- 隐藏 LVGL 首页→详情→播放器页面→返回，检查进程和 LVGL 堆。此模式禁止实际外部播放，是 UI 生命周期测量。
- API 宿主通过 4 KiB 小块、延迟消费者读完样例，采集宿主及其 FFmpeg 供流子进程。
- API 二进制 stdout → ffplay stdin，SDL dummy 后台软件解码；分别比较自动线程与单线程。不是 Cedar 硬解测量。

工作集包含共享/映射页面，私有提交不是板端 RSS。以下峰值来自不同进程及独立场景，**不能相加冒充同时占用，也不能直接折算成板端内存**。脚本发现并按所属父进程采集 FFmpeg，不混入其他应用的播放器；进程清理只针对本次实例。

## 实测结果

| 被测组件/场景 | 峰值私有提交 MiB | 峰值工作集 MiB |
| --- | ---: | ---: |
| API 宿主，受背压的复制 | 1.04 | 4.77 |
| FFmpeg 供流子进程 | 17.13 | 16.43 |
| ffplay 默认自动线程 | 60.10 | 57.00 |
| ffplay 单线程 | 32.28 | 31.90 |
| UI，默认 GPU 审计，6 轮 | 91.80 | 95.46 |
| UI，显式软件渲染审计，6 轮 | 12.05 | 27.69 |

单线程 ffplay 减少约 46% 私有提交峰值，媒体回归仍通过。软件 UI 的 12 轮测量中，返回后私有提交约 11.36～11.47 MiB，返回后的 LVGL 堆始终为 29,032 字节；新计数器复核的 6 轮 GPU/软件 UI 中，首页、详情、播放页分别约 28.35、22.73、25.82 KiB。该场景未见 LVGL 控件堆持续增长。

GPU UI 的进程增长并未伴随 LVGL 堆增长，软件后端也没有同样的大幅增长，说明本样例主要差异与桌面渲染后端有关；这不是已经定位/修复了驱动泄漏的证明。`WILIWILI_UI_AUDIT_SOFTWARE=1` 只对隐藏审计实例生效，普通 Windows 窗口默认渲染器未改变。

此 LVGL 版本的 `lv_mem_monitor().max_used` 没有填充，不能将零值当作历史峰值。审计通道改为维护并明确命名 `LvglHeapSampledPeakBytes`，只代表请求采样时的最高值，不捕捉每次渲染内部的全部临时峰值。

## 已落实的资源限制

实测证据位于构建目录 `h264_memory_baseline_20260926/`、`h264_memory_optimized_20260926/`、`ui_memory_software_verified_20260926/` 与 `ui_memory_gpu_verified_20260926/`；格式/尺寸回归位于 `memory_png_dimensions_20260926/`、`memory_jpeg_20260926/`、`memory_webp_20260926/` 与 `memory_bmp_final_20260926/`。2048x2048 PNG 压缩文件仅 21,288 字节，尺寸上限测试仍正确拒绝。

- 输出管道请求容量从 64 KiB 降为 16 KiB；验证器默认读取块从 32 KiB 降为 8 KiB，保留 API 最大 64 KiB 的兼容性。管道容量是 Windows 请求值，不代表实测内核总分配。
- FFmpeg 输入探测设置为 64 KiB、最多 32 个探测包、1 秒分析时长和单线程。样例中的供流进程仍约 17.1 MiB，**没有测到明显下降**；这些限制主要约束较大探测/排队的潜在开销，不降低外部工具的全部固定成本。
- 后台 ffplay 回归使用单线程及小探测窗口，不将 Windows 软件播放器作为板端实现。
- 封面 HTTP 单张上限 128 KiB，每个请求批次的压缩缓冲容量预算 1 MiB。并发任务先预留配额，完成后按实际 vector 容量扣账；失败下载释放数据，换页后停止启动旧批次的新封面任务。此预算不包括分配器开销、HTTP 内部缓冲或其他并行请求批次。
- Windows 封面解码前检查 PNG/JPEG/BMP/WebP 尺寸，每边最多 2048，总像素最多 512 Ki 像素；避免小压缩文件触发大幅全尺寸图像分配。超限或无法识别格式显示占位。像素上限不保证所有第三方解码器的总堆占用，也不替代板端的受限解码实现。
- `ui_regression.ps1 -OversizedCover` 验证压缩大小上限；`-LargeCoverDimensions -CoverFormat Png` 使用小于 128 KiB、2048x2048 的 PNG 验证尺寸拒绝。JPEG/WebP 正常分页封面与布局另做回归。

## 板端内存预算注意项

| 项目 | 已知大小/约束 | 状态 |
| --- | --- | --- |
| Windows 单 16 行绘制条带 | 50 KiB | 同步刷新路径已实现；原双 32 行为 200 KiB |
| RGB565 单 16 行绘制条带 | 25 KiB | 原双 32 行为 100 KiB；不是板端 DMA 验收 |
| LVGL 池 | Windows 512 KiB；板端配置 192 KiB | 预处理配置已存在，不代表全部应用堆 |
| 共享应用完整 ARGB 影子帧 | 正常实例不再分配；隐藏审计保留 1,536,000 字节（800x480） | 2026-09-30 改为原生条带上传；SDL/驱动自身纹理不计为已消除 |
| 32 张 186x104 ARGB 封面像素 | 2,476,032 字节 | 当前共享模型上限，另有列表/文本/压缩数据 |
| 同样 32 张 RGB565 封面像素 | 全不透明时 1,238,016 字节；全有透明度时 1,857,024 字节 | 2026-09-30 按真实透明度选择原生布局；Windows 16 位验证不等于板端解码验收 |
| 800x480 YUV420 单帧 | 576,000 字节，不含对齐和辅助缓冲 | 尺寸计算，不是硬解实际分配 |
| Cedar 参考帧/旋转/显示缓冲 | 必须按播放器和 SPS/参考帧实际分配测量 | 尚未验收 |

新 H.264 输出 API 不读取 `PLAYER_INMEMORY_CACHE`。UI 中现有 64/128 MiB 缓存选项不是接口当前分配量；板端 FIFO 适配不能直接照搬此桌面配置。板端应采用受限拉流/解封装→短 FIFO→已有 Cedar 播放器，不把 SDL、FreeType、ffplay 或 Windows FFmpeg 程序直接移植过去。

目前没有板端运行进程、内核/系统占用或硬解缓冲的实测，**不能承诺已经适合具体容量的小内存平台**。必须在完整可复现的板端播放器基线上补测 UI、拉流、FIFO、Cedar/CMA/显示缓冲及系统余量。

已修复与低内存图像限制有关的预览封面缺失：已知 B站图片 CDN 在下载前请求上游项目使用的 256x144 JPEG 缩图，再按 186x104 显示；非该 CDN 的 URL 不做改写，现有压缩大小与解码尺寸限制不放宽。离线 URL/颜色断言通过，实际在线图片 CDN 尚未验收。

## 2026-09-26 条带候选（历史实测）

同步 flush 在返回前完成颜色复制并通知 LVGL，当前路径不使用异步 DMA，因此改为单 16 行绘制缓冲。800x480 下 32 位绘制条带从 200 KiB 降到 50 KiB，16 位从 100 KiB 降到 25 KiB。以后若接入异步传输，必须重新设计缓冲所有权，不能提前调用 flush_ready。更小条带增加 flush 调用次数，当前回归没有测量板端吞吐量。

刷新改用 lv_color_to32，避免 RGB565 整数误当 ARGB；封面接口 platform_decode_thumbnail_lvgl 输出 LV_IMG_CF_TRUE_COLOR_ALPHA 对应原生像素。32 位继续为 4 字节，16 位为 RGB565 两字节加 alpha 一字节，保留透明度，32 张封面较 ARGB 少 619,008 字节。SDL 源图和缩放临时面仍为 ARGB，尚未减少这部分解码瞬时峰值。

Release 构建、三轮 UI 返回及 PNG 超尺寸/滚轮/追加封面对齐/异步导航/设置布局回归通过。六轮软件 UI 审计峰值私有提交 11.69 MiB、工作集 27.33 MiB；与前次 12.05/27.69 MiB 相比略低，进程峰值变化不能全部归因于条带调整。RGB565 main.cpp 与实际封面转换函数分别通过编译语法检查，未进行完整板端链接或画面验收。证据位于构建目录 lvgl_strip_regression_20260926/ 和 lvgl_strip_memory_20260926/。

## 复现命令

2026-09-30 新增 `windows_lvgl/ui_optimization_regression.ps1`：在隔离隐藏实例中检查暂停播放页的实际呈现计数、浅色/深色/自动主题切换、封面颜色不被主题改写，以及反复创建页面后的 LVGL 堆；同时读回实际 SDL 渲染纹理，与 LVGL 审计像素逐像素比较（每通道允许 7 的量化差异）。呈现计数不等于硬件视频 FPS；暂停样例不启动真实解码器。新增 CTest 单元测试覆盖 10,000 次相同标签更新、评论累计上限/旧请求身份、HTTP 查询或读取失败/短读/超限/结束确认，以及透明和不透明 PNG 的原生像素布局。

正常窗口的额外整帧和浅色映射画布均已移除。封面仍最多保留 32 张，不宣称已经实现可见区域按需解码；不透明 RGB565 的每像素 alpha 去除减少存储，SDL 原图解码/缩放瞬时峰值仍在。较早审计数字是旧候选实测，不能当作新版本或板端测量。

```powershell
& ./windows_lvgl/h264_memory_audit.ps1
& ./windows_lvgl/h264_memory_audit.ps1 -SoftwareUi -UiIterations 12 -OutputDirectory D:/AITools/build/wiliwili_ascii/h264_memory_optimized_20260926
& ./windows_lvgl/h264_memory_audit.ps1 -UiOnly -SoftwareUi -OutputDirectory D:/AITools/build/wiliwili_ascii/ui_memory_software_verified_20260926
```

默认 UI 样例目录来自此前 `h264_network_ui_20260926/fixtures`，可用 `-FixtureDirectory` 指向一次 `ui_regression.ps1` 生成的样例。`-SkipUi` 只测媒体；`-UiOnly` 只测 UI。工具不常驻，退出后关闭自建实例，无全局输入或窗口激活操作。
