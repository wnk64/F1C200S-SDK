# LAN applications candidate
Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: SOFTWARE/lan-apps userspace/menu applications
Base:5029985 source-reproducible menu/platform and MPlayer backend
Purpose: on-demand authenticated SD file transfer and DLNA renderer.

## Planned
Use CivetWeb1.16 HTTP parser/server, GMediaRender0.3.1 UPnP renderer with pupnp1.14.20 and existing MPlayer Cedar output (no SDL/GStreamer). Full upstream sources/licenses tracked. No kernel/DTB/network/audio/normal-player modification. Web binds only rtlwlan0 current IPv4:8080, one worker/64KiB IO buffer, streaming upload under SD/Uploads, authentication, reject overwrite/traversal/symlinks, limit2GiB. UI displays actual address/login and exits server. DLNA menu-exclusive app advertises rtlwlan0, hardware rotation backend, controls pause/seek/volume/stop, waiting UI releases before playback. Retain existing apps and protected Miracast, do not interrupt current casting. No boot services. New app directories and app-table entries only after build/tests; rollback remove new rows. Coldboots0, physical and dependent regressions pending.
