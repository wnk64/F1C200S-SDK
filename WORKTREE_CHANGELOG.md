## 2026-10-02 / Planned / Register wiliwili menu entry
Files: SCRCPT/cast-and-menu-source/apps.conf; add one external application row only.
Command: /root/aic_miracast/candidates/wiliwili_lvgl_port_20261002/launch-with-menu.sh.
Protected: existing three rows, menu binary/supervisor, cast/kernel/network/BT unchanged.
Test: wiliwili exit must return to existing menu; parent-owned launch returns normally rather than creating a second supervisor.

## 2026-10-02 / completed / all source categories physically relocated
Worktree: /home/wnk/Desktop/F1c200S_SOFT; moved original shared .git, not a new worktree.
Files: physical directory moves, source-index path moves, category README and existing Git metadata.
Verification: primary directory inode/full metadata comparisons passed; U-Boot HEAD/status identical; kernel/U-Boot SHA256 unchanged.
Preserved: complete source/config/build directories, untracked files, original histories and dirty historical DTB.
Mappings: no category symlinks at depth 1/2; removed old aliases created during this reorganization.
Build: not run; legacy absolute source/SDK/cache paths still require explicit updates before build acceptance.
Board: not contacted, rebooted, modified or exported.

## 2026-10-02 / completed / development directory reorganization
Hypothesis: classify existing sources without changing board runtime.
Files: repository location, compatibility symlinks, environment_layout_20261002/README.md.
Source: moved original repositories; no new git worktree; Windows LVGL imported with history at 39695b1.
Verification: seven repository HEAD/status pairs and shared tracked status unchanged; critical SHA256 checks passed.
Board: no files changed; mistaken rootfs export stopped and generated VM exports removed.
Decision: directory organization completed; no compile/deploy/cold-boot acceptance claimed.
Details: environment_layout_20261002/README.md and /home/wnk/Desktop/F1c200S_SOFT/records.

# Worktree Change Log

## 2026-10-02 pending / managed pool allocation and menu recovery verified
Source98f6254, same helper/launcher f3333ad4/fe515f1c and supervisorc4a414cf.
Validround3 Cma9120->16084KiB, pool9MiB, actual phone video. Normaldisconnect
final33alloc/peak8437760/fallback0/live0, supervisor22513 restoresGUI23464.
Round3firstSCP failed because finallog grew duringtransfer; retried afterclose,
full155xx-byte log acquired. No baseline/candidate artifacts removed.
Fourthvalid start managed23663/player23857/watch23858/sink23976; Cma9448->16224
preflight, one9437184arena. ADBconnected true, realcommits2566/246per5sec/
errors0, socketdrops0/FIFOfull0, available18820/Cma5092KiB. Defaultaudioheadphone,
not Bluetoothaudible acceptance. MenuGUI/helper absent duringcasting.
Current managedsession retained, next phoneclose restores ordinarymenu with
newpreflight config. OriginalnetworkGUI/defaultaudio scripts intact rollback.
No kernel/player edit; allocator reliability improved, no RAMsavingclaim.
Fourvalidwarm allocations/threecleanreleases, interruptedpermissionattempt
separate, cold0/3/longvisual/BT regression pending. Do not stablepromote.
Remoteaccess user: noVPS/recommendonly/nopaidservice; no accounts/tunnelinstall.
Recommend existing board-LAN Tailscale gateway, not boardresident VPN.
Official pricing fetch unavailable; no currentquota claims. Tmpwrappers cleaned.


## 2026-10-02 rejected / managed deploy missing execute bit, corrected
Two warmarena runs reserved9MiB and finalpeak8437760/33alloc/fallback0/live0.
Firstmanagedtest GUIreaped but exec cast-app denied(mode644 afterWindowsSCP),
so no newplayer/watch started; ADBconnection timedout. Supervisor22186
correctly restored GUI22198/error. No CMA/kernelfailure in this attempt.
Mark candidate scripts executable in Git and board. Deploymentguard require
all -x, newplayerPID distinct and alive before reading newpoollog/ADBconnect;
never treat oldloglines as candidate result. Stop only own22186/22198 and
rerun managedtest; originalmenu/phone/media retained. Cold0/3/warmstreakreset.


