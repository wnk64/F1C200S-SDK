# GBA / PS1 / MD 无 SDL 候选

基线ae053d7。新目录LVGL/retro_native，采用libretro接口统一原生显示/输入/音频；不引入RetroArch或SDL。
完整保存本轮启用代码：gpSP5819380c、PCSXacfca1d、PicoDrive26ecb2b6，依赖libretro-common及Cyclone/emu2413/dr_libs已纳入Git3ef1499。旧目录不修改。关闭CHD、物理光驱和异步工作线程，PS1优先ARMv5 ari64/unai候选，不承诺实机流畅。
使用共享LVGL控制/浏览器、直接framebuffer，ALSA default。BIOS/游戏不得下载商业资源；gpSP源自带开源BIOS保留许可证，PS1可用HLE作基础验证但不承诺兼容性。
板子不在，只做主机与ARM/QEMU离线验证，不部署。三个模拟器分别记录构建/可执行程度，不把能加载动态库当成能玩游戏。

## 结果

前端/构建/测试提交b97b578。三个核心ARMv5/软浮点编译通过；PS1采用ari64动态重编译/unai软GPU，QEMU动态代码执行自检通过。GBA旧Makefile不追踪头文件，已强制重编确保8MiB ROM缓存真正生效；MD Cyclone生成器必须用主机g++，不是ARM交叉编译器；PS1关闭CHD后stdio类型缺失已显式补头文件，并使用已有系统zlib。
自写测试而非商业游戏：GBA运行60帧/2次画面变化/38400非黑像素，MD60帧/59次变化/71680非黑像素，PS1短程10帧/9次变化/76800非黑像素。均收到模拟音频样本，未打开真实音频设备、不证明听感。
PS1最初60帧试验在30秒后超时，只完成37帧，已明确记录失败；后续10帧仅验证功能，不能称满速。QEMU耗时不能换算为板子游戏速度。
三种像素格式/stride/边界经ASan/UBSan测试通过。无SDL依赖；PS1额外依赖libstdc++，板端部署前需要核对。生成stage独立候选目录，包含字体、许可证与核心，未包含商业BIOS。
源码菜单用第12项“游戏模拟器”进入GBA/PS1/MD选择页，原11项不动，避免直接增加到14项碰到原shutdown退出码32/33。每种系统能选择目录与ROM，目录选择后记忆，退出回浏览器；SRAM/第一张记忆卡走通用保存，第二张卡由PCSX保存。真实存档/游戏/声音/界面触摸未验收。
完整源码/依赖来源、构建、文件SHA、限制见README及WORKTREE_CHANGELOG。没有部署板子、不改内核/蓝牙/网络/投屏；无一次性临时脚本，保留可复用构建/测试/打包工具。三冷与实际游戏兼容性/性能仍pending。
