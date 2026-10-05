"""Generate native-type command/reader sources from a checksum-locked ONS base.

This is a build transformation, not an SDL implementation or a game interpreter.
Input copyrights and license comments remain intact; original files are untouched.
"""
import hashlib
import pathlib
import re
import sys

output = pathlib.Path(sys.argv[1])
sources = {}
for line in pathlib.Path("source-baseline.sha256").read_text(encoding="ascii").splitlines():
    digest, filename = line.split(None, 1)
    source = pathlib.Path(filename.strip())
    data = source.read_bytes()
    if hashlib.sha256(data).hexdigest() != digest:
        raise ValueError("Baseline checksum mismatch: " + str(source))
    sources[source.name] = data.decode("latin1")
output.mkdir(parents=True, exist_ok=True)

names = {
    "SDL_Rect": "ons_rect", "SDL_Surface": "ons_surface", "SDL_Color": "ons_color",
    "Uint8": "uint8_t", "Uint16": "uint16_t", "Uint32": "uint32_t",
    "Sint8": "int8_t", "Sint16": "int16_t", "Sint32": "int32_t",
    "SDL_FreeSurface": "ons_surface_free", "_SDL_pixels_h": "_ONS_pixels_h",
    "SDL_PIXELFORMAT_RGB565": "ONS_PIXELFORMAT_RGB565",
    "SDL_PIXELFORMAT_ABGR8888": "ONS_PIXELFORMAT_ABGR8888",
    "SDL_PIXELFORMAT_ARGB8888": "ONS_PIXELFORMAT_ARGB8888",
}
pattern = re.compile(r"\b(" + "|".join(re.escape(name) for name in names) + r")\b")
for name, text in sources.items():
    if name == "FontInfo.cpp":
        constructor = text.index("FontInfo::FontInfo()")
        opening = text.index("void *FontInfo::openFont(")
        geometry = text.index("void FontInfo::setTateyokoMode(")
        license_end = text.index('#include "FontInfo.h"')
        text = text[:license_end] + '#include "FontInfo.h"\n' + text[constructor:opening] + text[geometry:]
    elif name == "AnimationInfo.cpp":
        text = text[:text.index("void AnimationInfo::stepAnimation(")]
    text = text.replace("#include <SDL.h>", '#include "engine_types.h"')
    text = pattern.sub(lambda match: names[match.group(0)], text)
    if "#include <SDL" in text or re.search(r"\bSDL_[A-Za-z0-9_]+", text):
        raise ValueError("Unported SDL dependency in generated source: " + name)
    (output / name).write_bytes(text.encode("latin1"))
(output / "generated.stamp").write_text("checksum-verified native command sources\n", encoding="ascii")
