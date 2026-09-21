from __future__ import annotations

import importlib.util
import hashlib
import json
import os
from pathlib import Path
import socket
import sys
import sysconfig
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
sys.path.insert(0, str(ROOT / "tools" / "groundstation"))
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_monitor_preflight.py"
UNIT = ROOT / "deploy" / "systemd" / "efis-qgc-monitor.user.service"
SPEECH_DISABLE_SCRIPT = (
    ROOT / "deploy" / "scripts" / "disable_qgc_synthetic_speech.sh"
)
SPEC = importlib.util.spec_from_file_location("qgc_monitor_preflight", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class QgcMonitorPreflightTests(unittest.TestCase):
    def test_systemd_uses_condition_not_restartable_prestart(self) -> None:
        unit = UNIT.read_text(encoding="utf-8")
        self.assertIn("ExecCondition=/usr/bin/python3", unit)
        self.assertNotIn("ExecStartPre=", unit)
        self.assertNotIn("RestartPreventExitStatus=", unit)

    def test_systemd_does_not_start_synthetic_speech_backend(self) -> None:
        unit = UNIT.read_text(encoding="utf-8")
        self.assertNotRegex(unit, r"(?m)^(After|Wants|Requires)=.*speech-dispatcher")

    def test_speech_disable_script_preserves_qgc_telemetry_gate(self) -> None:
        script = SPEECH_DISABLE_SCRIPT.read_text(encoding="utf-8")
        self.assertIn(
            "mask --now speech-dispatcher.service speech-dispatcher.socket", script
        )
        self.assertIn("is-active --quiet efis-qgc-monitor.service", script)
        self.assertIn(":14550", script)
        self.assertIn("audioMuted=true", script)
        self.assertIn("speech-dispatcher was spawned", script)
        self.assertIn("pgrep -u", script)
        self.assertIn(" -f ", script)
        self.assertNotIn("-x speech-dispatcher", script)

    def test_systemd_does_not_load_synthetic_speech_plugin_compatibility(self) -> None:
        unit = UNIT.read_text(encoding="utf-8")
        self.assertNotIn("LD_PRELOAD", unit)
        self.assertIn("ExecStart=%h/EFIS-ArduPilot/qgroundcontrol/", unit)

    def test_blocked_exit_is_non_transient_configuration_failure(self) -> None:
        self.assertEqual(MODULE.BLOCKED_EXIT, 78)

    def test_pass_requires_all_three_guards(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=False, port_available=True
        )
        self.assertEqual(report["mode"], "MONITOR_ONLY_NO_PIXHAWK_RETURN")
        self.assertEqual(report["status"], "PASS")

    def test_any_acm_device_blocks_start(self) -> None:
        report = MODULE.evaluate(
            serial_devices=["/dev/ttyACM0"],
            bridge_active=False,
            port_available=True,
        )
        self.assertEqual(report["status"], "BLOCKED")
        self.assertFalse(report["checks"]["pixhawk_usb_absent"]["pass"])

    def test_legacy_bridge_blocks_start(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=True, port_available=True
        )
        self.assertEqual(report["status"], "BLOCKED")

    def test_occupied_udp_port_blocks_start(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        listener.bind(("127.0.0.1", 0))
        try:
            port = listener.getsockname()[1]
            self.assertFalse(MODULE.udp_port_available(port, bind_address="127.0.0.1"))
        finally:
            listener.close()

    def test_free_udp_port_passes_probe(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        listener.bind(("127.0.0.1", 0))
        port = listener.getsockname()[1]
        listener.close()
        self.assertTrue(MODULE.udp_port_available(port, bind_address="127.0.0.1"))

    def test_report_contains_no_telemetry_payload(self) -> None:
        report = MODULE.evaluate(
            serial_devices=[], bridge_active=False, port_available=True
        )
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            MODULE.write_report(path, report)
            text = path.read_text(encoding="utf-8")
        self.assertIn('"status": "PASS"', text)
        self.assertNotIn("mavlink_payload", text)

    def write_manifest(self, directory: Path, *, digest: str | None = None) -> Path:
        appimage = directory / "QGroundControl.AppImage"
        appimage.write_bytes(b"approved-qgc-image")
        appdir = directory / "QGroundControl.AppDir"
        appdir.mkdir()
        executable = appdir / "AppRun"
        executable.write_text("#!/bin/sh\n", encoding="utf-8")
        executable.chmod(executable.stat().st_mode | 0o100)
        payload = appdir / "usr" / "bin"
        payload.mkdir(parents=True)
        (payload / "QGroundControl").write_bytes(b"approved-binary")
        appdir_digest = MODULE.digest_tree(appdir)
        manifest = {
            "schema": MODULE.MANIFEST_SCHEMA,
            "mode": MODULE.MODE,
            "application": "QGroundControl",
            "version": "5.0.8",
            "appimage": {
                "path": str(appimage),
                "sha256": digest or hashlib.sha256(appimage.read_bytes()).hexdigest(),
            },
            "appdir": {
                "path": str(appdir),
                **{
                    key: appdir_digest[key]
                    for key in (
                        "tree_sha256",
                        "file_count",
                        "symlink_count",
                        "directory_count",
                        "file_bytes",
                    )
                },
            },
            "executable": str(executable),
            "listener": {
                "bind": "0.0.0.0",
                "port": 14550,
                "allowed_source": "192.168.1.47",
            },
            "legacy_unit": MODULE.DEFAULT_LEGACY_UNIT,
        }
        if sys.platform != "win32":
            multiarch = sysconfig.get_config_var("MULTIARCH")
            candidates = [
                Path("/lib") / str(multiarch) / "libglib-2.0.so.0",
                Path("/usr/lib") / str(multiarch) / "libglib-2.0.so.0",
                Path("/lib64/libglib-2.0.so.0"),
                Path("/usr/lib64/libglib-2.0.so.0"),
            ]
            preload = next((path for path in candidates if path.is_file()), None)
            self.assertIsNotNone(
                preload,
                f"glib runtime fixture not found for MULTIARCH={multiarch}",
            )
            manifest["runtime_preload"] = {
                "path": str(preload),
                "required_symbol": "g_string_free_and_steal",
            }
        else:
            manifest["runtime_preload"] = {
                "path": str(Path(os.environ["SystemRoot"]) / "System32" / "kernel32.dll"),
                "required_symbol": "GetCurrentProcess",
            }
        path = directory / "manifest.json"
        path.write_text(json.dumps(manifest), encoding="utf-8")
        return path

    def test_library_symbol_probe_is_fail_closed(self) -> None:
        class Library:
            expected = object()

        self.assertTrue(
            MODULE.library_has_symbol(
                Path("library"), "expected", loader=lambda unused: Library()
            )
        )
        self.assertFalse(
            MODULE.library_has_symbol(
                Path("library"), "missing", loader=lambda unused: Library()
            )
        )

    def test_approved_artifacts_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = MODULE.inspect_artifacts(
                self.write_manifest(Path(directory)),
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertTrue(result["pass"])
        self.assertEqual(result["version"], "5.0.8")

    def test_modified_appimage_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = MODULE.inspect_artifacts(
                self.write_manifest(Path(directory), digest="0" * 64),
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("mismatch", result["reason"])

    def test_modified_appdir_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.write_manifest(root)
            (root / "QGroundControl.AppDir" / "usr" / "bin" / "QGroundControl").write_bytes(b"tampered")
            result = MODULE.inspect_artifacts(
                manifest,
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("AppDir", result["reason"])

    def test_missing_appdir_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.write_manifest(root)
            document = json.loads(manifest.read_text(encoding="utf-8"))
            document["appdir"]["path"] = str(root / "missing")
            manifest.write_text(json.dumps(document), encoding="utf-8")
            result = MODULE.inspect_artifacts(
                manifest,
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("missing", result["reason"])

    def test_invalid_appdir_digest_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.write_manifest(root)
            document = json.loads(manifest.read_text(encoding="utf-8"))
            document["appdir"]["tree_sha256"] = "short"
            manifest.write_text(json.dumps(document), encoding="utf-8")
            result = MODULE.inspect_artifacts(
                manifest,
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("AppDir SHA-256", result["reason"])

    def test_executable_outside_appdir_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest = self.write_manifest(root)
            outside = root / "outside-AppRun"
            outside.write_text("#!/bin/sh\n", encoding="utf-8")
            outside.chmod(outside.stat().st_mode | 0o100)
            document = json.loads(manifest.read_text(encoding="utf-8"))
            document["executable"] = str(outside)
            manifest.write_text(json.dumps(document), encoding="utf-8")
            result = MODULE.inspect_artifacts(
                manifest,
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("outside AppDir", result["reason"])

    def test_missing_manifest_is_rejected(self) -> None:
        result = MODULE.inspect_artifacts(
            Path("does-not-exist.json"),
            bind_address="0.0.0.0",
            port=14550,
            legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
        )
        self.assertFalse(result["pass"])

    def test_manifest_endpoint_mismatch_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            result = MODULE.inspect_artifacts(
                self.write_manifest(Path(directory)),
                bind_address="127.0.0.1",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("endpoint", result["reason"])

    def test_unapproved_observer_source_is_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = self.write_manifest(Path(directory))
            manifest = json.loads(path.read_text(encoding="utf-8"))
            manifest["listener"]["allowed_source"] = "192.168.1.99"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            result = MODULE.inspect_artifacts(
                path,
                bind_address="0.0.0.0",
                port=14550,
                legacy_unit=MODULE.DEFAULT_LEGACY_UNIT,
            )
        self.assertFalse(result["pass"])
        self.assertIn("approved Blackbox", result["reason"])


if __name__ == "__main__":
    unittest.main()
