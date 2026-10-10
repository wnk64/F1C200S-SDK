# Worktree Change Log

## 2026-10-10 planned / opt-in bounded display scheduling
Base5685c63, original repository SOFTWARE/cedar-player, source files clean; generated binaries/objects dirty and preserved. Existing player MD5a683cf70a95ba2759b5c9d2b1e0dc016 matches last deployed frame-gap player. Complete tracked source/config/build recipe and existing rollback archives retained.
Hypothesis: draining multiple framebuffer updates and blocking frame returns before ioctl causes avoidable burst coalescing/stalls.
Files: player-src/driver/drm_warpper.c, render/mediaplayer.c, tests/test_display_bounded.c, build-display-candidate.sh, Chinese change record.
Candidate: CEDAR_DISPLAY_BOUNDED=1 only; default legacy branch unchanged. One command per layer per vblank; pending ioctl retry, old frame retired only after next successful vblank, nonblocking frame returns, bounded ownership.
Counters: CEDAR_PIPELINE_STATS=1 reports raw access-unit submissions and decoded/enqueued/queue-dropped frames separately. Default probes off; no new thread or heap video buffer.
Protected: existing raw pacing/PTS, geometry, decoder rotation/CMA config, sink/audio, kernel/DTB/modules/Wi-Fi/BT/menu. No board access or deployment.
Build: separate candidate output directory, no clean/default tracked-object overwrite. Rollback: deployed binaries/scripts remain unchanged; unset new flags.
Tests planned: host fake-ioctl ownership/backpressure/retry/teardown sanitizer tests and existing lifecycle/frame-gap tests, ARM build. Physical playback/three coldboots pending.

## 2026-10-10 tested offline / bounded display candidate
Source commit68a13c4. sh player-src/test-display-candidate.sh: ASan/UBSan leak-enabled100ownership/retry/backpressure/control/cleanup runs PASS,50blocked-worker/idempotent lifecycle runs PASS,frame_gap PASS. These are host mocks, not hardware scanout validation.
Build: sh player-src/build-display-candidate.sh, original Buildroot GCC6.4, explicit relocated sysroot/CedarX include, no warnings; repeated outputs SHA256188829c65bdff014c6e8e826fb41ac8fd43374ce1ae11776381efe0cb0b65eb6.
Artifact: player-src/build-display-bounded/player-display-bounded. Original tracked objects/executable untouched; original executable MD5a683cf70a95ba2759b5c9d2b1e0dc016 remains.
Probe use for future live comparison: CEDAR_DISPLAY_BOUNDED=1 CEDAR_PIPELINE_STATS=1 CEDAR_FRAME_GAP_STATS=1 plus existing pool9/rotate90/raw80048060 settings. Baseline comparison omits DISPLAY_BOUNDED. No launcher/preflight hashes changed or board access attempted.
Caveats: extra retained rotated frame may cause NO_FRAME_BUFFER; one-per-vblank can increase queue latency during bursts; per-layer ioctls are not cross-layer atomic; real private-ioctl/vblank buffer lifetime must be verified on board. Default remains legacy. Stream PTS loss, AUD-only parsing and gettimeofday pacing unchanged.
Decision: pending hardware A/B and full dependent regressions/threecoldboots; no performance or stable acceptance claim. No one-off helper scripts created; durable build/test tools retained with source.

