setenv bootargs console=tty0 console=ttyS0,115200 panic=5 rootwait root=/dev/mmcblk0p2 rw video=HDMI-A-1:640x480@60 cma=20M
load mmc 0:1 0x80C00000 suniv-f1c100s-licheepi-nano.dtb
load mmc 0:1 0x80008000 zImage
bootz 0x80008000 - 0x80C00000
