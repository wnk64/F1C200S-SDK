# 新板FT6236外置适配候选

原理图：newboard_i2c_20260930/schematic-peripherals.png及schematic-soc.png，2026-09-30版本。
用户确认FT6236；I2C地址0x38，板端i2cget读A3=0x64。
六针FPC：1 VCC、2 CTRST、3 I2C1_SCL、4 CTINT、5 I2C1_SDA、6 GND。
I2C1 SCL=PE11/SDA=PE12；CTRST=PCA/TCA9555 P05，CTINT=P06，扩展器INT经PE5。
Linux现有总线编号i2c-0，扩展器0-0020、gpiochip标签0-0020。

直接使用当前完整5.7.1源码drivers/input/touchscreen/edt-ft5x06.c，另存edt_ft6236.c，
不修改触摸协议算法；原内核配置未启用CONFIG_TOUCHSCREEN_EDT_FT5X06。
ft6236_board.c只提供软件设备属性、GPIO lookup、嵌套IRQ及I2C设备实例。
IRQ不用固定动态GPIO编号，按扩展器label及硬件offset6解析。
复位使用active-low lookup offset5。内核共享input/touchscreen属性符号已导出。
尺寸先按原生480×800，坐标方向需实触四角验证，不能拿固定尺寸代替校准。
2026-10-02用户四角数据确认原始坐标800×480。现使用X=479-rawY/Y=rawX，
驱动软件属性先反转原Y再交换XY，evdev最终范围480×800；GUI轮询后备同映射，
evdev路径不重复转换。实测点变换落在四个按钮内，待用户确认点击与滑动。

候选不修改zImage、.config或启动DTB，也不改变扩展器其他GPIO及现有显示路径。
手动验证时先插入edt_ft6236.ko，再插入ft6236_board.ko。
回退先rmmod ft6236_board，再rmmod edt_ft6236，释放设备、GPIO及IRQ。
不自动加载到开机服务；标准/dev/input/eventN确认坐标后再接LVGL。
菜单进程退出时关闭evdev读取句柄；驱动小额常驻开销单独测量，不能称菜单全部内存。

构建用原内核树/原Linaro7.2.1，ARCH=arm、LOCALVERSION=；严格vermagic5.7.1。
改动保留在原外置panel仓库子目录ft6236_20261002，不新建开发工作树。
当前只作为候选，需触摸事件、父/子IRQ、输入范围、I2C其他设备、投屏/蓝牙与内存回归，
最终三次冷启动验收前不提升为稳定。
