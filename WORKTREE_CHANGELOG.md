## 2026-10-07 / pending / menu Wi-Fi bounded recovery
Sourcee717fc7; exact files and evidence in SCRCPT/network-and-audio-source/wifi_control_20261007/WORKTREE_CHANGELOG.md. Menu/helper/S17recovery candidate deploys only userspace, preserves kernel/driverbinary/BT/AIC/DTB. Fresh reject/AP/cooldown/manual safeguards, target relookup, IP confirmation, explicitUI failures. Same GCC6.4build/gatesPASS and three actual manualreconnect/staticIPv4PASS. SSH10.0.0.234/menu healthy. True driverstall and3coldboots0/full protected regressions pending, not permanent/stable claim. No temporary helper scripts/backups, serialCOM11released.

## 2026-10-05 / planned / user-supplied TK032F8004 timing
Baseline root1f3bd30/kernelb3d8ed3, source artifacts/release5.7.1 rechecked. User ZIP6029517d, exact example source70943fd3. Numeric timing from STM32 sample33MHz/524x809, retain board bus/polarity/init; complete evidence/build/rollback/dependencies in DRIVER/panel-and-board/vendor-timing-20261005.md. External DTS only; no kernel/application change, coldboots0 pending.
Sourcef511105/buildPASS/candidate8ca3cd42 boardboot hashmatch. Boot87274123, kernel5.7.1#250/menuSSH normal; DRM78Hz-rounded/33MHz/524x809 active. User visual red-band/flicker pending; coldboots0, no stable acceptance. Existing LG rollback unchanged, no temporary scripts/unsolicited backups. Full details in vendor-timing-20261005.md.

## 2026-10-05 / planned / kernel table timing comparison
Baseline rootf5782ee/kernelb3d8ed3. Running zImagebffc71e8 and DTB7018246d match source artifacts; release5.7.1 and complete kernel metadata checked. New external DTS selects COM43H4M85ULC seven timing fields from current panel-simple.c, not custom reduced-clock extrapolation. Details, full hashes/build/protected dependencies/rollback in DRIVER/panel-and-board/ortustech-timing-20261005.md. Keep kernel, bus polarity, panel init, wiring, applications untouched. Physical red-band and coldboot0 pending.
Sourcebcb5581, build PASS and bootfa37af22 exact candidate timing25MHz/515x809. User no change/still red bands; candidate rejected and original LG4573 DTB restored through verified filesystem copy/reboot. Full source/artifact/hash/test/rollback facts in ortustech-timing-20261005.md, coldboots0, no stable claim, kernel/apps untouched.

## 2026-10-05 / planned / application console handoff
Baseline370d46b. Exact hypothesis/files/rollback/dependencies in SCRCPT/cast-and-menu-source/console-handoff-20261005.md. Supervisor acquires graphics for session and restores previous mode only at service shutdown; application platform code unchanged. Helper explicit GCC6.4 target, SD deployment. Tests repeated reader/browser lifecycles and service mode restoration; all-app physical and three cold boots pending0.
Implementation8b6c0da/test35d5681. Build/board hashes and evidence in console-handoff-20261005.md. Three menu/reader-browser/TXT/media-browser transitions maintain mode1, service shutdown restores0, restart mode1/PID3882; test0. No resident helper/display heap, no kernel/player/app-table changes. Pending physical full-app and coldboots0; no stable promotion, no temp helper scripts.

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

## 2026-10-10 planned / offline HID input candidate
Base038fcee; modify LVGL/menu/platform.c and add header-only evdev-hid.h with deterministic host tests, HID kernel config fragment and Chinese record. Existing shared platform source tracked, built menu binaries retained; no new worktree/board access.
Hypothesis: capability-based evdev discovery and separate LVGL mouse/keypad inputs enable HID without disrupting direct-I2C touch fallback.
Scope: shared-platform menu, settings and browsers; default group navigation/text entry, relative mouse/cursor, bounded event queues and periodic hotplug discovery. Independent ONS/wiliwili/custom video inputs not automatically claimed supported.
Protected: kernel .config/zImage/modules, BlueZ/audio services, network/cast/default audio policy; only config fragment staged, kernel not built pending documented build-command gate.
Rollback: Git038fcee platform.c; no deployment. Tests: host synthetic input, GCC6.4 menu/media/recorder builds. Physical pairing, audio coexistence, scanout and3coldboots pending.

## 2026-10-10 offline tested / shared-platform HID source
Sourcea2ff9e7. sh LVGL/menu/test-hid.sh ASan/UBSan/leaks PASS: relative delta overflow/clamp, rapid press-release snapshots, shift/caps/repeat/release mapping, Tab/ShiftTab, disconnect releases, bounded queues.
Build: make -s -C LVGL/menu -j2; make -s -C LVGL/media_player -j2; make -s -C SOFTWARE/mic-recorder -> all0 with original GCC6.4, existing media_display.c _GNU_SOURCE warning only. Recorder Makefile now rebuilds shared platform instead of leaving stale linkage.
Artifacts SHA256: menu8fb6155b286cab75acbc83c74f058e8fa6f808fe4d0cdc70a56a9818a70e9f8f; mediaUI415d35cf66b07af3ad8bb32d70b4ab838ab1ff0928406b70edf57daa948c9042; browserafb6b36aee7ca06ecc276894bc345c101a6289ea80cca8a25e1e74102f06d61c; recorder18fec2caaed8e2c58ec5a391273a4bfb2e35d22baca325b05eb92c7a4c81e068.
BlueZ build source includes input/hog; existing pairing agent has DisplayPinCode/DisplayPasskey handling. Not runtime profile verification.
Kernel#250 source/config/outputs present, BT_HIDP/UHID off. Required Desktop/F1C200S_build_commands.md unavailable; kernel guard blocks guessed build. Added fragment only, no kernel configuration/module/image mutation or build, no board contact/deploy.
Limitations: relative mice only, US-English basic keymap/no IME, wheel changes focus not arbitrary document scrolling; Esc/right button emits LVGL cancel, not every app's Back/exit. ONS/wiliwili independently implemented inputs not integrated. Video hardware OSD cursor visibility unverified. Rebuilding shared-platform users does not prove application behavior.
Decision: pending; no GUI runtime input/actual hardware pairing/coexistence/coldboots, no stable acceptance. Test tool retained as reusable tracked source; no temporary helper scripts created.

