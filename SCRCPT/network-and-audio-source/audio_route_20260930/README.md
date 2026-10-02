# 新播放自动选择蓝牙或耳机

## 行为

用户确认只处理新打开的播放流，不迁移正在播放的声音。应用使用ALSA `default`：BlueALSA存在有效A2DP播放PCM时走蓝牙；音响断开、没有PCM、服务失败或无蓝牙控制器时回耳机Codec。不会把SCO、蓝牙录音PCM或仅设备配对视为音响可播放。多个音响可用时保持当前音响，否则采用确定顺序选择，不固定MAC地址。
录音和默认混音器仍用Codec。显式 `hw:Codec,0`、`hw:2,0` 或其他设备的应用不受默认选择影响；Cedar使用 `CEDAR_AUDIODEV=default` 才会跟随。断开时已经打开的蓝牙流不自动迁移，需应用关闭后重新打开。

运行时路由原子写入RAM中的 `/run/audio-route.conf`，只有变化时更新；同时更新全局asound.conf修改时间以让libasound后续打开PCM刷新缓存。`/etc/asound.conf`保留静态耳机兜底，因此开机服务尚未执行、运行时文件丢失时仍能用耳机。

## 文件与服务

- `/etc/init.d/S35audio-route`：开机启动、stop/start/restart和单实例检查。
- `/opt/f1-audio/f1-audio-route`：每秒通过BlueALSA结构化D-Bus接口查看A2DP播放PCM，调用有1秒超时。
- `/opt/f1-audio/audio-backend.sh`：有HCI才启动BlueZ/BlueALSA并启用控制器；不加载硬件驱动、不固定hci0、不自动配对陌生设备。
- `/opt/f1-audio/bluealsa`、`/usr/lib/libsbc.so.1`及BlueALSA控制插件：补齐现板缺少的依赖，原PCM插件哈希相同无需替换。
- `/run/audio-route.state`：显示 `headphone` 或 `bluetooth MAC`。

蓝牙模块尚未焊接，现在服务保持耳机；未来需先通过既有8723DS候选使HCI正常注册，再配对连接音响。没有部署蓝牙候选内核，也没有改变I2S禁用状态。

## 来源与验证

自研路由源及脚本在原RTL仓库 `f1c200s/audio_route_20260930`，实现提交5c9d473，缓存/单实例修正35be204。复制的D-Bus客户端来自完整BlueALSA源码82fdaea18130bf7b0c83a5e2566ddd4249901c82，保留MIT许可证，只将GetPCMs等待改为1秒。复用源码可追溯的BlueALSA2.1.0/SBC1.3运行文件，构建配置和完整来源bundle已保留。

选择器MD5：a172ec0fcaa4ba232ca73b90d95920a7。
asound.conf MD5：a3bd2f9ade5b0b4390b16261d9725bb0。
S35audio-route MD5：acb10974517210f41ee360861d858cfe。

板端选择逻辑自测通过；运行文件缺失时静态耳机兜底和服务恢复后兜底均通过48k双声道1秒静音PCM播放；重复start不重复创建进程，stop/start成功。选择器初版RSS约1452KB。不播放突发测试音，不修改当前耳机40/63音量。
实际蓝牙连接、断开切换和音响听音没有硬件条件，仍待验证；自测不冒充真实蓝牙测试。未做物理冷启动验收或稳定版GitHub推送。

## 回退

板端候选 `/root/aic_miracast/candidates/audio_route_20260930`，旧纯耳机asound.conf保存在 `rollback/asound.conf`（MD5 b38839aeeb2c132931546aec6877e74f）。回退先停止S35audio-route，恢复旧asound.conf，再移除本次新增S35audio-route和运行时路由文件；原S20headphone不动。库和BlueALSA文件保留也不会自动运行。
完整源码及构建/运行产物归档：`/home/wnk/F1C200S_archives/audio_route_20260930`。未改内核、设备树、AIC、投屏或播放器脚本。临时提交包装脚本已删除，构建/启动脚本作为项目工具保留。
