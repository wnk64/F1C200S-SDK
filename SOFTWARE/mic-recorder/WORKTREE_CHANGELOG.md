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
