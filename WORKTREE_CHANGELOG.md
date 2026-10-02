# Worktree Change Log

## 2026-10-02 planned / LG4573 timing-only flicker comparison
Source7577c7c; bash build-lg4573-timing.sh rc0/exactrelease5.7.1.
Baseline reproduced50499cb0527d269e0da7e436c092f643 byte-for-byte;
candidate7018246d33a30340690f9a4edd6406e7. Sorted DT diff only seven
planned properties; kernel/other DT properties unchanged. Deploy DTB only,
software reboot then reload unchanged touch modules/portrait testGUI.
Rollback uses existing50499cb0 display.dtb, no additional backup.
Base: panel1c14066 archived in verified full snapshot; current repo e227f98.
Running DTB50499cb0527d269e0da7e436c092f643, zImagebffc71e8, #250/5.7.1.
Reference: kernel panel-lg-lg4573.c default_mode, clock28341000,
H480+10+59+10=559, V800+15+15+15=845, nominal60Hz (roundedclock).
Hypothesis: EDID-derived porches may differ from native LG4573 timing.
Separate DTS overrides only seven clock/porch/sync-length properties.
Keep HS/VS positive, DE high, falling pixel clock, RGB666/FRM,
backlight1kHz/brightness4, init, kernel, touch, player, networking and audio.
Original repo candidate branch; preserve unrelated dirty historical DTB.
Build DTB only after exact5.7.1 preflight and reproduce baseline50499cb0.
Rollback: existing board newboard_panel_20260930/display.dtb (same50499cb0).
No new backup copies; existing full source/config/kernel outputs archived.
Dependencies: native LCD/LVGL/touch, BS15MHz SSH, Bluetooth and Miracast.
One visual comparison is not acceptance; all three cold cycles pending.


## 2026-10-02 planned / measured portrait touch orientation
Deploy7018213 plan completed: oldGUI3040 terminated normally and oldboard
unregistered; newboarda7922bd1/UI47b2884f verified and launchedPID4211.
ExistingEDT/kernel/DTB/display unchanged. Await visiblebutton/drag validation;
touch IRQ acceptance remains pending, fallback onlydiagnostic.
Sourcef44b988, measured4points/native4corners test pass. Boardmodulea7922bd1
and UI47b2884f buildsuccess, boardvermagic5.7.1. Fix Makefiledefault goal
after explicit object prerequisite insertion; no program behavior change.
Deploy versionedportrait files, stop ownGUI and unregisteroldboard, then
loadnewboard/reopen GUI. Retain old files as rollback, no new backup tree.
User01raw171,402/02raw198,90/03raw721,368/04raw707,62.
Map screenX=479-rawY, screenY=rawX; controllernative800x480 vs panel480x800.
Upstream applies inversionbefore swap: properties800/480,inverted-y,swappedxy.
GUI I2C fallback uses same map, evdev alreadytransformed (no double rotation).
Only boardproperties/testGUI/helper/tests/logs. Original framebuffer/video
and existing touch IRQ unchanged. Stop GUI before board unregister/rebind.

## 2026-10-02 pending / persistent LVGL touch test requested by user
FTsource2855195/buildrecorde9bb157 loaded onboot3950ce0d/#250/CMA20,
inputevent0 native0..479/0..799 nestedIRQ130/P06. Actualtapreader returned
zeroevents/IRQzero; never claim touch passed. Probea332200 initially built
with kernelLinaro failed userspace ABI, rebuilt with originalBuildrootSDK.
User requests visibleLVGL instead of timedlogs. Source02b7e99 plus colorfix
b74b39d uses existingfbdev32bit, LVGL8.2 complete source/license vendored,
128KiB heap/two16lineRGB565 strips/no newCMA or SDL. Runtime3040,
MD529db49c7f55dfb0021c9abe7c2a33420, RSS1064KiB, framebuffer screenshot
480x800 nonblank/layout verified. Fourbuttons/marker/coordinates remainlive.
Evdev preferred; bounded I2C readonlypoll fallback diagnostic only because
IRQ stillzero. Rawreading count3669/no touches in capturedsample; no
coordinate/physical screen acceptance. Program exit lv_deinit/unmap/close.
Reusable hostcapture tool writes no boardfiles. No kernel/DTB/bootautoload.

