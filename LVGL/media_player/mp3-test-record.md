# Local MP3 test / 2026-10-05

Plan: user requested C:/Users/26301/Downloads/测试.mp3, 6901849 bytes.
SHA256: 69560627bcf9f589fbd9cfb3eb9975147c56c1ec19729ea423a751604d49d47d.
Stop validated video UI PID28390 gracefully; its wrapper restores startup menu.
Use existing no-SDL MPlayer backend ff93fecb429f642f1728d51959fc24826bdd9a3eaebf2f45bb46e3e354c3315d with audio-only default ALSA, no forced FAAD decoder.
Transfer via SCP to new /root/roms/audio/test-user.mp3, verify hash. No kernel, source or audio-route changes. Actual audible result pending.

Result: transferred SHA256 matches. Existing MPlayer PID29822 selected mpg123 decoder; 44100Hz stereo S16_LE, 320kbit/s, duration172s. ALSA hw:Codec,0 RUNNING, hardware pointer advances. Audio is intentionally playing for user listening; startup menu restored. Command: mplayer-cedar-ui -noconsolecontrols -noconfig all -novideo -vo null -ao alsa /root/roms/audio/test-user.mp3. No forced video/FAAD decoder, no hardware configuration modifications. Log: candidates/mplayer_ui_20261005/mp3-test.log. Audible quality and complete end-of-file not yet verified; temporary command files removed.
