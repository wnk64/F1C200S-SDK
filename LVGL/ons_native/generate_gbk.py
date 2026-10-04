"""Generate a compact, read-only GBK BMP map using Python's standard codec."""
import pathlib
import sys

values = []
for lead in range(0x81, 0xff):
    for trail in range(0x40, 0xff):
        if trail == 0x7f:
            continue
        try:
            text = bytes((lead, trail)).decode("gbk")
            code = ord(text) if len(text) == 1 else 0
        except UnicodeDecodeError:
            code = 0
        if code > 0xffff:
            raise ValueError("GBK map must contain BMP codepoints only")
        values.append(code)
assert len(values) == 126 * 190
rows = [", ".join("0x{:04x}".format(code) for code in values[i:i + 12]) for i in range(0, len(values), 12)]
pathlib.Path(sys.argv[1]).write_text(
    "static const unsigned short gbk_map[23940] = {\n" + ",\n".join(rows) + "\n};\n", encoding="ascii")