## 2026-10-02 planned / one-shot managed lifecycle regression
Candidate supervisor adds --test-cast-once only for automated regression:
start realunchangedGUI, wait2sec/SIGTERM/reap it, selectfirstcast application
once, then ordinarymenu return on app exit. No GUIbinary/input/driver changes.
Production(noargument) still requires actualtouch selection. Validatefirst
registryentrycast; new supervisor/appsfaed868b/ccd30cdf already staged, new
testoption hash will be recorded. Thirdwarm run via managedwrapper, ADBphone
reconnect, verify frames/pool and menu restoration. Keep original supervisor.


## 2026-10-02 pending / two warm contiguous-pool starts verified
Sourcededa9d6/scriptsfe515f1c/f3333ad4; samekernel/player/sink/GO throughout.
Round1: beforeCma8692KiB/cache18172; after15644/cache7828. Pool9437184
reserved, real800x480/90/rawframes advance, no FIFOoverflow. ADBdisconnect
normalclose: peak8437760/33allocs/fallback0/live0, arena released.
Round2 intentional read existing11MiB CJKfont beforestart: cache25900,
CmaFree312KiB; preflight cache6804/CmaFree16344, pool9437184 reserved.
ADB reconnect true; commits998/window246/5.0sec/errors0, socketdrops0,
FIFO full0, available18960KiB/Cma5192KiB. Audio defaultheadphone actualroute,
not a Bluetooth A/B claim. No cold/visual acceptance yet.
Now add versioned candidateapps/supervisor referring unchanged networkGUI
db2d1b55 and newcache-preflight castwrapper. Oldmenu/config retained. Stop
round2 throughphone, verify release, startcandidate supervisor and trigger
menu launch through optional controlfile (no codechange yet) or standalone
candidate managedforeground test. Avoid orphan app/GUI overlap. Thirdwarm
start and normalmenu return regression pending; no stablepromotion.


## 2026-10-02 planned / reclaim idle file-cache before contiguous arena
Baseline originalmenu defaultaudio launcher6be8390d/wrapper408bfac2/player
a683cf70/sink338c87bf; running9772/10037, boot9868a347/#250/CMA20/DTB7018246d.
9MiB reservation failed EBUSY at5625s, bitmap4282/5120 free (16.727MiB),
7 candidatePFN rangesbusy; sameboot earlier two9MiB allocations succeeded
and released. Kernelmigration/compaction enabled; currentbusyarea has LRU
file/anon mappedpages, failure-time owners unproven. No kernelchange needed
for firsttest: after GUI and oldstream process exit, sync/drop_caches=1 only
before rawplayer allocation. No slabdrop/compact/cmaresize/audio/networkchange.
New cma_start_preflight_20261002 separate scripts in originalpanelrepo branch.
ADB phone authorized, currentlyconnected; preserve media playback, disconnect,
waitnormalcastcleanup/menu return, stop ownmenu for candidate test, start
candidate session existingGO/AIC/BS/BT unchanged, reconnect usingADB.
Compare arenareserved9MiB/actualframes/audio/memory and repeatedstarts.
Baseline source complete archived; current scripts/oldmenu retained rollback.
No source replacement until evidence, no stablepromotion/cold0/3.


## 2026-10-02 pending / live Wi-Fi and Bluetooth settings available
Source2c87b6b/buildrecord2100a6a, deployeddb2d1b55/helperaca7bf18/CJK3ae39f36
checked remotehash. Initialdeploy preflight refused absent/proc/.../children
(kernel option not enabled); no statechange. Switched ownGUI detection to
exe/PPid, stoppedold5012/5804, mock network-smoke exited0, new8946/GUI8952.
Realvisible scanning/list and pagination captured: Wi-Fi CU_201/ChinaNet/
CMCCrows,3/5page, alltouch targets fit. User browsed realpages duringcapture.
GUI RSS1452KiB/PSS487KiB/anon136KiB afterscans; no helper left, WPA runtime
socket removed. Underlying #250/CMA20/DTB7018246d/BS/BT/audio/cast unchanged.
No live WiFi join/disconnect/forget or Bluetoothpair/remove executed for testing;
these and DHCP/rebootpersistence/audio/cast regressions remainpending, cold0/3.
README describes supportedsecurity, unknownUnicode limits, command and rollback.
Local temporarywrappers cleaned, hostprotocol-testbinary removed.


