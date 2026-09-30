# PCF8563 / IP5209 new-board support

Hardware: PE11=SCL, PE12=SDA, hardware I2C0. PCF8563 at 0x51; alarm to TCA9555 P17 (offset15), parent PE5. IP5209 at 0x75; PDF's 0xEA is the 8-bit write address. IP5209 L3 is unconnected in the supplied schematic.

Reference PDFs SHA256:
- IP5209 register manual V1.2: CCF2A5519DCC1CE5043134A27B3620CC2C3CFBE17E5A051BB7E62E2AE77BDEC2
- New-board schematic:40969D57843FE3E379F56A9DC21FF5CB8C9A936D3DA9B28590B179F0195A280E

IP5209 provides /sys/class/power_supply/ip5209-battery: status, health, voltage_now, voltage_ocv, current_now, constant_charge_current_max and constant_charge_voltage_max. Voltage is in microvolts, current in microamps; current is charging-positive. Charge-current limit uses100000uA steps over0..3100000uA, retaining register0x25 bits7..5.

constant_charge_voltage_max accepts4200000,4300000,4350000uV. A write selects that nominal battery type, clears14/28/42mV compensation and selects internal voltage control; unrelated/reserved bits are preserved. The transaction verifies both registers and attempts masked rollback on failure. Reading this property in externalVSET mode returns ENODATA because the physical strap is not readable through the documented interface. Probe/boot do not automatically set a voltage; this is an explicit runtime user control.

I2C-device attributes:charger_enabled (RW), light_load_shutdown_enabled (RW), boost_enabled (RO), key_flags (RO; bit3=current press, bit1=long press, bit0=short press). Explicit writes use read/modify/write; probe does not change supply settings. Existing external VSET chemistry selection, NTC and boost remain untouched. No fabricated percentage, battery-present signal, temperature or input-online signal is exposed because the supplied manual does not document them.

ADC sampling uses high/low/high retry and signed14-bit conversion. VBAT and OCV use2600000uV+raw*268.55uV. Silicon IBAT positive represents discharge in OCV=VBAT+IBAT*R; current_now reverses that sign to charging-positive.

RTC uses the existing kernel RTC interface, preserves CLKOUT with nxp,keep-clkout and honors edge-triggered nested IRQ. /dev/rtc0 and hwclock provide date/time; alarm resolution is one minute. Kernel/config changes and external module must use release5.7.1.

Deployed2026-09-30: kernel5.7.1 #246, RTC binds0-0051, IP5209 binds0-0075; /etc/init.d/S47ip5209-power loads the module at boot. Use /sbin/hwclock (the noninteractive SSH PATH does not include /sbin). RTC voltage-low flag was cleared by writing current PC UTC. RTC alarm IRQ is registered, but actual alarm firing and backup retention were not tested.

Battery selection remains hardwareVSET:register0x26 bit6=1, schematic VSET23 is NC. Internal0x24 bits6:5=00 are not active in that mode; charge-current limit stays2.3A. Measured battery ADC about4.507V; this driver does not resolve the reported4.5V output.

Rollback files are /root/aic_miracast/candidates/rtc_ip5209_20260930/rollback. Restore zImage and the DTB to the FAT boot partition, restore sunxi-extra.ko/sunxi-kernel.ko to their recorded module paths and cedar_ve.ko/sunxi_ion_core.ko to /lib/modules/5.7.1/extra, then verify hashes, sync and unmount before reboot. Stop and remove only the added S47 service to disable the IP5209 candidate. Existing S46 AIC service is retained.
