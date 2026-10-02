#!/usr/bin/env python3
import hashlib
import struct
import sys
from pathlib import Path

base = Path(sys.argv[1])
files = {
    "rtlwifi/rtl8723bs_nic.bin": "a52b5a7be4841b4b2839eddf5122b3300a3610866abe4bb3c3c9e444c8ab7969",
    "rtl_bt/rtl8723bs_fw.bin": "20dd9e162d48099906579c5e1bd3c7b65a7e8cbf79954bba59ef0ce985e93827",
    "rtl_bt/rtl8723bs_config.bin": "e120988a772efa67cd3c55eee84b00d24aa7fbf0109f8d2adaecc3ec3ff6d280",
}
for name, digest in files.items():
    if hashlib.sha256((base / name).read_bytes()).hexdigest() != digest:
        raise SystemExit("Wrong firmware: " + name)
fw = (base / "rtl_bt/rtl8723bs_fw.bin").read_bytes()
if fw[:8] != b"Realtech":
    raise SystemExit("Invalid Bluetooth firmware (possible unresolved symlink)")
cfg = (base / "rtl_bt/rtl8723bs_config.bin").read_bytes()
magic, size = struct.unpack_from("<IH", cfg)
if magic != 0x8723AB55 or size + 6 != len(cfg):
    raise SystemExit("Invalid config header")
position = 6
uart = None
while position < len(cfg):
    if position + 3 > len(cfg):
        raise SystemExit("Truncated config entry")
    offset, length = struct.unpack_from("<HB", cfg, position)
    position += 3
    if position + length > len(cfg):
        raise SystemExit("Truncated config payload")
    if offset == 12:
        if uart is not None:
            raise SystemExit("Duplicate UART entry")
        uart = cfg[position:position + length]
    position += length
if uart is None or len(uart) < 13 or struct.unpack_from("<I", uart)[0] != 0x04928002 or not uart[12] & 4:
    raise SystemExit("Expected 1.5M UART + hardware flow control")
print("BS WLAN/BT firmware validated, 1.5Mbaud with hardware flow control")
