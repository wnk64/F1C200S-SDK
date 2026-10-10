# Native SFC Candidate

Standalone ARMv5 Snes9x/Retro-Go frontend, no SDL dependency. Core copied
verbatim from existing `/home/wnk/Downloads/retro-go_chaeng`, commit
`32bb29138005ca6d25c13971a663d4225177e6da`, `retro-core/components/snes9x/src`.
Core license and original copyright notices: `vendor/core/LICENSE`.
The upstream license includes historical Snes9x terms; do not represent this
combined component as unrestricted commercial software.

## Build

```sh
cd /home/wnk/Desktop/F1c200S_SOFT/LVGL/sfc_native
make -j2
sh test.sh
```

Uses original Buildroot GCC6.4, LVGL8 and shared menu platform. Framebuffer
must be 480x800 XRGB8888 with zero offsets. Game RGB565 is aspect-fitted
into the upper480x448; controls occupy the rest. One contiguous1.40MiB
core rendering allocation handles512x478 hires/interlace buffers. No
additional full-screen bitmap, SDL, DRM video layer, or hardware decoder.
Direct framebuffer writing does not guarantee tear-free scanout.

`SFC_FRAMESKIP=0..5` controls rendered-frame skipping, default1. Emulation
still attempts the ROM's PAL/NTSC timing; this does not promise full speed.
Audio32000Hz stereo is mixed every emulated frame and writes nonblocking
to ALSA default through a4096-frame ring. Slow audio drops old ring data
instead of blocking the UI; counters are printed at exit. Audio device
selection is made when the app starts, no live route-reopen implementation.

## Input And Files

Touch buttons, relative mouse, keyboard arrows, Z/B, X/A, A/Y, S/X, Q/L,
W/R, Enter/Start, Tab/Select, Esc/exit. Touch currently single-point:
movement+action requires keyboard or future multi-touch/gamepad support.
Gamepad joystick axes/buttons are not mapped yet. Bluetooth HID modules
are a separate pending candidate, not enabled by this application.

Browser accepts uncompressed `.sfc`/`.smc` regular files32KiB..8MiB+512.
ROM selection directory is remembered after choosing a game. SRAM written
atomically on graceful exit to `ROM-path.srm`; no save states. Don't load
untrusted ROMs: this old emulation core is not a sandbox.

## Candidate Deployment Layout

Future path `/root/roms/candidates/sfc_native_20261010` requires:
`sfc-native`, `sfc-browser`, `sfc-app.sh`, existing `cjk28.bin`, and the core
license. Menu entry is prepared in source only; no board deployment this turn.
Browser exits before emulator starts, emulator exits before browser returns.

`--headless frames ROM` omits display/audio-device/storage writes and is only
for deterministic core validation. `make_fixture.c` produces a self-authored
32KiB LoROM that changes backdrop color at vblank; no commercial game data.
QEMU tests are not board performance, actual display or real-game acceptance.
Physical input/audio/scanout, representative user ROM compatibility and three
coldboot dependent regressions remain pending.
