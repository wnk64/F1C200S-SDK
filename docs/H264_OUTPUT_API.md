# H.264 输出 API（2026-09-26）

## 目标与边界

数据流为：已解析的视频 URI → H.264 解封装 → Annex-B 字节流 API → 调用方写 FIFO → 现有硬解播放器。

公共接口为 `windows_lvgl/src/h264_output.h`，已由 `platform_services.h` 引入。Windows 参考实现构建为 `wiliwili_h264_output` 静态库，主程序和独立验证器均链接此库。接口不依赖 LVGL、SDL 或 Windows 类型，不自行创建板端 FIFO、不调用板端解码器、不转码、不缩放，也不替换现有 UI 的播放器按钮行为。

Windows 参考实现通过独立 FFmpeg 进程执行 video stream copy，使用 `h264_mp4toannexb`，删除音频/字幕/数据流。FFmpeg 必须明确提供路径或已位于 PATH；不再依赖旧播放器实现中硬编码的 `C:/msys64` 路径。此进程是 Windows 验证后端，**不是已经完成的 Buildroot 后端，也不要求未来板端安装桌面 FFmpeg**。

输入是本地文件或已解析的 HTTP(S) 视频地址，不是 BV 号或网页 URL。当前 `main.cpp` 的 `parse_play_source` 仍有编码回退逻辑；调用方应选择 AVC，输出后端会拒绝非 H.264，而不会为了成功返回而软件转码。

## 接口约定

- `platform_create_h264_output()`：创建独立实例。
- `open(options, error)`：停止旧流并启动新源。成功仅代表生产进程启动，编码/源文件校验错误通过后续 `read` 返回。
- `read(buffer, capacity, bytes_read, timeout_ms, error)`：返回 `Data / Timeout / End / Error`。容量为 1～65536 字节；等待为 0～120000 毫秒，0 是立即轮询。超时不是 EOF；非 Data 状态的 `bytes_read` 为 0。
- `stop()`：终止生产进程，释放管道，丢弃剩余数据；重复调用安全，可再次 `open`。析构同样清理，子进程绑定 kill-on-close job，避免调用进程退出后残留 FFmpeg。
- 同一实例由一个工作线程拥有并调用；不要在 LVGL 线程执行读取或写 FIFO，也不要并发 `read/stop`。调用方以短读取超时响应自己的停止标志。
- `http_headers` 支持 Referer/User-Agent 等头部，默认提供 B站请求头；不自动导入账户 Cookie。网络读写超时默认 10 秒，可配置 1～120000 毫秒。URI、工具路径、头部有长度上限。

**输出是连续的 Annex-B 字节流，不是“一次 read 一帧”。** 起始码、NAL 或帧可跨多个 read；FIFO 消费端必须保留跨块解析状态。SPS/PPS 随 Annex-B 输出，回归验证包含 SPS/PPS/IDR。裸流不附带容器 PTS/DTS、音频或应用私有包头，也没有内置实时发送节拍。若播放器要求帧长度头、时间戳或固定发送帧率，需明确其协议后再添加适配，不能直接假定原 FIFO 兼容。

Windows 输出管道请求大小为 16 KiB，诊断管道为 4 KiB，诊断尾部保留最多 4 KiB；验证器默认读取块为 8 KiB，停止读取后管道自然背压，不在应用中累计整段视频。输入探测限制为 64 KiB/32 包/1 秒，并采用单线程。此限制不代表 FFmpeg 自身内存消耗，也不是板端 RAM 验收结果。进程及 UI 的内存实测见 `docs/PLAYBACK_MEMORY_AUDIT.md`。

## 调用示例

在媒体工作线程中使用：

```cpp
H264OutputOptions options;
options.source_uri = resolved_video_uri;
options.ffmpeg_executable = ffmpeg_path;
auto output = platform_create_h264_output();
std::string error;
if (!output->open(options, error)) return;
std::array<uint8_t, 32768> buffer;
while (!stop_requested()) {
    size_t received = 0;
    const auto status = output->read(buffer.data(), buffer.size(), received, 100, error);
    if (status == H264ReadStatus::Timeout) continue;
    if (status != H264ReadStatus::Data) break;
    if (!write_all_to_fifo(buffer.data(), received)) break;
}
output->stop();
```

