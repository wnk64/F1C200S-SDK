# Worktree Change Log

## 2026-10-02 planned / recover live audio after route changes
Follow-up plan/result:versionedruntime/audio-reopen-20261002/start-cast.sh
selects338c87bf with same existing workflow/options/templates, old archived
launcher untouched. This avoids next start selecting unfixedff58. Shell
syntax and readonly boardpreflight required; coldmode remains unaccepted.
Live result:source072bbc3/deploy295bdab/338c87bf, phoneplayer21327/watch21328/
sink21380. Two speaker-only Disconnect/Connect cycles: headphone->Bluetooth
routes restored, audio written25176720->26359248 and28912368->30178304,
samevideo/sink PIDs, commits advancing, route_changes3..6/opens4..7.
LaterPCM32778880/retries2/drop986240/underruns2, FIFO full/failed0,
RTPmissing/socket_missing0/videoerrors0/maxgap134916us. CurrentBT andstream
kept. Hosttesttool and detailedruntime/audio-reopen-results-20261002.md
recorded. Actualspeaker poweroff/audible/threecold acceptance pending.
Planc05fac8/source072bbc3: onlywfd_audio.c plus fakeALSA lifecycle tests.
Six sanitizer lifecycle runs pass: initial/parameter failure, bounded retries,
ENODEV, EAGAIN/partialwrite/EPIPE, route change duringblockedwrite, explicit
device unaffected, stop whileunavailable. Demux/video-byte/RTSP800/IDR pass.
Two ARM Makefile builds338c87bf1517807e782af48c1157904e; oldff58 retained.
Next SSH versioned deploy, phone reconnect once, then speaker-only live
disconnect/reconnect with same video PID and PCM counters/resourcemonitor.
CurrentdefaultBluetooth restored but sinkwritten330244096 frozen: old audio
thread setsfailed and exits forever on PCM loss. Change onlywfd_audio.c:
bounded500ms route-state polling fordefault, close/reopen PCM on route change,
retry failures without permanent audio disable, discard stale queued PCM.
Original7043562/executableff58d9fc/source snapshot99bf218 archived/pushed;
retain old versioned sink/player/native800x48060/90/pool9/CMA20/AIC/BS/kernel.
Add deterministic mock ALSA lifecycle/route/retry/partialwrite/stop tests,
ASan/UBSan, existing video/RTSP regressions and reproducible ARM builds.
Candidate branch in original worktree only, no new worktree/driver changes.
ActualBT disconnect/reconnect with ongoing video required; threecold pending.

## 2026-10-02 tested / reconnect after archive-induced memory pressure
Plan localcrash_20261002/RECONNECT_TEST.md before runtime restore; same
kernel#250/CMA20/boot31071e77/AIC/BS/playera683cf70/sinkff58d9fc/fullBT.
Only temporary session commands, no source build/driver/service replacement.
Two ADB connections succeeded, native800x48060/90/pool9/defaultBluetooth.
Run1 140.24s/6640commits/max179340us; run2 140.27s/7703/max117335us;
both zero gaps>250ms/errors, six socketdrop deltas0 each. Run1 controlled
TEARDOWN sinkrc0, old processes exited/CmaFree17692KiB, then new session.
Run2 CPU73.82..78.87%, available18892..18972KiB/CmaFree7912KiB,
PCM advancing with startupdiscard/oneunderrun; not zeroaudio-loss claim.
No new OOM; old two events at7805..7808s correlate with boardtmpfs archive
and SFTP. No new board archive, /tmp188KiB. Currentplayer14603/watch14604/
sink14651 kept streaming. Hot reconnect not cold acceptance; no stable push.

## 2026-10-02 archived / successful runtime source snapshot uploaded
GitHubownerwnk64, snapshotbranch/tag-verified99bf218 remotely checked.
Kernelb3d8ed3/AIC53bec9e/player5685c63 complete-source refs verified.
Original source/archive docs7b55a6f, filename89c8d48, recovery7d881a6,
public metadata99bf218. Complete local archives/SHA256 passed, publicsubset
GITHUB_SHA256SUMS passed. LocalSDK/build prerequisites preserved; private
network configs replaced with templates only in archive. Full tarSHA256
84e97e0bb6be07a6313eeab5895805a942dc915a4f67f801a2dcc8604cfc22b2.
No board/source rebuild/new worktree/old stable ref overwrite. Pendingcold.

## 2026-10-02 planned / archive current successful runtime sources
User requests full-source GitHub snapshot; owner changedWnjbk->wnk64.
Add runtime/snapshot-20261002 documentation/archive tool/launcher recipe and
actual allowlisted runtime artifacts. No board deployment/new worktree.
Publish complete kernel/AIC53bec9e/player/sink independent refs to current
wnk64/licheepi_nano, preserve old stable tags; pending threecold acceptance.

## 2026-10-02 pending / full AV ON-OFF-ON results
Sourcea61ed97/build17357af, ff58d9fc/a683cf70 sameboot31071e77/CMA20.
ON1 1543.05s/OFF2354.73s/ON2 90.13s zero gaps>250ms/commiterrors.
Max184577/191762/117200us; content phases differ, no exact gain claim.
ON2 CPU70.32..77.13%, FIFO3.09..3.63%/123.51..140.35switches/s,
six socketdrop deltas0; MemAvailable18832..18860/CmaFree7912KiB.
Bluetooth PCM16627072bytes advancing; startupdrop140288/underruns0.
Keep VIDEO_RX_BATCH=1 for trial; old cold launcher still selects42d0cfa8.
Detailed RESULTS.md and frozen local ON1/OFF/ON2 evidence retained.
No new source/driver/kernel changes; threecold pending, no stable promotion.

