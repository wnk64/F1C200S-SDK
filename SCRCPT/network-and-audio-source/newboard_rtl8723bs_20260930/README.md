# 新板 RTL8723BS 候选（替代此前误认的DS）

## 已焊接后的当前状态

最新管理策略：BS开机自动连接wnk641_2.4G，SSH10.0.0.234；AIC不加载、不普通联网，只保留按需投屏入口。用户重启已验证。见 `../newboard_bs_primary_20260930/README.md`（本机）或原驱动树 `f1c200s/bs_primary_network_20260930/README.md`。蓝牙Wt-070低音量测试音已由用户确认听到；当前未自动重连音响，默认耳机。

现已部署内核#248（MD5 `2d16ba197f384e0570071aabf2094c44`）和新DTB（`d7016a6eeca0c2f0fa1c86adf90ae3b7`）。单线SDIO未接D1，原硬件中断不可达导致扫描结果为空；内核提交e7f7cc9增加MMC1显式中断轮询，BS DTB启用它并将时钟上限降6MHz。低速单独未恢复扫描，开启轮询后能扫描家中wnk641_2.4G及30多个网络。25MHz数据传输错误低速规避，电气/时序根因尚未证实。

`rtlwlan0`=04:4a:69:9d:1a:0e；SDIO024c:b723。蓝牙UART `hci0`=04:4a:69:c0:46:a8，固件1e4cc3ff、UP RUNNING/error0，已扫描到电脑WNK。尚未关联2.4GHz网络、未配对播放音响；不能宣称稳定共存通过。

家中串口COM9，AIC管理SSH `root@10.0.0.170`；Wi-Fi配置已私有保存。停用BU脚本S17和S45的加载/等待，S44保留USB恢复但不再加载BU；S48rtl8723bs加载当前poll候选BS驱动、启动接口，不改IP/默认路由。AIC仍管理联网，耳机自动音频服务/I2S禁用不变。
当前包和完整源在 `/home/wnk/F1C200S_archives/bs_poll_candidate_20260930`；板端 `/root/aic_miracast/candidates/rtl8723bs_poll_20260930`。回退旧#247/6M DTB在该目录rollback；#246/耳机DTB和旧BU脚本在first候选rollback。下文编译哈希是最初候选历史记录。

用户确认采购型号为RTL8723BS，且模块引脚与原理图相同。新的权威候选是本目录，旧DS包保留作历史记录，不能混用驱动/固件。没有新建Git工作树，也不为改目录名迁移原源码仓库。

## 配置

Wi-Fi使用当前完整Linux5.7.1源码 `drivers/staging/rtl8723bs` 的真实 `r8723bs.ko`，CONFIG_RTL8723BS=m原本已开启，未修改内核配置或驱动源码。ID包含024c:b723（以及原驱动0523/0525/0623/0626），不是DS的d723/d724。
蓝牙使用原外置H5源码，新增 `realtek,rtl8723bs-bt` 匹配；btrtl按BS身份（HCI rev0xb/ver6）选固件，不将DS固件改名冒充BS。

接线按用户确认保持：PC0 CLK/PC1 CMD/PC2 D0、一位SDIO初始25MHz；TCA P00 WLAN中断/P01 WLAN复位/P02 BT主机唤醒输入/P03 BT使能/P04 BT设备唤醒；UART2 PE7～PE10；PCF8563输出32768Hz。P02仅申请输入，休眠唤醒仍未实现/验证。I2S保持disabled，Codec保持okay。

独立BS DTS包含此前总线定义，再覆盖Wi-Fi compatible，并创建BS蓝牙节点；继承文件名含DS只是复用已验证总线描述，运行时compatible已核对为BS，不表示使用DS芯片驱动。

## 固件

- Wi-Fi：`/lib/firmware/rtlwifi/rtl8723bs_nic.bin`。
- 蓝牙：`/lib/firmware/rtl_bt/rtl8723bs_fw.bin` 与 `rtl8723bs_config.bin`。

固定Armbian来源中，BS蓝牙文件是指向8723b真实文件的符号链接。候选保存已解析的真实46236字节fw和68字节config，不能将raw链接文字当二进制。原字节未修改；SHA256/Git blob/魔数/配置条目/1.5M波特率及CTS-RTS校验通过。来源见 `firmware/PROVENANCE.md`。

## 编译与产物

实现提交face19e。Ubuntu原目录：
`/home/wnk/LicheePi_Nano/third_party/rtl8723ds/f1c200s/newboard_rtl8723bs_20260930`。
内核2360bde，原Linaro7.2.1，LOCALVERSION=，release严格5.7.1；tmux `f1-rtl8723bs-0930`，数字rc0。
只构建BS/H5模块与DTB，原候选zImage和.config哈希均未变；不clean/mrproper、不完整重编内核。

|产物|MD5|
|---|---|
|r8723bs.ko|b84ea759d436ab6fd3cc0d3585107ad1|
|hci_uart.ko|2ce367907d511e13ddbdcc7c2c7dd223|
|btrtl.ko|0761a0b2bd090aace818954b8254ca5e|
|BS组合DTB|406228300a5b8cfd0040c769030864c3|
|serdev候选zImage（复用）|75da971835ca3339b47eb649c2cf6697|

包内不包含8723ds.ko/DS固件；附带匹配USB/PHY/Cedar/ION模块、BS固件和手动加载工具。
全部模块vermagic为 `5.7.1 mod_unload ARMv5 p2v8`。构建和设备树/固件/无设备加载保护检查通过，硬件未焊接，联网/蓝牙/音频仍待实测。
完整候选归档：`/home/wnk/F1C200S_archives/rtl8723bs_candidate_20260930`。

## 焊接后的步骤（未执行）

先按受控流程保存板端当前#246耳机DTB、内核及匹配模块，再部署BS组合候选；当前内核未启用serdev，蓝牙需要包内serdev候选内核，不能只换蓝牙模块。全部经SSH传输/校验，FAT替换文件后sync/卸载再启动。
将BS固件安装到上述路径，BS模块和两个加载工具放在独立候选目录。检查SDIO枚举后运行 `sh load-wifi.sh`，使用rtlwlan0避免抢AIC接口；检查BS serdev节点后运行 `sh load-bluetooth.sh`，不要再用hciattach打开UART2。检查器件身份、固件下载、WLAN2.4GHz联网和蓝牙A2DP后，现有S35audio-route将按可用A2DP切换新播放默认设备。
不直接modules_install：内核原生hci_uart没有新OF匹配，使用本包外置hci_uart。不混用旧DS模块/固件，不写死hci0，不强制加载版本不一致的模块。

本轮没有部署、重启或改板端服务；#246、AIC联网、耳机/蓝牙自动选择服务保持原样。旧DS源码与候选保留；原Git标签 `before-rtl8723bs-correction-20260930` 可追溯回退。不推未验证候选到GitHub。
