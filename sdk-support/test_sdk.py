#!/usr/bin/env python3
import importlib.util
import io
import json
import os
import sys
from pathlib import Path
import subprocess
import tarfile
import tempfile
import unittest
from unittest import mock

package_spec = importlib.util.spec_from_file_location("package_sdk", str(Path(__file__).with_name("package_sdk.py")))
package = importlib.util.module_from_spec(package_spec)
package_spec.loader.exec_module(package)
sys.modules["package_sdk"] = package
archive_spec = importlib.util.spec_from_file_location("archive_sdk", str(Path(__file__).with_name("archive_sdk.py")))
archive_tool = importlib.util.module_from_spec(archive_spec)
archive_spec.loader.exec_module(archive_tool)
symbols_spec = importlib.util.spec_from_file_location("audit_symbols", str(Path(__file__).with_name("audit_symbols.py")))
symbols_tool = importlib.util.module_from_spec(symbols_spec)
symbols_spec.loader.exec_module(symbols_tool)

spec = importlib.util.spec_from_file_location("f1sdk", str(Path(__file__).with_name("sdk.py")))
sdk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sdk)


class OutputSafetyTests(unittest.TestCase):
    def test_uniform_app_runner_reaps_escaped_children_and_private_runtime(self):
        support = Path(__file__).with_name("runtime-menu")
        runner, fixture = self.root / "runner", self.root / "fixture"
        for source, output in (("f1_app_runner.c", runner), ("test_fixture.c", fixture)):
            subprocess.run(["cc", "-Os", "-Wall", "-Wextra", "-Werror", str(support / source),
                            "-o", str(output)], check=True)
        protected = self.root / "protected"
        protected.write_text("keep")
        unrelated = subprocess.Popen(["sleep", "15"])
        try:
            for mode, expected in (("normal", 0), ("crash", 137), ("wait", 143)):
                record = self.root / (mode + ".txt")
                process = subprocess.Popen([str(runner), str(fixture), mode, str(record)])
                if mode == "wait":
                    import time
                    deadline = time.monotonic() + 3
                    while not record.exists() or len(record.read_text().splitlines()) < 3:
                        if time.monotonic() > deadline: self.fail("fixture startup timeout")
                        time.sleep(0.01)
                    process.terminate()
                self.assertEqual(process.wait(timeout=6), expected)
                lines = record.read_text().splitlines()
                self.assertFalse(Path(lines[0]).exists())
                for pid in lines[1:]: self.assertFalse(Path("/proc/" + pid).exists())
                self.assertIsNone(unrelated.poll())
                self.assertEqual(protected.read_text(), "keep")
            self.assertEqual(subprocess.run([str(runner), "/no-such-f1-app"], timeout=6).returncode, 127)
        finally:
            unrelated.terminate(); unrelated.wait()
        for name in ("menu-supervisor.sh", "app-release.sh"):
            subprocess.run(["sh", "-n", str(support / name)], check=True)

    def test_uniform_exit_hook_and_wiliwili_runner_parent_contract(self):
        layout = json.loads(Path(__file__).with_name("runtime-layout.json").read_text())
        supervisor = next(e for e in layout["sources"] if "opt/f1/apps/menu/menu-supervisor.sh" in e["targets"])
        self.assertEqual(supervisor["origin"], "sdk")
        wili = next(e for e in layout["sources"] if "opt/f1/apps/wiliwili/launch-net-with-menu.sh" in e["targets"])
        self.assertIn("f1-app-runner", wili["replacements"]["grep -q 'menu-supervisor.sh'"])
        self.assertEqual(layout["artifacts"]["runtime-tools"]["f1-app-runner"], "opt/f1/apps/menu/f1-app-runner")

    def test_cast_entry_reinitializes_when_idle_radio_was_released(self):
        layout = json.loads(Path(__file__).with_name("runtime-layout.json").read_text())
        entry = next(item for item in layout["sources"] if "opt/f1/apps/cast/cast-app.sh" in item["targets"])
        old = 'mode=cold; [ ! -d "$R" ] || mode=session'
        self.assertEqual(entry["replacements"][old],
                         'mode=cold; [ ! -d "$R" ] || [ ! -e /sys/class/net/wlan1 ] || mode=session')

    def test_dlna_lifecycle_overlay_is_idempotent_and_refuses_unknown_source(self):
        support = self.root / "sdk-support/runtime-lan"
        support.mkdir(parents=True)
        original = Path(__file__).with_name("runtime-lan")
        for name in ("output_cedar.c", "dlna_process.h"):
            (support / name).write_bytes((original / name).read_bytes())
        runtime = support / "output_cedar.c"
        source = self.root / "output_cedar.c"
        source.write_bytes(runtime.read_bytes())
        sdk.prepare_lan_runtime(source)
        self.assertEqual(source.read_bytes(), runtime.read_bytes())
        self.assertTrue(source.with_name("dlna_process.h").is_file())
        source.write_text("unrecognized user changes")
        with self.assertRaises(RuntimeError):
            sdk.prepare_lan_runtime(source)
        self.assertEqual(source.read_text(), "unrecognized user changes")

    def test_dlna_process_group_cleans_cache_after_parent_is_killed(self):
        support = Path(__file__).with_name("runtime-lan")
        executable = self.root / "dlna-process-test"
        subprocess.run(["cc", "-Wall", "-Wextra", "-I" + str(support),
                        str(support / "test_process.c"), "-o", str(executable)], check=True)
        subprocess.run([str(executable)], check=True, timeout=5)
        for name in ("dlna-app.sh", "release-idle-cast.sh"):
            subprocess.run(["sh", "-n", str(support / name)], check=True)

    def test_dlna_icons_match_configured_package_data_subdirectory(self):
        layout = json.loads(Path(__file__).with_name("runtime-layout.json").read_text())
        for size in (64, 128):
            name = "grender-{}x{}.png".format(size, size)
            entries = [entry for entry in layout["sources"] if entry["path"].endswith("/" + name)]
            self.assertEqual(len(entries), 1)
            self.assertEqual(entries[0]["targets"], ["opt/f1/apps/lan/data/gmediarender/" + name])

    def test_web_frontend_is_installed_beside_its_launcher(self):
        layout = json.loads(Path(__file__).with_name("runtime-layout.json").read_text())
        entries = [entry for entry in layout["sources"]
                   if "opt/f1/apps/lan/index.html" in entry["targets"]]
        self.assertEqual(len(entries), 1)
        self.assertEqual(entries[0]["origin"], "project")
        self.assertEqual(entries[0]["path"], "SOFTWARE/lan-apps/index.html")
        self.assertFalse(entries[0].get("executable", False))

    def test_dynamic_symbols_match_default_versions_and_ignore_weak_undefined(self):
        parsed = symbols_tool.parse_symbols("""
  1: 00000000 0 FUNC GLOBAL DEFAULT UND foo@GLIBC_2.4 (2)
  2: 00000000 0 NOTYPE WEAK DEFAULT UND optional
  3: 00001234 8 FUNC GLOBAL DEFAULT 12 foo@@GLIBC_2.4
  4: 00001238 8 FUNC GLOBAL HIDDEN 12 hidden
  5: 00001240 8 OBJECT UNIQUE DEFAULT 12 unique
""")
        self.assertEqual(parsed["required"], {"foo@GLIBC_2.4"})
        self.assertEqual(parsed["defined"], {"foo", "foo@GLIBC_2.4", "foo@@GLIBC_2.4", "unique"})

    def test_symbol_dependency_closure_includes_interpreter_and_transitive_libraries(self):
        binaries = {"app": {"needed": {"lib.so": "lib.so"}, "interpreter": "/ld.so"},
                    "lib.so": {"needed": {"support.so": "support.so"}, "interpreter": None},
                    "support.so": {"needed": {}, "interpreter": None},
                    "ld.so": {"needed": {}, "interpreter": None}}
        members = {"ld.so": tarfile.TarInfo("ld.so")}
        self.assertEqual(symbols_tool.dependency_closure("app", binaries, members, sdk), set(binaries))
        del binaries["support.so"]
        with self.assertRaises(RuntimeError):
            symbols_tool.dependency_closure("app", binaries, members, sdk)

    def test_symbol_audit_detects_missing_strong_versioned_reference(self):
        archive_path = self.root / "rootfs.tar"
        with tarfile.open(str(archive_path), "w") as archive:
            for name in ("app", "lib.so"):
                data = b"\x7fELF" + name.encode("ascii")
                item = tarfile.TarInfo(name)
                item.size = len(data)
                archive.addfile(item, io.BytesIO(data))
        dependencies = {"issues": [], "binaries": {
            "app": {"needed": {"lib.so": "lib.so"}, "interpreter": None},
            "lib.so": {"needed": {}, "interpreter": None}}}
        def readelf(args):
            app = Path(args[-1]).read_bytes().endswith(b"app")
            if "-h" in args:
                return "Type: EXEC" if app else "Type: DYN"
            return "1: 00000000 0 FUNC GLOBAL DEFAULT UND foo@VERSION_2" if app else "1: 00000001 1 FUNC GLOBAL DEFAULT 1 foo@@VERSION_1"
        with mock.patch.object(sdk, "capture", side_effect=readelf):
            report = symbols_tool.audit(archive_path, dependencies, "readelf", sdk)
        self.assertEqual(report["executable_issues"][0]["unresolved"], ["foo@VERSION_2"])
        self.assertEqual(report["nonweak_symbols_checked"], 1)
        with mock.patch.object(sdk, "capture", side_effect=lambda args: "Type: DYN" if "-h" in args else readelf(args)):
            plugin_report = symbols_tool.audit(archive_path, dependencies, "readelf", sdk)
        self.assertEqual(plugin_report["executable_issues"], [])
        self.assertEqual(plugin_report["shared_objects_needing_host_or_other_scope"][0]["unresolved"], ["foo@VERSION_2"])

    def test_source_archive_roundtrip_preserves_files_modes_and_links(self):
        source = self.root / "source"
        source.mkdir()
        (source / "sdk").write_bytes(b"#!/bin/sh\n")
        (source / "sdk").chmod(0o755)
        (source / "alias").symlink_to("sdk")
        output = self.root / "sdk.tar.gz"
        report = archive_tool.archive_tree(source, output, sdk)
        self.assertEqual(report["files_verified"], 2)
        self.assertEqual(report["sha256"], sdk.digest(output))
        with tarfile.open(str(output)) as archive:
            self.assertEqual(archive.extractfile("F1c200S_SOFT/sdk").read(), b"#!/bin/sh\n")
            self.assertEqual(archive.getmember("F1c200S_SOFT/alias").linkname, "sdk")

    def test_source_archive_refuses_unclean_sdk_and_existing_archive(self):
        source = self.root / "source"
        source.mkdir()
        output = self.root / "sdk.tar.gz"
        (source / ".sdk-output").mkdir()
        with self.assertRaises(RuntimeError):
            archive_tool.archive_tree(source, output, sdk)
        (source / ".sdk-output").rmdir()
        output.write_bytes(b"keep")
        with self.assertRaises(RuntimeError):
            archive_tool.archive_tree(source, output, sdk)
        self.assertEqual(output.read_bytes(), b"keep")

    def test_package_refuses_existing_export_without_modifying_it(self):
        sdk.check_output(create=True)
        destination = sdk.OUT / "portable-sdk"
        destination.mkdir()
        (destination / "keep").write_bytes(b"keep")
        with mock.patch.object(sdk, "doctor", return_value=0):
            with self.assertRaises(RuntimeError):
                package.build_package(sdk)
        self.assertEqual((destination / "keep").read_bytes(), b"keep")

    def test_export_repository_pins_required_commits_before_removing_remote(self):
        calls = []
        with mock.patch.object(sdk, "run", side_effect=lambda args, *unused: calls.append(args)), mock.patch.object(sdk, "capture", return_value=""):
            package.export_repository(self.root / "original", self.root / "export", ["commit-a", "commit-b"], "commit-a", sdk)
        removal = next(index for index, args in enumerate(calls) if args[:3] == ["git", "remote", "remove"])
        for commit in ("commit-a", "commit-b"):
            self.assertLess(calls.index(["git", "update-ref", "refs/sdk-lock/" + commit, commit]), removal)

    def test_export_stub_cleanup_refuses_untracked_or_git_directory(self):
        root = self.root / "export"
        component = root / "component"
        component.mkdir(parents=True)
        (component / "tracked.c").write_bytes(b"source")
        with mock.patch.object(sdk, "capture", return_value=""):
            with self.assertRaises(RuntimeError):
                package.remove_export_stub(root, component, sdk)
        self.assertTrue(component.exists())
        (component / ".git").mkdir()
        with self.assertRaises(RuntimeError):
            package.remove_export_stub(root, component, sdk)
        (component / ".git").rmdir()
        with mock.patch.object(sdk, "capture", return_value="component/tracked.c"):
            package.remove_export_stub(root, component, sdk)
        self.assertFalse(component.exists())

    def test_dependency_copy_verifies_content_links_and_modes(self):
        source = self.root / "dependencies"
        source.mkdir()
        (source / "compiler").write_bytes(b"compiler")
        (source / "compiler").chmod(0o755)
        (source / "alias").symlink_to("compiler")
        destination = self.root / "copied"
        inventory = package.copy_tree(source, destination, sdk)
        self.assertEqual(inventory["alias"], {"symlink": "compiler"})
        self.assertEqual(inventory["compiler"]["mode"], 0o755)
        self.assertEqual((destination / "compiler").read_bytes(), b"compiler")
        with self.assertRaises(RuntimeError):
            package.copy_tree(source, destination, sdk)

    def test_dependency_copy_rejects_external_and_broken_links(self):
        source = self.root / "dependencies"
        source.mkdir()
        link = source / "escape"
        link.symlink_to("../outside")
        with self.assertRaises(RuntimeError):
            package.tree_inventory(source, sdk)
        link.unlink()
        link.symlink_to("missing")
        with self.assertRaises(RuntimeError):
            package.tree_inventory(source, sdk)

    def test_baseline_copy_refuses_different_tracked_file(self):
        source = self.root / "baseline"
        target = self.root / "target"
        source.write_bytes(b"baseline")
        target.write_bytes(b"different")
        with self.assertRaises(RuntimeError):
            package.copy_baseline(source, target, sdk)
        self.assertEqual(target.read_bytes(), b"different")

    def test_elf_version_parser_separates_definitions_parents_and_needs(self):
        text = """Version definition section '.gnu.version_d' contains 2 entries:
  0x001c: Rev: 1 Flags: none Index: 2 Cnt: 2 Name: GLIBC_2.4
  0x0038: Parent 1: GLIBC_2.3
Version needs section '.gnu.version_r' contains 1 entry:
  0x0000: Version: 1 File: libc.so.6 Cnt: 2
  0x0010: Name: GLIBC_2.4 Flags: none Version: 2
  0x0020: Name: OPTIONAL_1 Flags: WEAK Version: 3
"""
        result = sdk.elf_versions(text)
        self.assertEqual(result["definitions"], ["GLIBC_2.4"])
        self.assertEqual(result["needs"]["libc.so.6"], [
            {"name": "GLIBC_2.4", "weak": False}, {"name": "OPTIONAL_1", "weak": True}])
        self.assertEqual(sdk.elf_versions("No version information found in this file."), {"definitions": [], "needs": {}})

    def test_elf_version_audit_requires_actual_provider_definition(self):
        binaries = {"app": {"needed": {"libc.so.6": "lib/libc.so"}, "versions": {
            "needs": {"libc.so.6": [{"name": "GLIBC_2.4", "weak": False}]}, "definitions": []}},
            "lib/libc.so": {"needed": {}, "versions": {"needs": {}, "definitions": ["GLIBC_2.4"]}}}
        self.assertEqual(sdk.audit_elf_versions(binaries), (1, []))
        binaries["lib/libc.so"]["versions"]["definitions"] = ["GLIBC_2.3"]
        checked, issues = sdk.audit_elf_versions(binaries)
        self.assertEqual(checked, 1)
        self.assertEqual(issues[0]["error"], "missing ELF version definition")
        binaries["app"]["versions"]["needs"]["libc.so.6"][0]["weak"] = True
        self.assertEqual(sdk.audit_elf_versions(binaries), (1, []))
        del binaries["lib/libc.so"]
        self.assertEqual(sdk.audit_elf_versions(binaries)[1][0]["error"], "missing ELF version provider")

    def test_full_build_order_covers_all_components_and_image_last(self):
        names = [name for name, operation in sdk.full_build_steps(2, image=True)]
        self.assertEqual(len(names), len(set(names)))
        self.assertEqual(names[0], "rootfs-base")
        self.assertEqual(names[-3:], ["runtime-payload", "rootfs-integrated", "sd-image"])
        for first, second in (("kernel", "wireless-modules"), ("wireless-modules", "kernel-runtime"),
                              ("apps-ui", "lan"), ("media-backends", "runtime-payload"),
                              ("bluetooth-audio", "runtime-payload"), ("ons", "runtime-payload"),
                              ("retro-cores", "runtime-payload"), ("image-tools", "sd-image")):
            self.assertLess(names.index(first), names.index(second))

    def test_default_build_excludes_all_packaging_steps(self):
        names = [name for name, operation in sdk.full_build_steps(8)]
        self.assertIn("runtime-tools", names)
        for name in ("image-tools", "runtime-payload", "rootfs-integrated", "sd-image"):
            self.assertNotIn(name, names)

    def test_incremental_reuses_outputs_and_rebuilds_changed_input_or_output(self):
        sdk.check_output(create=True)
        directory = sdk.OUT / "images/runtime-tools"
        directory.mkdir(parents=True)
        report = directory / "build.json"
        called = []
        def operation():
            called.append(True)
            report.write_text("built")
        with mock.patch.object(sdk, "incremental_inputs", return_value={"source": "a"}):
            sdk.incremental_build("runtime-tools", operation)
            sdk.incremental_build("runtime-tools", operation)
            self.assertEqual(len(called), 1)
            report.unlink()
            sdk.incremental_build("runtime-tools", operation)
            self.assertEqual(len(called), 2)
        with mock.patch.object(sdk, "incremental_inputs", return_value={"source": "b"}):
            sdk.incremental_build("runtime-tools", operation)
            self.assertEqual(len(called), 3)

    def test_failed_incremental_build_does_not_leave_success_stamp(self):
        with mock.patch.object(sdk, "incremental_inputs", return_value={}):
            with self.assertRaises(RuntimeError):
                sdk.incremental_build("runtime-tools", lambda: (_ for _ in ()).throw(RuntimeError("failed")))
        self.assertFalse((sdk.OUT / "stamps/incremental/runtime-tools.json").exists())

    def test_packaging_requires_explicit_image_flag(self):
        for component in ("sd-image", "rootfs-integrated", "runtime-payload", "image-tools"):
            with mock.patch.object(sys, "argv", ["sdk", "build", component]), mock.patch.object(sdk, "locked") as lock:
                with self.assertRaises(SystemExit) as result:
                    sdk.main()
                self.assertEqual(result.exception.code, 2)
                lock.assert_not_called()

    def test_build_cli_passes_image_flag_and_default_jobs(self):
        for extra, expected in (([], False), (["--image"], True)):
            with mock.patch.object(sys, "argv", ["sdk", "build"] + extra), mock.patch.object(sdk, "locked"), mock.patch.object(sdk, "build_all") as build:
                self.assertEqual(sdk.main(), 0)
                build.assert_called_once_with(8, image=expected)

    def test_full_build_stops_on_error_and_records_failed_stage(self):
        called = []
        def failure():
            raise RuntimeError("intentional build failure")
        steps = [("first", lambda: called.append("first")), ("failed", failure),
                 ("must-not-run", lambda: called.append("must-not-run"))]
        with mock.patch.object(sdk, "doctor", return_value=0), mock.patch.object(sdk, "digest", return_value="hash"), mock.patch.object(sdk, "full_build_steps", return_value=steps), mock.patch.object(sdk, "incremental_build", side_effect=lambda name, operation: operation()):
            with self.assertRaises(RuntimeError):
                sdk.build_all(2)
        self.assertEqual(called, ["first"])
        report = json.loads((sdk.OUT / "images/full-build/build.json").read_text())
        self.assertFalse(report["build_complete"])
        self.assertFalse(report["hardware_accepted"])
        self.assertEqual(report["current_step"], "failed")
        self.assertEqual(report["completed_steps"], ["first"])

    def test_sd_mbr_preserves_verified_layout_and_boot_gap(self):
        partitions = sdk.sd_partition_table()
        result = sdk.sd_mbr(partitions, 2097152, 548864)
        self.assertEqual(result[510:512], b"\x55\xaa")
        for index, start in enumerate((2048, 34816, 1048576)):
            self.assertEqual(sdk.struct.unpack_from("<I", result, 446 + index * 16 + 8)[0], start)
        with self.assertRaises(RuntimeError):
            sdk.sd_mbr(partitions, 2097152, 2 * 1024 * 1024)
        partitions[1]["start"] = 2048
        with self.assertRaises(RuntimeError):
            sdk.sd_mbr(partitions, 2097152, 548864)

    def test_boot_module_plan_is_read_only_and_dependency_ordered(self):
        script = Path(__file__).parent / "runtime-cast/S12board-hardware"
        result = subprocess.check_output(["/bin/sh", str(script), "--list-modules"]).decode("ascii")
        names = [line.split("|", 1)[0] for line in result.splitlines()]
        self.assertEqual(names, ["phy_generic", "sunxi", "sunxi_ion_core", "cedar_ve"])
        self.assertNotIn("aic", result)
        subprocess.run(["/bin/sh", "-n", str(script)], check=True)
        data_script = Path(__file__).parent / "runtime-cast/S19data"
        subprocess.run(["/bin/sh", "-n", str(data_script)], check=True)
        self.assertNotIn("mkfs", data_script.read_text())
        self.assertNotIn("fdisk", data_script.read_text())

    def test_cast_touch_relocation_requires_exact_identity(self):
        source = self.root / "touch.c"
        source.write_text('return strcmp(exe, "/root/aic_miracast/candidates/audio_reopen_20261002/sink-audio-reopen");')
        sdk.relocate_cast_touch(source)
        self.assertIn('"/opt/f1/apps/cast/miracast_sink_dump.lowest"', source.read_text())
        sdk.relocate_cast_touch(source)
        source.write_text("unexpected")
        with self.assertRaises(RuntimeError):
            sdk.relocate_cast_touch(source)

    def test_cast_checksum_audit_rejects_tampered_target(self):
        payload = self.root / "payload"
        directory = payload / "opt/f1/apps/cast"
        directory.mkdir(parents=True)
        content = b"new executable"
        expected = sdk.hashlib.md5(content).hexdigest()
        lines = []
        for index in range(13):
            name = "tool{}".format(index)
            (directory / name).write_bytes(content)
            lines.append('check "$SELF/{}" {}'.format(name, expected))
        (directory / "start-cast-default-audio.sh").write_text("\n".join(lines))
        loader = payload / "root/aic_miracast/load-aic-for-miracast.sh"
        loader.parent.mkdir(parents=True)
        loader.write_text("#!/bin/sh\n")
        self.assertEqual(len(sdk.audit_cast_checks(payload)), 13)
        (directory / "tool0").write_bytes(b"tampered")
        with self.assertRaises(RuntimeError):
            sdk.audit_cast_checks(payload)

    def test_runtime_relocation_handles_nested_paths_longest_first(self):
        content = b'LOADER=$BASE/old/loader.ko\nFW=$BASE/old\n'
        result = sdk.relocate_runtime_text(content, {"$BASE/old": "/firmware", "$BASE/old/loader.ko": "/modules/loader.ko"})
        self.assertEqual(result, b'LOADER=/modules/loader.ko\nFW=/firmware\n')

    def test_runtime_checksums_use_current_preceding_installed_content(self):
        source_root = self.root / "source"
        source_root.mkdir()
        old_hash = "a" * 32
        (source_root / "app.sh").write_text("#!/bin/sh\ncheck /opt/tool " + old_hash + "\n")
        spec = {"origin": "project", "path": "app.sh", "targets": ["opt/app.sh"],
                "checks": {old_hash: "opt/tool"}}
        entries = sdk.runtime_source_plan({"sources": [spec]}, sdk.OUT / "runtime-payload", {"project": source_root}, set(), {"opt/tool": b"new executable"})
        self.assertIn(sdk.hashlib.md5(b"new executable").hexdigest().encode("ascii"), entries[0][2])
        self.assertNotIn(old_hash.encode("ascii"), entries[0][2])
        with self.assertRaises(RuntimeError):
            sdk.runtime_source_plan({"sources": [spec]}, sdk.OUT / "runtime-payload", {"project": source_root}, set())

    def test_rootfs_dependency_audit_reports_missing_library(self):
        archive_path = self.root / "rootfs.tar"
        with tarfile.open(str(archive_path), "w") as archive:
            member = tarfile.TarInfo("usr/bin/app")
            data = b"\x7fELFtest"
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        def readelf(args):
            if "-h" in args:
                return "Machine: ARM\n"
            if "-d" in args:
                return "(NEEDED) Shared library: [missing.so]"
            return ""
        with mock.patch.object(sdk, "capture", side_effect=readelf):
            report = sdk.audit_rootfs_dependencies(archive_path, "readelf")
        self.assertEqual(report["elf_count"], 1)
        self.assertEqual(report["issues"], [{"file": "usr/bin/app", "error": "missing shared library", "dependency": "missing.so"}])

    def test_rootfs_link_resolution_handles_absolute_relative_and_loops(self):
        library = tarfile.TarInfo("lib/libc-real.so")
        absolute = tarfile.TarInfo("usr/lib/libc.so")
        absolute.type = tarfile.SYMTYPE
        absolute.linkname = "/lib/libc-real.so"
        relative = tarfile.TarInfo("lib/libc.so")
        relative.type = tarfile.SYMTYPE
        relative.linkname = "libc-real.so"
        members = {item.name: item for item in (library, absolute, relative)}
        self.assertEqual(sdk.resolve_archive_path(members, "usr/lib/libc.so"), "lib/libc-real.so")
        self.assertEqual(sdk.resolve_archive_path(members, "lib/libc.so"), "lib/libc-real.so")
        self.assertIsNone(sdk.resolve_archive_path(members, "lib/missing.so"))
        relative.linkname = "libc.so"
        with self.assertRaises(RuntimeError):
            sdk.resolve_archive_path(members, "lib/libc.so")

    def test_rootfs_merge_preserves_special_nodes_and_installs_root_files(self):
        base = self.root / "base.tar"
        payload = self.root / "payload"
        payload.mkdir()
        (payload / "app").write_bytes(b"app")
        with tarfile.open(str(base), "w") as archive:
            directory = tarfile.TarInfo("dev")
            directory.type = tarfile.DIRTYPE
            archive.addfile(directory)
            device = tarfile.TarInfo("dev/console")
            device.type = tarfile.CHRTYPE
            device.devmajor, device.devminor = 5, 1
            archive.addfile(device)
            link = tarfile.TarInfo("bin/sh")
            link.type = tarfile.SYMTYPE
            link.linkname = "busybox"
            archive.addfile(link)
            service = tarfile.TarInfo("etc/init.d/S41dhcpcd")
            content = b"DAEMON=/sbin/dhcpcd\n"
            service.size = len(content)
            archive.addfile(service, io.BytesIO(content))
        output = self.root / "merged.tar"
        excluded = sdk.merge_rootfs_archive(base, payload, {"app": sdk.digest(payload / "app")}, output)
        self.assertEqual(excluded, ["etc/init.d/S41dhcpcd"])
        with tarfile.open(str(output)) as archive:
            self.assertTrue(archive.getmember("dev/console").ischr())
            self.assertEqual(archive.getmember("dev/console").devminor, 1)
            self.assertEqual(archive.getmember("bin/sh").linkname, "busybox")
            self.assertEqual(archive.getmember("app").uid, 0)
            self.assertNotIn("etc/init.d/S41dhcpcd", archive.getnames())

    def test_rootfs_merge_rejects_base_symlink_parent(self):
        base = self.root / "base.tar"
        payload = self.root / "payload"
        (payload / "opt").mkdir(parents=True)
        (payload / "opt/app").write_bytes(b"app")
        with tarfile.open(str(base), "w") as archive:
            link = tarfile.TarInfo("opt")
            link.type = tarfile.SYMTYPE
            link.linkname = "/tmp"
            archive.addfile(link)
        with self.assertRaises(RuntimeError):
            sdk.merge_rootfs_archive(base, payload, {"opt/app": sdk.digest(payload / "opt/app")}, self.root / "merged.tar")

    def test_cast_dhcp_adapter_only_emits_valid_ack_addresses(self):
        script = Path(__file__).parent / "runtime-cast/cast-dhcp.sh"
        logs = (b"udhcpd: sending OFFER of 192.168.49.52\n"
                b"udhcpd: sending ACK to 192.168.49.52\n"
                b"udhcpd: sending ACK to 10.0.0.2\n"
                b"udhcpd: sending ACK to 192.168.49.999\n"
                b"udhcpd: sending ACK to 192.168.49.1\n"
                b"udhcpd: error sending ACK to 192.168.49.9\n")
        result = subprocess.run(["/bin/sh", str(script), "--parse-ack"], input=logs, stdout=subprocess.PIPE, check=True)
        self.assertEqual(result.stdout, b"REQUEST udhcpd ip=192.168.49.52\n")

    def test_dhcp_config_generation_preserves_original_and_is_repeatable(self):
        source = self.root / "package/busybox/busybox.config"
        source.parent.mkdir(parents=True)
        original = b'CONFIG_SH_IS_ASH=y\n# CONFIG_UDHCPD is not set\n'
        source.write_bytes(original)
        self.assertTrue(sdk.prepare_busybox_config(self.root))
        self.assertFalse(sdk.prepare_busybox_config(self.root))
        self.assertEqual(source.read_bytes(), original)
        generated = sdk.config_values((self.root / "sdk-busybox.config").read_text(), prefix="CONFIG_")
        self.assertEqual(generated["CONFIG_UDHCPD"], "y")
        self.assertEqual(generated["CONFIG_SH_IS_ASH"], "y")

    def test_runtime_plan_aliases_share_verified_artifact(self):
        target, layout = self.runtime_fixture()
        layout["artifacts"]["example"]["app"] = ["opt/app", "opt/app-compatible"]
        plan = sdk.runtime_payload_plan(layout, target)
        self.assertEqual(len(plan), 2)
        self.assertEqual(plan[0][0], plan[1][0])
        self.assertEqual(plan[0][2], plan[1][2])
        self.assertEqual({entry[1] for entry in plan}, {target / "opt/app", target / "opt/app-compatible"})

    def test_runtime_plan_includes_complete_kernel_module_report(self):
        target, layout = self.runtime_fixture()
        images = sdk.OUT / "images/kernel-runtime"
        module = images / "drivers/example.ko"
        module.parent.mkdir(parents=True)
        module.write_bytes(b"module")
        manifest = {"kernel_release": "5.7.1"}
        report = {"kernel_release": "5.7.1", "modules": {"drivers/example.ko": {"sha256": sdk.digest(module)}}}
        (images / "build.json").write_text(json.dumps(report))
        layout["kernel_modules"] = True
        with mock.patch.object(sdk, "MANIFEST", manifest):
            plan = sdk.runtime_payload_plan(layout, target)
        self.assertIn(target / "lib/modules/5.7.1/kernel/drivers/example.ko", [entry[1] for entry in plan])
        report["kernel_release"] = "5.7.1+"
        (images / "build.json").write_text(json.dumps(report))
        with mock.patch.object(sdk, "MANIFEST", manifest):
            with self.assertRaises(RuntimeError):
                sdk.runtime_payload_plan(layout, target)

    def test_lan_backend_relocation_is_exact_and_repeatable(self):
        source = self.root / "output_cedar.c"
        old = 'static const char *backend="/root/roms/candidates/mplayer_geometry_20261005/mplayer-native-geometry";'
        source.write_text("preserved\n" + old + "\n")
        sdk.relocate_lan_backend(source)
        self.assertEqual(source.read_text(), 'preserved\nstatic const char *backend="/opt/f1/apps/media/mplayer-native";\n')
        sdk.relocate_lan_backend(source)
        source.write_text("unexpected\n")
        with self.assertRaises(RuntimeError):
            sdk.relocate_lan_backend(source)

    def test_runtime_script_relocation_is_narrow_and_requires_known_input(self):
        content = b'#!/bin/sh\nPLAYER=/root/test/candidates/old/player\nexec "$PLAYER"\n'
        result = sdk.relocate_runtime_text(content, {"/root/test/candidates/old/player": "/opt/f1/player"})
        self.assertEqual(result, b'#!/bin/sh\nPLAYER=/opt/f1/player\nexec "$PLAYER"\n')
        with self.assertRaises(RuntimeError):
            sdk.relocate_runtime_text(content, {"missing": "new"})
        with self.assertRaises(RuntimeError):
            sdk.relocate_runtime_text(content, {})

    def test_runtime_source_plan_checks_shell_and_preserves_source(self):
        source_root = self.root / "source"
        source_root.mkdir()
        content = b'#!/bin/sh\nexec /root/test/candidates/old/player\n'
        (source_root / "app.sh").write_bytes(content)
        spec = {"origin": "project", "path": "app.sh", "targets": ["opt/app.sh"], "executable": True,
                "replacements": {"/root/test/candidates/old/player": "/opt/f1/player"}}
        with mock.patch.object(sdk.subprocess, "run") as run:
            entries = sdk.runtime_source_plan({"sources": [spec]}, sdk.OUT / "runtime-payload", {"project": source_root}, set())
        run.assert_called_once_with(["/bin/sh", "-n"], input=b'#!/bin/sh\nexec /opt/f1/player\n', check=True)
        self.assertEqual((source_root / "app.sh").read_bytes(), content)
        self.assertEqual(entries[0][3], sdk.digest(source_root / "app.sh"))

    def test_runtime_resources_cannot_overwrite_compiled_artifact(self):
        source_root = self.root / "source"
        source_root.mkdir()
        (source_root / "asset").write_bytes(b"asset")
        target = sdk.OUT / "runtime-payload"
        spec = {"origin": "project", "path": "asset", "targets": ["opt/app"]}
        with self.assertRaises(RuntimeError):
            sdk.runtime_source_plan({"sources": [spec]}, target, {"project": source_root}, {target / "opt/app"})

    def runtime_fixture(self):
        sdk.check_output(create=True)
        images = sdk.OUT / "images/example"
        images.mkdir(parents=True)
        artifact = images / "app"
        artifact.write_bytes(b"compiled")
        (images / "build.json").write_text(json.dumps({"artifacts": {"app": {"sha256": sdk.digest(artifact)}}}))
        return sdk.OUT / "runtime-payload", {"artifacts": {"example": {"app": "opt/app"}}}

    def test_runtime_plan_checks_actual_artifact_hash(self):
        target, layout = self.runtime_fixture()
        plan = sdk.runtime_payload_plan(layout, target)
        self.assertEqual(plan[0][1], target / "opt/app")
        plan[0][0].write_bytes(b"tampered")
        with self.assertRaises(RuntimeError):
            sdk.runtime_payload_plan(layout, target)

    def test_runtime_plan_rejects_destination_escape(self):
        target, layout = self.runtime_fixture()
        layout["artifacts"]["example"]["app"] = "../escape"
        with self.assertRaises(RuntimeError):
            sdk.runtime_payload_plan(layout, target)

    def test_runtime_plan_rejects_symlink_parent_inside_payload(self):
        target, layout = self.runtime_fixture()
        (target / "real").mkdir(parents=True)
        (target / "opt").symlink_to(target / "real", target_is_directory=True)
        with self.assertRaises(RuntimeError):
            sdk.runtime_payload_plan(layout, target)

    def test_runtime_plan_rejects_duplicate_destinations(self):
        target, layout = self.runtime_fixture()
        images = sdk.OUT / "images/example"
        (images / "other").write_bytes(b"compiled")
        report = {"sha256": {"app": sdk.digest(images / "app"), "other": sdk.digest(images / "other")}}
        (images / "build.json").write_text(json.dumps(report))
        layout["artifacts"]["example"]["other"] = "opt/app"
        with self.assertRaises(RuntimeError):
            sdk.runtime_payload_plan(layout, target)

    def test_protocol_stage_excludes_old_objects_and_other_components(self):
        archive_path = self.root / "protocol.tar.gz"
        with tarfile.open(str(archive_path), "w:gz") as archive:
            for name in ("wpa_supplicant/Makefile", "wpa_supplicant/.config", "src/p2p/p2p.c", "src/p2p/p2p.o", "wpa_supplicant/wpa_supplicant"):
                member = tarfile.TarInfo("protocol-tools/wpa/" + name)
                member.size = 6
                archive.addfile(member, io.BytesIO(b"source"))
            member = tarfile.TarInfo("protocol-tools/old-driver/driver.c")
            member.size = 6
            archive.addfile(member, io.BytesIO(b"source"))
        manifest = {"protocol_source": {"archive": "protocol.tar.gz", "sha256": sdk.digest(archive_path), "member_root": "protocol-tools/wpa"}}
        with mock.patch.object(sdk, "MANIFEST", manifest):
            target = sdk.stage_protocol_source()
        self.assertTrue((target / "src/p2p/p2p.c").is_file())
        self.assertFalse((target / "src/p2p/p2p.o").exists())
        self.assertFalse((target / "wpa_supplicant/wpa_supplicant").exists())
        self.assertFalse((target / "old-driver").exists())

    def test_protocol_stage_rejects_escape_and_cleans_partial_export(self):
        archive_path = self.root / "protocol.tar.gz"
        with tarfile.open(str(archive_path), "w:gz") as archive:
            member = tarfile.TarInfo("protocol-tools/wpa/../../escape.c")
            member.size = 6
            archive.addfile(member, io.BytesIO(b"source"))
        manifest = {"protocol_source": {"archive": "protocol.tar.gz", "sha256": sdk.digest(archive_path), "member_root": "protocol-tools/wpa"}}
        with mock.patch.object(sdk, "MANIFEST", manifest):
            with self.assertRaises(RuntimeError):
                sdk.stage_protocol_source()
        self.assertFalse((sdk.OUT / "sources/wfd-protocol").exists())
        self.assertFalse((sdk.OUT / "escape.c").exists())

    def test_kernel_config_parser_handles_empty_and_disabled_localversion(self):
        values = sdk.config_values('CONFIG_LOCALVERSION=""\n# CONFIG_LOCALVERSION_AUTO is not set\nBR2_TEST=y\n', prefix="CONFIG_")
        self.assertEqual(values, {"CONFIG_LOCALVERSION": '""', "CONFIG_LOCALVERSION_AUTO": "n"})

    def test_source_metadata_allowlist_never_allows_driver_changes(self):
        source = self.root / "driver"
        source.mkdir()
        spec = {"commit": "a" * 40, "preserved_metadata_changes": ["WORKTREE_CHANGELOG.md"]}
        def git_capture(args, cwd=None):
            if "--show-toplevel" in args:
                return str(source)
            if "rev-parse" in args:
                return spec["commit"]
            return "WORKTREE_CHANGELOG.md\ndriver.c"
        with mock.patch.object(sdk, "component", return_value=(source, spec)), mock.patch.object(sdk, "capture", side_effect=git_capture):
            with self.assertRaises(RuntimeError):
                sdk.validate_source("driver")

    def test_source_metadata_allowlist_accepts_preserved_change_record(self):
        source = self.root / "driver"
        source.mkdir()
        spec = {"commit": "a" * 40, "preserved_metadata_changes": ["WORKTREE_CHANGELOG.md"]}
        def git_capture(args, cwd=None):
            if "--show-toplevel" in args:
                return str(source)
            if "rev-parse" in args:
                return spec["commit"]
            return "WORKTREE_CHANGELOG.md"
        with mock.patch.object(sdk, "component", return_value=(source, spec)), mock.patch.object(sdk, "capture", side_effect=git_capture):
            self.assertEqual(sdk.validate_source("driver"), (source, spec))

    def test_source_metadata_allowlist_rejects_arbitrary_source_name(self):
        source = self.root / "driver"
        source.mkdir()
        spec = {"commit": "a" * 40, "preserved_metadata_changes": ["driver.c"]}
        def git_capture(args, cwd=None):
            if "--show-toplevel" in args:
                return str(source)
            if "rev-parse" in args:
                return spec["commit"]
            return "driver.c"
        with mock.patch.object(sdk, "component", return_value=(source, spec)), mock.patch.object(sdk, "capture", side_effect=git_capture):
            with self.assertRaises(RuntimeError):
                sdk.validate_source("driver")

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name).resolve()
        self.patch = mock.patch.multiple(sdk, ROOT=self.root, OUT=self.root / ".sdk-output")
        self.patch.start()

    def tearDown(self):
        self.patch.stop()
        self.temp.cleanup()

    def test_missing_output_is_noop(self):
        sdk.check_output()
        self.assertFalse(sdk.OUT.exists())

    def test_owned_output_is_allowed(self):
        sdk.check_output(create=True)
        sdk.check_output()
        self.assertEqual((sdk.OUT / sdk.MARKER).read_text(), sdk.TOKEN)

    def test_clean_removes_only_owned_output(self):
        source = self.root / "source.c"
        source.write_text("preserve")
        sdk.check_output(create=True)
        (sdk.OUT / "generated.o").write_text("generated")
        with mock.patch("sys.argv", ["sdk", "clean"]):
            self.assertEqual(sdk.main(), 0)
        self.assertFalse(sdk.OUT.exists())
        self.assertEqual(source.read_text(), "preserve")

    def test_clean_unknown_directory_does_not_delete(self):
        sdk.OUT.mkdir()
        (sdk.OUT / "source.c").write_text("preserve")
        with mock.patch("sys.argv", ["sdk", "clean"]):
            with self.assertRaises(RuntimeError):
                sdk.main()
        self.assertTrue((sdk.OUT / "source.c").exists())

    def test_scoped_clean_preserves_other_components_and_sources(self):
        source = self.root / "active-source"
        source.mkdir()
        (source / "keep.c").write_text("preserve")
        sdk.check_output(create=True)
        generated = sdk.OUT / "sources" / "example"
        generated.mkdir(parents=True)
        (generated / ".sdk-source-commit").write_text("a" * 40)
        other = sdk.OUT / "sources" / "kernel"
        other.mkdir()
        (other / "zImage").write_text("preserve")
        manifest = {"components": {"example": {"path": "active-source"}}}
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", return_value=""):
            sdk.clean_component("example")
        self.assertFalse(generated.exists())
        self.assertTrue((other / "zImage").is_file())
        self.assertEqual((source / "keep.c").read_text(), "preserve")

    def test_scoped_clean_rejects_unmarked_component(self):
        (self.root / "active-source").mkdir()
        sdk.check_output(create=True)
        generated = sdk.OUT / "sources" / "example"
        generated.mkdir(parents=True)
        (generated / "source.c").write_text("preserve")
        manifest = {"components": {"example": {"path": "active-source"}}}
        with mock.patch.object(sdk, "MANIFEST", manifest):
            with self.assertRaises(RuntimeError):
                sdk.clean_component("example")
        self.assertTrue((generated / "source.c").is_file())

    def test_unknown_directory_is_preserved(self):
        sdk.OUT.mkdir()
        sentinel = sdk.OUT / "source.c"
        sentinel.write_text("user data")
        with self.assertRaises(RuntimeError):
            sdk.check_output()
        self.assertTrue(sentinel.exists())

    def test_symlink_output_is_rejected(self):
        external = self.root / "active-source"
        external.mkdir()
        sdk.OUT.symlink_to(str(external), target_is_directory=True)
        with self.assertRaises(RuntimeError):
            sdk.check_output(create=True)
        self.assertTrue(external.is_dir())

    def test_bad_marker_is_rejected(self):
        sdk.OUT.mkdir()
        (sdk.OUT / sdk.MARKER).write_text("not owned")
        with self.assertRaises(RuntimeError):
            sdk.check_output()

    def test_symlink_marker_is_rejected(self):
        sdk.OUT.mkdir()
        real = self.root / "marker"
        real.write_text(sdk.TOKEN)
        (sdk.OUT / sdk.MARKER).symlink_to(str(real))
        with self.assertRaises(RuntimeError):
            sdk.check_output()

    def test_lock_excludes_second_process(self):
        with sdk.locked():
            code = "import fcntl,os,sys; f=os.open(sys.argv[1],os.O_RDONLY); fcntl.flock(f,fcntl.LOCK_EX|fcntl.LOCK_NB)"
            result = subprocess.run(["python3", "-c", code, str(self.root)], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            self.assertNotEqual(result.returncode, 0)

    def archive(self, member):
        buffer = io.BytesIO()
        with tarfile.open(fileobj=buffer, mode="w") as archive:
            archive.addfile(member)
        buffer.seek(0)
        return buffer

    def test_archive_parent_escape_is_rejected(self):
        target = self.root / "stage"
        target.mkdir()
        with self.assertRaises(RuntimeError):
            sdk.extract_archive(self.archive(tarfile.TarInfo("../escaped")), target)
        self.assertFalse((self.root / "escaped").exists())

    def test_archive_symlink_escape_is_rejected(self):
        member = tarfile.TarInfo("evil")
        member.type = tarfile.SYMTYPE
        member.linkname = "/etc"
        target = self.root / "stage"
        target.mkdir()
        with self.assertRaises(RuntimeError):
            sdk.extract_archive(self.archive(member), target)

    def test_normal_archive_is_extracted(self):
        member = tarfile.TarInfo("source.c")
        target = self.root / "stage"
        target.mkdir()
        sdk.extract_archive(self.archive(member), target)
        self.assertTrue((target / "source.c").is_file())

    def test_config_updates_remove_duplicate_and_unset_lines(self):
        config = self.root / ".config"
        config.write_text('CONFIG_A="old"\n# CONFIG_B is not set\nCONFIG_A="duplicate"\nCONFIG_C=y\n')
        sdk.set_config(config, {"CONFIG_A": '"new"', "CONFIG_B": "y"})
        self.assertEqual(config.read_text(), 'CONFIG_C=y\nCONFIG_A="new"\nCONFIG_B=y\n')

    def test_config_reader_preserves_literal_values_and_unset_symbols(self):
        values = sdk.config_values('BR2_PACKAGE_A=y\n# BR2_PACKAGE_B is not set\nBR2_LITERAL="x=y # literal"\n')
        self.assertEqual(values, {"BR2_PACKAGE_A": "y", "BR2_PACKAGE_B": "n", "BR2_LITERAL": '"x=y # literal"'})

    def test_ons_baseline_relocation_keeps_checksum_contract(self):
        directory = self.root / "ons"
        engine = directory / "vendor/ONScripter-GBK"
        engine.mkdir(parents=True)
        source = engine / "ScriptHandler.cpp"
        source.write_text("locked source")
        expected = sdk.digest(source)
        baseline = directory / "source-baseline.sha256"
        baseline.write_text(expected + "  ../../old/ScriptHandler.cpp\n")
        sdk.relocate_ons_baseline(directory)
        self.assertEqual(baseline.read_text(), expected + "  vendor/ONScripter-GBK/ScriptHandler.cpp\n")
        sdk.relocate_ons_baseline(directory)
        self.assertEqual(source.read_text(), "locked source")

    def test_ons_baseline_rejects_mismatched_snapshot(self):
        directory = self.root / "ons"
        engine = directory / "vendor/ONScripter-GBK"
        engine.mkdir(parents=True)
        (engine / "ScriptHandler.cpp").write_text("different")
        baseline = directory / "source-baseline.sha256"
        original = "a" * 64 + "  ../../old/ScriptHandler.cpp\n"
        baseline.write_text(original)
        with self.assertRaises(RuntimeError):
            sdk.relocate_ons_baseline(directory)
        self.assertEqual(baseline.read_text(), original)

    def test_project_source_addition_preserves_existing_outputs(self):
        import json
        source = self.root / "APP"
        source.mkdir()
        sdk.check_output(create=True)
        work = sdk.OUT / "work"
        (work / "APP").mkdir(parents=True)
        (work / "APP/compiled.o").write_text("preserve")
        stamps = sdk.OUT / "stamps"
        stamps.mkdir()
        previous = {"commit": "a" * 40, "paths": ["APP"]}
        new = {"commit": "b" * 40, "paths": ["APP"], "source_additions_from": "a" * 40,
               "source_additions": ["APP/version.h"]}
        (stamps / "project-sources.json").write_text(json.dumps(previous))
        def git_capture(args, cwd=None):
            if "--show-toplevel" in args:
                return str(self.root)
            if "diff" in args and "a" * 40 in args:
                return "APP/version.h"
            return ""
        def git_archive(args, cwd=None, stdout=None, check=None):
            with tarfile.open(fileobj=stdout, mode="w") as archive:
                item = tarfile.TarInfo("APP/version.h")
                data = b"preserved source header"
                item.size = len(data)
                archive.addfile(item, io.BytesIO(data))
        with mock.patch.object(sdk, "MANIFEST", {"project_sources": new}), mock.patch.object(sdk, "capture", side_effect=git_capture), mock.patch.object(sdk.subprocess, "run", side_effect=git_archive):
            sdk.stage_project_sources()
        self.assertEqual((work / "APP/version.h").read_text(), "preserved source header")
        self.assertEqual((work / "APP/compiled.o").read_text(), "preserve")
        self.assertEqual(json.loads((stamps / "project-sources.json").read_text()), new)

    def test_project_stage_preserves_toolchain_and_uses_locked_archive(self):
        source = self.root / "LVGL/menu"
        source.mkdir(parents=True)
        (source / "Makefile").write_text("source")
        sdk.check_output(create=True)
        toolchain = sdk.OUT / "work/ROOTFS/output/host"
        toolchain.mkdir(parents=True)
        (toolchain / "preserve").write_text("toolchain")
        manifest = {"project_sources": {"commit": "a" * 40, "paths": ["LVGL/menu"]}}
        def git_capture(args, cwd=None):
            return str(self.root) if "--show-toplevel" in args else ""
        def git_archive(args, cwd=None, stdout=None, check=None):
            with tarfile.open(fileobj=stdout, mode="w") as archive:
                content = b"locked source"
                item = tarfile.TarInfo("LVGL/menu/Makefile")
                item.size = len(content)
                archive.addfile(item, io.BytesIO(content))
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", side_effect=git_capture), mock.patch.object(sdk.subprocess, "run", side_effect=git_archive):
            work = sdk.stage_project_sources()
        self.assertEqual((work / "LVGL/menu/Makefile").read_text(), "locked source")
        self.assertEqual((toolchain / "preserve").read_text(), "toolchain")
        self.assertEqual((source / "Makefile").read_text(), "source")

    def test_project_stage_rejects_existing_unowned_source(self):
        (self.root / "LVGL/menu").mkdir(parents=True)
        sdk.check_output(create=True)
        generated = sdk.OUT / "work/LVGL/menu"
        generated.mkdir(parents=True)
        (generated / "source.c").write_text("preserve")
        manifest = {"project_sources": {"commit": "a" * 40, "paths": ["LVGL/menu"]}}
        def git_capture(args, cwd=None):
            return str(self.root) if "--show-toplevel" in args else ""
        with mock.patch.object(sdk, "MANIFEST", manifest), mock.patch.object(sdk, "capture", side_effect=git_capture):
            with self.assertRaises(RuntimeError):
                sdk.stage_project_sources()
        self.assertEqual((generated / "source.c").read_text(), "preserve")

    def test_bluetooth_audio_publishes_installed_elf_not_libtool_wrapper(self):
        source = self.root / "bluealsa"
        source.mkdir()
        (source / "configure").write_text("generated")
        host = self.root / "host"
        prefix = str(host / "bin/arm-buildroot-linux-gnueabi-")
        env = {}
        with mock.patch.object(sdk, "app_environment", return_value=(self.root, host, prefix, env)), mock.patch.object(sdk, "stage", return_value=(source, source, {})), mock.patch.object(sdk, "capture", return_value="-ldbus-1"), mock.patch.object(sdk, "run") as run, mock.patch.object(sdk, "publish_apps") as publish:
            sdk.build_bluetooth_audio(2)
        files = publish.call_args[0][1]
        self.assertEqual(files["bluealsa"], source / "sdk-install/usr/bin/bluealsa")
        self.assertEqual(files["bluealsa-aplay"], source / "sdk-install/usr/bin/bluealsa-aplay")
        self.assertIn(["make", "install", "DESTDIR=" + str(source / "sdk-install")], [call[0][0] for call in run.call_args_list])
        self.assertEqual(env["CC"], prefix + "gcc")
        self.assertEqual(env["PKG_CONFIG_SYSROOT_DIR"], str(host / "arm-buildroot-linux-gnueabi/sysroot"))

    def test_sink_build_uses_locked_source_and_new_toolchain(self):
        source = self.root / "sink"
        host = self.root / "host"
        prefix = str(host / "bin/arm-buildroot-linux-gnueabi-")
        env = {"LC_ALL": "C"}
        with mock.patch.object(sdk, "app_environment", return_value=(self.root, host, prefix, env)), mock.patch.object(sdk, "stage", return_value=(source, source, {})) as stage, mock.patch.object(sdk, "run") as run, mock.patch.object(sdk, "publish_apps") as publish:
            sdk.build_miracast_sink(2)
        stage.assert_called_once_with("miracast-sink")
        command = run.call_args[0][0]
        self.assertIn("-B", command)
        self.assertIn("CROSS_COMPILE=" + prefix, command)
        self.assertEqual(run.call_args[0][1:], (source, env))
        publish.assert_called_once_with("miracast-sink", {"miracast_sink_dump.lowest.brarm": source / "miracast_sink_dump.lowest.brarm"}, prefix)

    def test_stage_rejects_code_change_without_deleting_previous_build(self):
        source = self.root / "active-source"
        source.mkdir()
        sdk.check_output(create=True)
        target = sdk.OUT / "sources/example"
        target.mkdir(parents=True)
        (target / ".sdk-source-commit").write_text("a" * 40)
        (target / "compiled.o").write_text("preserve")
        spec = {"commit": "b" * 40}
        with mock.patch.object(sdk, "validate_source", return_value=(source, spec)), mock.patch.object(sdk, "capture", return_value="source.c"):
            with self.assertRaises(RuntimeError):
                sdk.stage("example")
        self.assertEqual((target / "compiled.o").read_text(), "preserve")

    def test_stage_refreshes_only_metadata_without_resetting_build(self):
        source = self.root / "active-source"
        source.mkdir()
        sdk.check_output(create=True)
        target = sdk.OUT / "sources/example"
        target.mkdir(parents=True)
        (target / ".sdk-source-commit").write_text("a" * 40)
        (target / "compiled.o").write_text("preserve")
        spec = {"commit": "b" * 40}
        def git_archive(args, cwd=None, stdout=None, check=None):
            with tarfile.open(fileobj=stdout, mode="w") as archive:
                content = b"new record"
                item = tarfile.TarInfo("WORKTREE_CHANGELOG.md")
                item.size = len(content)
                archive.addfile(item, io.BytesIO(content))
        with mock.patch.object(sdk, "validate_source", return_value=(source, spec)), mock.patch.object(sdk, "capture", return_value="WORKTREE_CHANGELOG.md"), mock.patch.object(sdk.subprocess, "run", side_effect=git_archive):
            sdk.stage("example")
        self.assertEqual((target / "compiled.o").read_text(), "preserve")
        self.assertEqual((target / "WORKTREE_CHANGELOG.md").read_text(), "new record")
        self.assertEqual((target / ".sdk-source-commit").read_text().strip(), "b" * 40)


if __name__ == "__main__":
    unittest.main()
