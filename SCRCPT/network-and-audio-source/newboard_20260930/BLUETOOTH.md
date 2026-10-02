# RTL8723DS 蓝牙预配置（未部署）

型号已纠正为RTL8723BS，此文保留历史；应使用 `../newboard_rtl8723bs_20260930/README.md` 的BS模块/固件/设备树组合，不能混用此处DS固件。

## 已完成

原内核树开启serdev、TTY控制器和HCIUART/H5/Realtek配置，提交b13545f；不新开工作树，不改任何稳定内核驱动源码。
原RTL8723DS树暂存完整Linux5.7.1 HCIUART/H5源码，提交1f1c430；外置适配加入8723DS匹配、PCF8563时钟生命周期和P02主机唤醒输入申请。
已通过tmux增量构建，数字返回码0，所有候选模块版本严格5.7.1。

设备树：UART2 PE7=TX、PE8=RX、PE9=RTS、PE10=CTS；TCA9555 P03为使能（0复位/1运行），P04设备唤醒，P02主机唤醒输入。PCF8563 CLKOUT=32768Hz由Wi-Fi和蓝牙共享引用。UART0控制台与Wi-Fi一位SDIO保持原配置。

固件来自固定Armbian提交，原文件保留未修改；校验文件哈希、配置签名和长度、UART条目确认1.5M波特率/硬件流控。详情在原驱动树 `bluetooth/firmware/PROVENANCE.md`。
P02尚未接入休眠唤醒IRQ处理；不声称低功耗或休眠唤醒完成。

## 候选路径与哈希

Ubuntu工作树：`/home/wnk/LicheePi_Nano/third_party/rtl8723ds/f1c200s/newboard_20260930`。
组合产物：`bluetooth-artifacts/`。
完整候选归档：`/home/wnk/F1C200S_archives/rtl8723ds_bt_candidate_20260930`，含两份完整源码bundle、配置、zImage、vmlinux、System.map、生成前置文件和日志。

|产物|MD5|
|---|---|
|zImage|75da971835ca3339b47eb649c2cf6697|
|Wi-Fi+BT DTB|7f26f842d7180355a6e07c398ecb8ef7|
|hci_uart.ko（外置）|63b62d1042dbacffdaec40acb1d83848|
|btrtl.ko|0761a0b2bd090aace818954b8254ca5e|
|8723ds.ko|865feaabcc2f1821707a91d9dde61563|

匹配的sunxi、phy-generic、Cedar/ION模块已一起重编和收集，哈希与#246相同。SHA256和完整模块清单在产物 `manifest.txt`。

## 构建命令

```sh
tmux new-session -d -s f1-rtl8723ds-bt-0930 bash /home/wnk/LicheePi_Nano/third_party/rtl8723ds/f1c200s/newboard_20260930/build-bluetooth-session.sh
tail -f /tmp/f1-rtl8723ds-bt-0930.log
cat /tmp/f1-rtl8723ds-bt-0930.rc
```

同名会话已存在时不要重复创建。执行 `build-bluetooth.sh` 即可重建；不运行clean/mrproper，不改变LOCALVERSION，不部署。

## 焊接后启用步骤（目前未执行）

先保存板端当前#246的匹配内核/DTB/模块哈希和回退入口，经SSH部署组合内核、组合DTB及依赖并校验；按FAT文件替换流程处理boot，不能裸写分区。此次不生成自动部署脚本。
部署时不要直接运行 `modules_install`：内核树产生的原生hci_uart没有新增的8723DS匹配，应选择本包的外置hci_uart。不要同时加载原生版与外置版。
将两个固件放到 `/lib/firmware/rtl_bt/`，将本包模块与加载工具放到独立候选目录。启动候选内核、确认serdev蓝牙节点后，手动运行：

```sh
sh /候选目录/load-bluetooth.sh
sh /候选目录/load-wifi.sh /候选目录/8723ds.ko
```

蓝牙加载工具使用同目录btrtl和hci_uart。serdev内核不具备、节点未注册、固件缺失或hci_uart已加载时退出，不强制替换。不运行 `hciattach` 或 `rtk_hciattach`，不能再通过ttyS2打开同一UART。
随后核对dmesg中Realtek芯片识别/固件下载、`/sys/class/bluetooth/hci*` 和Bluetooth地址，再用现有BlueZ工具开启控制器并测试扫描、配对及目标音频功能。不要假定DS一定是hci0。

## 未验证

模块未焊接，不能实测HCI响应、配对、音频、WLAN/BT共存或休眠唤醒。未部署、未安装开机服务、未改变板端AIC、IP5209或RTC设置，未推GitHub。板端仍运行#246。
回退源码/配置/旧产物：`/home/wnk/F1C200S_archives/rtl8723ds_bt_baseline_20260930`；内核标签 `rtl8723ds-bt-before-serdev-20260930`，驱动标签 `rtl8723ds-before-bluetooth-20260930`。恢复构建配置应使用这些记录，不盲目git reset或清理用户文件。
