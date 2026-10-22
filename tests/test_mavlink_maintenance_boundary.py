"""No device/network is opened: fake pymavlink and deterministic receive clock."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]


def load_tool(name):
    spec = importlib.util.spec_from_file_location(
        "boundary_" + name, ROOT / "tools/hardware" / (name + ".py"))
    module = importlib.util.module_from_spec(spec)
    fake_util = SimpleNamespace(mavlink=SimpleNamespace(enums={}),
                                mavlink_connection=Mock())
    with patch.dict(sys.modules, {"pymavlink": SimpleNamespace(mavutil=fake_util)}):
        spec.loader.exec_module(module)
    return module


def message(kind, **fields):
    return SimpleNamespace(get_type=lambda: kind, **fields)


class Link:
    target_system = 1
    target_component = 1

    def __init__(self, messages=()):
        self.messages = iter(messages)
        self.sent = []
        self.mav = SimpleNamespace(param_request_list_send=self.request)

    def request(self, *args):
        self.sent.append(("PARAM_REQUEST_LIST", args))

    def wait_heartbeat(self, **_kwargs):
        return message("HEARTBEAT", type=1, autopilot=3, base_mode=0,
                       custom_mode=0, system_status=3)

    def recv_match(self, **_kwargs):
        return next(self.messages, None)

    def close(self):
        self.closed = True


class MavlinkMaintenanceBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.audit = load_tool("audit_pixhawk")
        self.backup = load_tool("backup_parameters")
        # Clock advances without sleeping or touching real devices.
        self.clock = iter(range(1000))
        self.tick = patch("time.monotonic", side_effect=lambda: next(self.clock))
        self.tick.start()
        self.addCleanup(self.tick.stop)

    def test_audit_missing_unsolicited_version_is_explicit_and_zero_tx(self):
        link = Link()
        result = self.audit.collect(link, 5)
        self.assertEqual(result["capture_mode"], "PASSIVE_RX_ONLY")
        self.assertEqual(result["version_status"], "NOT_OBSERVED")
        self.assertNotIn("version", result)
        self.assertEqual(link.sent, [])

    def test_audit_accepts_unsolicited_version_without_command_api(self):
        link = Link([message("AUTOPILOT_VERSION", flight_sw_version=0x04070000,
            middleware_sw_version=0, os_sw_version=0, board_version=1,
            vendor_id=1, product_id=2, uid=3, uid2=b"x", capabilities=0,
            flight_custom_version=b"y")])
        result = self.audit.collect(link, 5)
        self.assertEqual(result["version_status"], "OBSERVED")
        self.assertEqual(result["version"]["flight"], "4.7.0.0")
        self.assertEqual(link.sent, [])

    def test_audit_blocks_implicit_receive_side_write(self):
        link = Link()
        link.recv_match = lambda **kwargs: link.write(b"forbidden")
        with self.assertRaises(PermissionError):
            self.audit.collect(link, 5)

    def test_audit_closes_connection_on_missing_heartbeat(self):
        link = Link()
        link.wait_heartbeat = lambda **kwargs: None
        self.audit.mavutil.mavlink_connection.return_value = link
        with patch.object(sys, "argv", ["audit", "--device", "FAKE"]):
            with self.assertRaises(RuntimeError):
                self.audit.main()
        self.assertTrue(link.closed)
        self.assertEqual(link.sent, [])

    def test_backup_default_rejects_before_any_connection(self):
        with patch.object(sys, "argv", ["backup", "--device", "FAKE",
                                        "--output-dir", "UNUSED"]):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit) as error:
                    self.backup.main()
        self.assertEqual(error.exception.code, 2)
        self.backup.mavutil.mavlink_connection.assert_not_called()

    def test_backup_direct_collection_requires_consent_too(self):
        link = Link()
        with self.assertRaises(PermissionError):
            self.backup.collect(link, 5, 2)
        self.assertEqual(link.sent, [])

    def test_backup_explicit_request_is_only_parameter_list(self):
        link = Link([message("PARAM_VALUE", param_id="TEST", param_value=12.0,
                             param_index=0, param_count=1)])
        values, indices, declared = self.backup.collect(link, 8, 2, allow_request=True)
        self.assertEqual(values, {"TEST": 12.0})
        self.assertEqual(indices, {"TEST": 0})
        self.assertEqual(declared, 1)
        self.assertEqual(link.sent, [("PARAM_REQUEST_LIST", (1, 1))])

    def test_backup_firmware_listen_has_no_command_api(self):
        link = Link()
        self.assertIsNone(self.backup.firmware_of(link, 5))
        self.assertEqual(link.sent, [])

    def test_backup_bad_timeout_fails_before_connection(self):
        with patch.object(sys, "argv", ["backup", "--device", "FAKE",
                "--output-dir", "UNUSED", "--allow-parameter-request", "--timeout", "nan"]):
            with contextlib.redirect_stderr(io.StringIO()):
                with self.assertRaises(SystemExit):
                    self.backup.main()
        self.backup.mavutil.mavlink_connection.assert_not_called()

    def test_backup_records_active_mode_and_closes_connection(self):
        link = Link([message("PARAM_VALUE", param_id="TEST", param_value=12.0,
                             param_index=0, param_count=1)])
        self.backup.mavutil.mavlink_connection.return_value = link
        with tempfile.TemporaryDirectory() as directory:
            with patch.object(sys, "argv", ["backup", "--device", "FAKE",
                    "--output-dir", directory, "--allow-parameter-request"]):
                with patch.object(self.backup, "firmware_of", return_value=None):
                    with contextlib.redirect_stdout(io.StringIO()):
                        self.assertEqual(self.backup.main(), 0)
            manifests = list(Path(directory).glob("*/manifest.json"))
            self.assertEqual(len(manifests), 1)
            report = json.loads(manifests[0].read_text())
            self.assertEqual(report["capture_mode"], "ACTIVE_MAINTENANCE_PARAMETER_REQUEST")
            self.assertEqual(report["transmitted_messages"], ["PARAM_REQUEST_LIST"])
            self.assertEqual(report["firmware_status"], "NOT_OBSERVED")
            self.assertEqual(link.sent, [("PARAM_REQUEST_LIST", (1, 1))])
            self.assertTrue(link.closed)

    def test_gnss_and_airspeed_audits_never_request_streams(self):
        for name in ("audit_gnss", "audit_airspeed"):
            with self.subTest(tool=name):
                tool = load_tool(name)
                link = Link()
                result = tool.collect(link, 5)
                self.assertEqual(result["capture_mode"], "PASSIVE_RX_ONLY")
                self.assertEqual(result["messages"], {})
                self.assertEqual(link.sent, [])
                with self.assertRaises(PermissionError):
                    link.write(b"unexpected")

    def test_gnss_and_airspeed_audits_retain_unsolicited_data(self):
        for name, kind, fields in (
            ("audit_gnss", "GPS_RAW_INT", {"fix_type": 3}),
            ("audit_airspeed", "VFR_HUD", {"airspeed": 12.0}),
        ):
            with self.subTest(tool=name):
                tool = load_tool(name)
                sample = message(kind, to_dict=lambda: {"mavpackettype": kind, **fields})
                link = Link([sample])
                result = tool.collect(link, 5)
                self.assertEqual(result["messages"][kind], fields)
                self.assertEqual(link.sent, [])


if __name__ == "__main__":
    unittest.main()