## 2026-10-07 / pending acceptance / recorder labels and WAV browser
Base0e76791/sourcec361399. Recorder CJK asset omitted from prior deploy; corrected in independent SD candidate with fixed bounded/stateful labels and screenshot verification.
Local browser previously filtered WAV; added WAV to browser and audio-only frontend. Existing MP3/MP4/backend unchanged behavior tested.
Files/tests/deploy SHA/rollback: SOFTWARE/mic-recorder/WORKTREE_CHANGELOG.md and LVGL/media_player/WORKTREE_CHANGELOG.md; Chinese record SOFTWARE/mic-recorder/changes.cn.md.
WAV/MP3/800x480 MP4 pause-volume-seek/hide tests0; MP3 lyric test0, video pool fallback0/live0. User recording data retained.
Menu changes two app paths only; old application binaries retained. No kernel/DTB/module/network/BT/audio-route change.
No coldboots or full protected end-to-end regressions performed; not stable acceptance.

## 2026-10-07 / pending acceptance / on-demand MIC recording
New source: SOFTWARE/mic-recorder; menu entry: SCRCPT/cast-and-menu-source/apps.conf.
Base1f2b7d9, commits82795d4/1ca7e90/cfc8adc; existing unrelated dirty DTB preserved.
Existing Codec capture was present but ADC MIC route off; new app enables only capture controls and restores exact saved values.
Build: make -C SOFTWARE/mic-recorder, original Buildroot GCC6.4, no warnings; no kernel/DTB/module/network/audio-route changes.
Deploy/hash/tests/rollback: SOFTWARE/mic-recorder/WORKTREE_CHANGELOG.md and changes.cn.md.
8s and20s nonblocking capture passed without xrun, WAV valid; SIGTERM finalizes and mixer SHA unchanged, coreRSS2032KiB.
Active cast was not interrupted. GUI/audio listening and complete protected regressions/three coldboots unverified; candidate only.

## 2026-10-05 / planned / ONS application source baseline

Base: 97bbebd; source-reproducible application, board candidate SHA256 dd18b6e499ecc36b0a010ccbdc37cc3c7143b38af26c9a78eedcf8362b15c9c9 retained.
Files: LVGL/ons_native/vendor, BASELINE_20261005.md, changes_20261004.md.
Purpose: preserve exact legacy engine and audio dependency sources before integer Vorbis experiments; no edits to original source trees.
Build: make -C LVGL/ons_native -j2 all ons-lvgl, existing GCC 6.4 Buildroot toolchain.
Protected: kernel, drivers, Miracast/player, network/audio services, user game/save data unchanged.
Evidence: prior full-engine user feedback works; profile title-only RSS19616KiB/PSS18235KiB, 90s audio3938304frames/1underrun. Not stable; cold-boot gate pending.
Decision: pending; archive and remote source verification required before decoder changes.
Baseline preservation: commit8bc3ab7; subtree43787d706436caafb061468539f3be6d875e5a0b verified on GitHub wnk64/f1c200s-extras, ons-native-lvgl-candidate-20261005.
Archive: /home/wnk/F1C200S_host_archive/ons/ons-native-source-8bc3ab7.tar, SHA25680eac229492d98f904e3f8d2361da5a9c506ee6ed335288087670c1067f64007.
Planned decoder files: Makefile.tremor, vorbis_backend.h, prepare_engine.py, runtime_mixer.cpp, .gitignore; independent integer targets, no original engine edits.
Build: make -f Makefile.tremor -j2 ons-lvgl-tremor native-runtime-mixer-tests-tremor succeeded; deploy/tests pending.
Commits: 9204cc5 integer target, a79cfa1 time-seek unit adapter and diagnostic.
Deploy: separate ons_tremor_20261005 directory; retained float SHA256 dd18b6e499ecc36b0a010ccbdc37cc3c7143b38af26c9a78eedcf8362b15c9c9. No menu entry changed.
Integer binary SHA2565a3cbbb597e6528b0217e63e5d0d87adf87fed49f5a363f9ece0fd3d7fc8f7a0; no SDL dependency.
Test: diagnostic SHA25622802b2b50a0875888143d88cdfee6beab7d34018e0b2f92b9fd0083b1a06381 verified, real OGG chunk28928bytes versus28224expected; assert failed. Integer full engine not run.
Decision: rejected for full-game deployment, frame-count mismatch unresolved; old float runtime retained, menu running, cold boots pending.

## 2026-10-05 / pending / Integer ONS and rotated touch controls

