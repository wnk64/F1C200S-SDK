# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: no-SDL GBA/PS1/MD libretro frontend
Base: ae053d7; vendored source baseline3ef1499
Purpose: shared framebuffer/LVGL/ALSA frontend loading separate ARMv5 emulator cores.

## 2026-10-10 planned
gpSP5819380c2ffb0900219d700a382ee68c464ebb99; PCSXacfca1d099f076b5ddac5d43d04ac86de867a130; PicoDrive26ecb2b6358fefba24e3d68b9eb2efba7f10d5ee. Selected enabled dependency source vendored, missing disabled CHD/lightrec dependencies not represented as enabled/full SDK.
Independent LVGL/retro_native; preserve all legacy trees, no board deployment/kernel/audio-route/network changes. Libretro software video only, no SDL, no hardware render/context callbacks.
Scope: core builds, software-pixel frontend, keyboard/touch controls, ALSA default bounded audio, ROM browser and per-core save data. PS1 HLE optional/external BIOS user-supplied, no proprietary BIOS/game downloads.
Tests: ARM core ABI/load/dependency tests, own-code MD/GBA/PSX execution where feasible, mock frontend pixel/audio/input tests. Hardware speed/compatibility/RAM/coexistence and3coldboots pending.