## 2026-10-01 planned / one video handoff per received UDP batch
Planned7bea12c/sourcea61ed97. New exact byte/capacity/tail/invalid/mixedPID/
failure/control-stop/default tests and old byte/EPIPE ASan/UBSan pass. Mock800
and IDR explicitly ASYNC/RX_BATCH/COALESCE/VIDEO_RX_BATCH enabled pass.
Two ARM builds ff58d9fc8a47606e926575b65c6450ab, SSH versioned transfer verified.
Runtime plan: old42d0cfa8 retained, disconnect/wait terminal processes, same
a683cf70 player and20M boot31071e77, add only VIDEO_RX_BATCH=1 and sink path.
New emit counts represent batch handoffs, not per-RTP writes; report pending
and control-stop/error discard separately. No native frame/audio changes.
Base4283000/core42d0cfa8, original tree branchcandidate/rx-video-handoff-20261001;
exact baseline make -f Makefile.sink -B reproduces42d0cfa8. Rollback untouched.
Files miracast_sink_dump.c/tests/test_rx_video_handoff.c, no worker/audio changes.
Opt-in WFD_VIDEO_RX_BATCH=1 with asyncvideo accumulates <=16*RTP_MAX=32KiB
parsed H264 from already-received batch, flushes at end/capacity; never waits
for future datagrams. Existing128KiB queue/3ms worker/native90/pool9 unchanged.
Hypothesis reduce per-packet mutex/notifier overhead without bitrate/quality
reduction or disk capture. Count pending/peak/control-stop discard explicitly.
Tests exact bytes/order/invalid/mixedPID/capacity/tail/error/control-stop ASan,
old byte/EPIPE/RTSP/IDR, two ARM outputs; full AV actual cadence/CPU/RAM.
Already trialed CMA20 separately and positive; both handoff control and candidate
must use same20M, sameboot31071e77/playera683cf70/defaultBT/BS/AIC. Pending0/3.

## 2026-10-01 planned / reproduce current cold-start runtime
Follow-up for user-requested CMA20 trial: previous power transition retained
COM4 handle and ClearCommError failed. Change only host hub_cycle.py:
probe/close before OFF, fresh UART after ON, bounded read/open retries without
repeating power commands. Four mock tests pass: order, read retry, busy console
prevents power, missing OFF ack prevents ON. Actual UART capture still pending.
Core sink/player/protocol/driver unchanged. Record source commit before use.
Plan9de1e3e/sourcefed75fe, hosttimestampse2af014. Launcher MD5c312e492
SSH verified, shell syntax passes, artifact-only preflight passes. Cold mode
correctly refuses active runtime; /tmp confirmedtmpfs. Hot mode passes,
player11613/watch11614/sink11663, PCM73MB/advancing video/max169792us;
two CPU91.31/86.04%/drops0/0 samples, same original GO/services/phone media.
Saved prior stream2083.766915s(~34.7min)/90095commits,2gaps>250ms,
max251974us/errors0; better but not absolutely stall-free or optical proof.
Plan physical cycle1 only after graceful disconnect/terminal processes/sync,
provided hub helperCOM3 off/on5s with ack timestamps/COM4 boot capture.
Record exact new boot and all dependencies in COLD_START_TESTS_20261001.md.
Base25802ea, tag sink-before-cold-start-workflow-20261001, branch
candidate/cold-start-workflow-20261001 in original tree, no core changes.
Files runtime/cold-start-20261001/start-cast.sh plus exact active wpa.conf/
pbc-watch.sh/rtsp-watch.sh templates and host hub_cycle.py. Current WFD
subelements0/1/6/11 read from live WPA, GO5745, installed load scriptdec5f234
and memory helper0dcd3f87 preserved, no archived driver replacement.
Artifact preflight hashes sink42d0cfa8/playera683cf70/loaderbb2d68ed/fdrv334699cc,
firmware01acfbeb/protocol tools and templates; release5.7.1/native480x800/BS.
Cold refuses existing runtime; session waits old processes terminal; same
FIFO/raw800x48060/90/pool9/3options, route must Bluetooth pairedWt-070.
Connect via BlueZ D-Bus because this BlueZ5.48 bluetoothctl ignores one-shot
command args. No ordinary STA config printed/transferred, no boot integration.
Host delegates debug-hub skill power helper with currentCOM3, capturesCOM4,
verified OFF/ON acknowledgements and5s off; WMI denied, registry confirmed3/4.
Test shell syntax/artifact-only preflight/refuse active-cold/hot reconnect
before physical cold3cycles; protected BS SSH/BT/AIC/GO/native display/RAM.
Pending acceptance0/3; no stable push. Save only small diagnostic text logs.

