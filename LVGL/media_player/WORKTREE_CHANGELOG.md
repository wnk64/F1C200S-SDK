# Worktree Change Log

## 2026-10-07 / planned / WAV files absent from media browser
Baseline0e76791; board browser8ba618a5/frontend9eda7361/backend3cff48d2 exact source retained.
Observed: Recordings --list outputs no files although3regular WAV recordings present. Audio directory lists2MP3 files. Browser allowlist only MP3/MP4, UI audio-only detector only MP3.
Files: media_browser.c add WAV allowlist, mplayer_ui.c WAV audio-only detection, media-app.sh select colocated frontend and SD playback log; apps.conf media path only.
Hypothesis: support existing WAV PCM through audio-only backend, without allocating video OSD/CMA or waiting for video frame.
Deploy: new /root/roms/candidates/media_wav_20261007 with existing CJK asset, retain old menu/UI/backend. No new worktree or backup.
Rollback: restore media entry from0e76791; original media menu/UI binary paths unchanged.
Protected: MP3/MP4 UI, geometry backend, kernel/DTB/audio routing/network/Miracast. Tests: list/check WAV/MP3/MP4, existing-file playback control tests, three coldboots pending.

Result: source c361399, make -C LVGL/media_player -j2 PASS; existing _GNU_SOURCE redefinition warning only.
New SD candidate media_wav_20261007, hashes matched: UI30c587169db41ff381d7c76e38426f77795658b35c0db5f0511c05b74e312a3d; browserecf8aa4381a37c46b8b4d81bfff3aadb671b4e6a7470200c0c0c101c72fff5c0; launcheree9ca8eb821c20db29c55f3ebb50b58a4f2ad223a71899b17fb8de5cd32962ca; CJK asseta2cea8412a83661341e951e7549f1b9fef506c93c34b7e6fdece7892df86ace0.
Browser --list now includes3previous WAV recordings; --check0. Timed browser GUI rc0, recordings-browser-fixed.png shows file rows/paging/Chinese title correctly.
WAV selftest0: pause stable0.200, volume29.999996, seek9.600 target10.000, audio-ready PASS; no video OSD/CMA allocation.
MP3 CodeMonkey selftest0: pause/volume/seek/lyric/hide PASS, lyric index31 at94.900. Parser and LRC selftests0.
800x480 MP4 selftest0: visible480x800 source0,0 native rotation, pause/volume/seek/frame/hide PASS; pool peak10358784 capacity12582912 fallback0/live0. Backend3cff48d2 unchanged.
Menu apps SHAd0177492 verified, original binaries retained; user recordings not removed, no helper scripts created. No kernel, DTB, wireless, default audio route or Bluetooth service changes.
Decision: deployed candidate, actual user listening/touch and3coldboots pending; does not claim support for formats beyond MP3/MP4/WAV.
Launcher end-to-end: MEDIA_APP_START_DIR=Recordings / MEDIA_BROWSER_TEST_SELECT=existing20sWAV / MPLAYER_UI_SELFTEST=1 / MEDIA_APP_TEST_ONCE=1 -> rc0; browser selection, check, audio UI, pause/volume/seek and menu restoration passed. Playback log written on SD; original full rootfs was not expanded or cleaned.

## 2026-10-05 / planned / menu geometry backend selection
Switch media-app.sh BACKEND only to /root/roms/candidates/mplayer_geometry_20261005/mplayer-native-geometry SHA2563cff48d26822847db9fdf466d6d3f9168b4073086d291187fc56c480a04808c2. Backend source85440d9/6ceb14f validates decoded MB32 stride/window; multi-size decode tests fallback0/live0,800UI and MP3 controls regressions PASS,620 full UI EOF0. No kernel or UI changes. Existing menu-launched playback7192 untouched; new path applies on next script launch. Rollback previous backendff93 retained, media-app.sh commit767f6ba/55189f1. Actual visible non800 geometry pending user confirmation, not stable baseline.

