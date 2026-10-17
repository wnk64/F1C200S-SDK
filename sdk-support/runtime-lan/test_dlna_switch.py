#!/usr/bin/env python3
"""Opt-in live AVTransport replacement test; never downloads media locally."""
import argparse
import os
from pathlib import Path
import time
from urllib.request import Request, urlopen
import xml.etree.ElementTree as ET

SERVICE = "urn:schemas-upnp-org:service:AVTransport:1"
SOAP = "http://schemas.xmlsoap.org/soap/envelope/"


def action(address, name, **arguments):
    envelope = ET.Element("{%s}Envelope" % SOAP)
    body = ET.SubElement(envelope, "{%s}Body" % SOAP)
    command = ET.SubElement(body, "{%s}%s" % (SERVICE, name))
    ET.SubElement(command, "InstanceID").text = "0"
    for key, value in arguments.items():
        ET.SubElement(command, key).text = value
    request = Request(address, ET.tostring(envelope), {
        "Content-Type": "text/xml; charset=utf-8",
        "SOAPAction": '"%s#%s"' % (SERVICE, name),
    })
    with urlopen(request, timeout=12) as response:
        document = ET.fromstring(response.read(256 * 1024))
    return {item.tag.split("}")[-1]: item.text or "" for item in document.iter() if not list(item)}


def seconds(value):
    h, m, s = value.split(":")
    return int(h) * 3600 + int(m) * 60 + int(s)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--address", default="http://10.0.0.234:49494/upnp/control/rendertransport1")
    parser.add_argument("--uri-file", type=Path, required=True)
    parser.add_argument("--save-current", action="store_true")
    parser.add_argument("--cycles", type=int, default=3)
    args = parser.parse_args()
    if args.save_current:
        uri = action(args.address, "GetMediaInfo").get("CurrentURI", "")
        if not uri.startswith(("http://", "https://")):
            raise RuntimeError("no current network URI to test")
        descriptor = os.open(str(args.uri_file), os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
        with os.fdopen(descriptor, "w") as output:
            output.write(uri)
        print("Saved current URI privately; value not printed")
        return
    uri = args.uri_file.read_text().strip()
    try:
        for cycle in range(args.cycles):
            # Replace the URI directly while PLAYING, without sending Stop.
            action(args.address, "SetAVTransportURI", CurrentURI=uri, CurrentURIMetaData="")
            state = action(args.address, "GetTransportInfo")["CurrentTransportState"]
            if state != "STOPPED":
                raise RuntimeError("replacement left state=" + state)
            action(args.address, "Play", Speed="1")
            action(args.address, "Play", Speed="1")
            deadline = time.monotonic() + 40
            previous = None
            while time.monotonic() < deadline:
                info = action(args.address, "GetPositionInfo")
                position = seconds(info.get("RelTime", "0:00:00"))
                if previous is None:
                    previous = position
                if position >= previous + 3:
                    break
                time.sleep(0.5)
            else:
                raise RuntimeError("video position did not advance after Play")
            print("PASS switch cycle=%d state=STOPPED->PLAYING position=%ds" % (cycle + 1, position), flush=True)
        action(args.address, "Pause")
        if action(args.address, "GetTransportInfo")["CurrentTransportState"] != "PAUSED_PLAYBACK":
            raise RuntimeError("pause state mismatch")
        time.sleep(1)
        first = action(args.address, "GetPositionInfo")["RelTime"]
        time.sleep(1.5)
        if action(args.address, "GetPositionInfo")["RelTime"] != first:
            raise RuntimeError("paused position changed")
        action(args.address, "Play", Speed="1")
        print("PASS pause/frozen position/resume", flush=True)
    finally:
        action(args.address, "Stop")
    if action(args.address, "GetTransportInfo")["CurrentTransportState"] != "STOPPED":
        raise RuntimeError("Stop state mismatch")
    print("PASS stop; replacement used same network media, not different codecs")


if __name__ == "__main__":
    main()