Hypothesis: initial seekable Tremor stream requires PCM origin; legacy object transform does not rotate child labels in LVGL8.2.
Files: LVGL/ons_native/vorbis_backend.h, codec_probe.cpp, runtime_display.cpp, runtime_controls_tests.cpp, Makefile.tremor, .gitignore, BASELINE_20261005.md and Chinese change record.
Commits: 8c96929 probe, f913c65 origin fix, 09afad8 controls, 8551709 ineffective style rotation, c8f36d2 raster rotation, 249eac2 refresh context fix.
Build: make -f Makefile.tremor -j2 ons-lvgl-tremor native-runtime-controls-tests; original GCC6.4. Float full build still SHA256dd18b6e499ecc36b0a010ccbdc37cc3c7143b38af26c9a78eedcf8362b15c9c9.
Deploy: separate /root/aic_miracast/candidates/ons_tremor_20261005/ons-lvgl-tremor-controls-rotated SHA256210571105eaaced5d5c28165e81226d1622366c7162e5532e8e9423668c50f37; SCP hash verified. Prior origin2 retained as rollback.
Tests: correct short OGG frames and mixer PASS; 60s real full engine/user works/0underruns/rc0; 90s controls engine Auto+Skip logged/0underruns/rc0. Raster layout/key-pair/exit test SHA25690d9699c550315abe46fd4a856485a59b8f65977e6a908c709c7cd3e44e59efe PASS.
Screenshot: SHA2562c941382aedd2135b598229bfc58a8ea3a93a96468609093ab9286df5590dff1 verifies actual rotated control text/icons outside game viewport; style-only candidate rejected. Temporary board raw captures cleaned.
Decision: candidate running for real touch confirmation; exit restores menu. No kernel/driver/network/player/game script edits, no persistent menu entry; saves remain original engine behavior. Save/load/long-run/cold-boot gates pending.

## 2026-10-05 / pending / ONS ROM selection and boot menu

Hypothesis: standalone directory browser exits before engine, preserving low RAM and original menu supervisor lifecycle.
Files: LVGL/ons_native/rom_browser.c, Makefile.browser, ons-app.sh, MENU_PLAN_20261005.md, .gitignore; SCRCPT/cast-and-menu-source/apps.conf adds one ONS entry.
Base: 02236c3; old source and apps config retained in Git, existing engine artifacts unchanged.
Commits: 61905a8 browser, 0bbb510 menu entry; make -f Makefile.browser -j2 ons-rom-browser succeeded, launcher sh -n passed.
Deploy: ons_menu_20261005 browser SHA2569b228ad7c221230e63b2cad3ca03b2a9a92031483e2e39400d850fd9ce9b56f5; launcher4f85ad36845419b021d2eb57d943e7016beda4f899bd88259e55d719fcb32fc5; menu configf5bd02bbeeee6fb44e124c45592f423a65707402430c8288775c5814fc6abc7c. All remote hashes verified.
Tests: actual directory list/game check/invalid paths passed; menu validator0 and restart; framebuffer browser screenshotff5313bd30301fefc933367f2e5c097721244245559b96bb4599bc4ed7742056 verified. Real game launch/back workflow pending user confirmation.
Rollback: apps.conf from Git02236c3; no ad hoc board backup directories. Service/supervisor/kernel/drivers/other apps unchanged. Runtime selection temp file cleaned by launcher trap on exit; raw capture removed.
Decision: entry deployed, browser active for touch test; three cold boots and end-to-end launch/exit not yet accepted. mplayer read-only inventory found ordinary SDL1.3.0 binary without Cedar codec, historical Cedar launcher separate; no mplayer/player mutation.

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

## 2026-10-03 / pending完整回归 / 菜单图形控制台修复结果

源码提交96b4c2c；实际根因是fbcon日志覆盖运行中的菜单，不是服务未启动。platform.c保存/切换KD_GRAPHICS、退出恢复，Makefile与头文件路径适配实体迁移；在原LVGL/menu中make -j2 f1-menu增量rc0。部署及哈希、回退入口详见SCRCPT/cast-and-menu-source/menu-boot-change-20261003.md。新GUI bc7f3fd5c37771143038f11609c50897cf071c2b1eccf7b3ff488cc3291b5171，服务adf6a5eb29b6c04e5f3fd7f79f9029ba7b2a02c40dfb43e816d85937f532c57f，管理器e724a70d8f7804c9a4f4d7d58f179e6caf4b29861ef11bba2f695c1d55bfac36。

板端候选menu_graphics_boot_20261003，原候选保留；apps.conf不变。网络过程状态已更新：最终保存wnl64优先级0和家庭2.4G优先级10，冷启动自动SSH10.0.0.234。三轮实际断电菜单/SSH/日志覆盖测试均通过，boot_id分别75faa115-7d7e-41aa-bbd4-d91470eea675、a8beb893-bc68-4601-8e14-642703aab959、798b510a-1f70-4bbb-8048-2901bcf2b66f。每轮fb0 MD5在日志注入前后均a0a288720b64b2765f44dc403dbab569。未重测投屏/蓝牙音频/wiliwili播放，完整基线验收仍pending。未重建rootfs镜像、未改kernel/驱动/播放器，未推稳定标签。

## 2026-10-03 / planned / PCF8563开机时间校验

用户要求RTC读取时间与当前运行kernel编译时间比较，早于编译时间判异常，联网取最新时间并回写PCF8563。基线7be9484，新增SCRCPT/network-and-audio-source/rtc_time_20261003/f1-rtc-time.c及S18rtc-time；不改内核、驱动、播放器或网络配置。RTC低电压标记导致RTC_RD_TIME失败、系统1970年、wiliwili证书校验失败已实测。后台SNTP不阻塞菜单和SSH，成功后回写/回读验证并退出；无效回复不调时，网络不可用每30秒后台重试。详细中文记录同目录changes_20261003.md。原Buildroot GCC6.4编译rc0、主机自测通过；二进制72adcc708ff11d62c6d4d143dd0c8c8a5f8241b433e877ebc0a9574132d29991。待部署、真实授时、冷启动与HTTPS回归；rollback仅移除新增服务/工具，不影响原服务。

## 2026-10-04 / pending / PE4电源与菜单系统状态

