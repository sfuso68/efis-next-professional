from __future__ import annotations

import copy
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/hardware/capture_g1_sensor_node.py"
SPEC = importlib.util.spec_from_file_location("capture_g1_sensor_node", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1NodeCaptureTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        files = {
            "etc/os-release": 'PRETTY_NAME="Debian GNU/Linux 12 (bookworm)"\nVERSION_ID="12"\n',
            "proc/device-tree/model": "Raspberry Pi 4 Model B Rev 1.5\x00",
            "proc/meminfo": "MemTotal: 8000000 kB\nMemAvailable: 7000000 kB\n",
            "boot/firmware/config.txt": "dtparam=i2c_arm=on\n",
            "sys/class/i2c-dev/i2c-1/name": "bcm2835 (i2c@7e804000)\n",
            "dev/i2c-1": "",
        }
        for name, content in files.items():
            path = self.root / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text(content, encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    @staticmethod
    def runner(command: tuple[str, ...]) -> dict:
        outputs = {
            "hostname": "ems-sensor",
            "addresses": "192.168.1.89 fd00::1",
            "kernel": "Linux ems-sensor 6.12 aarch64 GNU/Linux",
            "architecture": "aarch64",
            "storage": '{"blockdevices":[{"name":"mmcblk0","size":"53.7G"}]}',
            "filesystems": "Filesystem Type 1024-blocks Used Available Capacity Mounted on",
            "network": '[{"ifname":"wlan0"}]',
            "gpio_i2c": "2: a0 pu | hi // GPIO2 = SDA1\n3: a0 pu | hi // GPIO3 = SCL1",
            "throttled": "throttled=0x0",
            "temperature": "temp=44.8'C",
        }
        name = next(key for key, value in MODULE.COMMANDS.items() if value == command)
        return {"argv": list(command), "available": True, "returncode": 0,
                "stdout": outputs[name], "stderr": ""}

    def capture(self) -> dict:
        return MODULE.capture(
            root=self.root, runner=self.runner,
            captured_utc="2026-09-18T18:49:45Z",
        )

    def test_reference_capture_passes_ten_controls(self) -> None:
        report = self.capture()
        self.assertEqual(report["schema"], MODULE.SCHEMA)
        self.assertEqual(len(report["controls"]), 10)
        self.assertTrue(all(item["status"] == "PASS" for item in report["controls"]))
        self.assertEqual(report["node"]["hostname"], "ems-sensor")

    def test_capture_never_invokes_probe_vendor_sudo_or_shell(self) -> None:
        calls: list[tuple[str, ...]] = []

        def audited(command: tuple[str, ...]) -> dict:
            calls.append(command)
            return self.runner(command)

        report = MODULE.capture(
            root=self.root, runner=audited,
            captured_utc="2026-09-18T18:49:45Z",
        )
        self.assertEqual(set(calls), set(MODULE.COMMANDS.values()))
        self.assertFalse(any(call[0] in MODULE.FORBIDDEN_COMMAND_NAMES for call in calls))
        self.assertFalse(report["i2c"]["address_probe_performed"])
        self.assertFalse(report["safety"]["shell_used"])

    def test_arbitrary_and_i2c_probe_commands_are_rejected(self) -> None:
        with self.assertRaises(MODULE.CaptureError):
            MODULE.run_fixed_command(("echo", "unsafe"))
        with self.assertRaises(MODULE.CaptureError):
            MODULE.run_fixed_command(("i2cdetect", "-y", "1"))

    def test_each_control_fails_closed_when_its_evidence_is_removed(self) -> None:
        mutations = [
            ("G1-C10-01", lambda value: value.update(schema="unknown")),
            ("G1-C10-02", lambda value: value.update(captured_utc="")),
            ("G1-C10-03", lambda value: value["node"].update(model=None)),
            ("G1-C10-04", lambda value: value["resources"].update(meminfo=None)),
            ("G1-C10-05", lambda value: value["network"].update(available=False)),
            ("G1-C10-06", lambda value: value["i2c"].update(boot_config_sha256=None)),
            ("G1-C10-07", lambda value: value["i2c"].update(address_probe_performed=True)),
            ("G1-C10-08", lambda value: value["i2c"]["gpio"].update(stdout="GPIO inputs")),
            ("G1-C10-09", lambda value: value["health"]["temperature"].update(available=False)),
            ("G1-C10-10", lambda value: value["safety"].update(flight_approval=True)),
        ]
        reference = self.capture()
        for control, mutate in mutations:
            with self.subTest(control=control):
                invalid = copy.deepcopy(reference)
                mutate(invalid)
                with self.assertRaisesRegex(MODULE.CaptureError, control):
                    MODULE.validate(invalid)

    def test_missing_i2c_arm_enablement_fails_closed(self) -> None:
        (self.root / "boot/firmware/config.txt").write_text(
            "#dtparam=i2c_arm=on\n", encoding="utf-8"
        )
        with self.assertRaisesRegex(MODULE.CaptureError, "G1-C10-06"):
            self.capture()

    def test_missing_i2c_one_fails_closed_without_scanning(self) -> None:
        (self.root / "dev/i2c-1").unlink()
        with self.assertRaisesRegex(MODULE.CaptureError, "G1-C10-07"):
            self.capture()

    def test_report_remains_no_go_with_zero_verified_boards(self) -> None:
        report = self.capture()
        self.assertEqual(report["safety"]["boards_verified"], 0)
        self.assertEqual(report["safety"]["decision"],
                         "NO_GO_PENDING_PHYSICAL_BOARD_INVENTORY")
        self.assertFalse(report["safety"]["flight_approval"])


if __name__ == "__main__":
    unittest.main()
