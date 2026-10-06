# Worktree Change Log

## 2026-10-07 / planned / missing font and recorder buttons
Baseline0e76791; deployed recorder f0419d30, launcherbf5311c1. Readlink-based CJK font loader requires cjk28.bin beside executable, but recorder deployment omitted it.
Files: recorder.c explicit bounded button labels/state; recorder-app.sh font preflight.
Deploy required: existing cjk28.bin asset copied onto SD (vfat cannot symlink), verify hash; no shared font loader changes.
Rollback: existing recorder source cfc8adc and launcher1ca7e90; independent SD candidate, original application paths retained.
Protected: kernel, modules, default audio routing, Wi-Fi/cast; test screenshot and capture/stop/playback states.

Result: source c361399, make -C SOFTWARE/mic-recorder PASS with original GCC6.4/no warnings.
Deployment: /root/roms/candidates/mic_recorder_ui_20261007; recorder4632a9bf6cd6204a75c5e9f03927c26a56f73037afa57394f41783806d075fe4, launcher31372514f3aa07d35a5521ff55f33113c92322fbb3c1c5074ec5620b582f148c, CJK asseta2cea8412a83661341e951e7549f1b9fef506c93c34b7e6fdece7892df86ace0, hashes matched.
GUI --ui-test20000 rc0; framebuffer screenshot recorder-ui-fixed.png shows clear Chinese labels, outlined88px buttons, no overlap. Interactive user recording7.027s/224856bytes during test, xruns0; saved user WAV retained.
Menu switched only recorder/media entries, apps SHAd01774920b6a4cd79bc9f239548102b63f7b707e385b964239f916138a919067, supervisor14624 restored.
Capture closed and MIC switch off afterward; no one-off helper scripts, no diagnostic recordings generated/deleted this round.
Decision: deployed candidate; audible quality, full protected regressions and three coldboots pending.

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
