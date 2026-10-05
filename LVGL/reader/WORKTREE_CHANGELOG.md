# Worktree Change Log
Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: LVGL/reader independent userspace app
Base:4216e5e, menu/browser/player full sources tracked
Purpose: TXT/image viewer through startup menu without SDL.

## 2026-10-05 / planned
Files: new LVGL/reader, apps.conf appended eighth? existing6 entries so seventh reader.
Text: bounded paged pread, UTF8/GBK/UTF16 BOM handling, next/previous, progress, encoding selector. GBK mapping reused from ONS generated table. No full book heap.
Image: upstream nothings/stb stb_image.h (MIT/public domain), JPEG/PNG/BMP/GIF first frame/TGA/PNM/PSD; dimensions<=4096 and <=2M pixels, allocation budget12MiB, 480x600 RGB565 output, source memory freed after rendering. Previous/next same-directory images, fit/1:1 mode. No animated GIF or arbitrary huge-image claim.
Browser copied existing independent media-browser layout; only supported files. Viewer exits before browser returns. Menu and browser don't overlap. SD deployment /root/roms/candidates/reader_20261005 due full root partition.
Protected: existing MPlayer/UI/ONS/wiliwili/cast/kernel/network/BT unchanged, only append apps.conf seventh entry. Rollback apps.conf4216e5e, existing menu all prior commands preserved.
Tests: build/parser/encoding fixtures, real text pages/image decoders, screenshots, exit/menu restoration. Three cold boots/actual touch pending.
