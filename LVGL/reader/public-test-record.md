# Downloaded document test / 2026-10-05

Plan: user requests several downloaded tests. Existing codee7a9e37/binary82fe43535275cbdfd6c637d7057b24d8ffd9a1535b1a5cf7bdedf39a1ddacf38 unchanged; no rebuild or driver/kernel/config change. SSH-only copy to /root/roms/reader_samples/public, existing samples preserved. Run each sequentially with timeout, first/last render checksums, then UI controls/screenshots for representative multi-page PDF/EPUB. One-page PDF does not run a selftest requiring next page. Public inputs are unmodified, retain source/hash and test output; no stable/three-cold-boot claim.

Sources:
- w3c-dummy.pdf: https://www.w3.org/WAI/ER/tests/xhtml/testfiles/resources/pdf/dummy.pdf ; SHA2563df79d34abbca99308e79cb94461c1893582604d68329a41fd4bec1885e6adb4.
- mozilla-tracemonkey.pdf: https://raw.githubusercontent.com/mozilla/pdf.js/master/test/pdfs/tracemonkey.pdf ; SHA2563662ff519e485810520552bf301d8c3b2b917fd2f83303f4965d7abed367e113.
- mozilla-images.pdf: https://raw.githubusercontent.com/mozilla/pdf.js/master/test/pdfs/images.pdf ; SHA256f7a44ca650d951cfea007440cda297dabbf2cf0c8f77a948a580cc0e99496725.
- mozilla-ccitt.pdf: https://raw.githubusercontent.com/mozilla/pdf.js/master/test/pdfs/ccitt_EndOfBlock_false.pdf ; SHA256f2289b94e7e3f05f9e37754c399d3f525edf9444fb32c3279959cf64b575fe62.
- alice.epub: https://www.gutenberg.org/ebooks/11.epub3.images ; Alice's Adventures in Wonderland, SHA2564f81245d31380ac1d6eafb9a321e3691369bc70985736ae4bccf849a4f784036.
- chinese-sanzijing.epub: https://www.gutenberg.org/ebooks/12479.epub3.images ; 三字經, SHA256a30ecb0e8b139ebf1c957e6ab982b2d49736b1b72d5617f3d9cd756f92384c1c.

Retain publisher licenses/notices embedded in samples; no assertion of public-domain status outside source jurisdiction. Source selection uses official W3C/Mozilla test resources and Gutenberg downloads, no paywall bypass.

Initial results: fourPDFs and Alice pass CLI; Alice450 reflow pages/control selftest0. Chinese三字經72 layout pages but cover fails malloc11520000 due20MiB allocation cap. ZIP image inspection: embedded PNG1600x2400 (3.84Mpx). Original file preserved. UI screenshot explicitly shows error; safe error/normal UI exit not acceptance. Previous selftest screenshot attempt missed page transition and captured restored menu; not proof of Chinese-body display.
Plan add standalone document-page-probe (test target only) using exact same document.c allocator/render adapter, selecting arbitrary page to verify body. No reader runtime/decoder/memory-budget/menu change. PPM output is diagnostic page evidence only, not a workaround substituting for EPUB acceptance. Keep this reproducible test tool source tracked; temporary shell wrappers removed after tests.

## Results

All six SSH-transferred input SHA256 values match host. Sequential CLI30s watchdog never fired. Same reader binary82fe435 unchanged; no driver/kernel/menu/config changes.

| File | Pages | CLI Result | Seconds | Document Allocator Peak |
| --- | --- | --- | --- | --- |
| w3c-dummy.pdf | 1 | PASS | 0 | 2348218 bytes |
| mozilla-tracemonkey.pdf | 14 | PASS first/last | 2 | 2269804 bytes |
| mozilla-images.pdf | 1 | PASS | 3 | 6929540 bytes |
| mozilla-ccitt.pdf | 1 | PASS | 0 | 1270500 bytes |
| alice.epub | 450 | PASS first/last | 4 | 11332031 bytes |
| chinese-sanzijing.epub | 72 | FAIL cover render | 1 | 15904344 bytes before refusal |

UI: TraceMonkey14-page PDF and Alice450-page EPUB next/previous/font-reflow selftests exit0. Mozilla imagePDF and Alice covers independently framebuffer-captured, visible/nonblank and correctly fitted. PreviewRSS9784KiB for imagePDF,8088KiB Alice,6268KiB Chinese error screen. Normal timed preview exit0 is lifecycle result, not acceptance of the Chinese failed render. EPUB3 version warning is emitted by engine; successful Alice shows warning alone is not a failure.

Chinese ZIP inspection:1600x2400 PNG cover, compressed45656 bytes. Requested additional11520000 bytes for cover raster exceeds20MiB current heap budget with simultaneously held decode intermediates. Independent same-adapter probes pages4/6/10/35/72 render480x600 successfully, peaks about1.8MiB. Page6 PNG evidence shows actual Chinese三字經正文. Pages35/72 are publisher license text, not substitute proof of Chinese body. Original EPUB unchanged. Classified partial compatibility failure: large embedded PNG cover path needs streaming/downsample investigation, not blindly raising RAM cap. No implementation fix made in this test-only turn.

Probe source8f7c3da, c17d0a6e9bea8423ad5a1b4f1654d6bc1685e79e7f2856b1efa8c3a84553d919. Build `make -C LVGL/reader document-page-probe -j2`, no effect on default reader executable; probe retained in reader_documents candidate for immediate follow-up and tracked source. PNG screenshots retained as evidence; own raw dumps/PPM intermediates and two temporary shell wrappers removed. Sample input files retained per request at `/root/roms/reader_samples/public` with logs subdirectory. Menu restored; no reading process/selection temp left. Three cold boots not done, no stable label.