`write_all_to_fifo` 和 `stop_requested` 由调用方实现。FIFO 写端必须处理部分写、背压、消费者退出和停止请求；发生读取 Error 后已收到的数据可能只是截断视频，应把错误传给播放器控制层。停止/换源需要协调播放器清空旧 FIFO/解码状态，不能把新旧码流直接拼接。当前不提供 seek；重新 open 从源的开始位置重新供流。

## Windows 验证

从源码根目录运行：

```powershell
& ./windows_lvgl/h264_regression.ps1
```

工具默认位于 `D:/AITools/ffmpeg/ffmpeg-9.0.2-essentials_build/bin`；可通过 `-MediaDirectory` 覆盖。验证器为构建目录中的 `wiliwili_h264_output_test.exe`。

```powershell
& D:/AITools/build/wiliwili_ascii/wiliwili_h264_output_test.exe --source sample.mp4 --ffmpeg D:/AITools/ffmpeg/ffmpeg-9.0.2-essentials_build/bin/ffmpeg.exe --output sample.h264
```

`--stdout` 可输出二进制码流供播放器管道消费，不能同时使用 `--output`。**不要用 PowerShell 5.1 的文本管道转发二进制视频**；回归工具使用 .NET BaseStream 直接连接 API 验证器 stdout 与 ffplay stdin。

回归生成 800x480/25 fps/50 帧 H.264 MP4，检查中文/空格路径、MP4→Annex-B、7 字节小块读、解码后逐帧 MD5 与源完全一致、SPS/PPS/IDR、只有视频且尺寸不变、停止/重开/无效参数、缺失源/工具、损坏输入、无视频输入以及 MPEG-4/HEVC/AV1 拒绝。

播放器消费测试是 **API 实时管道 → Windows ffplay（SDL dummy，无可见窗口）**；另用 FFmpeg 解码帧哈希证明输出内容可解码。该测试不是实际屏幕显示、音画同步、在线 B站可用性或 Cedar 硬解验收。板端 FIFO 和实际播放器尚未接入。

2026-09-26 最终 Release 验证通过，证据位于 `D:/AITools/build/wiliwili_ascii/h264_final_20260926/`，包含帧哈希、码流、探测信息、生产者/播放器日志与 `results.json`。另验证单帧短源及五次完整输出字节哈希，链接新库后的三轮 UI 回归也通过。测试进程全部退出。

## 本机网络边界回归

```powershell
& ./windows_lvgl/h264_network_regression.ps1
```

脚本独立生成小型 H.264 样例，启动仅监听 `127.0.0.1` 随机端口的 `h264_http_fixture.py`，通过完整、慢速、截断和等待响应测试 API；不访问 B站或外部网络。可用 `-Source` 指定本地视频文件，`-Python` 覆盖已有 Python 路径。服务端与生产进程均为隐藏实例，finally 清理。

- 完整及慢速 HTTP 输出与本地 Annex-B 参考逐字节哈希一致；4 KiB 小块慢消费者也不丢字节。
- 校验 Referer/User-Agent 头部已传递，并支持样例服务器的 Range 响应。
- 2026-09-26 复现并修复 HTTP 中断误报正常结束：原后端在约 230 KiB 截断输出后仍返回 End；启用 FFmpeg `-xerror` 后，已报告的解封装错误会返回 Error。该机制不能保证发现没有长度/错误指示的所有裸流截尾。
- 1 秒 I/O 超时的本机样例约 1.26 秒返回；等待期间工作线程主动取消，约 327 毫秒完成。调用方仍通过短 read 超时检查停止标志，不支持同一实例跨线程并发 stop。
- 验证器拒绝输出覆盖输入及非法数字参数，避免误毁样例或超时参数整数回绕。测试用 `--cancel-after`、`--consumer-delay` 不增加 API 实时节拍或自动重连语义。

网络回归不代替 HTTPS 证书、真实 CDN、账户鉴权、完整在线播放、音视频同步或板端 FIFO/硬解验收。

最终证据位于 `D:/AITools/build/wiliwili_ascii/h264_network_acceptance_20260926/`。严格错误模式下原媒体回归和三轮 UI 回归均通过；回环服务器、FFmpeg、ffplay 与测试实例已全部退出。两个新增网络测试文件是保留的项目工具，不是后台常驻服务。
