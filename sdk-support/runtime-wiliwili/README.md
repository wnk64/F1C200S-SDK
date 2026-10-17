# wiliwili runtime recovery overlay

These hash-gated overlays apply to the SDK-locked wiliwili source without
changing the protected Cedar renderer. Build incrementally with:

```sh
./sdk build media-backends -j8
python3 -m unittest discover -s sdk-support -p test_sdk.py
```

No image is packaged by this command. Unchanged overlays preserve timestamps.

## Optional board diagnostics

Normal runtime does not enable either diagnostic. Reserve the display before
running them, stop each diagnostic before starting another display owner, and
restore the menu afterward. Do not use these commands to power-cycle hardware.

Build the progress probe with the existing SDK compiler:

```sh
.sdk-output/work/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-gcc \
  -Os -Wall -Wextra -o /tmp/f1-wili-progress-probe \
  sdk-support/runtime-wiliwili/playback_progress_probe.c
```

Transfer through SSH, then run on the board with a stream URI on stdin:

```sh
F1_CA_FILE=/opt/f1/apps/wiliwili/cacert.pem \
  /tmp/f1-wili-progress-probe /tmp/cedar-net-player 55000 40 < /tmp/private-uri
```

Arguments are player executable, seek milliseconds, wall timeout seconds, and
optional required advancing-video seconds (default 8). PASS requires at least
60 increasing committed YUV timestamps and the required video-time span.
It does not certify visible image contents, audio quality, or absence of short
stalls. Logs can contain the signed URI: do not publish them unredacted.

Set `F1_WILI_COVER_SELFTEST=1` when launching the UI to test actual feed loading,
detail entry, return, and full cover restoration. The test exits on success or
after 120 seconds with failure. Keep the normal CA, config, and player settings;
the test uses the live network and requires a feed containing more than four
covers. Unset the variable for normal use.

Remove board-side diagnostic binaries, private URI and temporary logs after
testing. Source and tests remain here for reproducibility.
