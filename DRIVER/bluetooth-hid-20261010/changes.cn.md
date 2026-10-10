# 蓝牙键鼠离线适配候选

2026-10-10 基线038fcee，原工作树修改，不新增工作树、不访问板子。原菜单platform.c固定打开event0，忽略REL与键盘；改为按设备能力分类，触摸仍保留FT6236直读兜底。
计划：有界evdev读取，键鼠独立LVGL输入，鼠标光标、默认焦点组、Tab切换/Enter操作/方向键/英文与数字输入。热插拔重扫，不依赖event编号；不添加常驻服务。
内核当前BT_HIDP/UHID关闭，但完整#250源码/config/vmlinux/System.map/zImage均在。必读的Desktop/F1C200S_build_commands.md缺失，遵守kernel基线技能，暂不自行猜测完整构建命令、不改现用.config，先保存最小候选fragment与验收要求。
本轮只保证共享平台组件编译与模拟输入测试，不能宣称蓝牙已能配对、所有应用均支持键鼠。独立ONS/wiliwili与视频OSD需要单独接入/验证。
