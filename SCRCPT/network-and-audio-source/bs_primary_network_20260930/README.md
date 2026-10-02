# 8723BS 管理联网，AIC只投屏

已部署运行，用户重启确认：boot13c988c0-74d6-44b0-8f39-09c1f615d810、Linux5.7.1 #248。管理Wi-Fi只有rtlwlan0，连接wnk641_2.4G并自动DHCP10.0.0.234，默认路由/DNS由BS管理。SSH `root@10.0.0.234`（密码仍1）。没有加载AIC或8723BU模块，BT hci0开机UP/error0；显示card0、RTC和Codec存在。

S17rtl8723bs-sta早加载BS，后台等待WPA关联并启动DHCP，不阻塞后续init；S18ssh提前调用现有S50dropbear。S46aic-sta为no-op，S48只加载蓝牙。BU的S17/S45仍为no-op；USB恢复脚本保留但不重载BU。
`/etc/rtl8723bs-sta.conf`是板端私有配置，权限600，通过supplicant保存，源码只含无密码模板。不要上传或打印该配置。

内核时间证据：BS模块4.04秒初始化、4.30秒Wi-Fi固件、8.74秒关联成功。用户确认启动快多了；这不是电源上电至SSH的精确计时。源码实现16fbe5c；启动脚本MD5：S17=25add32d4e1e6e42fe5c9ecf917c3378，S18=8285639e180da99851d178732e2af6c2，S46=b540c915ce728f1858d4bffba6ba918a，S48=66f449e6f8969f56875151ebb7e43ee5。

## AIC手动投屏入口

```sh
sh /root/aic_miracast/load-aic-for-miracast.sh
```

该工具使用已核对的UGREEN rxguard驱动/固件，只加载AIC并注册wlan1，不做普通STA关联、DHCP或默认路由修改。它是准备入口，未在本次重新启动投屏验证；没有修改原投屏或播放器脚本，也没有用旧归档驱动替换。启动完整投屏链路仍需对应的已验证流程，不能把注册网卡等同投屏成功。

## 简短速度测量

MMC1保留6MHz、一位、原生SDIO IRQ轮询，原始数据线速上限6Mbit/s（0.75MB/s），不含协议开销。板子到电脑SSH读取2MiB `/dev/zero`、电脑只计数不落盘：2097152字节、4.201秒、0.499MB/s/3.994Mbit/s、退出0。这是含SSH开销的实测单向吞吐，不是无线标称速率或纯TCP上限。
一次4MiB纯HTTP测试未成功（wget5秒超时），没有修改主机防火墙；本机一次性HTTP服务正常结束，没有后台遗留。未做长时间压力测试或调高时钟。

DAT1在一位SDIO下仍承担带内中断。可通过支持外部中断的GPIO转接，但需要MMC host/驱动支持和中断转交；不能当作专用SDIO DAT1直接使用，不因此变成四位总线。本次仅回答能力，不改接线或驱动。

## 回退与未验收项

板端旧S46/S48在 `/root/aic_miracast/candidates/bs_primary_network_20260930/rollback`；源码标签before-bs-primary-network-20260930，完整来源见原内核/驱动候选归档。本目录与最终来源记录归档在 `/home/wnk/F1C200S_archives/bs_primary_network_20260930`。
此前Wt-070已配对连接，A2DP/SBC48k双声道，默认蓝牙静音及低音量提示音完成，用户确认听到声音；断开后路由回耳机。当前重启后未主动重连音响，不宣称自动重连功能已实现。没有三轮物理冷启动/长时共存或新板投屏稳定验收，不推GitHub。
