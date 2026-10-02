# RTL8723DS firmware provenance

Source: https://github.com/armbian/firmware
Commit:2a9e1c19460401443267926181191d57e3ff175d
Paths:rtl_bt/rtl8723ds_fw.bin, rtl_bt/rtl8723ds_config.bin
Git blob IDs:4c323a3d92dcd21e0481492819c656b9d4996b90,
a08fa3a9f8f6cbab10ad4584a36703336168df07, respectively.
Sizes:48988 bytes firmware;47 bytes config.
SHA256 firmware:19c1bed7a9359b1a021c20f81b525bdebf5067f9021716991d94be9101f76e63.
SHA256 config:7d1e18df7654da8f6382520841bb535f2d1246fc8495e3ae652f7b23e820168d.

Files retained unchanged. Config signature0x8723ab55; UART entry offset0x0c,
baud code0x04928002=1500000 per Linux5.7.1 btrtl, flow-control bit enabled.
This is an upstream generic configuration, not a measured board calibration.
Firmware compatibility, pairing/audio/coexistence and UART integrity await hardware.

Realtek firmware license text obtained from linux-firmware mirror:
https://github.com/wkennington/linux-firmware
Commit:711d3297bac870af42088a467459a0634c1970ca
Path:LICENCE.rtlwifi_firmware.txt.
