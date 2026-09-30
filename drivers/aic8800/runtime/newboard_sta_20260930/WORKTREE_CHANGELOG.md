# Worktree Change Log

Component: new-board AIC STA runtime
Base: UGREEN rxguard db0d0b4; kernel 7d8b46c, release 5.7.1
Purpose: start the verified AIC modules and connect the new board at boot.

## 2026-09-30 / planned / AIC STA boot service
Files: S46aic-sta; private /etc/aic-sta.conf (not tracked).
Hypothesis: the manual module, WPA and DHCP sequence can be repeated at boot.
Protected: current USB kernel/modules, direct AIC topology, SSH recovery on COM11.
Rollback: remove only /etc/init.d/S46aic-sta and /etc/aic-sta.conf added by this candidate; manual serial sequence remains available.
Tests: 10-minute ping and 64 MiB SSH download; three physical cold boots pending.
Decision: pending, no Miracast acceptance claim.

## 2026-09-30 / rejected first startup / interface discovery correction
10-minute test: 601/601 ping replies, zero loss; min/avg/max 2/21/521ms. 64 MiB SSH download rc=0; same boot ID and USB device3.
Service source: b6984df; UTF-8 record filename corrected at d219063. Deployed service MD5 7767ee4c1de5f65b974cdc5b826e6c74; private config mode600, no credentials in Git.
First software reboot: modules auto-loaded, but no WPA/DHCP. Serial shows wlan0/device points directly to USB 1-1 and has no driver/module symlink.
Correction: identify the AIC netdev via USB idVendor=a69c/idProduct=8d83, retain module-path fallback. Serial manually restarts WPA from the deployed private config to recover SSH; no serial file transfer.
Acceptance: zero cold boots; first startup failure recorded, corrected restart test pending.
