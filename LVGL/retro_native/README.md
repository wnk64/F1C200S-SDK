# GBA / PS1 / MD Native Candidates

No SDL and no RetroArch. Shared ARMv5 libretro frontend uses the existing
LVGL8/menu platform, direct480x800 XRGB8888 framebuffer, capability-based
evdev keyboard/touch/mouse, bounded ALSA default audio. Emulator cores are
separate dynamic libraries, loaded one at a time; browsers exit before games.
Core files/license notices remain under vendor. Original legacy trees intact.

## Pinned Sources

- gpSP: libretro/gpsp5819380c2ffb0900219d700a382ee68c464ebb99.
- PCSX-ReARMed: acfca1d099f076b5ddac5d43d04ac86de867a130, from existing local source.
- PicoDrive:26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee, from existing local source.
- PCSX libretro-common:0abedaac6a795c093f2e1a22f3028fca9efdf3c9.
- Cyclone:3ac7cf1bdeecb60e2414980e8dc72ff092f69769.
- libpicofe:c3031dbfda557f7058756583b329b59ea92a72dc.
- emu2413:83254be0de24b3506438c45b2870017885cb778c.
- dr_libs:dfe8377631000664666519fdb83da193fd8037f4.

Disabled CHD/lightrec/physical-CD code dependencies are not required by this
build; this is the complete enabled-build source, not every upstream SDK.
Two scoped core changes: gpSP ROM cache32->8MiB (demand paging retained),
PCSX cdriso explicitly includes stdio when CHD header is disabled.

## Build And Tests

```sh
cd /home/wnk/Desktop/F1c200S_SOFT/LVGL/retro_native
sh build-cores.sh
make -j2
sh test.sh
sh stage.sh
```

Uses original Buildroot GCC6.4 soft-float ARM926EJ-S. gpSP's old Makefile
does not track header dependencies: forced rebuild ensures the8MiB cache is
actually compiled. Cyclone's generator is a host program, compiled separately
with hostg++ before ARM assembly. No clean/replacement of historical objects.
Core version strings pinned to source commits rather than parent repo HEAD;
SOURCE_DATE_EPOCH fixes build-date macros. Upstream warnings are in build logs.

Stage is an independent deployable layout, not a backup and not deployed.
Future directory `/root/roms/candidates/retro_native_20261010`. Source menu
uses one hub entry for3systems to stay within existing12app/exit-code limit.
Do not insert14entries or repurpose shutdown exit codes32/33.

## Runtime

Uncompressed GBA .gba/.agb, MD .md/.gen/.bin/.smd, PS1 .cue/.bin/.iso/.img/.exe.
Prefer CUE for multitrack PS1; sibling track files must remain present. No CHD
support in this candidate. BIOS directory `bios/`; no proprietary BIOS bundled.
gpSP uses bundled open-source BIOS fallback. PS1 HLE boots test EXEs but has
known compatibility limits; users may provide a legally obtained BIOS.

Framebuffer supports0RGB1555/RGB565/XRGB8888 with aspect-preserved top480x448;
large controls below. Pointer UI, keyboard directions/Z/X/A/S/Q/W/E/R,
Enter/Tab and Esc. MD uses6button mapping; PS1 hasL1/L2/R1/R2. Single-touch
cannot hold direction+action simultaneously; gamepad axes/buttons and Chinese
input are not implemented. Bluetooth HID kernel candidate remains separate.

ALSA device chosen on startup, no live Bluetooth route reopen. Audio ring8192
stereo frames; overflow drops old samples instead of freezing input. Save RAM
up to1MiB persists atomically to ROM-path.nativeMODE.sav on graceful exit.
PCSX second memory card is managed by core in saves/. No savestates/disc-swap UI.
Do not load untrusted ROM/images; these old cores are not sandboxes.

## Validation Boundaries

Own-code GBA ARM, MD68000 and PS1 R3000 fixtures are generated from tracked
source into ignored build/, not downloaded commercial games. QEMU tests
validate execution and nonblank/changing pixel callbacks and audio sample
generation, not physical display, listening, UI touch or F1C200S speed/RAM.
PS1 original60frame QEMU run hit30s timeout after37frames; final bounded test
uses10frames, reported explicitly, no full-speed claim. Board absent: no
deployment, no physical BIOS/game compatibility or3coldboot acceptance.
Runtime dependencies must be checked on board, especially PS1 libstdc++.
