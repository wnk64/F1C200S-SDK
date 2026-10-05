# Worktree Change Log

## 2026-10-05 / planned / MP3 controls and synchronized LRC
Baseline: e5e99e8, existing video frontend8b978fd7368f0cbf8766c8c17494ad0d5849e083d8420dfdea151f855eea3f64 retained.
Files: mplayer_ui.c audio-only mode; lyrics.c/.h bounded UTF8 LRC loader and tests; Makefile.
MP3: mpg123/default ALSA, visible full framebuffer UI without video OSD or CMA reservation; audio position from MPlayer ANS_TIME_POSITION. Metadata title/artist; drag pause, seek release, volume, exit. Lyrics from same basename.lrc; no fabricated lyric if absent.
Protected: video frame-driven position and private OSD, backend, kernel, Bluetooth/network configuration, startup menu. Test parser/LRC, MP3 controls with existing user file, video regression. Three cold boots pending.
Code commit907a7df; build8bf41cc04eabfc8d893fd398e96bcf18d847713825feae9aec9e0adad06d12ad. Board parser/LRC selftests PASS; audio UI seek86.000 target86.000, pause/volume PASS; video regression PASS/fallback0/live0. Screenshot confirms metadata/Chinese no-lyric state/controls. Paused get_time_pos emits ALSA DELAY(-77) because paused device state; planned skip position polling while paused, retain last played position. Strip MPlayer metadata transport quotes.
User requested real matching MP3/LRC download: Code Monkey by Jonathan Coulton, public Internet Archive audio and LRCLIB18493616 timed lyrics. Verify download hashes, playback duration, visible lyric advancement and seek reset; do not fabricate lyrics. Preserve test.mp3 and old binaries.
Probe: MPLAYER_UI_DEBUG=1 opt-in index/position logging (default off); selftest now verifies post-seek displayed lyric equals the timed LRC entry. Paused-position fix be6333c rebuilt3ee1174eab509bc983b6dea00bbd9f7b4d9cbe5d46d7ae2947d1ff1cea91dd46; user MP3 selftest PASS without previous ALSA warnings.

## 2026-10-05 / planned / OSD plane enumeration
Baseline: d86d841, frontend7d4a01b16d608a75fc6df34c5fb8a1be58ad8a16693de8b549e28c591a2d02cc.
Hypothesis: without UNIVERSAL_PLANES, primary layer0 is omitted; planes[1] mounts layer2 while private visibility commands move layer1. Hidden widgets clear the still-visible layer2 to black.
Files: media_display.c only; enable atomic/universal plane enumeration before selecting primary0/OSD1, log selected IDs and validate primary type.
Protected: Cedar decoder/backend, native rotated geometry, audio, network, kernel, menu and wiliwili sources unchanged.
Rollback: existing mplayer-ui-volume candidate; source d86d841. Build: make -C LVGL/media_player -j2. Visual hide and three cold boots pending.
Commit: ee528d0. Build PASS with existing _GNU_SOURCE redefinition warning; SHA256 8b978fd7368f0cbf8766c8c17494ad0d5849e083d8420dfdea151f855eea3f64.
Deploy: SSH/SCP to /root/aic_miracast/candidates/mplayer_ui_20261005/mplayer-ui-planes; board SHA256 matches, old mplayer-ui-volume untouched.
Board selftest exit0: primary0=31 OSD1=35; pause/volume/seek/frame-ready/hide PASS, pool peak10358784 fallback0 live0.
Interactive looping playback started with same backend/test_800x480.mp4; exit returns startup menu. Decision: pending actual visible hide confirmation and three cold boots; synthetic hide is not visual proof. No kernel/service configuration changed.

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: LVGL/media_player frontend and optional Cedar MPlayer frame notification
Base: MPlayer618d09a/noSDL candidate96a9cab0df56079ac251f639d59ace224ff94541224ed637cfa019df6ab7ebbf
Purpose: add real touch playback controls with separate low-memory OSD.

## 2026-10-05 / planned
User confirmed local hardware-rotated playback. Pool12MiB peak10358784/fallback0/live0, FAAD S16 ALSA; old system/players preserved.
Files: new frontend/landscape display adapter; optional MPLAYER_OSD_FD frame messages from VO after successful commit/vblank.
UI: center toggles controls, idle10s hides OSD offscreen; pause, real frame-driven progress, drag pause/absolute seek, software volume, exit.
Display: proven wiliwili86x800 RGB565 layer1 pattern, no devmem crop hacks, no SDL; underlying video stays layer0.
Position: hardware picture PTS with decoder origin, no fabricated wall-clock progress, loading until target-frame commit.
Memory: LVGL128KiB, striped render buffers,137600-byte OSD, no full-screen bitmap cache.
Tests: build/dependency, command parser and controls, actual layer appearance/hide, pause/seek/audio/exit plus menu recovery required; three cold boots pending.

## 2026-10-05 / planned / control selftest
Commits: a51b48a frontend/frame notification,35a99f8 opt-in selftest/hidden control rendering.
Build: frontend ELF needs libdrm/libm/libc only; no SDL. Backendff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d built.
Files: mplayer_ui.c startup/pipe failure handling and selftest flag; parser selftest rejects malformed/nonfinite replies.
Selftest: explicitly MPLAYER_UI_SELFTEST=1; pause stability, actual volume acknowledgement, drag-controller seek and target-frame readiness, hide request, exit/cleanup. Synthetic LVGL actions do not prove real touchscreen or visible hide.
Default: no selftest,10s idle hide; control widgets hidden when OSD hidden; progress redraw4Hz.
First board selftest: parser PASS, actual frame received, pause stable0.366s; volume displayed29 versus request30 because float truncation, exit1/menu restored. No leak: pool live0.
Planned mplayer_ui.c: round float volume for integer slider, retain full double acknowledgement and require within0.1percent of requested30; no fake acknowledgement or relaxed playback-frame check.