83ece05独立DRIVER/board-power-20261004模块，仅关机最终阶段PE4高2.5秒，reboot不触发，init低电平；原kernel#250匹配源码/config/zImage前检通过、release5.7.1、模块vermagic正确。源码/构建/部署/回退详见同目录changes_20261004.md。634e3c8菜单加入电源确认、SSH地址及电池/充电接口。模块f2788eb3542757e37bc9db8daf801bceb8cfc1ecbd8c8dcc6c2bc847beaba7ac加载GPIO132 out lo，system-smoke通过，正常reboot成功boot_id0110a188-4e64-4c5e-86ff-82405ed002a6、菜单SSH恢复、PE4仍低。最终切总电源未测试；IP5209无ACK/总线超时，battery unknown不能称已读取真实电量。

## 2026-10-04 / pending恢复 / 静态IP与菜单

7db63c0/5a22ab6/78d6698新增f1-ip-config、SSID绑定静态策略、S17启动调用、连接helper换网激活、菜单IPv4/CIDR/网关/DNS编辑；不改kernel/无线驱动/密码/路由器/播放器。当前静态10.0.0.234/24、gateway/DNS10.0.0.1，SSID wnk641_2.4G。主机输入校验/ARM构建通过，运行configure成功、停止udhcpc、SSH保持、GUI实际按钮自测rc0、截图布局通过。详情SCRCPT/network-and-audio-source/static_ip_20261004/changes_20261004.md。

但正常reboot后SSH无响应，ping/ARP不可达，电脑无串口，无法验证启动原因；已告知用户接回串口恢复。不称静态开机成功、不归档稳定；须先恢复SSH再修正失败fallback或回退。运行修改已经持久保存，旧S17/menu/helper源码及二进制保留，回退DHCP使用f1-ip-config configure dhcp。未再次断电、未通过串口传文件。

## 2026-10-04 / 实测补记 / COM11网络与IP5209

用户接回COM11后当前boot已Static IPv4 applied/SSH可用，补523e1f7广播与开机失败DHCP回退，0c9d3d9明确广播地址及用户确认普通4.2V电池OCV估算。正常reboot boot_id766d322a-3f35-4b45-bc68-e638ae823834，静态10.0.0.234/24、广播10.0.0.255、无udhcpc、菜单SSH恢复，通过单次正常重启（不是三冷验收）。IP5209接口可读Charging，插电电流-777mA；拔电后Not charging/+305mA，电流方向与旧driver注释相反未修，不能称符号已校准。用户电池4.2V已确认，UI用OCV带~估算，充电图标来自真实status；不改充电寄存器。

## 2026-10-04 / pending真实触摸与三冷回归 / 投屏等待三击退出

计划/文件/回退详见SCRCPT/cast-and-menu-source/cast-touch-exit-20261004.md。a26d456新增独立cast-wait-touch（原始FT6236 I2C方向，中心x140..339/y260..539、2秒三次按下释放，长按/滑动不算），cast-app管理生命周期；0505381/3942b56直接WPA STA-FIRST检查P2P关联及sinkPID/exe，手机开始握手或连接时释放检测，不触发退出。第一次空STA响应误判修正后测试重做。不是PBC/协议/驱动修改，不保留LVGL。

原GCC6.4增量构建、host及ARM手势自测通过；实际未连接投屏启动到ready、显式模拟三击→cleanup→rc0→菜单SSH恢复，final没有watch/player/sink/手势检测器遗留。helper PSS115KiB/private60KiB。59c225c菜单cast入口切独立cast-app-touch.sh，旧cast-app.sh保留。SCP哈希helper0ec48d5c6f7da49ded0a585f1e92c825be2f00713a77aefd355816a49d9fe816、脚本31fe30b11011f8a527beb3eb511ee1a0fa8b373dc76ebe4e54f63ba85f99f097、apps001ce6f4f9134d8c2b60f23f70da5500494923691ec9ab2276dd24c0e919c99e一致。kernel/驱动/视频/音频/其他菜单入口不改。真实三次触摸、真实连接下不退出、三冷完整回归尚未完成，不标stable。测试变量正式默认未设置，临时包装和运行日志清理。

## 2026-10-11 / pending / Isolated full SDK integration
Hypothesis: locked source exports and an owned output directory can support clean rebuilds without changing active source/build/runtime baselines.
Files: sdk; sdk-support/{sdk.py,test_sdk.py,manifest.json,README.md,configs,changes-20261011.md}.
Commits: c0e1f72 initial entry; b7ad1e1 Python API compatibility; 092afc7 verified Python3.5 compatibility.
Build: original Linaro7.2.1; make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- LOCALVERSION= -j8 in generated source export only. Active kernel b3d8ed3/#250 zImage SHA256 20e69f109abd3d8d95c73620c01287a82c17b10a5fbdb55020f0a419a5dfa4c9 retained.
Tests: initial10 output/archive/locking safety tests passed. Fresh kernel/U-Boot builds started; results pending. Rootfs preflight correctly identifies dirty legacy overlay submodule, not a new SDK change.
Dependencies: kernel+modules+DTB+U-Boot; Buildroot toolchain/libs; AIC/WFD; BS/BT/audio; LVGL/menu/apps/emulators; boot/runtime scripts; SD partition/image layout. Integration and full regression still pending.
Deploy: none; board observed read-only over SSH, active casting not stopped. SD partition starts2048/34816/1048576; p1 size32768 sectors, p2 size1013760 sectors. No SD writes.
Decision: SDK integration in progress; no complete-image or stable-baseline claim. Rollback: remove/revert SDK-only commits; active sources/configs/artifacts unchanged. Existing build outputs are not deleted by SDK clean.

