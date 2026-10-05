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
