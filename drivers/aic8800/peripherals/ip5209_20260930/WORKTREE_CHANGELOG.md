# Worktree Change Log

Component: external IP5209 power supply driver
Worktree: original UGREEN repository peripherals/ip5209_20260930; no new worktree.
Base: Linux5.7.1 #245, kernel963147c, driver43c1f10; source-backed archives in /home/wnk/F1C200S_archives/rtc_ip5209_baseline_20260930.
Purpose: bind IP5209 at I2C0 address0x75 and expose documented battery registers.

## 2026-09-30 / planned / IP5209 battery monitor and controls
Files: ip5209_power.c, Makefile, S47ip5209-power, this log and Chinese record.
Reference: user IP5209/IP5109/IP5207/IP5108 register manual V1.2; new-board schematic20260930.
Scope: signed14-bit ADC, VBAT/OCV=2600000uV+raw*268.55uV; silicon current positive discharge, Linux charge-positive current=-raw*745.985uA; documented status/health and current limit. No guessed capacity/presence/NTC temperature.
Writes: no writes at probe. User-requested controls use regmap_update_bits, preserving reserved bits. No boost-disable write or chemistry/NTC override.
Hardware:0xEA/0xEB in manual become7-bit0x75. Address acknowledges on I2C0. L3 is not connected on schematic, so no invented IRQ; periodic status monitoring.
Protected: AIC SSH, TCA9555, PE11/PE12 bus, boot power supply and external battery-voltage strap.
Rollback: remove the new startup service/module and restore the recorded #245 kernel/DTB candidate when necessary.
Build/deployment: pending, exact release5.7.1, SSH-only transfer and hashes.
Decision: pending; user requested driver implementation and direct verification, no repeated network stress or cold-boot campaigns this turn.
