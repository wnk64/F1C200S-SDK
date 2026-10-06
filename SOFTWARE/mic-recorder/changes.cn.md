# MIC 录音适配

2026-10-07 计划：基线 1f2b7d9，使用原 Buildroot GCC6.4、原 LVGL8 和菜单显示/触摸平台。
原理图 MIC1 经 C39 1uF 接 MICIN，VCC 经 R16 1k 偏置。运行 Codec 已有 capture PCM；ADC Mixer Mic Capture Switch 原来关闭。
仅新增按需运行的录音应用，不改内核、DTB、模块、蓝牙/耳机自动路由或联网配置。
16kHz/S16_LE/单声道 WAV 流式写 SD /root/roms/Recordings；每分钟1.83MiB，最长1小时。
录音前保存所有6个ADC输入开关、ADC音量与MIC增益；只选MIC，ADC=3、Boost=1(24dB)，结束恢复。
回放使用 aplay -D default；退出停止回放并释放资源。提供 --record 秒数 输出文件 的可复用诊断入口。
新应用回退只需移除菜单新增项；现有应用二进制保持不变。不创建额外备份、工作树或常驻服务。
真实声音/界面/播放回归和三次冷启动未测，不称稳定验收。