## 2026-10-01 planned / opt-in successful video commit intervals
Results: diagnostic source940fdc5, host summary305e685 (3 Python tests pass
on Windows and VM; zero-frame windows after first commit included).
IDR-on sample125.250343s:4842 commits/38.66Hz,25 gaps>250ms,max1266585us.
IDR-off sample130.284807s:4502 commits/34.56Hz,16 gaps>250ms,max3983463us,
sampled idle3349888us. Both errors0/audio sustained; no strict same-frame
alignment, so not an exact performance-ratio claim. Off CPU84.02..96.02%,
localdrops35..178 per12.27..17.37s, bitrate4.675..5.93Mbps.
Decision: retain recovery IDR; lower drop counts alone do not mean smoother
playback. Restore WFD_LOSS_IDR=1, player5566/watch5567/sink5617, same boot
ea1b461e-c82b-4fec-9d32-5256689ece80 and protected drivers/BT route.
Restored session already reports max1784684us, so no stall-free claim.
Live diagnostics remain opt-in and pending; cold acceptance0/3/no push.
Next hypothesis: bounded UDP receive batching reduces select/recvmsg overhead
without changing packet order, RTSP fairness, payload bytes or FIFO format.
Live candidate4862/sink4948 connected through ADB with existing phone media.
Native output480x800/pool9MiB confirmed, sustained LPCM writes, FIFO full0.
Successful-commit diagnostics show genuine gaps up to1266585us, errors0;
three 12.92..13.11s samples CPU94.94..96.38%, RTPdrops385/443/264 at6.1Mbps.
No scanout acceptance claim. Plan single-variable comparison: disable only
WFD_LOSS_IDR recovery requests; same player/sink binaries/audio/initial IDR.
Retain IDR-on logs locally; add read-only host summary with two unit tests.
Plan003456e/source940fdc5. Host GCC ASan/UBSan assertions pass. Two ARM
make-B-j4/strip outputs byte-identical a683cf70a95ba2759b5c9d2b1e0dc016.
Planned runtime: CEDAR_FRAME_GAP_STATS=1 with existing rotate90/pool9/default
and raw800x48060 FIFO. Original player0bd4666c retained, candidate distinct
/root/aic_miracast/candidates/player_frame_gap_20261001/player-frame-gap.
Disconnect through ADB only, wait old process terminal before starting;
same loss-IDR sink273b1d0a and same WPA/GO/BT route. No driver reset.
Base50426d5, original tree, branch candidate/frame-gap-stats-20261001.
Existing complete rollback archive player_decoder_idle_20261001/candidate
contains source.bundle/source-objects.tar.gz/player-idle. Baseline rebuilt
make -C player-src -B -j4; make -C player-src strip: exact MD5
0bd4666c69b494c0a9288a32be9cad63. No new worktree or redundant backup.
Files: driver/drm_warpper.c, utils/frame_gap.h, tests/test_frame_gap.c.
CEDAR_FRAME_GAP_STATS=1 records wall-clock successful YUV video ioctl gaps,
not synthetic PTS lateness. Count once per commit even with multiple mounts.
Five-second windows plus current idle age/lifetime max/error count.
Accepted ioctls do not prove physical scanout or visual content changes.
Unchanged: pacing, decoder, audio, geometry, rotation, pool, FIFO, kernel,
AIC, BS, BT, phone media/orientation/repeat. Pure helper sanitizer tests and
repeat ARM build precede versioned deployment; no stable promotion/push.

## 2026-10-01 planned: decoder idle backoff from original player
Source1e28c1a/planefe169e. Two consistent make-B-j4/strip ARM builds equal
0bd4666c69b494c0a9288a32be9cad63, SSH versioned deployment verified at
/root/aic_miracast/candidates/player_decoder_idle_20261001/player-idle.
Idle-FIFO SIGINT test returned0 and releasedVE/CMA, no forced shutdown.
ADB automatic disconnect old sink25048 normal0, then newplayer26339/
watch26340/sink26383, unchanged async99a700f8/GO/WPA/DHCP/PBC/BT.
Main exit diagnostics included for future EPIPE, not an EPIPE root-cause fix.
Live first10s CPUdelta1028/user295/sys633/idle100 (~90.3%busy), RTPdrops
475->583 (+108). Decoder26349cpu51ticks (~5%) /3296voluntary switches
over~10s (~330/s) versus old24795 ~21% /1758/s; same video but not frame-
aligned controlledA/B, no exact overall performance percent claim.
Results at20480calls: r0=1/r1=1824/r2=9/r3=13/r4=0/r5=18633/r6=0;
~91% NO_BITSTREAM polls confirms idle-loop hypothesis. Native geometry,
bitstream semantics, PTS, successful frame pacing and display unchanged.
Read-only corrected sampler two windows12.82/13.80s: CPU88.72/75.32%,
socketinode161375 drops+81/+152; decoder~5.09/4.83%, switches356/341s.
Five later11.85..13.51s windows CPU86.99..92.88%, drops41/139/83/100/96,
decoder4.85..5.38%, switches295..373/s. Videoqueuefull0, still late-clock
warnings/audio discontinuities; connected after sampling, RAM~18.3MiB
available/CMA~10.5MiB free. These are not proof of visually stall-free A/V.
Decision: idle-poll work reduced, useful candidate, full stall fix unaccepted.
No phone video/rotation/loop changes, no kernel/module/boot changes or stable
push. Rollback e1d6fed2 remains; pending threecold0/3. Temp helpers cleaned.
Base8f8a7b0/e1d6fed2, branchcandidate/decoder-idle-wait-20261001; prior AUD
candidate preserved but not stacked. Complete source/object/bundle and exact
binary copied to player_decoder_idle_20261001/before, no new worktree.
Dynamic decoder thread24795 had9228 voluntary switches in5.25s (~1758/s),
112CPUticks (~21%). Current per-loop50us sleep wakes even with no input.
Change decoder only: remove unconditional50us; if Cedar says NO_BITSTREAM5
or NO_FRAME_BUFFER4, sleep2000us then recheck stop/state/free pictures.
Keep successful decode timing/PTS/fps/native90/CMA/protocol/audio unchanged.
Add low-frequency result counts every4096calls and main exit state/signal
diagnostics for prior unexplained reader-close/EPIPE. No behavioral recovery
or silent resolution/driver changes. Record compile results then real same
phone video metrics, cancellation/returncodes. Kernel preflight exact5.7.1.
Automatic ADB cast-only disconnect/reconnect, no phone media/rotation/loop
operations. Rollback original e1d6fed2; threecold0/3, no stable push.