## 2026-10-01 planned / opt-in monotonic FIFO coalescing
Later same-session validation: /proc8790/exe MD542d0cfa8 and /proc8743/exe
a683cf70 match selected artifacts. 25519successful commits, lifetime max
236346us/errors0 (longer than saved210s max120992us, not a contradiction).
Phone still connected. HCI UART UP RUNNING, TX405244844bytes/1138295ACL,
errors0 at one observation; single HCI snapshot alone is not audible proof.
Results FIFO_COALESCE_RESULTS_20261001.md. On1 240.450371s/10339commits/
43.00Hz/max156842us; on2 210.316681s/10760commits/51.16Hz/max120992us;
both zero gaps>250ms/errors0, not matched-phase or optical smoothness proof.
Same-binary off option: CPU92.96..95.29%, worker738..820switches/s vs on1
304..341 and on2 276..339/s. On2 CPU81.20..87.08% and sixdrop deltas0.
Full Bluetooth PCM written38.5->39.8MB, FIFO full0, no kernel/driver/HCI reset.
Off full player-log PSCP transfer assertion1525 failed/zero-byte result;
only actual SSH-observed max733317us and surviving sink log cited. No fabricated
off window/total. New immutable diagnostic snapshots transferred and removed.
Current8743/8744/8790 kept with all3options1; cold0/3/not stable/no push.
Next longer test plus exact cold-start pipeline/regressions, no further core
change until current improvement and protected dependencies are validated.
Additional test7ba416f (4KiB no wait + writer accounting) ran3 times ASan/
UBSan pass. Build record5f5621c. Live player7954/watch7955/sink8008, enabled1,
same boot/BT/WPA/BS/AIC. First six12.96..15.18s samples CPU82.94..89.31%,
bitrate5.701..6.063Mbps, all localdrop deltas0; sink worker6.51..7.00% and
304..341voluntary/s versus old611..676/s. PCM written24.3MB and rising;
FIFO full0, peak44064, submitted-written736bytes pending intentional wait,
not discarded. 50940RTP video submissions ->18705actual successful writes.
Early max video-commit gap156842us/errors0. Plan same-binary option0 control,
then restore option1; keep batch and loss-IDR on, no phone media changes.
Plan6d8a1fb/source df407c6, host off/on20lifecycle/full/EPIPE sanitizer tests
and 3ms-tail/50stop tests pass; mock800 and IDR pass with batch+coalesce.
Two existing Makefile ARM builds MD5 42d0cfa89120992a427abfde403770c9.
Additional test planned: wait for writer accounting after read (avoid test
race), then verify exactly4KiB queued does not enter timed coalescing wait.
No production binary change for this test addition. Deploy separate target
miracast_fifo_coalesce_20261001/sink-coalesce, retain c4cfdcdd rollback;
same a683cf70 player/fullBluetooth/native90/pool9, batch and loss-IDR enabled.
Base681cddb/c4cfdcdd, tag sink-before-fifo-coalesce-20261001, original tree
branch candidate/fifo-coalesce-20261001. Existing complete dependency/source
archive plus tracked receive-batch delta and retained c4cfdcdd rollback; exact
make -f Makefile.sink -B baseline reproduces c4cfdcddbedbb1851e65fbe0b2cb3e52.
Files wfd_video.c/tests/test_video_coalesce.c only, plus these records.
WFD_VIDEO_COALESCE=1 collects up to4096bytes with maximum3ms intentional
CLOCK_MONOTONIC condition wait, no deadline restart on spurious signals.
Signal on empty->data/threshold crossing/error/stop, not every tiny append.
Add success-write/signal/coalesce-block counters, no per-write logging.
Same128KiB queue/4KiB scratch/64KiB worker, no extra video buffers or files.
Preserve bytes/order/full-as-error/stop/EPIPE, source/audio/IDR/UDP batch,
native90degree player/pool9/BT route, BS/AIC/kernel/drivers/phone media.
Tests tail flush/exact bytes/50stop cycles plus existing20lifecycle/full/EPIPE
tests off/on ASan/UBSan, protocol/receive tests, two identical ARM builds.
Reconnection through ADB only, old-process terminal gate. Pending cold0/3.

## 2026-10-01 planned / bounded nonblocking UDP batch reception
Final evidence RECVMMSG_RESULTS_20261001.md. Run1 170.316938s/7730commits,
45.39Hz/0gaps>250ms/max200550us; run2 210.46676s/10136commits,48.16Hz/
1gap>250ms/max1083776us. Baseline636.235922s/25888commits,40.69Hz/
106gaps>250ms/max1784684us; not frame-aligned percentage proof. Errors0.
Full audio persisted through both reconnects, fallback0/truncated0/full0.
Run1 six localdrop deltas34/0/0/4/0/0; run2 55/8/0/28/0/1. Run2
CPU86.41..93.03%, sink main10.83..12.62%, worker7.41..9.54% and~598..776
voluntary switches/s. Same kernel/drivers/BT route/BS SSH/phone settings.
Current6796/6797/6846 kept; clear improvement but occasional1s stall remains.
Decision pending; no stable promotion, cold0/3. Next bounded FIFO coalescing.
Live first run player6442/watch6443/sink6497, same boot ea1b461e, ADB
connection succeeds and restores media. Header reports batch16/fallback0.
First53k datagrams/3979receive calls (~13.4/call), truncation0. PCM written
16846208 continues, video40853528bytes submitted=written/full0. Initial
three13.43..14.01s samples CPU89.72..96.06%, localdrops34/0/0 at5.515..5.963Mbps;
baseline12.81/12.51s CPU97.50/95.59%, drops330/437 at6.061/6.033Mbps.
Early max real video-commit gap200550us versus baseline1784684us; errors0.
Content phases are not exact frame aligned; improvement promising but pending,
not smoothness/long-duration/cold-boot proof. Plan same-candidate reconnect
to confirm full audio reopening and sustained native output, no driver reset.
Plan1441615/source5dc6425. Actual socket helper and original RTP-byte/EPIPE
ASan/UBSan tests pass; mock800 M1-M7 and rate-limited IDR tests pass with
WFD_RTP_BATCH=1. Profile first invocation lacked required800 argument and
failed wrapper only; corrected invocation and full rerun passed.
Two ARM builds c4cfdcddbedbb1851e65fbe0b2cb3e52, existing Makefile/toolchain.
Planned SSH candidate /root/aic_miracast/candidates/miracast_recvmmsg_20261001/
sink-recvmmsg. Baseline273b1d0a untouched; same a683cf70 frame-gap player.
Runtime watcher receives WFD_RTP_BATCH=1/WFD_LOSS_IDR=1 via environment;
wait old sink/player/watcher terminal before starting and phone reconnect.
Original tree branch candidate/recvmmsg-20261001 from21afbcd, immutable tag
sink-before-recvmmsg-20261001. Existing full source/dependency/binary archive
miracast_loss_idr_20261001/candidate; exact baseline reproduced with
make -f Makefile.sink TARGET=/tmp/f1-rx-batch-baseline -B:273b1d0a.
Files miracast_sink_dump.c/Makefile.sink/wfd_rx.h/tests/test_wfd_rx.c.
WFD_RTP_BATCH=1 uses recvmmsg maximum16 already-available datagrams, no
batch-fill wait. Default recvmsg1; ENOSYS fallback1. Reset per-slot metadata
and retain per-datagram SO_RXQ_OVFL. Reject truncated invalid datagrams.
~34KiB bounded receive storage, no threads/queue growth/disk video capture.
Hypothesis: reduce select/recvmsg syscalls/CPU/localdrops/output stalls.
Protected: packet order, payload bytes, RTP/PES/audio/IDR/RTSP, FIFO, same
frame-gap player/native90degree/pool9, BT route, BS SSH, AIC/kernel/drivers.
Tests actual UDP sockets/order/empty/zero/truncation/reuse/fallback sanitizer,
existing byte-equivalence and mock RTSP/IDR, two identical ARM builds, liveAV.
Rollback existing273b1d0a; no stable push, threecold0/3 pending.

