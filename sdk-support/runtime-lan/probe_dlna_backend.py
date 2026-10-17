#!/usr/bin/env python3
"""Opt-in decoder-only probe; requires an otherwise idle board display."""
import argparse
from pathlib import Path
import shlex
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--uri-file", type=Path, required=True)
parser.add_argument("--audio", action="store_true")
args = parser.parse_args()
ssh = [r"D:\AI工具\putty\plink.exe", "-batch", "-hostkey",
       "SHA256:uKw6EdGW7mdHRgfPwSwASOAjkAGwemfB9iX8Jo/gBxo", "-pw", "1", "root@10.0.0.234"]
player = ["/opt/f1/apps/media/mplayer-native", "-noconfig", "all", "-nocache",
          "-frames", "120", "-vc", "cedarh264", "-vo", "cedar_drm:native"]
player += ["-afm", "faad", "-ao", "alsa"] if args.audio else ["-nosound"]
player += [args.uri_file.read_text().strip()]
command = "echo PROBE_PID=$$; exec env CEDAR_ROTATE=90 CEDAR_CMA_POOL_MB=12 CEDAR_DEBUG=1 "
command += " ".join(shlex.quote(value) for value in player)
command += " > /tmp/f1-dlna-decoder-probe.log 2>&1"
process = subprocess.Popen(ssh + [command], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
identity = process.stdout.readline().decode().strip()
if not identity.startswith("PROBE_PID=") or not identity[10:].isdigit():
    process.kill()
    raise RuntimeError("missing remote probe identity")
pid = int(identity[10:])
try:
    process.wait(timeout=25)
    print("decoder-only audio=%s exit=%d" % (args.audio, process.returncode))
except subprocess.TimeoutExpired:
    print("decoder-only audio=%s timed out" % args.audio)
finally:
    cleanup = 'if test "$(cat /proc/%d/comm 2>/dev/null)" = mplayer-native; then kill -TERM %d; fi' % (pid, pid)
    subprocess.run(ssh + [cleanup], check=True, timeout=10)
    try:
        process.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        subprocess.run(ssh + ["kill -KILL %d" % pid], check=True, timeout=10)
        process.communicate(timeout=5)
print("Remote diagnostic: /tmp/f1-dlna-decoder-probe.log; may contain private URI")
