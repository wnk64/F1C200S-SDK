# Worktree Change Log

Worktree:/home/wnk/LicheePi_Nano/third_party/rtl8723ds
Component:new-board SPI1 NAND boot investigation
Base:a667c7f; complete source and kernel695543f artifacts archived.
Purpose:verify BootROM media selection without hardware changes or flash writes.

## 2026-09-30 / planned / read-only BootROM investigation
Files:f1c200s/newboard_flash_20260930/read-brom.c and records.
Hypothesis:the actual32KB ROM mapped at0 (manualp45) can be read via an
O_RDONLY/PROT_READ mmap even though read(/dev/mem) returns EFAULT.
Scope:no register writes, no Flash writes, no power/reboot, no kernel edit.
Baseline:/home/wnk/F1C200S_archives/rtl8723ds_bt_candidate_20260930.
Manualp38 states SPI NOR/NAND boot, but does not identify SPI controller.
U-Boot fafaf45 board.c uses MMC0 then SPI0; this is not alone proof of ROM.
Build:original Linaro7.2.1 ARMv5 EABI userspace tool.
Deployment:temporary /tmp read-only executable and ROM output; remove after use.
Commit/result:pending. No GitHub push or no-card boot success claim.
Tool commit:dad91b1. Mapping low address0 produced userspace SIGSEGV, output0bytes;
no kernel reboot or Flash write. Follow-up permits read-only high-alias at
0xffff0000 with large-file offsets; do not claim the low dump contains ROM.

## 2026-09-30 / blocked boot evidence / read-only results
Follow-up source8fd520b; both mappings produced userspace SIGSEGV and zero-byte
output. The board SSH remains available; no kernel changes, power/reboot or
flash writes occurred. Empty dumps are not BootROM evidence.
Additional corroboration only: xboot commitaddb5abfe40edf2397bd30b8ba52e20f05267393,
src/arch/arm32/mach-f1c200s/sys-spinor.c uses PC0..3 and SPI0@01c05000.
Neither xboot nor SPL is the actual ROM; SPI1 BootROM support remains unproven.
Linux5.7.1 already has W25N01GV IDef:aa support; MTD_SPI_NAND is disabled.
Current DTS tk032f8004-init claims PA0 GPIO CS; schematic assigns PA0 to SPI1
FlashCS and PC3 to LCDCS. Enabling SPI1 without resolving current display
ownership would create a runtime resource conflict. No such candidate deployed.
Next required evidence:USB FEL ROM extraction with user-coordinated no-card
power-up/USB connection, or an authoritative exact-chip ROM dump. No wiring
change required for this diagnostic; do not confuse FEL host-loaded boot with
autonomous no-card Flash boot. User's requested SD-first/no-card-Flash behavior
is NOT implemented or verified. No substitute SD-to-Flash-rootfs workflow.
Cleanup:temporary executable/dump removed on board and VM; helper source
retained as version-controlled diagnostic. No stress tests or GitHub push.
