"""Behavior checks: rejection before any connection or MAVLink dependency."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

SCRIPT = Path(__file__).resolve().parents[1] / "tools/hardware/set_mavlink_parameter.py"


def load_writer():
    spec = importlib.util.spec_from_file_location("quarantined_writer", SCRIPT)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


class QuarantineTests(unittest.TestCase):
    def test_cli_refuses_every_legacy_write_override_without_dependencies(self):
        for device in ("/dev/ttyACM0", "udpin:10.77.10.3:14550", "tcp:192.168.1.47:5760"):
            with self.subTest(device=device):
                result = subprocess.run([sys.executable, "-I", str(SCRIPT), "--device", device,
                    "--name", "SERVO1_FUNCTION", "--expected", "0", "--value", "4",
                    "--commit", "--allow-protected"], capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 2)
                record = json.loads(result.stdout)
                self.assertEqual(record["outcome"], "refused_not_authorized")
                self.assertFalse(record["connection_opened"])
                self.assertFalse(record["transmitted"])

    def test_direct_api_does_not_touch_armed_or_unarmed_link(self):
        module = load_writer()
        class Tripwire:
            def __getattribute__(self, name):
                raise AssertionError("link was accessed: " + name)
        for name in ("SCR_HEAP_SIZE", "ARSPD_USE", "ARMING_CHECK", "SERVO1_FUNCTION", "RC1_OPTION", "FLTMODE1", "FENCE_ENABLE"):
            with self.subTest(parameter=name), self.assertRaises(module.ParameterError):
                module.write_parameter(Tripwire(), name, 1, 9, 5)

    def test_cli_refusal_never_constructs_connection_or_opens_files(self):
        module = load_writer()
        with patch("socket.socket", side_effect=AssertionError("socket opened")), \
                patch("builtins.open", side_effect=AssertionError("file opened")), \
                contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(module.main(["--commit", "--allow-protected"]), 2)
        self.assertFalse(json.loads(output.getvalue())["write_authorized"])

    def test_dry_run_is_also_inert_not_a_parameter_query(self):
        module = load_writer()
        with contextlib.redirect_stdout(io.StringIO()) as output:
            self.assertEqual(module.main([]), 2)
        self.assertFalse(json.loads(output.getvalue())["transmitted"])

    def test_audit_argument_cannot_enable_io_or_leak_device_credentials(self):
        with tempfile.TemporaryDirectory() as directory:
            absent = Path(directory) / "must-not-be-created"
            result = subprocess.run([sys.executable, "-I", str(SCRIPT), "--commit",
                "--audit-dir", str(absent), "--device", "tcp:user:secret@192.0.2.1:5760"],
                capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 2)
            self.assertFalse(absent.exists())
            self.assertNotIn("secret", result.stdout + result.stderr)
            self.assertFalse(json.loads(result.stdout)["audit_persisted"])

    def test_help_has_no_network_dependency(self):
        result = subprocess.run([sys.executable, "-I", str(SCRIPT), "--help"],
                                capture_output=True, text=True, timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertIn("quarantined", result.stdout)

    def test_import_never_loads_pymavlink(self):
        import builtins
        original = builtins.__import__
        def guarded(name, *args, **kwargs):
            if name.startswith("pymavlink"):
                raise AssertionError("pymavlink imported")
            return original(name, *args, **kwargs)
        with patch("builtins.__import__", side_effect=guarded):
            load_writer()


if __name__ == "__main__":
    unittest.main()
