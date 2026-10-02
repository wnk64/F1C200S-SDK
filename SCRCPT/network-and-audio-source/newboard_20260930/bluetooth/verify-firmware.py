#!/usr/bin/env python3
"""Validate pinned firmware and the Realtek UART configuration without editing it."""
import hashlib
import struct
import sys
from pathlib import Path

folder = Path(sys.argv[1])
expected = {
    "rtl8723ds_fw.bin": "19c1bed7a9359b1a021c20f81b525bdebf5067f9021716991d94be9101f76e63",
    "rtl8723ds_config.bin": "7d1e18df7654da8f6382520841bb535f2d1246fc8495e3ae652f7b23e820168d",
}
for name, digest in expected.items():
    if hashlib.sha256((folder / name).read_bytes()).hexdigest() != digest:
        raise SystemExit("Firmware checksum mismatch: " + name)

config = (folder / "rtl8723ds_config.bin").read_bytes()
magic, length = struct.unpack_from("<IH", config)
if magic != 0x8723AB55 or length + 6 != len(config):
    raise SystemExit("Invalid Realtek configuration header")
position = 6
uart = None
while position < len(config):
    if position + 3 > len(config):
        raise SystemExit("Truncated configuration entry")
    offset, size = struct.unpack_from("<HB", config, position)
    position += 3
    if position + size > len(config):
        raise SystemExit("Truncated configuration data")
    if offset == 0x0C:
        if uart is not None:
            raise SystemExit("Duplicate UART configuration")
        uart = config[position:position + size]
    position += size
if uart is None or len(uart) < 13:
    raise SystemExit("Missing UART baud/flow-control configuration")
baud_code = struct.unpack_from("<I", uart)[0]
if baud_code != 0x04928002 or not uart[12] & 4:
    raise SystemExit("Expected 1500000 baud and hardware flow control")
print("Pinned firmware/config valid: 1500000 baud, hardware flow control enabled")
