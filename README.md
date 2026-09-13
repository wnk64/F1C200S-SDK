# TK032F8004 external panel initialisation candidate

External Linux 5.7.1 module that sends only the supplied LG4573A three-wire
initialisation sequence. RGB timing and DRM binding are handled by the DTS
`simple-panel` node, not by this module.

The module does not modify the stable kernel tree. Build against the matching
kernel build tree only after release-lock preflight.

Required GPIO mappings: `cs-gpios = PA0`, `sclk-gpios = PA2`, and
`mosi-gpios = PA1`. `miso-gpios = PA3` is recorded for a future readback
protocol but is not used by this write-only initialisation driver. No reset or
backlight GPIO is required or driven.