## 2026-10-05 / planned / startup menu media entry
Baseline bb2932d; user confirmed playback controls/lyrics work. Keep frontend9eda73615eb9320c2aacdde3d24f4f3eb4019b9baf7779f543e667bfbf094bbf and backendff93 unchanged.
Files: media_browser.c (independent adaptation of tracked ONS browser), media-app.sh, Makefile, SCRCPT/cast-and-menu-source/apps.conf.
Browser: portrait480x800 large rows, directory traversal/paging, readable regular MP3/MP4 only, newline filenames rejected. Browser exits before player opens; player exits before browser returns. Selection path treated as data, never eval. Temporary selection removed on exit.
Protected: ONS browser/menu UI/supervisor source, cast/wiliwili/network/kernel/audio configuration. Old apps.conf Git bb2932d hashf5bd02bbeeee6fb44e124c45592f423a65707402430c8288775c5814fc6abc7c rollback. Only add sixth app; preserve all five existing commands.
Tests: supported-file check/list, browser timed render, opt-in selection/player selftest, launcher cleanup/menu return. Physical touch and three cold boots pending.
Commit767f6ba. Build make -C LVGL/media_player media-browser -j2 PASS, GCC6.4. Browser8ba618a566abf59c5df7d0759d95dde3f5dbdc89eb7178ef931f461ac7756123, launcher75c167fcccffec5734440cda2386e3c3e3c99359890a810a0fc038f8224974fa, appsad8fb0b16a856d7163ee8e2b4c4836b67d04259088021d12a08cce0b720020c2; SSH board hashes match.
Runtime: new mplayer_menu_20261005 candidate, existing shared cjk28.bin referenced; only existing apps.conf receives added sixth row. Board --validate0; launch-selection selftests for CodeMonkey.mp3 and test_800x480.mp4 both0, controls/lyric/target-frame checks PASS, video pool fallback0/live0. Selection temp removed; menu supervisor restored PID2640. Transient existing service tr/read process-exit race printed during stop, service still completed stop/start; no service source edits. Cold boots0, actual menu touch pending.
Build follow-up: include media-browser in default all target and ignore its generated executable, so source-only commits remain reproducible without generated binaries.
Follow-up commit55189f1; default make PASS. Timed portrait browser render rc0; screenshot media-browser.png confirms large non-overlapping rows, path/up/back/page controls. Temporary raw captures removed, no helper scripts created. Menu restored PID2793, no browser/player left active. Candidate integrated for user touch test, not three-cold-boot stable baseline.

## 2026-10-05 / planned / MP3 controls and synchronized LRC
Baseline: e5e99e8, existing video frontend8b978fd7368f0cbf8766c8c17494ad0d5849e083d8420dfdea151f855eea3f64 retained.
Files: mplayer_ui.c audio-only mode; lyrics.c/.h bounded UTF8 LRC loader and tests; Makefile.
MP3: mpg123/default ALSA, visible full framebuffer UI without video OSD or CMA reservation; audio position from MPlayer ANS_TIME_POSITION. Metadata title/artist; drag pause, seek release, volume, exit. Lyrics from same basename.lrc; no fabricated lyric if absent.
Protected: video frame-driven position and private OSD, backend, kernel, Bluetooth/network configuration, startup menu. Test parser/LRC, MP3 controls with existing user file, video regression. Three cold boots pending.
Code commit907a7df; build8bf41cc04eabfc8d893fd398e96bcf18d847713825feae9aec9e0adad06d12ad. Board parser/LRC selftests PASS; audio UI seek86.000 target86.000, pause/volume PASS; video regression PASS/fallback0/live0. Screenshot confirms metadata/Chinese no-lyric state/controls. Paused get_time_pos emits ALSA DELAY(-77) because paused device state; planned skip position polling while paused, retain last played position. Strip MPlayer metadata transport quotes.
User requested real matching MP3/LRC download: Code Monkey by Jonathan Coulton, public Internet Archive audio and LRCLIB18493616 timed lyrics. Verify download hashes, playback duration, visible lyric advancement and seek reset; do not fabricate lyrics. Preserve test.mp3 and old binaries.
Probe: MPLAYER_UI_DEBUG=1 opt-in index/position logging (default off); selftest now verifies post-seek displayed lyric equals the timed LRC entry. Paused-position fix be6333c rebuilt3ee1174eab509bc983b6dea00bbd9f7b4d9cbe5d46d7ae2947d1ff1cea91dd46; user MP3 selftest PASS without previous ALSA warnings.
Probe source9b9abbb rebuilt9eda73615eb9320c2aacdde3d24f4f3eb4019b9baf7779f543e667bfbf094bbf, board hash matches. Real downloaded MP3/LRC test exit0, lyrics60, post-seek displayed index32 at95.500s PASS. Normal playback beyond60s: two framebuffer samples show changed lyrics/time/slider, ALSA RUNNING. Details/source media hashes: lyrics-test-record.md. Candidate pending user listening/touch and three cold boots. MP3/LRC retained by request; temporary startup command and raw captures removed, no new backups.

