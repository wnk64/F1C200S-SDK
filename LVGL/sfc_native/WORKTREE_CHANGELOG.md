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

## 2026-10-10 offline tested / native SFC candidate
Source9b6a182 plus blit-alignment/color assertions. make -C LVGL/sfc_native -j2 using original GCC6.4 ARMv5/softfloat, no frontend warnings after correction.
sh LVGL/sfc_native/test.sh PASS: ASan/UBSan blit dimensions/stride/RGB/control-band bounds; self-authored32KiB LoROM executes120frames under qemu-arm-static,119changedframe hashes,57344nonblack pixels, deterministic42b6061d aggregate; browser check/list, zero-frame argument rejected, launcher shell syntax.
Readelf NEEDED only libasound.so.2/libm.so.6/libz.so.1/libc.so.6; no SDL or graphics toolkit runtime beyond statically linked LVGL. QEMU runtime is not an F1C200S FPS/RAM measurement.
Game buffer contiguous512x478x6 bytes to support core hi-res/interlace, rather than unsafe old256x239 allocation. No actual game ROM or hardware display/audio/input acceptance.
Future candidate path and assets documented in README.md; source apps.conf gets11thSFC entry, existing10entries unchanged. Board not contacted/deployed; no boot/service/kernel/network/audio configuration changed.
Decision: pending user ROM compatibility, touch/mouse/keyboard/audio and hardware scanout/3coldboots. Single-touch combination limits, no savestates, specialchip compatibility and CPU performance unverified; no stable promotion.
No temporary helper scripts; reusable build/tests/fixture generator retained in tracked source. Generated fixture lives only under ignored build directory.
