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
