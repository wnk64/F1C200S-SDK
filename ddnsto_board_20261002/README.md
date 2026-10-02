# 板端 DDNSTO 兼容性调查

## 结论

当前找到的三种官方ARM客户端均无法在F1C200S ARM926/ARMv5用户态正常执行。
三者各一次实际--help探测均返回132（SIGILL/非法指令），没有带Token、没有
登录或注册板端设备、没有常驻进程或开机服务。不能将“ARM32/软浮点/静态”
名称等同ARMv5兼容。未改内核/驱动或尝试破解二进制。

| 官方构建 | 大小 | 结果 |
| --- | ---: | --- |
| lite 4.2.2 ddnsto.arm | 754876字节 | SIGILL132 |
| 官方linux/arm/ddnsto（2023-08-25） | 约550.9KiB | SIGILL132 |
| 官方ddnstox.arm（2026-02-27） | 约3.49MiB | SIGILL132 |

4.2.1 zig arm32仅做主机ELF检查，未在板子重复测试；与lite相同类静态压缩
格式，仍无ARMv5兼容依据。官方现行Linux安装脚本只支持x64与ARM64，未执行。
源码不开放，当前材料为二进制观察基线；不能自行重编译厂商客户端。

## 证据和测试范围

板端armv5tejl、Linux5.7.1 #250、glibc2.26、64MiB、无硬件FPU。
ELF均ARM EABI5/静态、ARM属性缺失/压缩封装，不能从属性确认最低ISA。
lite元数据公开SHA256与提取文件一致，其他文件从官方HTTPS来源下载，
本机计算哈希供重复性使用，不冒充独立厂商签名。

- lite：7adffae9dae7d6b88329fdd64943fadd2f8835f0ac5027249a8c9e7e51462343
- legacy：a896960f32dc199e8bb87a5eef8f1eede0d275be027932d2582b92872396e1eb
- ddnstox：f25f0a2ea287d3d8581a1c258a6e11583771f5c18077e5452ff9a6c8e60da25d
- zig主机检查：8d5e0cfd23ac3feebb24600874df300eadde1358c75dd4dc618872a15cb77ec9

探测无Token、5秒限定、禁止core、栈1MiB/虚拟空间24MiB，只测试--help。
首轮因BusyBox缺timeout未执行返回127，改为只向exe匹配的自有子进程超时
信号后实际测试，不把127当兼容结果。SIGILL前未建立云端连接，无内存驻留实测。
不能严格归因具体非法指令/解压器/应用阶段，只能确认当前二进制实际不兼容。

官方来源：

- https://fw.koolcenter.com/binary/ddnsto/linux-binary/
- https://fw.koolcenter.com/binary/ddnsto/ddnsto-zig/
- https://fw.koolcenter.com/binary/ddnsto/linux/arm/
- https://fw.koolcenter.com/binary/ddnsto/linux/ddnstox/

## 清理与后续

失败候选三个板端可执行文件与空候选目录移除，主机下载样本/哈希/测试记录
保留可复现；路由器/VM只删除本轮/tmp下载和展开文件，不动原应用或归档。
原投屏player23857继续、SSH路由器穿透保持；无重启、无内核改动。

下一步需向厂商索取明确支持ARMv5TE/ARM926EJ-S/soft-float的轻量客户端，
且不能依赖VFP/NEON/ARMv6+指令。拿到兼容构建后再测试实际联网/RAM/CPU、
套餐独立设备授权和安全HTTP转接，不能凭help运行就宣称穿透可用。
本轮未向厂商发送外部消息或提交账号材料。

在没有兼容客户端前，板子换网需要新网络中的路由器/旁路设备代为接入，
或另行部署轻量反向SSH至真正可达的公网入口；家里路由器DDNSTO已验证的
WebSocket入口本身不等同板端能主动使用的通用SSH中转客户端。