## 2026-10-01 / planned / bounded WFD LPCM to ALSA default
Base67ad37c/b96fa528, tag sink-hh800-before-audio-20261001, complete source
and binary archived under miracast_audio_20261001/before; original tree kept.
Existing M4 already selects LPCM48k stereo; reuse current video TS/FIFO path.
Add bounded TS/PES LPCM demux (PID1100, 65541byte PES ceiling) and separate
ALSA nonblocking worker (16KiB/85ms PCM ring, 64KiB stack,40ms requested buffer).
PCM framing verified against AOSP ESQueue::dequeueAccessUnitPCMAudio:
a0/numAUs/reserved/11, 80 samples per AU, stereo16bit big endian.
ALSA plug performs endian conversion; default selects connected Wt-070 A2DP.
Opt-in WFD_AUDIO_ENABLE=1, no audio files/FIFO backlog/no AAC decoder; drop
old audio on overflow rather than block video. Stop/drop instead of drain.
Tests: all184 header split points, duplicates/CC loss/resync/TEI/invalid
format/fuzz, ASan/UBSan and ALSA-null threaded lifecycle; unchanged M1-M7.
No kernel/DTB/CMA9MiB pool/rotation/driver/audio-route changes. Live source
has already sent TEARDOWN, sink0; no agent stop/reboot. Need phone reconnect
and audible/sync/memory/long-run verification; three cold cycles pending0/3.

Worktree: /home/wnk/LicheePi_Nano/third_party/lazycast_host_20260721
Component: Miracast sink
Base: 4f4a2cc plus previously untracked miracast_sink_dump.c
Purpose: preserve exact 640-stage source before 800x480 negotiation candidate

## 2026-10-01 / baseline established / reproducible lowest sink
Existing source MD5f1a08e9f4fc8e645842bd78f400ec127; standalone libc-only
program, original directory retained. Buildroot2018.02.11 GCC6.4.0:
arm-buildroot-linux-gnueabi-gcc -Os -fno-builtin -march=armv5te
-o miracast_sink_dump.lowest.brarm miracast_sink_dump.c.
Exact resulting MD5ceeeb9958f8f84f2a0b7c6c3df63db0e matches active board.
Initial default-architecture builds differed; do not infer source identity
from adjacent files. With explicit armv5te it is byte-reproducible.
Track original source and Makefile.sink before edit; preserve unrelated files.
Archive full Git bundle, original source/build recipe/exact binary separately.
640 stage uses player e1d6fed2, #250/fbdev100/CMA24MiB, current AIC rxguard,
5745MHz GO and FIFO. User requests stage backup, not stable certification.
Board snapshot MD59a797dc49737a464135f25f07122bf83 in Windows and VM verified.
No video capture or private management Wi-Fi credentials archived.
No live process interrupted yet; no new worktree or GitHub push.

## 2026-10-01 / planned / prefer handheld800x480p60
Base05324af/tagmiracast-sink-640-stage-20261001, exact binaryceeeb995.
Original worktree branchcandidate/miracast-hh800-20261001, no new worktree.
Only M3 wfd_video_formats native00->0a (HH1), HH bitmap0->2 (800x480p60).
Keep mandatory CEA0 fallback and all codec/RTSP/RTP/PES fields unchanged.
Mode mapping verified in existing miraclecast src/ctl/wfd.c HH index1.
Phone M4 and decoded picture must actually select800x480, never count640
fallback as800 or scale it. Isolated runtime watcher only changes SINK path.
Use existing e1d6fed2 player raw80048060/rotate90/native480x800, pool9MiB
to accommodate larger frames; CMA24MiB/kernel#250/DTB/AIC/BS/BT unchanged.
Record pool peak/fallback and ordinary RAM after phone starts; 9MiB is a
candidate budget, not a guarantee. Restore immutable640 snapshot on failure.
Tests: byte diff, deterministic ARM builds, mock M1-M7 negotiation, realphone.
Decision: pending, no stable certification or GitHub push.

