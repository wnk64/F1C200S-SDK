# Worktree Change Log

## 2026-10-05 / planned / non800 native geometry diagnosis
Baseline e2a8f5a, backendff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d retained. User reports non800x480 displays misplaced.
First scoped diagnostic: CEDAR_DEBUG=1 logs actual rotated VideoPicture storage/stride/visible offsets, disabled by default. Files vd_cedar.c only. No guessed display correction before observing640x480/620x480/480x270/800x480.
Current VO uses stream dimensions for view and ignores picture crop/stride. Kernel private type6 is not implemented in current source; do not blindly invoke it or change kernel. Protected MP3/lyrics/UI/ONS/network/BT and kernel5.7.1 unchanged.
Build: incremental make using same GCC6.4/config, CEDAR_TARGET_LIBDIR=current target/usr/lib. Restore old backend via media-app.sh Git baseline; diagnostic candidate separate. Three cold boots pending.
Diagnostic33ca3e8 built356309f9828594c043aa44f6eb021cd77e92530e9a78c079ebce626ec1cbf952. Root full blocked transfer; only own partial file deleted, candidate moved to /root/roms/candidates/mplayer_geometry_20261005 on SD. Runtime640x360: storage384x640/stride384/visible8,0..368,640;480x270: storage288x480/stride288/visible2,0..272,480;800x480: storage480x800/stride480/offset0.640x480 aligned has no stride mismatch;620x480 storage480x640 visible480x620. All decode probe rc0, not visual acceptance.
Planned fix: native VO waits for actual picture metadata, validates MB32, center-clips larger visible regions without scaling, uses actual pitch and residual tile-window offsets; tile-aligned virtual buffer addresses for centered source crop. Existing /dev/mem FE window method from working wiliwili applied only to stride/tile registers after normal DRM view ioctl, format checked; kernel/source/services unchanged. Map lifetime tied to VO; pure geometry unit tests and multi-resolution display/control regressions required.
Fix85440d9/legacy CLOEXEC6ceb14f, built3cff48d26822847db9fdf466d6d3f9168b4073086d291187fc56c480a04808c2. Host gcc -std=c99 -Wall -Wextra -Werror f1c200s/cedar_geometry_test.c PASS; same GCC6.4 incremental MPlayer build PASS, board SCP hash matches. Five3s decoder-only tests640x360/640x480/620x480/480x270/800x480 rc0, layout logs match picture and fallback0/live0. Native no scaling1:1. UI800 pause/volume/seek/frame-ready/hide regression PASS; MP3 timed lyrics/seek PASS. Visual multi-format acceptance pending.
Incident: user reported620 playback apparent crash while diagnostics were running. Board uptime7h07 unchanged/SSH alive; menu playback.log records SIGTERM in init_video_codec then620 pool peak8273920/fallback0/live0, no evidence of620 fatal signal. Service stop may have interrupted user's menu-launched playback; do not describe as independent620 crash. Separate720p decoder-only test saturated CMA and repeated ION allocation errors (fallback2470/live11378688 at pool close), cannot claim720 support or successful large-crop runtime. It ended; MemAvailable36128/CmaFree15644KiB recovered. Cancel remaining test command session, ensure no overlapping display owners.
Now only620x480 actual UI test, same candidate backend, /root/roms/candidates/mplayer_geometry_20261005/620x480.ui.log. Menu restores on normal player exit; backend menu integration remains oldff93 until user confirms display. Do not auto-reboot or change kernel. Temporary probe/test/launch commands removed; unit binary/build log removed after use. Candidate pending real visual confirmation and three cold boots.
Actual620 UI ended normally childexit0/PLAYER_EXIT0, pool8273920/fallback0/live0. Source MP4 emits old demux H264 VUI/golomb warnings but hardware decode and complete EOF succeed; cannot equate warning with fatal crash. User then relaunched old menu backendPID7192. Do not interrupt active playback. Planned media-app.sh backend path only switch to SD native candidate for next launch; oldff93 remains rollback. Visual confirmation still pending, no stable promotion.

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

## 2026-10-05 / planned / current Cedar adapter features

Source baseline imported as3415c6c, build scriptsed33a98; full source tracked, original remote b96fac2 verified.
Next hypothesis: use current Cedar rotation fields/zero extra holds and exact current centralized CMA allocator; preserve MPlayer demux/audio/clock.
Files: libmpcodecs/vd_cedar.c, libvo/vo_cedar_drm.c, Makefile, f1c200s/cedar_cma_pool.c/.h.
Reference: SOFTWARE/cedar-player5685c63, original allocator source copied unchanged and hash-checked; no shared player source edits.
Rotation: explicit CEDAR_ROTATE=90 only, native geometry available through cedar_drm:native; fit/stretch/crop retained.
Lifecycle: release VO-held picture before decoder reset/destruction, wait next vblank before recycling previous scanout frame, open/close bookkeeping.
Memory: CEDAR_CMA_POOL_MB remains opt-in1..16; copied original allocator fallback behavior retained. No fixed guessed reservation.
Tests: decode/EOF/seek/rotation/local800x480 plus audio and menu recovery required. Not a network-TV validation or stable label.

## 2026-10-05 / pending / first local decode observation

Commits: 3415c6c baseline, ed33a98 noSDL script, 7eb31fb current rotation/pool/lifecycle.
Build: full noSDL succeeded; candidate4c65834b823384b27f06090311d502d62bcc0d60338dff6ef70bbd4ca0de3c97 SSH hash verified. SDL=no, SDL_IMAGE=no; original codec/status listing uses exit1 normally and prints Cedar entries without segfault.
Deploy: /root/aic_miracast/candidates/mplayer_current_nosdl_20261005/mplayer-cedar-current-nosdl; old binaries untouched.
Test: local test_800x480.mp4 is H26430fps, CEDAR_ROTATE90/cedarh264/cedar_drm:native/nosound/endpos30, exit0 and menu restored. Hardware decode/picture output logged, visual confirmation pending.
Memory observation: during decode MemAvailable24912KiB/CmaFree1120KiB; after exit/menu37516KiB/13520KiB. CMA free includes cache accounting; no proof of leak from this alone.
Planned: invert stream display aspect metadata when90degree rotation, retain native pixel mapping. No scaling based on misleading MPlayer d_width/d_height; optional allocator and audio still to test.
