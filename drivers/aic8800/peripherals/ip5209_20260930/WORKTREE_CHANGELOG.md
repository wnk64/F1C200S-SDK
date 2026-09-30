# Worktree Change Log

Component: external IP5209 power supply driver
Worktree: original UGREEN repository peripherals/ip5209_20260930; no new worktree.
Base: Linux5.7.1 #245, kernel963147c, driver43c1f10; source-backed archives in /home/wnk/F1C200S_archives/rtc_ip5209_baseline_20260930.
Purpose: bind IP5209 at I2C0 address0x75 and expose documented battery registers.

## 2026-09-30 / planned / nominal 4.2V voltage control
Base:driverff20f4d, moduleb32b00aa54dd72009239e05f81397b9b; tagip5209-monitor-before-voltage-20260930 and full source/config/module baseline in /home/wnk/F1C200S_archives/ip5209_voltage_baseline_20260930.
User request:attempt nominal4.2V using the IP5209 control interface. Existing registers24=85,26=ff,01=fe, batteryADC4506974uV.
Files:ip5209_power.c; S47 startup path/hash after build; logs/README.
Change:add constant_charge_voltage_max property. Read-modify-write register24 mask0x66 (battery type and voltage compensation), then register26 bit6 to internal selection.4.2V means type00 plus zero compensation. Readback verification and masked rollback on failure; keep boost/charger/current/NTC/reserved bits.
Deployment:isolated updated module, unload/reload only IP5209 monitor; kernel/RTC/AIC untouched. Boot service upgrades the driver but does not automatically write voltage policy.
Source:bdf0e50; tmux numeric rc=0, incremental module only. New moduleMD5 660b80d1ef8be261ec49437406291ec7, vermagic5.7.1 mod_unload ARMv5 p2v8.
Startup:upgrade S47 module path/hash to isolated ip5209_voltage_4200_20260930. Preserve the old module and old service; new service loads the interface but does not auto-apply4.2V policy.
Deploy:module660b80d1ef8be261ec49437406291ec7 verified in isolated board directory; old b32 module remains untouched. Hot-unload/reload only IP5209, same boot_id14a29e34-43f5-485b-8951-c17517b67ed2. S47 source5bad8e4, deployed MD53512550d22583d7d97fbf2e5fbacbec3; old service preserved at ip5209_voltage_4200_20260930/rollback/S47ip5209-power.
Request:echo4200000 to constant_charge_voltage_max returned success. Readback24=81,26=bf, nominal4200000uV; charger/boost01=fe and current25=d7 unchanged. Internal type00 and zero compensation selected.
Actual:voltage_now stayed4506974uV, current0..745uA, Charging. One charger-only restart via charger_enabled0/1 changed01 fe->fc->fe while retaining boost; ADC still4506974uV afterward. No power cycle or network stress. User asked whether this can achieve4.2V; register setting succeeded, physical voltage reduction remains unproven. Need distinguish batteryVBAT from VOUT and know whether a battery is connected.
Decision:retain requested nominal4.2V configuration and upgraded interface, but do not claim actual4.5V issue fixed. No automatic boot voltage policy; IP register state may reset after chip power loss. Software rollback does not itself restore the old voltage-selection fields.