Sourceaaef280, ARM sinkb96fa52854a2255555ab40c9c771496d, watcher27055c3b.
Two builds identical; native mock M1-M7 for640 and800 profiles pass, no video
acceptance inferred from mocks. Isolated board candidate deployed, oldceeeb995
and640 snapshot untouched. Complete640 checkpoint248MiB SHA25665fb6546fbd445447aa5a3beafc6975a3b78296bfc6d34c5237314da83b901f1
verified in Windows/Ubuntu before switching. 640 sink exits0, poolpeak6799360,
fallback0/live0; no board reboot. DHCP7001 blocksrecv onTERM, scoped KILL after
checking executable; no DHCP code/config changes. New GO5745MHz DIRECT-Ij;
player12679/sink13259, pool9MiB/raw80048060/rotate90/native480x800.
User confirms success. Real phone M4 HHmask00000002/CEAmask0; decoded frame
480x800 native/no scaling. Same#250/bootea1b461e-c82b-4fec-9d32-5256689ece80.
Streaming snapshot MemAvailable20008/MemFree14788/CmaFree9968KiB;
ordinary free4820KiB, Slab12592/SUnreclaim11312KiB; compared64022292KiB
available, decrease2284KiB. Pool reserved9MiB, peak/live usage not measured
without stopping; no claim of peak margin from CmaFree. Keep stream active.
Decision: user-confirmed initial800 success; duration/reconnect/three cold
cycles still pending0/3. No audio pipeline change, no GitHub stable push.
# 2026-10-01 audio candidate deployment (pending)
Source commit: 1e92840, branch candidate/wfd-lpcm-audio-20261001.
Host ASan/UBSan/LPCM fragmentation/fuzz/ALSA-null lifecycle tests passed;
mock M1-M7 HH800 profile passed. Two ARM builds byte-identical.
Board sink MD5: 2abf061085a4f828af9b6c1c6124bb1a.
Watcher MD5: 42f8ae3cba198044164ba043f38c24cf.
Deployment: /root/aic_miracast/candidates/miracast_audio_20261001/.
Pure-video watcher MD5: 27055c3b9291a15f35d13bc4793ce577, preserved at
/root/aic_miracast/candidates/miracast_hh800_20261001/rtsp-watch.before-audio.sh.
Original video sink b96fa52854a2255555ab40c9c771496d remains untouched.
Rollback: after graceful candidate shutdown, restore that watcher to
/tmp/manual_miracast_20260920/rtsp-watch.sh and restart player/watch for
a fresh phone connection. Do not copy a watcher over a live watcher process.
Current player16712/watch16713; existing WPA6960/DHCP12705/PBC7006 retained.
Player e1d6fed2, pool9MiB/raw80048060/rotate90/native480800/FIFO unchanged.
Default route is Bluetooth Wt-070 12:11:71:41:9C:4A.
No kernel/driver/DTB/CMA-total/audio-route changes; no reboot/active-stream kill.
Real phone reconnect, LPCM headers, audible output, sync, RAM and stability
are unverified. Three complete cold boot cycles remain 0/3, no stable push.
Helper final diagnostic failed because ip is absent after launch; processes
and artifact hashes independently verified. No launch failure inferred.
# 2026-10-01 planned: RTP-scoped FIFO write batching