## 2026-10-02 planned / deploy connection UI after mock page smoke
Source2c87b6b, menu db2d1b552887486b3874f06ebd30aa12,
helper aca7bf187ef1cbcbd4f49884ede8e82d,
CJK3ae39f360235a5ebcf731d761065dc76. Buildrc0; helper read-only scans passed.
Deploy root/aic_miracast/candidates/lvgl_connections_20261002 with sameapps/
supervisor referencing existing menu castwrapper408bfac2. Check only ownGUI
child (not an activecast), orderly stop oldsupervisor, run network-smoke dummy
devices/password/PIN layouts, then start normalmenu. Never testswitchSSH.


## 2026-10-02 pending / real read-only Wi-Fi and Bluetooth scans passed
Menu74a63311/helper329d1af8 sourcea81abe6; helper tested onboard5.7.1.
Wifi-list correctly current/savedwnk641_2.4G; wifi-scan12networks including
ChineseSSID. BTlist pairedWt-070; 8secBTscan3devices inclWNK, no pairing or
network disconnect, SSH intact. All DONE/exit0 and workers terminated.
UnknownChinese device names need glyphs beyond95UISubset: add openTakao
fixed528byte CJKrecords on ext4, 16glyphcache (~8.5KiB), pread onlyusedglyphs,
no fullfontload/mmap/fontengine. Preserve sourcefont+license+generation tool,
fallback afterUI subset beforeASCII. Add actionallowlist; build/deploy GUI
isolated after privatepage/keyboard smoke. Originalmenu61dfd292 retained.


## 2026-10-02 pending / network UI and helper build
Sourcef3f5bc8/402537a; hostnetprotocol ASan/UBSan assertions passed and temp
testbinary removed. Firstcompile TRUE/FALSE fixed to C99bool; oldGUI unchanged.
Newmenu27638c0b/helpera705a5c built successfully with originalBuildrootGCC6.4.
Fix ownhelper indentation/zero-init warnings and isolate connectionlog path;
upstream LVGL tlsf-unused/drawlabel warning left unchanged (same library).
Nextdeploy helper only first, read-onlywifi-list/bt-list and explicitscans;
then verify GUI-page/keyboard mocksmoke before ownsupervisor transition.


## 2026-10-02 planned / on-screen BS Wi-Fi and BlueZ device management
Base3ac7d66/menu61dfd292 retained. Original repo candidatebranch, no newtree
or kernel/DTB/driver/LVGLlibrary changes. Settings adds largeWiFi/BT entries,
pageddevices/connect/disconnect/forgetconfirmation/maskedkeyboard/PIN/confirm.
Shortlived forkexec/pipe worker; WPA AF_UNIX onlyBSrtlwlan0, safeSSIDhex/PSK,
existing SAVE_CONFIG/update_config1 and guardedBS DHCP renew. Failednewjoin
restoresoldselection. Open/WPAPSK only; WEP/EAP/SAE-only rejected. No keylogs.
BlueZ DBus objects/discovery/Agent1/Pair/trust/connect/disconnect/RemoveDevice;
authorize selecteddevice only, user confirmscodes. ExistingA2DP routing intact.
Private lv_conf btnmatrix/textarea, sameLVGL src in privateobjectdir; oldtest
objects untouched. Chinese95glyph/31417bitmapbytes. Hostprotocolsanitizer,
ARMcompile, realreadonlyscans, mockpage/keyboard smoke/screenshots planned.
Never disconnect/forget SSH network or pair realdevices automatically.
Deploy isolatedruntime; rollback existing61dfd292 menu/supervisor. Actual
physicalconnect/audio/casting and threecold0/3 pending; no bootautoload.


## 2026-10-02 pending / headphone startup path verified and menu restored
Sourcee6a4787, launcher6be8390d03a110d65a290133ad61821d,
wrapper408bfac290de785a3458c56106457f08; sh-n and artifactpreflight passed.
Stopped owned GUI4355/supervisor4033 before test. Real session with route
headphone reached readyforphone, player4751/watch4752 alive; no GUI remained.
No phone sink started; terminated only owned testwrapper, confirmed player/
watch exit, restored supervisor5012/menu5017. Originallauncher9168c75b intact.
No BTconnection gating now; actual headphoneaudible/Bluetooth switching and
phonecast stillpending, threecold0/3. No driver/DTB/kernel/player/sink changes.


