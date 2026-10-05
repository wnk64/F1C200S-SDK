# MP3 with synchronized lyrics / 2026-10-05

Requested: download matching MP3 and LRC, test the actual audio player page.
Audio: Jonathan Coulton, Code Monkey. Source https://archive.org/details/jonathan-coulton-code-monkey ; file Jonathan Coulton - Code Monkey.mp3.
Source metadata: size7671971, MD5 35d709d8a0ae43004a0830334d21f174, reported duration190.45s.
Lyrics: https://lrclib.net/api/get/18493616 ; same artist/title, album Thing a Week 3, duration189s,60 timed lines. UTF8 without BOM. The duration is close, not proof of audible alignment.
LRC SHA256: 7fd228d339bfec2ad7ab65b25c4f40b59b2c18393892754ec1699c675265a663.
Frontend source9b9abbb, SHA2569eda73615eb9320c2aacdde3d24f4f3eb4019b9baf7779f543e667bfbf094bbf; optional MPLAYER_UI_DEBUG=1 records only position/index, defaults off.
Original user MP3 and prior candidates preserved. Same backendff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d, default ALSA, audio software decode. No video/CMA buffer or driver/kernel/network changes.
Planned tests: checksum, actual LRC load, pause/volume/seek, displayed lyric index after seek, independently sampled framebuffer images, audible timing needs listening. Three cold boots pending.