## 2026-10-01 planned: loss-triggered rate-limited IDR recovery
HCI down/up + paired connect restored full audio on baseline99a700f8:
player886/sink930 wrote>10MB PCM, HCI TX increased; exact driver/firmware
cause remains unknown, interface reset is a runtime recovery, not source fix.
Baseline normalized samples14.10/12.50s:6.026/5.712Mbps, CPU91.23/86.20%,
local drops102/210. CMAfree4228KiB includes dynamic movable/cache usage;
no leak conclusion from that metric alone. Same HCI init applied to next test.
IDR full-A/V player1308/watch1309/sink1354, WFD_LOSS_IDR1, sink273b1d0a,
same idle-player0bd4666c/native80060/pool9/rotate90; phone original task.
Actual source accepted recovery CSeq176/177/178 and later351/691..695
with200OK; counters reached592 requests, missing13483==socket_missing13483,
duplicate/reordered0. Decoded keyframes increased (r3=215 at114688calls).
Audio written194241024bytes at103466PES, async video written=submitted,
peak32496/full0; continuous audio now real, not the earlier failed-audio test.
Full samples13.07/12.00/13.27s:6.120/5.066/5.771Mbps, CPU96.69/91.68/92.96%,
drops328/161/259, RAM~17.8..18.0MiBavailable/CMAfree6164KiB. Different
video phases prevent strict A/B %; loss not reduced, possible CPU cost.
Decision: protocol recovery verified on phone, full stall improvement pending.
Do not call video-clock-late equivalent to actual frozen-display duration:
raw FIFO synthesizes PTS60 even with variable source cadence. Next measure
successful frame-output gaps and reduce receive/IPC overhead before accept.
Current full-A/V stream remains active; source/rootfs/module unchanged other
than isolated candidate/helper. HCI recovery and supervisor restart documented,
all temporary command wrappers removed. Threecold0/3, no stable push.
Isolated1s defaultPCM probe was interrupted after3s (rc1), HCI TX+15575bytes/
ACL+45; not proof zero transmission, but didn't complete normally. timeout
utility absent, first attempt didn't run; owned-PID bounded wrapper used.
Verified BlueALSA252 executable beforeTERM, supervisor207 restored32232;
paired session recreated. IDR candidate rolled back to99a700f8, same idle
player0bd4666c; player32260/sink32302 still failed audio after383616bytes.
Confirms issue not dependent on IDR code. Phone Bluetooth reported state
disconnected (speaker name cached), no evidence competing phone A2DP.
UARTserial physical1C25800 RTS|CTS, HCIerrors0; observations not root cause.
Plan standard hci0 down/up and paired reconnect only to clear controller
session state, no module/firmware/kernel/source changes. Preserve current
video-only cast while reset; full A/V test requires reopening audio afterward.
Paired DeviceDisconnect/Connect succeeded but thirdcastplayer30929/sink30975
audio still failed after180224bytes. /run/audio-bluealsa.log contains BT
socketETIMEDOUT followed by transportRelease UnknownObject. This indicates
failure below sink PCM; HCIerrors0/Connectedtrue are insufficient health.
IDR remains unexercised (requests0/loss0 after audio disabled). Plan isolated
one-second ALSA S16_LE silence with6s timeout, compare HCI counters/backend
errors, video retained; no phone/media/driver/service changes. If backend
fails independently, restore baseline sink and diagnose Bluetooth separately
before judging recovery candidate, not accept video-only as full goal.
Initial deployment was blocked because old player still exiting; PowerShell
then incorrectly proceeded to phone connect, which timed out. No candidate
started then. Wait-for-old-exit + fail-fast host checks added; old process
later proven gone. Deployed273b1d0a/record47db517, firstplayer29845/sink29892,
then same-version retryplayer30386/sink30431. Both audio attempts failed
after~76KiB with BlueALSA FIFO EPIPE/ALSAENODEV; audio output disabled.
Video still runs, local socketdrops0, recoveryrequests0: invalid A/V comparison,
not evidence IDR fixed stalls. Before full-A/V samples~6Mbps/CPU90%/+120,+90
drop windows; failed-audio after samples3.5..6Mbps/CPU49..82%/drops0.
BlueZ DeviceConnectedtrue, BlueALSA PCMsource48k2ch visible, HCIerrors0;
does not prove usable transport. Runtime plan reconnect only paired speaker
12:11:71:41:9C:4A to recreate transport, then reopen same cast. No service,
driver, kernel, phone media/orientation/loop changes. Preserve failed logs.
Sourceb85e58e/plan7ccd593; policy wrap/SSRC/cooldown/100k packets and
RTP byte/EPIPE sanitizer tests pass; mock800 M1-M7 and gap+source200+
duplicate/reorder/cooldown tests pass. Two ARM builds identical
273b1d0a7beec8b0aebb60b17bbf319e. Not deployed yet. Add sampler NIC rxMbps
counter and fixture for I-frame bandwidth comparison (read-only host tools,
no sink binary effect), unit validation before use.
Baseeab7388/async99a700f8 exactly rebuilt before edit; complete source,
Makefile/dependencies/bundle/binary in miracast_loss_idr_20261001/before.
Original tree branchcandidate/loss-idr-recovery-20261001, no worktree.
Hypothesis: lost reference frames stall display until next source IDR;
source only receives initial IDR request today. Opt-in WFD_LOSS_IDR=1:
track RTPseq/SSRC, trigger on forward gaps or socket RXoverflow, maximum
one request per1000000us including initial request cooldown. Duplicate/
old packets do not advance high-water mark; wrap/SSRC restart accounted.
No H264 filtering/drop/rescale, no queue/driver/player/kernel change; keep
idle-player0bd4666c and user landscape/loop video for controlled comparison.
Tests pure policy wrap/duplicate/reorder/SSRC/overflow/cooldown, sanitizers,
mock M1-M7+RTP gap/source200 responses; final double ARM build. Live verify
source replies, decoder-keyframe counts, stalls/drop/CPU/RAM/bandwidth.
Recovery does not reduce network loss and can increase I-frame traffic;
rollback old99a700f8 if regression. Threecold0/3, no stable/push claim.

## 2026-10-01 planned: unattended cast-only session and read-only sampler
Plan a1747b7/fix7f9f600/tools e6d2840 on candidate/cast-auto-test-20261001;
CLI connect/disconnect/status tested on phone dda57287, same target, BACK
restores original media task. HostPython>=3.6 help checks and two sampler
fixture/delta/socket-identity unit tests pass. Corrected live sampler reports
actual inode161375, PIDs26339/26383, two+five windows collected without
restarting/interrupting stream. Source helpers are Windows-side tools only,
not board startup changes; keep for following goal turns. Complete bundle
at /home/wnk/F1C200S_archives/cast_auto_test_20261001/candidate/source.bundle.
Host tools require Python>=3.6 (f-string syntax); use existing WindowsPython,
not VM16.04 Python3.5. First help check on VM failed before source commit;
do not install or mislabel that as app/build failure. Local host help/tests
used instead. Sampler first trial parsed UDPrefcount2 as inode; corrected
column9, added fixture and identity-change tests. Old trial file is not
validated socket-identity evidence; rerun corrected sampler for comparisons.
Add tools/adb_cast_session.py, tools/board_sample.py on original sink tree,
base10d2789/async99a700f8; no sink/runtime binary change. Session helper uses
actual enabled target bounds, disconnect confirmation, remote-display-state
checks, then BACK to prior media. Never changes video/rotation/loop settings.
Read-only sampler validates live PIDs/socketinode; measures aggregateCPU,
thread ticks/voluntary switches, socketdrops/queue-accounting and RAM.
Changed socket/PID or failed read is not automatic restart permission.
Session connect/disconnect/status already exercised on live phone; target
remainsF1C200S-AIC98:a1:4a:08:13:6a, serialdda57287. Sampler test current
idle-decoder candidate without changing stream. Tools intentionally retained
for next automated comparison; source/Git/Chinese records required.

