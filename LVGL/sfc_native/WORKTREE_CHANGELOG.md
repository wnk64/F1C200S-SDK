# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: independent native SFC frontend
Base: 97ce5cd; upstream retro-go32bb29138005ca6d25c13971a663d4225177e6da core preserved verbatim at b2ee8b2
Purpose: software SNES emulation without SDL, direct framebuffer output and LVGL controls/browser.

## 2026-10-10 planned
Files: LVGL/sfc_native/{vendor/core,Makefile,frontend.c,display.c,blit.h,tests,rom_browser.c,sfc-app.sh,changes.cn.md}.
Protected: existing emulators/menu binaries, shared platform source, kernel/DTB/drivers/Wi-Fi/BT/audio route/cast unchanged. No board access/deploy.
Display: portrait480x800; game aspect-preserved in upper480x448, large controls below; nearest-neighbor mapping, no full-screen LVGL image cache or SDL dependency.
Input: existing shared platform touch/mouse, independent HID key-state joypad; single-touch limitations disclosed. ALSA default32kHz stereo with bounded nonblocking ring. SRAM preserved atomically next to ROM, raw uncompressed .sfc/.smc only.
Tests: pure blit bounds/geometry, ARM build/no-SDL ELF dependency, self-authored minimal LoROM via qemu headless core execution. Not a game/performance/physical display proof.
Rollback: original applications untouched; candidate menu entry not activated. Threecold acceptance pending.
