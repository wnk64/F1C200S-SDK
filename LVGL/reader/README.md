# F1C200S LVGL Reader

Build: `make -C LVGL/reader -j2` from `/home/wnk/Desktop/F1c200S_SOFT`.
Uses existing GCC6.4 Buildroot sysroot, LVGL8 tree, menu platform/input code and streamed `cjk28.bin`. No SDL dependency.

Runtime: `/root/roms/candidates/reader_20261005/reader-app.sh`.
Menu launches file browser, browser exits before viewer, viewer exits before browser returns. Top left exits viewer; lower arrows turn pages or change images. Text top-right cycles encoding; image top-right switches Fit / 1:1 center-window.

TXT: UTF8 with/without BOM, GBK, UTF16LE/BE BOM; invalid sequences become replacement glyphs. Reads4KiB source window per page.16 display lines/page,4096-page history; no full text cache. Manual encoding change resets to first page. No resume/bookmarks or arbitrary font-size control yet.
Images: JPEG,PNG,BMP,GIF(first frame only),TGA,PNM(binary PGM/PPM),PSD through upstream stb_image. Limits4096 per dimension,2M pixels,12MiB decoder allocation. Decoded source is freed after RGB565 screen-size conversion. Fit does not enlarge small images;1:1 clips a centered window (no pan yet). No animated GIF/WebP/HEIC/SVG/PDF support.

Vendored `stb_image.h`: https://github.com/nothings/stb , MIT/public-domain dual license included at file end. Exact SHA256594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3. Not a newly hand-written image parser.
GBK map copied from tracked ONS generator output, SHA2568be7fbdd95c7535e65056fd1f3eb8ebf6dabe4fbf1c9dc09cd8628754b96fde0. Recreate with `python3 LVGL/ons_native/generate_gbk.py LVGL/reader/gbk_map.h`.
Reader punctuation font generated with `py -3 generate_reader_font.py` on Windows GDI/Microsoft YaHei; generated C is tracked for Linux builds. Original menu font/source unchanged.

Diagnostics default off: `READER_TEST_MS=3000` timed exit; `READER_SELFTEST=1` widget event tests; `MEDIA_BROWSER_TEST_SELECT=filename` automatic browser selection; `READER_TEST_ONCE=1` launcher single selection. Normal menu sets none. CLI `reader --selftest-text`, `reader --check-image path`, `reader-browser path --check/--list`.

Tests recorded in WORKTREE_CHANGELOG.md. Menu integration and interactive candidate installed, not three-cold-boot stable acceptance.
