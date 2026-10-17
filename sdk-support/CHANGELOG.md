# SDK Changes

## 0.2.0-rc1 - 2026-10-18

- New 512 MiB SD image layout leaves data space for first-boot FAT32 creation.
- First boot validates the SDK MBR, running root, card identity and size; creates
  p3 in remaining space, checks FAT32, creates application folders, removes the
  one-time initializer/state/helper, syncs and requests one reboot.
- Reject existing/foreign partitions or mounted data. Retain state on failure;
  resume only the recorded card and layout. Pending initialization blocks normal
  data mounting. No changes to existing cards through application-only updates.
- FAT32 file size is limited to 4 GiB minus one byte. This is not exFAT support.
- Fix DLNA URI replacement leaving PLAYING with no player, and MPlayer H.264
  scanout/rotation buffer starvation. Live protocol replacement/pause/resume/stop
  regression passed; one phone SOAP timeout remains an unresolved observation.
- Fix wiliwili seek-preroll audio deadlock and missing covers after returning
  from details. Content-dependent playback stalls are not claimed resolved.
- Fix local media touchscreen callback selection after keyboard registration.
- Keep unchanged runtime overlays timestamp-stable for incremental builds;
  add `./sdk --version` and `/etc/f1-sdk-version`.

Validation: 109 automated host tests; disposable loop-backed 2 GiB card tested
with mounted system partition, real BLKPG, FAT32/fsck/directories/self-removal
and a fake reboot. Host image/rootfs/ELF dependency audits passed. Current board
card was not repartitioned. New-image physical first boot, interruption tests
on real SD hardware, complete application regressions and three cold boots are
still pending. This tag is a release candidate, not a hardware-stable baseline.

Build: `./sdk build -j8` compiles incrementally without packaging;
`./sdk build --image -j8` explicitly assembles an image; `./sdk clean` removes
only owned build outputs. Source and third-party license files remain intact.
