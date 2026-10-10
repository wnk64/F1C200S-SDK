# GBA / PS1 / MD 无 SDL 候选

基线ae053d7。新目录LVGL/retro_native，采用libretro接口统一原生显示/输入/音频；不引入RetroArch或SDL。
完整保存本轮启用代码：gpSP5819380c、PCSXacfca1d、PicoDrive26ecb2b6，依赖libretro-common及Cyclone/emu2413/dr_libs已纳入Git3ef1499。旧目录不修改。关闭CHD、物理光驱和异步工作线程，PS1优先ARMv5 ari64/unai候选，不承诺实机流畅。
使用共享LVGL控制/浏览器、直接framebuffer，ALSA default。BIOS/游戏不得下载商业资源；gpSP源自带开源BIOS保留许可证，PS1可用HLE作基础验证但不承诺兼容性。
板子不在，只做主机与ARM/QEMU离线验证，不部署。三个模拟器分别记录构建/可执行程度，不把能加载动态库当成能玩游戏。
