# 无 SDL SFC 模拟器

采用现存Retro-Go Snes9x轻量核心，上游32bb2913，47个核心文件与许可证已完整纳入Git b2ee8b2，核心不改。基线97ce5cd，不新增工作树。
新建独立LVGL/sfc_native，游戏画面直接写framebuffer，LVGL仅绘制控制与浏览器，不依赖SDL、不使用硬解或GPU。屏幕竖向480×800，游戏画面上部等比例适配，下面提供方向/A/B/X/Y/L/R/Start/Select及暂停/退出。
32kHz立体声走现有ALSA default，采用有界非阻塞队列；不更改蓝牙或耳机服务。键盘方向键，Z/X映射B/A，A/S映射Y/X，Q/W映射L/R，Enter/Tab映射Start/Select，Esc退出。板载触摸仍是单点，不能声称触摸同时按方向与动作键。
支持未压缩.sfc/.smc与目录选择，SRAM保存于同名.srm，不接入旧版不安全snapshot格式。前端ROM大小先做边界校验，但古老核心不能视为恶意ROM沙箱。
板子不在，先ARM构建、边界测试与自己编写的最小LoROM执行测试。不下载商业ROM，不宣称实机流畅或游戏兼容，三冷及实机显示/音频/输入待测。
