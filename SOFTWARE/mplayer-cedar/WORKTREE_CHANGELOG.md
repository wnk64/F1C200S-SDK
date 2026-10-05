# Worktree Change Log

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: SOFTWARE/mplayer-cedar userspace application
Base: GitHub wnk64/licheepi_nano b96fac2dd917284bf6eedf780840cf6a9b623b3c, full source
Target release: current board kernel5.7.1 unchanged
Purpose: restore actual Cedar MPlayer and update without SDL dependency.

## 2026-10-05 / planned / no-SDL source baseline

Source: cedar-local-mp4-packet-valid-20260906, complete MPlayer1.3.0 including original Cedar VD/VO.
Acquisition: GitHub commit archive SHA256a5d21c5daf7d1fd4e71368d0c4412f11b7677767aa4df78236af1a61f62723d9.
Baseline binary: board /root/roms/tv/candidates/mplayer_cedar_packet_valid_20260906/mplayer-cedar-packet-valid SHA256893fed734d793053bbf293f471a32de363004f1849f4310f998aec48de621f05. Retained, not a proven working baseline on current board.
Hypothesis: rebuild exact full source with current relocated GCC6.4/sysroot, disable SDL; keep original Cedar decoder/display first.
Protected: system mplayer, ONS menu/player, Miracast, kernel/drivers, audio/network services unchanged.
Build: new reproducible local-file playback build script, no clean/mrproper; candidate source initially has no object files.
Tests: ELF dependency/codec listing first; display/decode only after current Cedar APIs and ownership are audited. No network-TV experiment or acceptance claim.
Decision: pending; newest decoder/render/pool integration remains required, no wrapper substitution.
