# 2026-10-02 Live audio route reopen candidate

Original lazycast tree, branchcandidate/audio-route-reopen-20261002,
planc05fac8/source072bbc3/build-deploy295bdab. Baselineff58d9fc exactly
reproduced before edits; complete99bf218 snapshot and rollback path retained.
Onlyproduction wfd_audio.c changed, no video/player/kernel/driver/CMA change.
Two ARM builds MD5 338c87bf1517807e782af48c1157904e, remote/proc exe verified.
Versionedtarget /root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen.
Rollback /root/aic_miracast/candidates/miracast_rx_handoff_20261001/sink-handoff.

Old thread permanentlyfailed afterENODEV, even while default route restored.
Candidate closes invalidPCM and retries at500ms; watches default route state
at500ms and reopens onlyaudio, refreshing ALSA configuration cache.
Existing16KiB queue/64KiBstack unchanged; offline PCM explicitly dropped,
no unbounded buffer or stale audio backlog. Explicitdevice not rerouted.
New opens/retries/route_changes/unavailable counters separate from oldstats.

Six ASan/UBSan mock lifecycle runs pass, including ENODEV/open/params failure,
EAGAIN/partialwrite/EPIPE, route change whileblocked, bounded retry and stop.
OriginalLPCM demux/fuzz, RX exactbytes, mock800 and IDR regressions pass.
Build originalmake-fMakefile.sink-B with Buildroot GCC6.4/ARMv5, repeatcmp exact.

Candidate phone sessionplayer21327/watch21328/sink21380, sameboot31071e77,
kernel5.7.1#250/CMA20/native800x48060/90/pool9/defaultBluetooth.
Initial3samples CPU74.40/75.86/79.39%, localdrops0, available18792..18860KiB,
CmaFree7912KiB. Automatic speaker-only Device1.Disconnect/Connect two cycles:
cycle1 headphonePCMwritten25176720 -> bluetooth26359248,
cycle2 headphone28912368 -> bluetooth30178304.
Everyphase sameplayer/sink PID, route changes3/4/5/6, opens4/5/6/7,
unavailable0 at checkpoints, video7073/7286/7958/8577 increasing.
LaterPCM32778880 and increasing, retries2, queue9344, dropped986240,
underruns2; intentional switching losses reported, not zeroaudio-loss claim.
RTPmissing/socket_missing0; FIFO full/failed/discarded0; commiterrors0,
maxgap134916us at that observation. FinalrouteBluetooth, stream persists.

Two live hot-switch tests positive; no actual poweroff/threecold acceptance
or optical/audible user acceptance yet. Do not overwrite/push oldstabletag.
Current archived launcher selectsff58d9fc; future starts must explicitly
select338c87bf versioned sink, keeping existing batch/coalescing/IDR options.
Host-only reusable tooltools/test_speaker_audio_reconnect.py preserves phone
media and records bounded phase results, no boardtmpfs archive.