## 2026-10-01 / ADB-control verified / original candidate unchanged
Plan promote tested local ADB-shell transport helper into tools/adb_cast_control.py
on existing tree, no new worktree; existing authorized server5037 only, no
new adb keys/config/service. Native CLI fails android-home, protocol works.
Phone dda57287/model23013RK75C: openCAST_SETTINGS, dumpXML, choose enabled
F1C200S-AIC98:a1:4a:08:13:6a bounds76,759..387,825, tap350,817 succeeds.
Restart ended player/sink with unchanged async99a700f8, same watchdog/runtime.
Player22623/watch22624/sink22715; phone activeDisplayState2 and
mRemoteDisplayConnectedtrue; RTSP negotiated, player decoded native480800.
Settings-page stream first4096RTP socketdrops0/audio discontinuities0,
queue peak1808/full0; this low-motion stream is not a video-stability result.
No driver/kernel/CMA/player/source changes besides helper/docs. Current
stream remains active. Phone dump under/data/local/tmp removed per read.
Local one-off probe/start command removed; reusable helper intentionally
retained for next automatic reconnect/test, use Python and -- before --ui.
Rollback same unchanged candidate; threecold0/3 and stall fix unaccepted.

## 2026-10-01 planned: bounded asynchronous FIFO delivery
Deployment: user disconnected, old sink20548 exited0. New player21525/
watch21526 waiting new DHCP after11lines; sink99a700f8e1592a68d06d608ff983dfdd
and watcher2ad42f12091f64f548df07dc36fe9e8f verified overSSH. Build/docs3f26038.
Candidate /root/aic_miracast/candidates/miracast_async_fifo_20261001/.
Rollback old sink-burstf8a1058c untouched; watcher rtsp-watch.before-async.sh
retains WFD_SINK override, so restore with WFD_SINK pointing to old sink-burst
(otherwise defaults to observer). Save pretest logs sink-burst.log/player-burst.log.
Runtime/tmp only, no reboot or driver/boot-service changes. Live queue/CPU/
RTP-drop/A/V performance still pending. Temporary helpers cleaned after use.
Implemented1de102b, exit-status fix083544c (explicit failed output returns1,
not normal0). Host20lifecycle/exact-order/wrap/full/EPIPE/stop tests pass
ASan/UBSan/leaks; existing RTP batch sanitizer and mock800 pass. Final ARM
two builds equal99a700f8e1592a68d06d608ff983dfdd; final host regression rerun
before staging. Old intermediate22b0eab is not deployed. g_h264_bytes in
async mode counts submitted bytes; async written counter is actual output.
Watcher enables WFD_VIDEO_ASYNC1 and chooses versioned sink by WFD_SINK;
old runtime unchanged awaiting user disconnect, no force kill/reboot.
Base7bc75cf/source548f939/f8a1058c and complete archive retained.
Original branch candidate/async-fifo-20261001, no new worktree. Hypothesis:
FIFO writes in receive loop delay audio/RTP; independent output worker can
absorb bursts while preserving exact stream bytes. Opt-in WFD_VIDEO_ASYNC=1.
128KiB ring +4096byte in-flight block, 64KiB thread stack; nonblocking write
and50ms poll, cancellable stop/join. Queue full causes explicit candidate
failure, never silently discard H264 fragments. Stop may discard queued
tail after disconnect; report it. No video file/unbounded cache added.
Keep800/native90/player/CMA/audio/driver/sysctl unchanged, diagnostic receive
counters and360448socket capacity retained for same-base comparison.
Tests byte order/wrap/slow-consumer/full/EPIPE/idle-stop/full-pipe-stop,
ASan/UBSan/leaks and800RTSP; no acceptance from tests. Live reconnect needed,
monitor queue peak/drop/CPU/RAM/A/V delay. Rollback sink-burstf8a1058c.
Shared FIFO/player/network/audio scheduling; threecold0/3, no stable push.

## 2026-10-01 planned: observe live FIFO/receive stalls
Observed live: phone TEARDOWN/sink0, 21822RTP packets, socket drops1550;
receive gap max108226us. FIFO single-write max19838us, no >20ms, interval
write totals sum12.97s. Cannot establish multi-second FIFO blockage from this.
Pool peak8437760/cap9437184/fallback0/live0, clean lifecycle, no OOM.

