"""Reconnect the paired speaker without restarting the live Miracast session."""
import argparse
import json
import re
import subprocess
import time
from pathlib import Path

PLINK = r"D:\AI工具\putty\plink.exe"
HOSTKEY = "SHA256:I1li/9+0wRwOzVnASuXFl8VwY6uZTMTKaE13b84S/Dc"
R = "/tmp/manual_miracast_20260920"
DEVICE = "/org/bluez/hci0/dev_12_11_71_41_9C_4A"


def ssh(command):
    run = subprocess.run([PLINK, "-batch", "-ssh", "root@10.0.0.234", "-pw", "1",
                          "-hostkey", HOSTKEY, command], capture_output=True, timeout=25)
    if run.returncode:
        raise RuntimeError(run.stderr.decode(errors="replace"))
    return run.stdout.decode(errors="replace")


def snapshot():
    data = ssh("cat {r}/player.pid {r}/sink.pid /run/audio-route.state; "
               "tail -n 80 {r}/sink.log; tail -n 2 {r}/player-frame-gap.log; "
               "cat /proc/meminfo".format(r=R))
    lines = data.splitlines()
    player, sink = int(lines[0]), int(lines[1])
    audio = [line for line in lines if line.startswith("LPCM stats:")]
    video = [line for line in lines if "video commits:" in line]
    if not audio or not video:
        raise RuntimeError("No live audio/video statistics")
    ssh("kill -0 {} && kill -0 {}".format(player, sink))
    result = {"player": player, "sink": sink, "route": lines[2],
              "audio": dict((k, int(v)) for k, v in re.findall(r"(\w+)=(\d+)", audio[-1])),
              "video": dict((k, int(v)) for k, v in re.findall(r"(\w+)=(\d+)", video[-1]))}
    result["memory_kib"] = {k: int(v) for k, v in re.findall(
        r"^(MemAvailable|CmaFree|CmaTotal):\s+(\d+) kB", data, re.M)}
    return result


def wait_phase(route, before, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        current = snapshot()
        if (current["player"], current["sink"]) != (before["player"], before["sink"]):
            raise RuntimeError("Video/sink session changed unexpectedly")
        if (current["route"] == route and current["audio"]["written"] > before["audio"]["written"]
                and current["audio"].get("unavailable", 1) == 0
                and current["audio"].get("route_changes", 0) > before["audio"].get("route_changes", 0)
                and current["video"]["total"] > before["video"]["total"]):
            return current
        time.sleep(1)
    raise RuntimeError("Route/audio/video did not recover before deadline")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cycles", type=int, default=2)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    if args.cycles < 1:
        parser.error("positive cycle count required")
    records = [snapshot()]
    try:
        for cycle in range(args.cycles):
            before = snapshot()
            if before["route"] != "bluetooth 12:11:71:41:9C:4A":
                raise RuntimeError("Speaker is not the current default")
            ssh("dbus-send --system --print-reply --dest=org.bluez " + DEVICE + " org.bluez.Device1.Disconnect")
            headphone = wait_phase("headphone", before)
            records.append(headphone)
            print(json.dumps({"cycle": cycle + 1, "phase": "headphone", "snapshot": headphone}), flush=True)
            ssh("dbus-send --system --print-reply --dest=org.bluez " + DEVICE + " org.bluez.Device1.Connect")
            bluetooth = wait_phase("bluetooth 12:11:71:41:9C:4A", headphone)
            records.append(bluetooth)
            print(json.dumps({"cycle": cycle + 1, "phase": "bluetooth", "snapshot": bluetooth}), flush=True)
    finally:
        Path(args.output).write_text(json.dumps(records, indent=2), encoding="utf-8")
        if ssh("cat /run/audio-route.state").strip() != "bluetooth 12:11:71:41:9C:4A":
            ssh("dbus-send --system --print-reply --dest=org.bluez " + DEVICE + " org.bluez.Device1.Connect")


if __name__ == "__main__":
    main()
