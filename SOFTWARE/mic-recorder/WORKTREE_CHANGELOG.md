# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: independent microphone recording application
Base: 1f2b7d9, tracked LVGL/menu platform and LVGL8 source
Target release: running 5.7.1 #250, unchanged
Purpose: on-demand analog MIC capture to SD WAV without SDL or a daemon.

## 2026-10-07 / planned / MIC capture
Hypothesis: existing Codec capture hardware works when ADC MIC route is enabled.
Files: SOFTWARE/mic-recorder/{recorder.c,Makefile,recorder-app.sh}; menu apps.conf.
Dependencies: Codec capture, ALSA, SD, shared LVGL/menu platform; default audio playback only for replay.
Protected: kernel, DTB, modules, Wi-Fi, AIC/Miracast and Bluetooth services unchanged.
Rollback: remove new app entry; existing menu/app binaries unchanged, Git 1f2b7d9 apps.conf.
Tests: build, headless WAV capture/header/signal statistics, mixer restoration, UI and playback; three cold boots pending.
Decision: pending; no stable acceptance or source archive claimed.

## 2026-10-07 / deployed candidate / capture checks
Commits: 82795d4 menu entry, 1ca7e90 complete source, cfc8adc explicit nonblocking capture start and 3-second no-data timeout.
Build: make -C SOFTWARE/mic-recorder; existing Buildroot GCC6.4, no warnings.
Binary SHA256: f0419d30a8828c0898c232e610a4b6d2a0213f9e9c8d1b95050fa1f78ef9f0b5.
Launcher SHA256: bf5311c1b0af145308eb2f8790d3ed3d6703da69bccba800cbd50f2e1177c70f.
Menu apps SHA256: d46dfee19a1729ccef2047ca0b0e1fed2cb004750049406dcf848ef24fe54bff.
Deploy: SCP, hashes matched; added /root/roms/candidates/mic_recorder_20261007 and one menu entry, no application/service binary replacement.
Tests: initial PREPARED/no-data candidate rejected; explicit snd_pcm_start then 8.002s/256064bytes and 20.006s/640176bytes captured, xruns=0.
Eight-second WAV: 256108-byte file, PCM16 mono16k, one clipped initial sample; seconds1..7 no clips, RMS995.85..1627.30. Human voice/content not verified.
SIGTERM: 0.966s/30904bytes finalized; full mixer before/after SHA d2382c7448d3fdf9530e594b889b86382ea1d365d3694bc7702b318bb5082aa2 identical.
No overwrite test rc1; zero-duration argument rc1. Headless RSS2032KiB/anonymous176KiB/one thread; not a GUI memory measurement.
Capture closed after test; existing player9368 and Codec playback9495 remained running. Audio quality not checked.
Rollback: remove recorder line only; existing apps.conf is tracked at1f2b7d9. Do not overwrite other menu entries blindly.
Pending: physical GUI touch, actual microphone listening/replay, protected end-to-end regressions and3coldboots. No stable acceptance.
