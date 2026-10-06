# 8723BS 联网现场恢复 / 2026-10-06

## 现象与证据
COM11/115200实测；kernel5.7.1#250，rtlwlan0存在、SSID wnk641_2.4G信号-45dBm。关联失败反复ASSOC-REJECT status_code1，尚未到DHCP或静态地址配置。初次driver日志auth success/start assoc后，后续rtw_set_802_11_connect反复fw_state0x8，没有新的auth记录。OpenWrt10.0.0.1只记录本次MAC04:4a:69:9d:1a:0e authenticated，没有associated或四次握手；无关联客户端，不能仅凭status1断言路由器拒绝策略。其旧日志确认同MAC此前在同SSID成功握手。本次凭据未修改/未保存在记录。

## 操作与结果
只做现场会话恢复，无源码/配置/密码/IP/路由器/蓝牙/内核修改，无整机重启。
1. wpa_cli disconnect/reassociate未恢复。
2. S17rtl8723bs-sta stop、ifconfig down/up、S17 start未恢复。
3. S17 stop、rmmod r8723bs、S17 start重载原驱动，然后select_network1；出现新auth/assoc success、CCMP握手完成，静态IPv4恢复10.0.0.234。
原模块SHA256b214048ec14f933dea1e641b801016b8c07d8fafb56c64af5781779d3f6700de；没部署新模块。无serial文件传输。
SSH root@10.0.0.234成功，菜单服务PID257正常。板到网关3/3成功11..19ms，电脑到板5/5成功10..26ms；wpa_stateCOMPLETED。只支持驱动运行状态恢复有效，首次失败和持续卡住的内部原因仍未定位，不能称永久修复或自动恢复已实现。

## 状态
暂时恢复。三冷验收0，没有自动重载脚本。串口已关闭释放，未创建临时脚本/备份。Ubuntu192.168.199.128 SSH超时，本轮无源码改动，此现场记录先留Windows项目目录，尚未同步源仓库。