## 2026-09-30 / planned / IP5209 battery monitor and controls
Files: ip5209_power.c, Makefile, S47ip5209-power, this log and Chinese record.
Reference: user IP5209/IP5109/IP5207/IP5108 register manual V1.2; new-board schematic20260930.
Scope: signed14-bit ADC, VBAT/OCV=2600000uV+raw*268.55uV; silicon current positive discharge, Linux charge-positive current=-raw*745.985uA; documented status/health and current limit. No guessed capacity/presence/NTC temperature.
Writes: no writes at probe. User-requested controls use regmap_update_bits, preserving reserved bits. No boost-disable write or chemistry/NTC override.
Hardware:0xEA/0xEB in manual become7-bit0x75. Address acknowledges on I2C0. L3 is not connected on schematic, so no invented IRQ; periodic status monitoring.
Protected: AIC SSH, TCA9555, PE11/PE12 bus, boot power supply and external battery-voltage strap.
Rollback: remove the new startup service/module and restore the recorded #245 kernel/DTB candidate when necessary.
Build: kernel b8b1d71, driver d7dfb13; tmux /tmp/f1-rtc-ip5209-build-0930.rc=0. zImage MD5 2b9b8cf3f41d88fc7182be2c63bcdf22; DTB c9904bd6d2e90634e165895928f804c6; ip5209_power.ko b32b00aa54dd72009239e05f81397b9b, vermagic5.7.1 mod_unload ARMv5 p2v8.
Dependency: rebuilt sunxi.ko MD5 f9e4d79850a9ed80dbf8b965a2df2014 differs from the current board module; the matched kernel/module set must be prepared before deployment. phy-generic.ko unchanged3103db03a0bfe371ddd30922cfbeff28. No candidate deployed yet.
Battery-selection readback:0x26=0xff, bit6=1 means external VSET pin selection; schematic pin23 is NC/floating.0x24=0x85, bits6:5=00 are internal4.2V selection but are not active in external mode.0x25=0xd7 gives2.3A charge-current limit. No selection/current setting modified.
Startup service now validates the built module MD5 b32b00aa54dd72009239e05f81397b9b before insertion. User reports output4.5V; no assumption that this is battery terminal or VOUT, and no battery-type/charge/boost policy changed.
Deployment: source d7dfb13, startup hash-check94c2708. SCP archive extracted in /root/aic_miracast/candidates/rtc_ip5209_20260930; all candidate hashes verified. Kernel2b9b8cf3f41d88fc7182be2c63bcdf22 and DTBc9904bd6d2e90634e165895928f804c6 copied as filesystem boot files; FAT unmounted before software reboot. S47ip5209-power MD5aa7cf45479b86299f8c62a3fd6fb24c7, executable, points to the isolated module.
Matched modules installed:both sunxi paths f9e4d79850a9ed80dbf8b965a2df2014; cedar_ve53126e9c2cffa4c66e5634f99e15f3f9; sunxi_ion_corefbed3af9de750c73cc77d599e9c55601; allvermagic5.7.1. No player/display/AIC source changed. Old kernel/DTB and modules preserved in isolated rollback directory; original sunxi cf1d3dca576543161957efe0d33d121d, cedar1fd69ae33f71882c9e6f9d07f7329699, ionbe1aaa8c2bfa23ac06ed6905d0d9a0a7.
Runtime: kernel5.7.1 #246, boot_id14a29e34-43f5-485b-8951-c17517b67ed2. PCF8563 binds0-0051, /dev/rtc0 exists, RTC alarm nested IRQ128 points to PCA9555 offset15; IP5209 binds0-0075 and ip5209-battery exists, S47 loaded automatically. AIC SSH restored without intervention.
RTC: initial hardware voltage-low flag caused EINVAL/time-unreliable; set system UTC from the PC, /sbin/hwclock -w -u -f /dev/rtc0 clears the flag, readback2026-09-30 06:09:45UTC. CLKOUT register0x0d=0x80, enabled32768Hz, not disabled by CCF. Real alarm firing/backup retention not tested per the user's narrowed verification scope.
IP readback:statusCharging, healthGood, voltage4506974uV, OCV4509391uV, current745uA, current limit2300000uA. charger/light-load-shutdown/boost=1; keyflags0x00.0x24=0x85,0x26=0xff,0x25=0xd7 unchanged. User-reported4.5V is consistent with battery-terminal ADC; source of elevated voltage is not solved by this driver deployment.
Decision: drivers implemented, deployed and directly verified; retained as a provisional configuration, not a three-cold-boot stable archive. No additional network stress or cold campaigns, no GitHub push.
