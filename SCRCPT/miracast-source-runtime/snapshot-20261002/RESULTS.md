# RX video handoff comparison, 2026-10-02

Sourcea61ed97/build17357af, ARMff58d9fc8a47606e926575b65c6450ab.
Sameboot31071e77-b129-46b2-a043-59b6eb863662, kernel5.7.1#250/CMA20,
playera683cf70/native800x48060/rotate90/pool9/defaultBluetooth audio.
Phone media/orientation/loop unchanged. Original source tree and branch used.

ON1:1543.051246s,62535commits,40.53/s,max184577us.
OFF:2354.733613s,99514commits,42.26/s,max191762us.
ON2 restored:90.13338s,5234commits,58.07/s,max117200us.
All zero gaps>250ms and commiterrors. Successful ioctls are not physical
refresh or end-to-end latency. Content phases differ, not matched-frame A/B.

ON2 six windows12.06..13.10s:CPU70.32/75.93/74.14/76.50/76.17/77.13%,
wlan1 receive4.890/5.234/5.355/5.576/5.515/5.618Mbps, socket drops0 each.
FIFO thread8332 CPU3.09..3.63%, voluntary switches123.51..140.35/s.
OFF previous three windows:CPU78.47/70.91/84.33% at5.132/3.993/5.699Mbps;
FIFO5.76/4.75/6.70%,265/205/323switches/s. ON1 FIFO4.07..4.59%,127..145/s.
Worker overhead improvement supported; no exact totalCPU improvement claim.

MemAvailable18832..18860KiB, CmaTotal20480KiB, CmaFree7912KiB.
PCM written16627072bytes and advancing, queue0/dropped140288/underruns0.
Startup audio discard is nonzero. RTPmissing/socket_missing0, FIFO
full/failed/discarded0; handoffpeak20608/workerpeak44888. /proc/exe hashes
verified. Currentplayer8241/watch8242/sink8331 intentionally kept streaming.

Retain WFD_VIDEO_RX_BATCH=1 for continued trial, not certified stable.
Existing cold launcher selects old42d0cfa8; future startup must explicitly
select ff58d9fc and VIDEO_RX_BATCH=1 plus existing coalescing/RTP/IDR options.
Rollback:option0 or retained42d0cfa8 sink; CMA original boot.scr at
/root/aic_miracast/candidates/cma20_20261001/rollback/boot.scr.before.
No kernel/DTB/module/driver/phone-media changes or stable GitHub promotion.
Three-cold acceptance and optical verification still pending.

Later live observation:17890commits/lifetime_max183898us/errors0, no
gaps>250ms seen in latest windows. PCMwritten71513344/dropped144896/
underruns2, RTP208897/missing0/socket_missing0, FIFO full/failed0.
This supersedes early90s maximum only for ongoing session, not saved sample.
Small jitter and audio underrun still need investigation; do not call fixed.
Results commits935c66e/0f64d70. Disposable launch/transfer wrappers removed.
