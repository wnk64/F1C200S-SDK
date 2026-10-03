# 短暂蓝牙PCM背压导致误重开修正

基线5571b6e，原wiliwili独立仓库及分支不变。用户视频正常、声音偶尔卡，日志实测audio transport failed: Resource temporarily unavailable后耳机回退/3秒重开蓝牙，BlueALSA无断链日志。上一轮500ms超时把EAGAIN当永久失效不合理。

仅改board_linux/audio_output.h：EAGAIN/零写入使用20ms低CPU背压等待，检查停止/暂停/路由变化；最长持续5秒无进展才允许恢复输出，真实ENODEV/断管立即恢复。EPIPE优先原句柄prepare，不因短暂欠载重开；EINTR重试。保持同一PCM与采样参数，保留上轮display1/rotate1帧预留。新增显式F1_PLAYER_DEBUG=1且F1_AUDIO_EAGAIN_TEST设置时单次650ms背压注入，证明旧500ms边界不再误重开；正式启动器unset测试变量。

新增板端候选cedar-audio-backpressure，旧cedar-audio-recovery保留，启动器只改播放器路径，不改UI/内核/蓝牙/投屏。回退启动器F1_CEDAR_PLAYER至旧候选。计划同一蓝牙音响/同一视频实机注入、暂停恢复、退出，检查没有EAGAIN回退、实际帧持续更新；音频耳听与长时/三冷验收另记录，未通过前不标稳定。
