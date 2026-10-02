# 耳机默认声卡，关闭I2S

已部署：`/etc/asound.conf` 按 `Codec` 名称选择板载耳机；新增 `/etc/init.d/S20headphone`，开机设置DAC直通、解除静音、耳机音量40/63（约-23dB）。不依赖数字声卡序号，不修改蓝牙工具。
独立耳机DTB禁用i2s/sound_i2s/pcm5102a。仍运行原5.7.1 #246内核，未部署蓝牙候选。

源码提交：内核b1deb8b、运行配置2359092，原工作树Git跟踪。
DTB MD5：670b67cf44be2d21acdca82dab9e3d4b。
asound.conf MD5：b38839aeeb2c132931546aec6877e74f。
S20headphone MD5：497f0e98f7ec58fc43acbe4b4b2dea1b。

软件重启后只有Dummy/Loopback/Codec，无I2Smaster。默认amixer和48k双声道静音PCM播放成功；开机音量保持，AIC联网/SSH、显示设备、RTC/TCA正常注册。这不是听音或物理冷启动验收。

回退配置和旧DTB：`/root/aic_miracast/candidates/headphone_20260930/rollback`；源码及产物：`/home/wnk/F1C200S_archives/headphone_20260930`。回退需恢复旧asound.conf、移除本次S20headphone、在FAT恢复旧DTB并sync/卸载后重启。

应用若指定 `hw:3,0` 或蓝牙设备，不会被默认声卡配置重定向；应改用 `default` 或 `hw:Codec,0`。Cedar播放器显式设置时使用 `CEDAR_AUDIODEV=default`，不在本次擅自改投屏脚本。
