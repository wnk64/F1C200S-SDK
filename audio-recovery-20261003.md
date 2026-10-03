# wiliwili 蓝牙音频故障恢复候选

原源码LVGL/wiliwili_lvgl，原分支candidate/f1c200s-fbdev-20261002，基线f7422a3，原播放器37785e3252a4396d6e3bd1202d9d683a5e79455650bcc0dde9fcdd75c38bb911。保护原cedar源码、Miracast播放器、蓝牙驱动和rootfs音频路由；不新建工作树。

实测：BlueALSA BT socket timeout后播放器PCM Broken pipe/No such device，音频线程退出而audio_clock_active未清除，视频等待停止更新的音频时钟。播放器还处于SIGSTOP全线程停止，OSD不可处理显示命令；GUI仍运行。H5校验和乱序错误与重连接同时出现，但日志不足以确定驱动根因，不能因这轮失败否认之前稳定投屏。

计划：对照投屏恢复策略，仅改应用wrapper。非阻塞PCM写、最长500ms等待、失败回退耳机并三秒后重试默认设备；检查audio-route.state切换，重新打开输出。音频线程不因失效PCM直接退出；audio_clock超过750ms不更新则视频解除同步等待。暂停使用现有OSD socket命令8/9协作暂停解码/音频/预读，不冻结OSD与退出线程；暂停释放PCM，恢复重开，不保留断开的蓝牙句柄。停止使用drop代替无界drain，仍保留主机两秒停止超时回收。

待构建与实机自动测试：暂停/控制显示/恢复、跨循环、蓝牙断连重连及运行中切换、退出回菜单。每个测试探针显式开启，不带入正式启动环境；三次冷启动及完整回归前不称稳定。保留旧loop二进制和入口，单独部署候选后再切入口。