## 2026-10-02 planned / optional Bluetooth for menu cast launch
Userclicked twice; application.log preflightpassed/wlan1registered then
Bluetoothconnectionfailed; bt-connect.log Hostisdown, currentrouteheadphone.
GO5745 COMPLETED valid; no driver/USB/RTSP failure evidence.
Original audio-reopen launcher9168c75b requires fixedspeaker unconditionally.
Add separate start-cast-default-audio.sh derivedfrom original; preserve original
assets/pbc/sink/player/hashes. Only audio readiness uses current ALSA default
instead of forcedBlueZConnect; S35audio-route already provides Codec fallback.
Point menu castwrapper to localcandidate launcher; existingmenu binaryunchanged.
Rollback originalwrapper2a1db860 and originallauncher unchanged. No driver,
DTB/kernel/sink/player/audio-daemon changes. Test shellsyntax/read-onlypreflight,
headphone session startup after GUIexit, then returnmenu if not streaming.
Real phone and threecold regression pending.


## 2026-10-02 pending / live application menu deployed
Finalsourcee2f9269/buildrc0/f1-menu61dfd292bb53b358c7581d00906cda7f,
shellsyntax/config checks pass; finalhome/settings/touch/home smokeexit0.
Oldsupervisor3742/GUI3747 bothterminated and /run/f1-menu.lock removed before
replacement; current supervisor4033/GUI4038. Snapshot480x800 Chinese/icon/
all432x156 buttons fit; rootbackground/cornercursor corrected.
GUI RSS1392KiB/PSS434KiB/anon104KiB; supervisor PSS195KiB/anon64KiB.
Userland-only, unchanged LG4573DTB7018246d/#250/CMA20/BS/BT/FTmodules.
Menu app selection uses reap-before-launch, but real casting/phone disconnect
and physicaltouch/pagination/brightness acceptance not yet tested. No boot
autoload or stablepromotion; threecold0/3. Temporary local wrappers removed.


## 2026-10-02 planned / final idle pacing and console cursor suppression
Source6fa5d01/f1-menu6022abc4: normal/fourappconfig valid, invalid rejected;
both4page smoke runs exited0. Persistent supervisor3742/GUI3747; actual
fb capture Chinese/icons/layout clear and bounded. GUI RSS1380KiB/PSS430KiB,
anon104KiB; supervisor PSS204KiB/anon64KiB. No newCMA buffer.
Set rootbgopaque, hide tty0cursor (visible blackcorner artifact) using VTescape,
sleep by LVGL nexttimer1..16ms instead of fixed5ms. Retain original polling.
Rebuild candidate then orderlystop only ownsupervisor/GUI and restart verified
files; no init/driver changes. Real touch/menu-cast acceptance stillpending.


## 2026-10-02 rejected / stack-local LVGL driver lifetime
Firstcandidatecb0dae00 configvalid/4valid/invalidrejected passed. GUI322 exited
normally, firstsmoke then SIGSEGV. No kerneloops or caststarted. Root cause:
LVGL8.2 registration retainsdriver pointers; platform_open locals expired.
Fix: bothdisplay/input driver descriptors static until lv_deinit; onlymenu
backend changed. Rebuild and rerun bothsmokes before starting supervisor.


