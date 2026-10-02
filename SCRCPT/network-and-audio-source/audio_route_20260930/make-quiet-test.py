#!/usr/bin/env python3
"""Generate a short, ramped stereo test sound at -36 dBFS peak."""
import math
import struct
import sys
import wave

rate = 48000
duration = 4
with wave.open(sys.argv[1], "wb") as output:
    output.setnchannels(2)
    output.setsampwidth(2)
    output.setframerate(rate)
    data = bytearray()
    for index in range(rate * duration):
        time = index / rate
        phase = time % 2
        envelope = min(1, phase / 0.15, max(0, (1.5 - phase) / 0.15))
        frequency = 440 if time < 2 else 660
        value = int(32767 * 10 ** (-36 / 20) * envelope * math.sin(2 * math.pi * frequency * time))
        data.extend(struct.pack("<hh", value, value))
    output.writeframes(data)
