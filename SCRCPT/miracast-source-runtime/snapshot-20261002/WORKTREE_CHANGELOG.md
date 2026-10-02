# Worktree Change Log

## 2026-10-02 planned / user-requested source archive and GitHub upload
Archive current successful800x480/native90/fullBluetooth/CMA20 snapshot.
No stable promotion, threecold acceptance pending. GitHub owner nowwnk64.
Publish complete kernel/AIC53bec9e/player/sink source on independent refs in
wnk64/licheepi_nano; retain prior refs. No new development worktree.
Local archive includes full source/bundles/kernel generated prerequisites,
actual board artifact allowlist, protocol/Cedar/Buildroot source/build SDK.
Original working trees preserved; no compilation or board deployment.
Secrets excluded: home Wi-Fi config/SSH keys/access tokens/video captures.
Archive launcher only updates sink hash/path and VIDEO_RX_BATCH switch;
existing board launcher and running session unchanged. Verify SHA256/member
lists/source counts, actual runtime MD5, remote refs via git ls-remote.

## 2026-10-02 verified / complete local recovery snapshot
Source/archive metadata7b55a6f, filename correction89c8d48.
All archive SHA256 checks pass after correcting GBK names and recomputing
manifest; first packaging rc1 was metadata filename encoding, not accepted.
Full kernel tree72563members; AICdriver136/player48/sink90, plus complete
protocol1440/Cedar1419/panel14/RTLsupport585/BlueALSA91/SBC46 source members.
Local package includes SDK/config/generated build prerequisites/artifacts,
seven bundles and actual runtime hashes. BlueALSA bundle shallow parents
missing; verified entire82fdaea tree exported, not full-history certified.
Buildroot personal network config replaced by placeholders in archive only.
Kernel/AIC/player remote refs independently verified; sink snapshot staged
for final push. No board deployment/source rebuild/new development worktree.

Initial snapshot publish7d881a6/tagmiracast-snapshot-20261002 remotely verified.
Final metadata refresh adds GITHUB_SHA256SUMS for public subset and
miracast-snapshot-20261002-verified tag; historical refs are not overwritten.
Verified means archive checksums only, not threecold stable acceptance.