## 2026-10-02 planned / extensible low-memory 3.2-inch application menu
Source3600861/b69f0ef, originalBuildroot make-j4/strip rc0, warning-free final.
f1-menucb0dae00b883b05ef85626cf275d0fa9 (~234KiB), appsb9db6d8c,
supervisor2520d456, castwrapper2a1db860; shellsyntax checks passed.
Added config validation/four-app pagination fixtures; no originalsource edits.
Deploy isolatedcandidate only; stop own portraitGUI322 after executable/config
checks, run smoke without launcher, verify exit then start persistentmenu.
Base panelb016002 with complete unchanged LVGL8.2 source. Original repo branch
only, preserve unrelated dirty historicalDTB. No kernel/DTB/module changes.
New lvgl_menu_20261002: nativefbdev backend/GUI/app registry/shellsupervisor,
cast wrapper calls unchanged audio-reopen launcher. Three432x156 targets,
Chinese28px4bpp/52glyph subset17250bitmapbytes generated using hostGDI.
Reuse existing LVGL128KiB heap/two16line strips30KiB; no SDL/fontengine/CMA.
External app selection exitsGUI/waitreaped then starts app; endreleases own
processes before reconstructUI. PIDstarttime guards; independentcast refused.
Internaltouch/brightness1..8 pages; up to12apps/3perpage registry extension.
Build originalBuildrootGCC6.4; testconfig/smokepages/screenshots/RSS and exit.
No bootautoload. Rollback stop only own supervisor/GUI and restore existing
portraitGUI47b2884f. Protect LG4573DTB/BS/BT/800cast/audio/FTorientation.
Actual app/casting lifecycle and threecold acceptance pending; do not promote.


## 2026-10-02 pending / user confirms flicker absent after timing switch
Deployed candidate7018246d33a30340690f9a4edd6406e7, checked FAT target,
sync/unmount/software reboot. Boot9868a347-aab7-4a9d-877c-e13ce0be6a47,
unchanged kernel#250/5.7.1/CMA20; BS automatically associated and SSH works.
LVGL portrait47b2884f PID322 and same EDT/board modules restored.
User: visible flicker disappeared. Only seven timing properties changed;
backlight4/8, PWM0x5DBF2F0F, FRM0x80000000, polarity0x03000000 unchanged.
TCON H0x022E0044/V0x069A001D/sync0x003A000E => totals559x845.
PLL0x91006E01/DCLK0x8000002F => about28.340426MHz/~60Hz, register-derived.
Supports timing-set hypothesis; does not isolate any individual porch/clock
or prove a manufacturer-spec match. No Miracast/BTaudio regression this boot;
three complete cold cycles0/3. Keep as visual-pass candidate, not stable tag.
Source7577c7c/build-deploy-recordc28edaa; rollback existing50499cb0 DTB.


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
# 2026-10-03 菜单开机显示修复

## 计划 / pending

源码：/home/wnk/Desktop/F1c200S_SOFT，原分支 candidate/cma-start-preflight-20261002，基线 57ad956。原菜单二进制 SHA256 c3d363346434285edfef9285d8611059742030a9124ad3223f7cab8e93fe5ba4。保留原候选目录，不新建工作树、不改内核、驱动、播放器。

实测：COM4 115200，菜单 supervisor PID255、GUI PID263 从启动时正常运行；DRM 原生480x800输出 active=1。SSH恢复后读取 framebuffer，画面主要是内核日志、残留少量菜单标题，证实 fbcon 覆盖 GUI。platform.c 仅隐藏光标，没有 KD_GRAPHICS。修改为打开 tty0，保存 KDGETMODE，申请 KD_GRAPHICS，退出恢复原模式；失败明确输出错误。内核及串口日志不关闭。

文件：LVGL/menu/platform.c，SCRCPT/cast-and-menu-source/S90f1-menu 和 menu-supervisor.sh。新板端候选 menu_graphics_boot_20261003，旧二进制和管理器仍在原目录，服务回退可从 Git 57ad956 提取原脚本。应用表保持最新57ad956投屏、设置、触摸和wiliwili入口不变。顺便纳入本地已有服务 alive() 对服务包装阶段的识别。

网络恢复：板子新网络wnl64，IP10.21.174.124；电脑仍在家庭网10.0.0.129，暂时无法连接wnl64。通过COM4的wpa控制添加临时家庭2.4G网络，利用电脑已保存密码在内存推导对应SSID的PSK；恢复板端SSH10.0.0.234。未保存临时网络、没有删除wnl64。旧5G哈希不能直接用于2.4G，因为PSK推导包含SSID；已纠正，日志不保存密钥。

验证计划：增量构建、源码提交后SCP部署并核验；菜单重复启动不重复进程；模拟内核日志后framebuffer保持；停止恢复文本模式、重启恢复菜单；冷启动、网络SSH回归。三轮完整依赖验收前不称稳定版。