### First clean builds and provenance results
SDK commits: c0e1f72, 092afc7, 5e9951e. Kernel full build/modules and U-Boot full build succeeded in isolated Git exports. Kernel release5.7.1; both configs unchanged. Kernel candidate SHA2567c7cce0300d5e7bb28d92f79731a9be814f080c79dfee3ffbdcefe12d150c51d; U-Boot candidate SHA2567a129c54badd5902ca94158a61044d94e809d5f033d17f62d8dac18bb1256460. No board deployment.
SDK safety tests13/13 passed, including actual clean of a generated fixture and protection of unknown directories.
Buildroot olddefconfig failed because linux/Config.in is missing from the original directory and committed source, not just from the export. Restore exact matching2018.02.11 upstream integration source after a full missing-file audit; preserve current customizations and dirty legacy overlay. Source-completeness gate expanded to detect this before builds.
Live SD U-Boot read-only verification: offset8192 length548864 SHA25671e34730de102099163f014c528ca0656bb1ef5c81fc2756f6a32477cd3cac04, banner2018.01-00008-gaf73846-dirty built2026-08-22 10:08:59 -0700. Current workspace binary differs. Historical af73846/c5335a4/fafaf45 source correspondence remains pending. Never label candidate U-Boot as the live original.
Full goal remains active: integrate complete rootfs, exact drivers/libs, current applications/emulators and boot/runtime files, generate flashable SD image, validate independent rebuild and clean, then cold-boot regressions. No complete-firmware claim.

## 2026-10-11 / in progress / Complete Buildroot and exact board boot provenance
Buildroot official2018.02.11 archive SHA2564d9cb4b862877eb2493bd857e0e3d49427b00fdeecd03bf5c24bfc952b586e5a. Full audit found10058 identical files,13 custom files,9 missing linux integration files; imported only9 files in931c3e9. Reaudit10067 stock files matched/missing0. Discovered package/Config.in replaced by13-line RTL8723DS/libnl/hostapd fragment;30bdc8e restored full upstream index with Cedar package entries. Current complete source9133440. Original.config/output and dirty overlay preserved.
Initial toolchain build succeeded but rootfs failed with5MiB target because corrupted package index deselected packages. Fixed index restores required packages; original architecture/toolchain/ABI selections verified unchanged before cache reuse. SDK now checks required packages after Kconfig and essential init/SSH/ALSA/BT files in rootfs tar. Full package build remains in progress; no valid full-rootfs claim yet.
Historical U-Boot c5335a4 + original.config + original Linaro7.2.1 reproduces full548864-byte SD boot region exactly after preserving independent program timestamp and image-header timestamp1787418595. SHA25671e34730de102099163f014c528ca0656bb1ef5c81fc2756f6a32477cd3cac04 matches live SD. Normal ./sdk clean --component uboot then ./sdk build uboot -j2 independently passed the same exact hash. Original U-Boot workingHEAD fafaf45/binary9114... retained, not replaced.
./sdk build board -j2: current vendor DTB byte-identical8ca3cd42bc6c2ebea132d93e8bbe7027f863c66c5edad8540ff5284fc0b996ff; panel/power/hidp/uhid builds passed, all vermagic5.7.1. No deployment.
SDK tests20 safety/config/staging/metadata cases; actual result recorded in task log. New project sources, modules and UI build recipes use owned .sdk-output only; all-app/runtime/image integration still pending. Board SSH observation/read-only boot files only; mounts cleaned, no reboot or writes.

### Base system and application build result
Complete base rootfs build passed; required init/BusyBox/account/SSH/ALSA/BT archive contents verified. First app build found missing libdrm selection hidden by old output residue; declared BR2_PACKAGE_LIBDRM=y only in generated SDK config and required-package checks. Rebuilt base archive61982720bytes SHA25668f6a59f4cda5e3797959ca06ac90932c56777d554e43dd67591e85f60e8509f, effective.config SHA256f7a83a2a3d3adbb6485c8e4d9e8107e43500a6bc8cda8191e507701f4cfecee1.
./sdk build apps-ui -j2 passed all13 declared binaries: menu/helpers, mediaUI/browser, recorder, SFC/browser, retro frontend/browser, reader/browser; MuPDF fully rebuilt from tracked source. Exact hashes in.sdk-output/images/apps-ui/build.json. No original artifact/toolchain dependency, no board deployment or runtime acceptance claimed.
Remaining full goal: ONS/wiliwili/media backend/emulator cores, AIC/WFD/BTaudio/peripherals, startup+runtime layout and assets, SD image assembly, independent clean/rebuild and hardware acceptance. release_ready remainsfalse.

## 2026-10-11 / pending / Remaining source-only runtime builds
Hypothesis: add the source-locked Miracast receiver to the isolated SDK, without importing old executables or changing the board.
Files: sdk-support/sdk.py, manifest.json, test_sdk.py, README.md, changes-20261011.md.
Commit: cf0a4f3; source SOFTWARE/miracast-sink ab5c7635cf0753dbb43a6b1a5a6ba8e5456787ee.
Build: ./sdk build miracast-sink -j2; generated Buildroot GCC6.4; ARM ELF34532bytes SHA2561082d105f74a04275970f36644a1504a496abcc2b31c4f51c223e48d1342ecd1. No SDL; NEEDED ALSA/pthread/libc. Report images/miracast-sink/build.json.
Tests: SDK24 tests passed, including fresh-toolchain/force-build receiver recipe.
Previous build verification: media-backends report confirms all4 executables built, ARM/noSDL. ONS/retro/LAN reports retained. None is a full image or runtime acceptance claim.
Deploy: none. Rollback: revert SDK-only integration commits; original build/runtime artifacts unchanged.
Decision: full goal still active; wireless protocol/BT audio/peripherals/runtime/image/independent rebuild/cold-boot acceptance pending.

