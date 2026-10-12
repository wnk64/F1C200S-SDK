#!/usr/bin/env python3
"""Read-only dynamic symbol-name audit, not a substitute for runtime tests."""
import argparse
import json
from pathlib import Path
import re
import tarfile
import tempfile


def parse_symbols(text):
    defined, required = set(), set()
    for line in text.splitlines():
        match = re.match(r"\s*\d+:\s+\S+\s+\d+\s+\S+\s+(\S+)\s+(\S+)\s+(\S+)\s+(\S+)", line)
        if not match:
            continue
        binding, visibility, index, symbol = match.groups()
        if index == "UND":
            if binding == "GLOBAL":
                required.add(symbol)
        elif binding in ("GLOBAL", "WEAK", "UNIQUE") and visibility in ("DEFAULT", "PROTECTED"):
            defined.add(symbol)
            if "@@" in symbol:
                defined.add(symbol.split("@@")[0])
                defined.add(symbol.replace("@@", "@"))
    return {"defined": defined, "required": required}


def dependency_closure(name, binaries, members, sdk):
    seen, pending = set(), [name]
    while pending:
        current = pending.pop()
        if current in seen:
            continue
        if current not in binaries:
            raise RuntimeError("Dynamic symbol provider was not audited: " + str(current))
        seen.add(current)
        binary = binaries[current]
        pending.extend(binary["needed"].values())
        if binary["interpreter"]:
            provider = sdk.resolve_archive_path(members, binary["interpreter"])
            if not provider:
                raise RuntimeError("Dynamic interpreter provider missing")
            pending.append(provider)
    return seen


def audit(archive_path, dependencies, readelf, sdk):
    if dependencies["issues"]:
        raise RuntimeError("Resolve rootfs dependency issues before auditing symbols")
    symbols = {}
    with tarfile.open(str(archive_path)) as archive, tempfile.TemporaryDirectory() as temporary:
        members = {sdk.archive_name(member.name): member for member in archive.getmembers()}
        elf = Path(temporary) / "elf"
        for name, binary in sorted(dependencies["binaries"].items()):
            with archive.extractfile(members[name]) as stream:
                data = stream.read()
            if not data.startswith(b"\x7fELF"):
                raise RuntimeError("Dependency report refers to a non-ELF file")
            elf.write_bytes(data)
            text = sdk.capture([readelf, "--dyn-syms", "--wide", elf])
            header = sdk.capture([readelf, "-h", elf])
            symbols[name] = parse_symbols(text)
            symbols[name]["executable"] = bool(binary["interpreter"]) or bool(re.search(r"Type:\s+EXEC", header))
    issues, plugins, checked = [], [], 0
    for name, spec in sorted(symbols.items()):
        providers = dependency_closure(name, dependencies["binaries"], members, sdk)
        available = set()
        for provider in providers:
            available.update(symbols[provider]["defined"])
        missing = sorted(spec["required"] - available)
        checked += len(spec["required"])
        if missing:
            item = {"file": name, "unresolved": missing, "providers": sorted(providers)}
            (issues if spec["executable"] else plugins).append(item)
    return {"rootfs_sha256": sdk.digest(archive_path), "elf_count": len(symbols),
            "nonweak_symbols_checked": checked, "executable_issues": issues,
            "shared_objects_needing_host_or_other_scope": plugins,
            "scope": "dynamic symbol/version names in NEEDED closure and interpreter; not relocation execution, dlopen host validation, calling ABI or hardware acceptance"}


def main():
    import sdk
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rootfs", type=Path, default=sdk.OUT / "images/rootfs-integrated/rootfs.tar")
    parser.add_argument("--readelf", default=str(sdk.OUT / "work/ROOTFS/buildroot-2018.02.11/output/host/bin/arm-buildroot-linux-gnueabi-readelf"))
    args = parser.parse_args()
    dependencies_path = args.rootfs.parent / "dependencies.json"
    build = json.loads((args.rootfs.parent / "build.json").read_text())
    if sdk.digest(args.rootfs) != build["rootfs_tar_sha256"] or sdk.digest(dependencies_path) != build["dependency_report_sha256"]:
        raise RuntimeError("Rootfs/dependency report does not match its build record")
    report = audit(args.rootfs, json.loads(dependencies_path.read_text()), args.readelf, sdk)
    print(json.dumps(report, indent=2), flush=True)
    return 1 if report["executable_issues"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
