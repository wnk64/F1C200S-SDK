# MP3 with synchronized lyrics / 2026-10-05

Requested: download matching MP3 and LRC, test the actual audio player page.
Audio: Jonathan Coulton, Code Monkey. Source https://archive.org/details/jonathan-coulton-code-monkey ; file Jonathan Coulton - Code Monkey.mp3.
Source metadata: size7671971, MD5 35d709d8a0ae43004a0830334d21f174, reported duration190.45s.
Lyrics: https://lrclib.net/api/get/18493616 ; same artist/title, album Thing a Week 3, duration189s,60 timed lines. UTF8 without BOM. The duration is close, not proof of audible alignment.
LRC SHA256: 7fd228d339bfec2ad7ab65b25c4f40b59b2c18393892754ec1699c675265a663.
Frontend source9b9abbb, SHA2569eda73615eb9320c2aacdde3d24f4f3eb4019b9baf7779f543e667bfbf094bbf; optional MPLAYER_UI_DEBUG=1 records only position/index, defaults off.
Original user MP3 and prior candidates preserved. Same backendff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d, default ALSA, audio software decode. No video/CMA buffer or driver/kernel/network changes.
Planned tests: checksum, actual LRC load, pause/volume/seek, displayed lyric index after seek, independently sampled framebuffer images, audible timing needs listening. Three cold boots pending.

Results: full7671971-byte MP3 matches source MD5; SHA2560235a640168081a248437fdc6d9333ad18d2fa16d88173baccbc3212259a8ab6. Board audio and LRC hashes match host. Source audio metadata is untitled/Unknown Artist; public source identifies Code Monkey, lyrics same title/artist. No source tags rewritten.
Real-song selftest exit0: lyrics60, pause1.200 stable, volume29.999996, seek95.000 target95.000, displayed lyric at95.500 index32 PASS. Decoder reports duration190s; lyric source189s, audible alignment unverified.
Normal interactive playback: optional debug trace from index0 at1.000s to index20 at60.800s; two independently captured framebuffer screenshots at00:02/01:00 show different current/next lyrics and progressing slider. ALSA RUNNING and hw_ptr advances139280->2702526. No warnings in normal run to this point. Screenshots retained on host as lyrics-ui-a.png/b.png; temporary raw dumps and launch command removed. Same track remains playing for user's listening/touch test; menu restores at exit/EOF. Full EOF, audible timing, physical touch and three cold boots not claimed accepted.