## 2026-10-01 / planned / ordered display-worker teardown
Original player tree /home/wnk/f1c200s_display_480x800_candidate_20260914,
base7263725/tagplayer-before-lifecycle-20261001, branchcandidate/player-lifecycle-20261001.
Full source/object snapshot and bundle at player_lifecycle_20261001/before;
make-B/strip exactly reproduces1259e0f3. Preserve dirty generated objects/boot.
Old mixed-object dtsview11386ab exits139 on SIGINT; current consistent1259e0f3
same MP4/SIGINT exits0. Existing drm destroy still closes fd/resources and
destroys live queues before joining its worker, with possible blocked return.
Change driver/drm_warpper.c only: stopflag/queueclose -> join -> drain metadata
and destroy queues -> free DRM resources -> close fd, idempotent destruction.
No header/layout change, decode/rotation/frame ownership/protocol/kernel unchanged.
Add mocked lifecycle test with a full return queue, delayed/blocking worker,
ASan/UBSan/leak detection, repeat destroy; baseline must fail ordering assertion.
Build consistent objects using existing player Makefile and Buildroot toolchain;
deploy isolated candidate, retain1259e0f3/11386ab. Board tests SIGINT/SIGTERM,
repeat MP4 rotation and FIFO/raw shutdown, returncodes/logs/memory. Pending,
no stable acceptance or GitHub push; full cold/regression gate remains0/3.
Stage1 source43d19d0/candidatec4efe667: ASan/UBSan50 cases pass, baseline
negativecontrol aborts134; two ARM builds identical. Board4 MP4 INT/TERM exits0,
but idle FIFO INT hangs in blocking open; own candidate forcibly stopped137,
no baseline target overwritten, stage1 not accepted for raw live use.
Follow-up same shutdown hypothesis: raw reader O_NONBLOCK plus100ms poll,
preserve regular-file EOF and FIFO writer-close EOF; check stop/error while
waiting for decoder stream space. No data format/frame parsing change.
Final sourcea57e979, ARM ELF e1d6fed2782154bcf934fee67a298813, two builds
byte-identical; host50 cases ASan/UBSan/leak detection pass, old negative134.
Deployed only /root/aic_miracast/candidates/player_lifecycle_20261001/player-lifecycle;
original1259e0f3 poolplayer and11386ab dtsview unchanged, protocol entry unchanged.
Board#250/e a1b461e boot unchanged (full ID ea1b461e-c82b-4fec-9d32-5256689ece80).
Eight tests pass rc0/no badfd/no timeout: MP4 INT/TERM repeated2x (800x480,
rotate90/native480x800/defaultaudio); raw FIFO INT/TERM with no writer and
with connected idle writer (640x480/800x480, pool7MiB). No actual streamed
raw frames in FIFO tests, no full Miracast/visual acceptance claim. MP4 parser
is configured to loop, so natural MP4 EOF exit is not claimed or changed.
Raw cases report peak1MiB/allocations1/fallback0/live0; post-tests ION total0,
orphaned0. Debugfs temporarily mounted by inspection, then unmounted.
Available RAM35260->35196KiB over8 cases, later35212KiB; no monotonic-leak
proof claimed from meminfo alone. All test processes/FIFO/logs removed.
Source changes remain only two C files plus mock test/records; same kernel,
DTB/CMA24MiB/fb0480x800/AIC/BS/BT. Decision: verified shutdown candidate;
phone-streaming, visual and three cold/regression cycles pending0/3, no push.

