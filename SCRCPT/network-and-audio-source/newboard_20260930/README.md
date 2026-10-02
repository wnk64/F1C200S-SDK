# RTL8723DS 新板候选（2026-09-30）

型号已纠正：用户实际采购RTL8723BS且确认引脚相同。此DS目录仅保留历史记录，最新候选见 `../newboard_rtl8723bs_20260930/README.md`，不要部署旧DS驱动/固件。

## 范围与现状

RTL8723DS 尚未焊接。现已准备 Wi-Fi 单线 SDIO 和蓝牙 UART H5 的驱动、固件、候选设备树及手动加载工具，均编译通过；不替换运行中的内核、DTB、AIC服务，不能宣称 WLAN 或蓝牙已实测。

最新组合候选见 [BLUETOOTH.md](BLUETOOTH.md)：内核配置提交b13545f，外置H5驱动提交1f1c430，tmux返回码0。Ubuntu原驱动树中 `bluetooth-artifacts/` 含组合候选。原Wi-Fi单独候选及下述构建记录保留为历史入口；当前内核树配置已开启serdev/HCIUART，应使用 `build-bluetooth.sh` 构建，而非只接受旧配置哈希的 `build.sh`。

原源码目录：`/home/wnk/LicheePi_Nano/third_party/rtl8723ds`。
目录在原工作树内：`f1c200s/newboard_20260930`。
使用真实 RTL8723D 驱动（SDIO IDs 024c:d723/d724），不是 staging/rtl8723bs。
Wi-Fi 固件由驱动源码内嵌。RTL8723DS 仅支持2.4GHz，不提供5GHz Wi-Fi。

## 原理图接线

|信号|接线|处理|
|---|---|---|
|SDIO CLK/CMD/D0|PC0/PC1/PC2|MMC1，一位总线，最高25MHz初始配置|
|SDIO D1/D2/D3|未接|不启用四位总线|
|WL_DIS#|TCA9555 P01，偏移1|低有效复位，mmc-pwrseq-simple|
|SUSCLK_IN|PCF8563 CLKOUT|32768Hz ext_clock|
|WL_DEV_WAKE_HOST|TCA9555 P00，偏移0|设备树记录IRQ；Wi-Fi使用SDIO带内中断，不启用带外唤醒|
|BT_DIS# / BT_HOST_WAKE_DEV / BT_DEV_WAKE_HOST|TCA P03 / P04 / P02|组合候选控制使能/设备唤醒，主机唤醒仅作输入|
|UART_IN / UART_OUT|PE7 TX / PE8 RX|组合候选启用UART2|
|UART_CTS|PE9 RTS|1.5M波特率，硬件流控|
|U2CTS|PE10|1.5M波特率，硬件流控|

TCA9555 A0/A1/A2 接地，当前地址0x20。已用放大图确认WLINT接P00，不是A2。厂商Wi-Fi驱动当前不读取该设备树IRQ作为带外唤醒，所以不宣称已实现休眠唤醒。

## 构建

基线内核提交 d7e9679，配置MD5 `21d91bd8757f7b2dbb20c8b0d5927a7a`，release严格5.7.1。已有 MMC_SUNXI、PWRSEQ_SIMPLE、CFG80211，无须修改内核配置。

在 Ubuntu 上用 tmux 承载构建，运行本目录 `bash build.sh`。脚本只构建原目录外置驱动和本目录 DTB，产物放 `artifacts/`。不得 clean/mrproper；不得把产物目录加入Git。构建后保持 tmux shell 并用数字返回码判断成功。

2026-09-30 已完成：源码提交 `865d5bf`；tmux数字返回码0；模块vermagic=`5.7.1 mod_unload ARMv5 p2v8`。
模块MD5 `865feaabcc2f1821707a91d9dde61563`，DTB MD5 `c0507192bb648018e595cd5d35768eb3`。
完整日志/产物/源码bundle：`/home/wnk/F1C200S_archives/rtl8723ds_candidate_20260930`。
这是编译通过的候选，不是实板验证通过的稳定版。

## 焊接后调通顺序（未执行）

1. 确认模块供电与CLKOUT。
2. 经SSH保存当前DTB的回退路径与哈希，安装候选DTB，重启。
3. 查看 MMC1 和 `/sys/bus/sdio/devices`，确认024c:d723或d724枚举；尚未枚举不要反复加载Wi-Fi驱动。
4. 经SSH传输模块与 `load-wifi.sh`，校验哈希后运行 `sh load-wifi.sh /候选路径/8723ds.ko`。
5. 检查 `rtlwlan0` 并连接2.4GHz网络；不停止当前AIC管理联网，不擅自更改默认路由。
6. 完成 WLAN、共享I2C/RTC、显示和AIC回归后，再讨论自动加载与蓝牙启用。

## 蓝牙当前配置

`bluetooth.config` 已应用于原内核树的候选配置，板端尚未应用。外置H5驱动加入 `realtek,rtl8723ds-bt` 匹配和管理PCF8563时钟引用；完整源码和固定来源固件均Git跟踪。TCA GPIO使用cansleep接口；使能P03、设备唤醒P04、主机唤醒输入P02。固件配置已检查为1.5M波特率/CTS-RTS流控，实板兼容性仍待验证。

本候选不处理蓝牙唤醒/休眠，也不作投屏稳定基线归档。