## 2026-10-01 planned: bounded per-socket receive burst capacity
Source548f939, plan178de23, watcher89d3c9e; ASan/UBSan byte/EPIPE and
mock800 pass, two ARM builds equal f8a1058ccde51d0f44c8ac2f2e718cc9.
Deployed versioned sink-burst, remote hash verified, watcher64148622ebaaede823b023a7dc9e7ec0.
Player20413/watch20414/sink20548, phone connected, originalWPA/DHCP/PBC.
Actual SO_RCVBUF effective360448 confirmed in log. No sysctl/driver changes.
Uptime12539.69..12549.93 (10.24s): socketinode111433 drops1390->1522
(+132), CPU1009ticks/user330/system569/softirq1/idle110 =>89.1%busy.
Intervals include FIFOmax30587us, receivegap98221us; syscall wall duration
includes scheduling, not proof all that time is pipe-full blocking. Drop
counter cumulative1436 in ancillary, proc1522; no contradiction (async).
MemAvailable18992KiB, CmaFree9264KiB/total24576; video-clock late warnings.
Decision: insufficient to fix frequent stalls; not accepted. Different
content/time means no controlled percentage improvement from previous132/190.
Current stream kept active, no rollback interruption yet. Before next source
candidate, separate hypothesis and baseline required. Proposed next scope
is bounded receive/output separation; no unbounded/disk video buffering.
Rollback observerf2d95119 and candidate/rtsp-watch.before-burst.sh retained.
Base6c64cc1/dc42acc/f2d95119, exact source/build archived. Original tree
branch candidate/rtp-receive-burst-20261001, no new worktree. Only set
SO_RCVBUF requested180224 before bind; board default/max180224 and Linux
doubles explicit request, expected effective360448 (verify getsockopt).
No sysctl change; kernel memory is demand-driven, not all upfront; cannot
claim added video RAM equals requested socket-accounting bytes. Existing
FIFO/audio queues unchanged; buffering may add latency when overloaded.
Hypothesis: absorb ~100ms short stalls, not cure sustained CPU overload.
Keep diagnostic counters and test same media drops/gap/CPU/RAM/audible sync.
Rollback f2d95119 and observer watcher. Threecold0/3, pending, no stable push.
Deployment now active after user disconnect: player19151/watch19152, waiting
DHCP after7existing lines. Remote sinkf2d951196746d8ada684310d8c53f1f6,
watcher53c679dbce544d9d1a7a8e998ae7f0ff verified. Existing WPA6960/DHCP12705
and PBC retained; defaultBluetooth unchanged, no driver/kernel/reboot changes.
Rollback watcher copied to candidate/rtsp-watch.before-observe.sh, old
sink/player logs preserved there. Runtime/tmp only, no startup integration.
Performance diagnosis pending fresh phone stream. Temporary deploy helper
removed after successful execution.
Source dc42acc (plan e1a209e), ASan/UBSan batch-byte/EPIPE and mock800
tests passed; two ARM builds identical MD5f2d951196746d8ada684310d8c53f1f6.
Artifact staged and SSH remote MD5 verified, current live sink-batch18173
is not replaced yet. Waiting user disconnect; no stop/reboot. New watcher
candidate only changes sink path; original installed watcher is rollback.
Base3aff659/sourcea282376/db9732dd, exact source bundle and binary retained.
Branch candidate/rtp-stall-observe-20261001 in original tree, no worktree.
User reports frequent stalls; batching alone insufficient, not certified.
Only sink timing/counters: monotonic FIFO write duration and receive gaps,
socket-local SO_RXQ_OVFL ancillary counter via recvmsg, periodic summaries.
No new buffering, thread, socket capacity, scheduling priority, video codec,
player, driver or CMA change. Stats every4096RTP packets, not per-packet logs.
Test old byte-output/EPIPE sanitizer tests, mock800 RTSP, deterministic ARM.
Rollback sink-batch/db9732dd and current watcher remain untouched. Live
reconnect needed to deploy diagnostics; optimize only from measured evidence.
Shared dependencies FIFO/player/audio/network scheduling, threecold0/3.
Deployment verified: sinkdb9732dd9062657d84465281a9186d99,
watcher0c84cef40e4190957f423f4add99c982. User disconnected; old sink16874
exited0 and player released VE. New player17760/watch17761 waiting DHCP
after5existing lines; no forced kill/reboot/driver/service change.
Logs: /tmp/manual_miracast_20260920/player-batch800.log and sink.log.
Runtime under/tmp, not integrated into boot image. Pending reconnect and
performance comparison. Build/results record commit6bfaaab.
Source commit a282376, plan102e26d. ASan/UBSan exact-output/EPIPE tests pass,
mixed video/nonvideo TS six writes become one; short final RTP flush verified.
Mock800 M1-M7 passed; twice-built ARM MD5db9732dd9062657d84465281a9186d99.
Candidate staged to board versioned directory with matching MD5. User has
disconnected; switch uses same DHCP/PBC/protocol/player, only sink path.
Rollback old audio sink2abf0610 and rtsp-watch.before-batch.sh preserved at
/root/aic_miracast/candidates/miracast_rtp_batch_20261001/.
Runtime performance unmeasured; do not claim CPU or drop reduction from mocks.
Base f6bd85a/source1e92840, sink2abf0610, exact original source/build retained.
Original tree branch candidate/rtp-fifo-batch-20261001, tag
sink-audio-before-rtp-batch-20261001; archive miracast_rtp_batch_20261001/before.
User requests optimization after audible success with intermittent stalls.
10.62s diagnosis: CPU92% busy, board UDP RcvbufErrors+565; not WLAN proof.
Change only sink H264 output batching within each RTP packet, <=RTP_MAX
static scratch bytes; flush immediately per packet, no interpacket delay.
Keep PES extraction byte-identical, audio worker, RTSP, driver/player/CMA
and resolution unchanged. Test exact bytes across mixed TS/headers/tails,
write-count reduction, EPIPE and invalid RTP under ASan/UBSan; mock M1-M7.
Retain old board audio sink/watcher; runtime switch requires reconnect.
Shared dependencies: FIFO/player scheduling, audio delivery, network receive.
Acceptance: real A/V/counters/RAM and three cold cycles pending0/3.