## 2026-10-01 / runtime pending / new player Miracast readiness
Plan sourcecd0b01a/MIRACAST_LIFECYCLE_20261001.md. Existing protocol/config
hashes unchanged; only use e1d6fed2 player candidate. Same#250/ea1b461e boot.
WFD setters allOK, GO5745MHz/DIRECT-fI/COMPLETED, deviceF1C200S-AIC;
WPA6960/player6974/DHCP7001/PBC7006/RTSPwatch7007, no phone DHCP/sink yet.
Pool7MiB/rotate90/raw640x48060/FIFO, no scaling or audio implementation change.
Available20544KiB before stream; default BS route and headphone preserved.
Runtime/tmp/manual_miracast_20260920 kept for connection; one-off wrappers
removed locally. Await phone/visual/long test; full acceptance0/3, no push.

## 2026-09-30 / planned / opt-in private CMA arena
Historical provenance resolved: reconstruct main.o using drm_warpper.h from
3993c9b parent; compile all other sources with current header. Exact ELF MD5
11386abeb2137bfb3f28c47832b05552 reproduced, not an approximated binary.
Save existing dirty source as a dedicated snapshot commit, preserving objects.
Candidate: Makefile header dependencies; optional raw-H264 CMA pool adapter.
One arena with page-aligned suballocations, physical/cache operations delegated
to the verified original ION adapter; no reference frame/VBV reduction.
InitializeVideoDecoder replaces vConfig.memops, so wrap the process-private
adapter table returned by MemAdapterGetOpsS, not only the caller config pointer.
Only explicit CEDAR_CMA_POOL_MB enables it, default remains original allocation.
Protected: active casting untouched, native90degree display, AIC/BS/BT/kernel.
Tests: allocator mock tests, deterministic ARM builds, isolated player candidate.
Rollback: active player11386ab, preflight complete archive; no default replacement.
Snapshot00bb212, immutable tagplayer-before-cma-pool-20260930; preserved user's
preexisting source changes separately from the arena implementationa9a4ad6.
Real-test sourced5254ca. ARM candidate1259e0f3f151a06fc2270ff885dc4443;
two make -B/-j4 and strip runs identical. Host ASan/UBSan tests pass, including
4 threads/2000 suballocations, alignment/reuse/full pool/native fallback and
invalid frees; no reference frame or VBV reduction.
Board MemAdapter MD5 f67be6bb553890ae32c35bdcf05fae72 matches build dependency.
First isolated real7MiB test while old decoder running failed with PFNs busy;
pool used native fallback; strict contiguous test aborted, OS released all
temporary allocations. Old playback unaffected; ION returned6799360 bytes.
User approved one interruption. Stopped old sink2234/player1719 gracefully;
ION0/CMAused1072 pages. Repeated real test succeeded: 33 suballocations,
CPU/VE offset and CPU reverse mapping/cache/reuse checks; peak6799360,
capacity7340032, allocations34, fallback0, live0 after release; CMAused1072.
Started candidate6588 with CEDAR_CMA_POOL_MB=7, 90degree/native640x480@60;
one arena reserved. RTSP watcher6589, same GO/AIC and BS route; awaiting phone.
No current default player overwritten, no boot/kernel/module changes.

Worktree: /home/wnk/f1c200s_display_480x800_candidate_20260914
Component: native Cedar rotation player
Base: 0569026 plus existing dirty source/objects; reproduction gate failed
Purpose: investigate clustered CMA allocation without disrupting active casting.

## 2026-09-30 / pending provenance / clustered decoder allocation
User requests concentrated player memory allocation. No functional source edit.
Protected: current AIC/sink/FIFO, hardware rotation/native geometry, BS SSH,
Bluetooth audio routing, kernel/DTB/modules. Board binary11386ab retained.
Preflight archive: /home/wnk/F1C200S_archives/player_before_cma_pool_20260930.
Build check: make -B -j4 && make strip with existing Buildroot toolchain.
Output dd4ba586 differs from board11386ab; only main.o differs from saved objects.
main.o old g_drm_warpper0x550 vs full-source rebuild0x558; historical3993c9b
adds two display geometry fields, Makefile does not track header dependencies.
Generated artifacts restored from our preflight archive; existing user dirty
source unchanged. No board deployment or code change, no GitHub push.
Decision: stop functional edit until an exact complete source/build baseline is
established. Detailed evidence and prospective arena constraints in
CMA_INVESTIGATION_20260930.md. No temporary helper retained.
