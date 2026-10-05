# Worktree Change Log

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
