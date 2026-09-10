# TK032F8004 external DRM panel candidate

External panel module for Linux 5.7.1. It drives the reference panel's
three-wire command interface using DTS GPIOs and obtains all RGB timing from
the DTS `display-timings` node.

The module does not modify the stable kernel tree. Build against the matching
kernel build tree only after release-lock preflight.

Required DTS GPIO names: `reset-gpios`, `enable-gpios`, `backlight-gpios`,
`cs-gpios`, `sclk-gpios`, and `mosi-gpios`.