## 2026-10-02 planned / upstream FT6236 software board registration
Source2855195, externalmodules buildrc0 with originalLinaro7.2.1/LOCALVERSION=.
EDT492e73bc93c06e5b20d8db8f114e9f3f/board7e0a5e11d3e1ad0ec3ad12d6a262f7ab,
bothvermagic5.7.1 mod_unload ARMv5 p2v8. No kernelimage/DTB change.
Deploy newisolatedcandidate, verify hashes/release/unownedaddress38, register
board afterEDT. Bindingfailure rollback both modules; preserve existingcast.
Basepanel1c14066/full source snapshot preserved; kernelb3d8ed3/#250/
zImagebffc71e8/config3e4a7d09 preflight and exactrelease5.7.1 passed.
SchematicI2C1 PE11SCL/PE12SDA, Linuxbus0/address38/A3=64; PCA P05reset/
P06 IRQ/PE5parent. Stage unmodified upstream EDT module plus small board
glue: GPIOlookup/nestedIRQ/I2Cproperties480x800. No kernel/DTB/hardware change.
Originalpanelrepo candidatebranch only; preserve unrelated dirtyDTB.
Rollback unregisterboard then unloadtouch; no bootautoload. Actualtouch/
IRQ/I2C/cast/audio/memory and threecold acceptance pending.

Worktree: /home/wnk/SoftWare/Driver software/tk032f8004-panel
Component: external panel initializer and new-board DTS
Base: 3445a58, tag panel-before-newboard-20260930, full source archived
Target release: 5.7.1
Purpose: adapt confirmed TK032F8004 control pins and PT4103 backlight.

## 2026-09-30 / planned / new-board screen control
Measured board kernel5.7.1 #248, source165df40 and complete bs_poll archive;
15MHz BS DTB retained. Ports user-confirmed COM12 power and COM13 console.
Schematic: CS=PC3, SCLK=PA2, MOSI=PA1, RESET=TCA9555 P07 active-low;
backlight=PE6/PWM1 -> PT4103 EN. No hardware modification.
Keep native480x800 timing/RGB18, BS15MHz polling, Bluetooth, I2C and MUSB.
Driver: optional reset20ms assert/120ms settle; remove hardcoded old pin log.
DTS: inherit BS15MHz, override CS/reset and add PWM1/backlight at1kHz/50%.
PWM register layout checked against F1C200S manual; driver already built-in.
Baseline module b74f6926b6b09ce32d9dc6a3745d052e; rebuild differs only GCC
local symbol sequence.22080->sequence.22079. objcopy rename reproduces exact
old binary, runtime instructions/relocations unchanged. Archive includes original
source/objects, configs referenced by complete kernel archive, reproduction logs.
Rollback: old DTB0b0ab18eb88f3bb7be12426e797cae38 and panelb74f692.
Decision pending; external module + DTB only, no kernel build or GitHub push.
Sourcea6f4b85/c665e2e (Linux5.7 normal PWM polarity uses literal0). Module
88cfcf04e437864349bd024741559aa5, DTB50499cb0527d269e0da7e436c092f643,
startupa7b7c20ea20df3b943a811c4b1a66fda, vermagic5.7.1.
Initial remote-to-local multi-source SCP failed; first deploy guard refused
missing artifacts without mounting/writing boot. First COM12 OFF/ON was therefore
old baseline only, not a candidate test; initial COM13 recorder lost device and
exited on disconnect. After retransferring files, hashes/sync/unmount succeeded.
Candidate cold boot: COM12 exactOFF/ON, COM13 retry reader captured startup;
boot1856f724-2bd3-40c1-9eff-e5491e019ba7, kernel#248/zImage unchanged.
Panel reports reset controlled; GPIO67PC3 CS high, GPIO407P07 reset high(active-low);
PE6 muxpwm1. Backlight PWM1 requested/enabled period1000000 duty501960ns;
brightness4/max8. MMC1 actual15MHz, BS autoSSH/default route intact.
Local decoder-only check: original dtsview11386ab plays test_800x480.mp4 with
CEDAR_ROTATE=90 CEDAR_AUDIODEV=default; native480x800/no scaling.
Visual result pending; no Miracast/BTaudio acceptance, complete cold cycles0/3.
Rollback at board candidates/newboard_panel_20260930/rollback (old DTB/startup/module).
