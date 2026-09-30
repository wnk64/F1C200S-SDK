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
