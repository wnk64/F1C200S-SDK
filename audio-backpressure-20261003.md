# 短暂蓝牙PCM背压导致误重开修正

基线5571b6e，原wiliwili独立仓库及分支不变。用户视频正常、声音偶尔卡，日志实测audio transport failed: Resource temporarily unavailable后耳机回退/3秒重开蓝牙，BlueALSA无断链日志。上一轮500ms超时把EAGAIN当永久失效不合理。

仅改board_linux/audio_output.h：EAGAIN/零写入使用20ms低CPU背压等待，检查停止/暂停/路由变化；最长持续5秒无进展才允许恢复输出，真实ENODEV/断管立即恢复。EPIPE优先原句柄prepare，不因短暂欠载重开；EINTR重试。保持同一PCM与采样参数，保留上轮display1/rotate1帧预留。新增显式F1_PLAYER_DEBUG=1且F1_AUDIO_EAGAIN_TEST设置时单次650ms背压注入，证明旧500ms边界不再误重开；正式启动器unset测试变量。

新增板端候选cedar-audio-backpressure，旧cedar-audio-recovery保留，启动器只改播放器路径，不改UI/内核/蓝牙/投屏。回退启动器F1_CEDAR_PLAYER至旧候选。计划同一蓝牙音响/同一视频实机注入、暂停恢复、退出，检查没有EAGAIN回退、实际帧持续更新；音频耳听与长时/三冷验收另记录，未通过前不标稳定。

源码dadad4b、注入时机修正3a8cee6，原目录make -C board_linux -j2 net-player selftest rc0（原GCC6.4、未clean）。首次注入与暂停重开相交，虽然未回退仍不算完整650ms证明；修正为首写3秒后才注入。第二轮真实蓝牙测试：frames200→暂停冻结→恢复1066→19100，退出rc0；DEBUG注入650ms后650918us同句柄恢复，无audio transport failed、无plughw耳机回退。另有120985～182021us真实背压均原输出恢复，原BlueALSA日志无断链错误。未证明所有偶发声音停顿消除，网络/2.4G共存/欠载仍可产生其它停顿。

板端新播放器SHA256aa4c72500e5ded6e5e0cde310f0a25eac88fb027c65e8404a2096c85e5ee106b，SCP核验一致，正式launch-audio-recovery-with-menu.sh已指向它，保持完整UI与display1/rotate1预留。正式入口unset F1_AUDIO_EAGAIN_TEST/F1_AUDIO_RECOVERY_TEST/F1_DIAGNOSTIC_NO_OSD，F1_PLAYER_DEBUG默认0。旧播放器保留，未改投屏。最终菜单恢复运行、未留测试播放，一次性主机包装已删除；冷启动完整验收pending，不推稳定标签。