## 2026-10-11 / pending / BlueALSA and automatic route source rebuild
Hypothesis: source-locked BlueALSA plus base SBC/GLib/DBus/BlueZ builds can replace archived runtime executables in final image integration.
Files: sdk-support/sdk.py, manifest.json, README.md, test_sdk.py, changes-20261011.md.
Source: BlueALSA82fdaea18130bf7b0c83a5e2566ddd4249901c82 and project2f1d627 automatic route sources. Original sources/configs/artifacts preserved.
Build: ./sdk build bluetooth-audio -j2 using generated BuildrootGCC6.4; autoreconf/configure --host=arm-buildroot-linux-gnueabi --prefix=/usr --disable-debug; make install DESTDIR inside generated source only.
Result: bluealsa/bluealsa-aplay/PCM+CTL ALSA plugins/f1-audio-route all ARM/noSDL. SHA256 and sizes in images/bluetooth-audio/build.json. Initial src/bluealsa libtool wrapper rejected by ELF gate; publish corrected to installed real binaries, no weakened checks.
Tests: previous24 SDK tests passed; installed-ELF/DESTDIR/sysroot regression added, result to be recorded after run.
Verified:25 tests passed after installed-ELF regression. Commits281727e/ad13bd4/0576873. Clean-test output concerns temporary fixtures only, generated SDK component builds preserved.
Deploy: none. Rollback: revert SDK-only commits, no original tree or board runtime rollback needed.
Decision: source build passed, runtime/image/cold-boot acceptance pending; full objective unchanged.

## 2026-10-11 / pending / Wireless and IP5209 module source builds
Hypothesis: preserve accepted AIC source separately from later peripheral development and build all external runtime modules against the isolated exact-release kernel.
Files: sdk-support/sdk.py, manifest.json, README.md, test_sdk.py, changes-20261011.md.
Source: AIC export53bec9ea327282707ceb9996f1607b51f117728b, IP5209 source4c5338d85e21bef04dd8a2b42b4e8bcfe67d4e20; H5 project2f1d627. Existing AIC root changelog preserved; all actual source changes still rejected.
Build: ./sdk build wireless-modules -j2 using original Linaro7.2.1, ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- LOCALVERSION=; AIC CONFIG_PREALLOC_RX_SKB=y CONFIG_F1C200S_PREALLOC_RX_SMALL=y.
Result: all4 modules built and vermagic5.7.1 mod_unload ARMv5 p2v8. Report images/wireless-modules/build.json records source commits/hashes. Original kernel/config/DTB/module outputs and board unchanged.
First preflight: config parser recognized BR2 only; corrected parameterized parser recognizes CONFIG_ and empty/disabled LOCALVERSION, no kernel config change or relaxed check.
Tests: kernel parser and metadata allowlist regression added. Full runtime/cold-boot checks not performed.
Deploy: none. Rollback: revert SDK-only commits.
Decision: source compilation passed; protocol tooling, formal runtime layout, image construction, independent rebuild and physical acceptance remain pending.

## 2026-10-11 / pending / WFD protocol build
Hypothesis: rebuild archived complete WFD source with fresh SDK dependencies rather than install archived programs.
Files: sdk-support/sdk.py, manifest.json, README.md, test_sdk.py, changes-20261011.md.
Source: archive274cd479e49be7488a87242c702e653de82e29590093b99d226b7307a48bef4d, only protocol-tools/wpa_supplicant_2_4_aic_wfd; original archive retained. Objects/dependency files/binaries excluded, symlinks/escape rejected.
Build: ./sdk build wfd-protocol -j2; generated GCC6.4 and libnl/OpenSSL sysroot; original WFD/P2P/AP configuration retained. Initial command-line CFLAGS prevented internal include/macros and failed; environment defaults retain Makefile additions.
Result: supplicant1020584bytes SHA2565bd65e3088766a30d40f0be22f9566958f445fe18fef181180a9bb508c5b27fe; cli66004bytes02f9855142115e75da8dd3ad63706e6e4380096e3a267f23a7b05c629544f219; passphrase30604bytes70111e1983380b8b94f6f65344be1c50210334f5ad97b1a9e3bc49cb08fcc4ed. ARM/noSDL checks passed. Report images/wfd-protocol/build.json includes archive/config hashes.
Commits: initial ddede0b, verified source export/provenance ec55335;31 tests passed. No running build remains.
Deploy: none. Rollback: revert SDK-only commits, original source and board runtime unchanged.
Decision: protocol source build passed; DHCP/runtime/image/portable clean rebuild and cold-boot acceptance pending.

## 2026-10-11 / pending / Runtime tools and installation layout
Hypothesis: rebuild missing menu backend tools and install all compiled components with explicit paths/checksums before assembling the final rootfs.
Files: sdk-support/sdk.py, runtime-layout.json, test_sdk.py, README.md, changes-20261011.md.
Commits:08a0b27 runtime-tools;cc2033c runtime payload.
Build:./sdk build runtime-tools -j2, generated GCC6.4; RTC9748bytes SHA2564436975e60e4e7efbd229ce1db928113b5bb21be526389603381d0ff1d0e29ec; IP9812bytes33af6a8b70de98a322926e3b1fb7a37a8e91ced8c74d04bdc7d1f644ea9f5408. ARM/noSDL checked.
Tests: host-built same-source RTC selftest passed timezone/NTP/manual date; IP validate accepted valid config and rejected3 invalid configs. No configure/apply/sync/RTC write or host network mutation.
Integration:./sdk build runtime-payload -j2 installed43 compiled artifacts/modules using report hash checks. Formal paths /opt/f1/apps, existing /opt/f1-time and /opt/f1-network, compatibility WPA /root/aic_miracast. Report images/runtime-payload/build.json records43 installed hashes.
Safety:35 tests passed including tampered artifact, duplicate destination, path escape and internal symlink-parent rejection. Unknown payload output cannot be overwritten.
Deploy:none; no board/SD changes. Rollback:revert SDK-only commits, active source/build/runtime baselines preserved.
Decision:compiled payload integration passed; fonts/firmware/services/shared libraries/base rootfs/SD image/portable clean rebuild/physical cold boots still pending. No complete firmware claim.