## 2026-10-05 / planned / OSD plane enumeration
Baseline: d86d841, frontend7d4a01b16d608a75fc6df34c5fb8a1be58ad8a16693de8b549e28c591a2d02cc.
Hypothesis: without UNIVERSAL_PLANES, primary layer0 is omitted; planes[1] mounts layer2 while private visibility commands move layer1. Hidden widgets clear the still-visible layer2 to black.
Files: media_display.c only; enable atomic/universal plane enumeration before selecting primary0/OSD1, log selected IDs and validate primary type.
Protected: Cedar decoder/backend, native rotated geometry, audio, network, kernel, menu and wiliwili sources unchanged.
Rollback: existing mplayer-ui-volume candidate; source d86d841. Build: make -C LVGL/media_player -j2. Visual hide and three cold boots pending.
Commit: ee528d0. Build PASS with existing _GNU_SOURCE redefinition warning; SHA256 8b978fd7368f0cbf8766c8c17494ad0d5849e083d8420dfdea151f855eea3f64.
Deploy: SSH/SCP to /root/aic_miracast/candidates/mplayer_ui_20261005/mplayer-ui-planes; board SHA256 matches, old mplayer-ui-volume untouched.
Board selftest exit0: primary0=31 OSD1=35; pause/volume/seek/frame-ready/hide PASS, pool peak10358784 fallback0 live0.
Interactive looping playback started with same backend/test_800x480.mp4; exit returns startup menu. Decision: pending actual visible hide confirmation and three cold boots; synthetic hide is not visual proof. No kernel/service configuration changed.

Worktree: /home/wnk/Desktop/F1c200S_SOFT
Component: LVGL/media_player frontend and optional Cedar MPlayer frame notification
Base: MPlayer618d09a/noSDL candidate96a9cab0df56079ac251f639d59ace224ff94541224ed637cfa019df6ab7ebbf
Purpose: add real touch playback controls with separate low-memory OSD.

## 2026-10-05 / planned
User confirmed local hardware-rotated playback. Pool12MiB peak10358784/fallback0/live0, FAAD S16 ALSA; old system/players preserved.
Files: new frontend/landscape display adapter; optional MPLAYER_OSD_FD frame messages from VO after successful commit/vblank.
UI: center toggles controls, idle10s hides OSD offscreen; pause, real frame-driven progress, drag pause/absolute seek, software volume, exit.
Display: proven wiliwili86x800 RGB565 layer1 pattern, no devmem crop hacks, no SDL; underlying video stays layer0.
Position: hardware picture PTS with decoder origin, no fabricated wall-clock progress, loading until target-frame commit.
Memory: LVGL128KiB, striped render buffers,137600-byte OSD, no full-screen bitmap cache.
Tests: build/dependency, command parser and controls, actual layer appearance/hide, pause/seek/audio/exit plus menu recovery required; three cold boots pending.

## 2026-10-05 / planned / control selftest
Commits: a51b48a frontend/frame notification,35a99f8 opt-in selftest/hidden control rendering.
Build: frontend ELF needs libdrm/libm/libc only; no SDL. Backendff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d built.
Files: mplayer_ui.c startup/pipe failure handling and selftest flag; parser selftest rejects malformed/nonfinite replies.
Selftest: explicitly MPLAYER_UI_SELFTEST=1; pause stability, actual volume acknowledgement, drag-controller seek and target-frame readiness, hide request, exit/cleanup. Synthetic LVGL actions do not prove real touchscreen or visible hide.
Default: no selftest,10s idle hide; control widgets hidden when OSD hidden; progress redraw4Hz.
First board selftest: parser PASS, actual frame received, pause stable0.366s; volume displayed29 versus request30 because float truncation, exit1/menu restored. No leak: pool live0.
Planned mplayer_ui.c: round float volume for integer slider, retain full double acknowledgement and require within0.1percent of requested30; no fake acknowledgement or relaxed playback-frame check.
