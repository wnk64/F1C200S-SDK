---
name: f1c200s-wiliwili-reference-video
description: Use the user-selected reference video for repeatable F1C200S wiliwili playback tests.
---

# 固定 wiliwili 测试视频

## Trigger

进行 F1C200S 的 wiliwili 网络视频播放、音视频连续性、UI控制和性能回归测试时使用。

## Purpose

用户于2026-10-03确认：以后 wiliwili 固定使用当前这部经典测试视频，避免每轮更换内容影响对照。

## Workflow

1. 视频：Rick Astley《Never Gonna Give You Up》官方MV，B站 BV1GJ411x7h7，当前 cid=137649199。CID以API实际返回为准。
2. 每次通过B站API获取新的播放地址，不保存或复用过期的CDN签名URL。初始对照使用360p H.264、正常速度、90度硬解旋转。
3. 记录实际解码尺寸、音频设备、帧提交、CPU、普通RAM、CMA及卡顿事件；确认真实画面持续变化和声音。
4. 检验播放、暂停、继续、返回和资源释放。三次冷启动与依赖回归通过前仅称候选。

## Constraints

只适用于wiliwili，不替代投屏测试、指定本地视频测试或CCTV-1网络电视的独立输入规则。不把下载到本地的副本当成网络播放验收。

## Resources

https://www.bilibili.com/video/BV1GJ411x7h7/