## 2026-10-11 / pending / Runtime assets and initial services
Hypothesis: install locked source resources and narrowly relocated service/app scripts without importing historical rootfs binaries or candidate paths.
Files:sdk-support/sdk.py, runtime-layout.json, test_sdk.py, README.md, changes-20261011.md.
Source:project2f1d627, AIC53bec9e firmware export, BlueALSA82fdaea freshly installed config. Original trees/files unchanged.
Integration:runtime-payload includes CJK font for8 apps, Takao font/license, RTL8723BS WiFi/BT firmware/license,7 AIC firmware files,9 app launchers, DLNA icons, RTC/headphone/audio-route services, ALSA/D-Bus configs.
Relocation:MEDIA/ONS backend and BlueALSA/audio-route executable paths only in install copies; known input required, candidate paths rejected, shell -n checks passed, source+installed hashes recorded.
Results:38 SDK tests passed, initial82 files installed; subsequent headphone service adds1. Actual installed fmac MD5=01acfbebdfb15755e3fe853e7bc95c7d matches established cast firmware. Installed scripts have no /candidates/ residue.
Commit:6200adb initial resource integration; subsequent service/result commit recorded in Chinese SDK log.
Verified:5de1bb0 headphone service/documentation,83 installed files and38 tests passed. Resource provenance40 targets plus43 compiled artifact targets. Report retained; all build processes terminal.
Deploy:none, services not run; board/kernel/DTS/source/build baselines preserved. Rollback:revert SDK-only commits.
Decision:resources and initial services integrated, not a complete rootfs/SD image. Full menu/WLAN/cast/wiliwili startup, remaining dependencies, clean portable rebuild and physical acceptance still pending.

## 2026-10-11 / pending / Kernel runtime modules and startup integration
Hypothesis: replace remaining hardcoded backend references and install full matching kernel module set plus normal WLAN/BT/menu/wiliwili startup files.
Files:sdk-support/sdk.py, test_sdk.py, runtime-layout.json, README.md, changes-20261011.md.
DLNA:generated output_cedar.c unique backend literal relocated only; original source retained. LAN rebuilt, gmediarender185884bytes SHA256f46c13e97cc4745af228ffb7dfaab72a4bf54cf6028cb0761d5ef00c3b066c2e; binary includes /opt/f1/apps/media/mplayer-native and no /candidates/. Source-transform SHA in images/lan/build.json.
Modules:b5d740b exports17 modules from complete configured kernel modules.order; each5.7.1. Generic hci_uart explicitly superseded by already rebuilt external H5. Kernel source/config/zImage hashes and modules recorded in images/kernel-runtime/build.json. No kernel source/config/build baseline changed.
Services:install copies of S17normalWLAN/S18SSH/S48Bluetooth and loaders point to exact kernel/extra module paths; AIC not added to normal boot/WLAN. Credential-free WPA template only.
Menu:dc842c3 installs S90/supervisor/12-entry app list and wiliwili launchers/CA/font. Helper-ip compatibility name uses same verified fresh helper. Menu heap-release/app-return lifecycle retained.
Tests:41 passed; shell syntax checks for install scripts; exact/idempotent/reject-unknown DLNA transform, dynamic module report/release checks, same-hash helper aliases verified.
Result:114 runtime files installed, reports retained. No deployment, service execution or SD writes. Full cast launcher/hardware boot/libs/base rootfs/SD image/portable rebuild/cold-boot acceptance pending.
Rollback:revert SDK-only integration commits; original sources/outputs and board runtime preserved.

## 2026-10-11 / pending / DHCP and merged rootfs dependency audit
Hypothesis: rebuild missing DHCP service from existing BusyBox source, then combine verified base and runtime files without importing old binaries or conflicting global network startup.
Source: no tiny_dhcpd_49 C source found in full host search/current archives/history, old binaries preserved and excluded from SDK. BusyBox1.27.2 complete Buildroot source used; CONFIG_UDHCPD=y in generated config only. Adapter be239f6/a47348e converts valid ACK to REQUEST watcher interface, RAM lease/FIFO and owned-child cleanup; no boot DHCP server.
Build:base rootfs63313920bytes SHA256941aba2dfffcd037ad55159b7dedfd2a0ed320d4f29cfcfa8e8fb1017a773478. Udhcpd applet installed;117 runtime files integrated. Actual protocol/hardware acceptance pending.
Assembly:b180d49 merge preserves tar nodes/links/permissions, root-owned runtime files, only S41dhcpcd excluded after verifying its known service content. Original Buildroot target/archive/config not overwritten.
Audit:38ce1d5 initial dependency audit failed on function-name typo;4f48218 corrected and added missing-library simulation.47 tests passed.301 ARM ELF files inspected; interpreter/NEEDED files resolve in archive, issues0. Not symbol-version/ABI or runtime acceptance.
Result:images/rootfs-integrated/rootfs.tar201338880bytes SHA256988a814d9da8a138da4c85386d1177baa2ca3ec7957789b0ec8c2da3ec8b9c4c; dependencies.json6400a205779d42eef94ddeed3b2b1567240faa4664acd6dde39b687c15ebea13.
Deploy:none; all build sessions terminal, no board/kernel/SD writes. Rollback:revert SDK-only commits, active baselines preserved.
Decision:assembled rootfs candidate and file dependencies verified; complete cast entry/hardware startup/ABI/SD image/portable clean rebuild/three cold boots remain pending. Full objective active.

