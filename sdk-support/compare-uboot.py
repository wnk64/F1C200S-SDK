#!/usr/bin/env python3
"""Compare immutable read-only SD reference bytes with historical build outputs."""
import hashlib
import json
from pathlib import Path
import struct
import datetime

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / ".sdk-output/provenance"
EXPECTED = "71e34730de102099163f014c528ca0656bb1ef5c81fc2756f6a32477cd3cac04"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def regions(left, right):
    result, start = [], None
    for offset, (a, b) in enumerate(zip(left, right)):
        if a != b and start is None:
            start = offset
        elif a == b and start is not None:
            result.append([start, offset])
            start = None
    if start is not None:
        result.append([start, min(len(left), len(right))])
    return result


def main():
    reference = (OUT / "live-uboot.bin").read_bytes()
    if len(reference) != 548864 or sha(reference) != EXPECTED or reference[4:12] != b"eGON.BT0":
        raise RuntimeError("Live SD reference hash/size/header mismatch")
    report = {"reference_sha256": sha(reference), "reference_spl_length": struct.unpack_from("<I", reference, 16)[0],
              "reference_spl_md5": hashlib.md5(reference[:32768]).hexdigest(), "candidates": {}}
    if struct.unpack_from(">I", reference, 32768)[0] != 0x27051956:
        raise RuntimeError("Unexpected main U-Boot legacy image header")
    epoch = struct.unpack_from(">I", reference, 32776)[0]
    report["legacy_header_timestamp_epoch"] = epoch
    report["legacy_header_timestamp_utc"] = datetime.datetime.utcfromtimestamp(epoch).isoformat() + "Z"
    for source in ("af73846", "c5335a4"):
        path = OUT / ("uboot-" + source) / "u-boot-sunxi-with-spl.bin"
        candidate = path.read_bytes()
        differences = regions(reference, candidate)
        report["candidates"][source] = {
            "sha256": sha(candidate), "bytes": len(candidate),
            "spl_sha256": sha(candidate[:32768]), "spl_matches": candidate[:32768] == reference[:32768],
            "main_matches": candidate[32768:] == reference[32768:],
            "main_payload_matches": candidate[32832:] == reference[32832:],
            "difference_regions": len(differences), "different_bytes": sum(end - start for start, end in differences),
            "first_regions": differences[:30], "last_regions": differences[-10:],
        }
    (OUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
