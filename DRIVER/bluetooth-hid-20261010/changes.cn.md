# 蓝牙键鼠离线适配候选

2026-10-10 基线038fcee，原工作树修改，不新增工作树、不访问板子。原菜单platform.c固定打开event0，忽略REL与键盘；改为按设备能力分类，触摸仍保留FT6236直读兜底。
计划：有界evdev读取，键鼠独立LVGL输入，鼠标光标、默认焦点组、Tab切换/Enter操作/方向键/英文与数字输入。热插拔重扫，不依赖event编号；不添加常驻服务。
内核当前BT_HIDP/UHID关闭，但完整#250源码/config/vmlinux/System.map/zImage均在。必读的Desktop/F1C200S_build_commands.md缺失，遵守kernel基线技能，暂不自行猜测完整构建命令、不改现用.config，先保存最小候选fragment与验收要求。
本轮只保证共享平台组件编译与模拟输入测试，不能宣称蓝牙已能配对、所有应用均支持键鼠。独立ONS/wiliwili与视频OSD需要单独接入/验证。

## 离线结果

源码a2ff9e7，新增evdev-hid.h、tests/test_evdev_hid.c与持久test-hid.sh。按EV_KEY/EV_REL/EV_ABS能力区分键鼠与触摸，最多8个HID设备、64条按键和鼠标状态队列；每设备读取有上限，8ms内不重复灌入LVGL，1秒重扫热插拔。SYN_DROPPED/断联关闭该描述符并释放已按下状态。
独立LVGL鼠标和键盘设备，保留原触摸为第一个设备及I2C兜底；默认焦点组自动加入标准按钮/输入框，聚焦轮廓可见，Tab/ShiftTab与滚轮切换焦点，Enter及方向键按LVGL控件处理，英文/数字/常用符号支持Shift/Caps。快速点击按下/抬起逐条保留，不在一次读取中合并丢失。
ASan/UBSan及泄漏检查通过映射/快速点击/组合键/释放/断联/队列边界测试。菜单、媒体UI/浏览器、录音机原GCC6.4构建通过；媒体仅已有_GNU_SOURCE重复定义警告。发现录音机Makefile缺共享平台更新依赖，已改为构建前更新平台，避免链接旧输入代码。
当前BlueZ构建源码包含input/hog，原配对agent已有DisplayPinCode/DisplayPasskey；但板端未验证。内核仅新增BT_HIDP=m/UHID=m配置fragment，未改现用.config、zImage、模块、启动脚本或音频服务。
限制：相对坐标鼠标、基础美式英文键位，无中文输入法；滚轮切换焦点，不是所有应用的滚动；Esc/鼠标右键仅发LVGL取消键，非全局退出。独立ONS/wiliwili尚未接入，视频层光标遮挡与真实触摸回归尚未验证。
构建路径与SHA、回退见根WORKTREE_CHANGELOG.md。本轮无板端部署，不称蓝牙已适配完成、不称稳定版；需先恢复受控kernel构建记录，再生成HID模块并实测配对/自动重连/键鼠与蓝牙音响并用/三冷回归。
# 2026-10-10 键盘安全通道诊断（待验证）

Xbox 菜单操作已由用户确认有效；连续按键测试未收到键盘事件。键盘为 BLE HOG，Xbox 为传统 HIDP。键盘磁盘记录无 LongTermKey，内核出现 security requested but not available，抓包出现 BT_SECURITY Invalid argument。计划通过 BlueZ 重新开启适配器验证 audio-backend 中 hciconfig up 与 bluetoothd 初始化竞争的假设，不替换内核，不删除已有配对。适配器切换会短暂断开 Xbox，之后恢复。尚未进行冷启动验收。