## 2026-10-11 / pending / Formal cast entry installation and checksum verification
Hypothesis: retain existing FIFO/PBC/RTSP/native-rotation lifecycle while installing locked source launchers at formal paths and refreshing every checksum to source-built artifacts.
Files:sdk-support/sdk.py, test_sdk.py, runtime-layout.json, runtime-cast/wpa.conf, README.md, changes-20261011.md.
Source:project2f1d627 direct git blobs for cast scripts, SDKtracked credential-free WFD config. No historical unchecked executable imported, no original source changed.
Integration:f5e304b installs cast entry/start/loader/PBC/RTSP/memory preparation, uses BusyBox DHCP adapter, keeps5745MHz/800x480@60/rotate90/ALSAdefault/FIFO RAM. Source and install hashes plus checksum input paths recorded.
Correction:024a529 generated cast-wait-touch sink literal migrated to formal sink identity and apps-ui rebuilt. Exact/repeatable transformation test added, unknown source refused.
Verification:51 tests passed, including nested path longest-first relocation, current-content MD5 substitution, missing checksum input rejection, cast installed-target tamper rejection.13 distinct preflight target MD5s plus session watcher checks agree with actual payload. Shell syntax checks passed.
Result:125 runtime files; rootfs201369600bytes SHA256761018d076661b468738cb4c5632f2ccc1f1d94873b9e988a8bf0e6b66bf8770;301 ARM ELF dependencies resolve. Reports retained.
Deploy:none, no cast session/hardware/SD mutation. Build processes terminal. Rollback:revert SDK-only commits; original sources/runtime unchanged.
Decision:cast source installation and offline checks passed; actual DHCP/PBC/RTSP/display/triple-touch behavior, hardware startup/data mount/SD image/portable build-clean/cold-boot acceptance pending. Full objective active.

## 2026-10-11 / pending / Hardware startup and first SD image candidate
Hypothesis: integrate exact-release hardware module startup and non-destructive data mounting, then generate a three-partition SD image file from source-built inputs.
Files:sdk-support/sdk.py/test_sdk.py/runtime-layout.json/runtime-cast/S12board-hardware/S19data/manifest.json/README.md/changes-20261011.md.
Startup:38195d8 installs dependency-ordered USB PHY/MUSB and ION/Cedar loader, existing power/panel/HID/IP5209 services with formal module paths/checksums.131 runtime files,52 tests,301 ARM ELF dependency audit passed; rootfs201390080bytes6bdcb7e8dff7e09df9c51af910baf95669eeed2f6542232d84e631fd7262a3ae. No BU/old gmenu startup. S19data only mounts existing VFATp3; never partitions/formats a card.
Tools:old host mke2fs lacks-d;image-tools builds e2fsprogs1.43.9 from locked Buildroot in generated host. No system-tool replacement.
Image:20cd2e5 generated1073741824byte .sdk-output/images/sd-image/f1c200s-sdk-candidate.img SHA256b930ea25b0f1d52f57644090c20500e07aecc92b08dbeb3e6af4eb37f156cb55.
Layout:U-Boot at8192; boot FAT16 start2048 size32768 sectors; rootfs ext2 start34816 size1013760; data FAT32 start1048576 size1048576. Existing verified boot layout retained; data contains empty app folders only.
Checks:53 tests passed, overlapping partitions/boot region refused;3 FAT boot files copied back/hash matched;ext2 e2fsck all5 passes;console root character node;both FAT fsck passed;merged image partition regions equal generated filesystem hashes;U-Boot region matches71e34730... . Report images/sd-image/build.json retained.
Deploy:none, no physical card/board writes, cold_boots0. All build sessions terminal, generated temporary filesystem trees removed after assembly.
Decision:image file generated and offline verified, still unaccepted candidate. One-command full build/clean, portable independent rebuild, ABI and full hardware/application three-cold-boot regressions remain pending. Full objective active.

## 2026-10-11 / pending / Full build and version-requirement audit
Build:d9b2bba ./sdk build -j2 completed all19 stages, exit0,55 tests. Candidate1073741824bytes SHA25676a4b72076e9faf33ad0e667ba6912f66f24e59b40c9d5443b78706f625470d3. Integrated rootfs115ea23470c26fe5f81887c6c9f8420b9352824846d2b8ecd13245b0a434fa89. Existing isolated cache used; not an independent clean rebuild.
Planned:extend readelf audit from file resolution to GNU version definitions/requirements on actual resolved providers. Unit tests cover successful and missing versions/providers,weak requirements and parsing. No original component source/config or board changes.
Files:sdk-support/sdk.py/test_sdk.py/changes-20261011.md/README.md and this record.
Protected:all image applications and dynamic libraries; no hardware deployment. Full symbol relocation/dlopen and runtime behavior are not covered by this offline version check.
Rollback:d9b2bba SDK recipe/tests; existing original component repositories and board remain unchanged.
Portability:TOOLCHAIN directory absent, kernel compiler still external /opt Linaro7.2.1; must package compiler before portable acceptance. Full objective active,cold_boots0.
Result:fb5c20f,57 unit tests passed; rootfs-integrated301 ARM ELFs and840 GNU version requirements checked,issues0. sd-image assembly exit0,all offline filesystem/region checks passed,new image SHA25643006d9ac611b7f5f17d897e178e83b6610e913d92c6a4a0a72999e660e19374,size1073741824bytes. Old full-build report remains historical evidence for d9b2bba/76a4b720 image,not evidence for a complete rebuild of the new recipe. No board writes. Full symbol relocation/runtime checks remain pending; no claim of total ABI validation. Temporary wrappers removed after collecting results; reports retained. Decision:offline check passed,unaccepted candidate,full goal active.
