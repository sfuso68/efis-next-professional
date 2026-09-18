import copy
import json
from pathlib import Path
import tempfile
import unittest

from tools.ui.assemble_hmi_hardware_evidence import KINDS, assemble, validate_results


ROOT = Path(__file__).resolve().parents[1]


def ready_results(bundle: Path) -> dict:
    runs = []
    for role in ("PFD", "MFD", "EMS"):
        artifacts = {}
        for kind in KINDS:
            name = f"{role.lower()}-{kind.lower()}.txt"
            (bundle / name).write_text(f"synthetic packager fixture {role}/{kind}\n", encoding="utf-8")
            artifacts[kind] = name
        runs.append({
            "role": role,
            "node": f"synthetic-{role.lower()}",
            "release": "a" * 40,
            "measurements": {
                "duration_minutes": 60,
                "minimum_fps": 40,
                "p95_frame_time_ms": 25.0,
                "maximum_temperature_c": 55.0,
                "throttled": "0x0",
                "cold_boot_pass": True,
                "reboot_pass": True,
                "scenarios_passed": ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"],
                "lighting_modes_passed": ["DAY", "NIGHT"],
                "minimum_touch_px": 44,
                "transport_replay_pass": True,
                "ethernet_fault_recovery_pass": True,
            },
            "artifacts": artifacts,
        })
    return {
        "schema": "avionix.hmi-hardware-results.v1",
        "status": "READY_FOR_ASSEMBLY",
        "operator_confirmed": True,
        "operator": "synthetic-test-only",
        "captured_utc": "2026-09-18T08:30:00+00:00",
        "runs": runs,
    }


class AssembleHmiHardwareEvidenceTests(unittest.TestCase):
    def test_not_run_template_is_safe_but_cannot_be_assembled(self):
        path = ROOT / "config/ui/avionix-hmi-hardware-results-not-run-v1.json"
        data = json.loads(path.read_text(encoding="utf-8"))
        self.assertEqual(validate_results(data, path.parent, allow_not_run=True), [])
        self.assertTrue(any(error.startswith("[HMI-P20-03]") for error in validate_results(data, path.parent)))

    def test_twenty_packaging_controls_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            baseline = ready_results(bundle)

            def mutate(control, data):
                run = data["runs"][0]
                measured = run["measurements"]
                if control == 1: data["schema"] = "unknown"
                elif control == 2: data["unknown"] = True
                elif control == 3: data["operator_confirmed"] = False
                elif control == 4: data["captured_utc"] = "local-time"
                elif control == 5: data["runs"].reverse()
                elif control == 6: run["unknown"] = True
                elif control == 7: run["node"] = ""
                elif control == 8: run["release"] = "short"
                elif control == 9: measured["unknown"] = True
                elif control == 10: measured["duration_minutes"] = 59
                elif control == 11: measured["minimum_fps"] = 39
                elif control == 12: measured["p95_frame_time_ms"] = 25.1
                elif control == 13: measured["maximum_temperature_c"] = True
                elif control == 14: measured["throttled"] = "0x50000"
                elif control == 15: measured["minimum_touch_px"] = 43
                elif control == 16: measured["scenarios_passed"].pop()
                elif control == 17: measured["lighting_modes_passed"].pop()
                elif control == 18: measured["cold_boot_pass"] = False
                elif control == 19: measured["transport_replay_pass"] = False
                elif control == 20: run["artifacts"].pop("SCREENSHOT")

            for control in range(1, 21):
                with self.subTest(control=control):
                    data = copy.deepcopy(baseline)
                    mutate(control, data)
                    marker = f"[HMI-P20-{control:02d}]"
                    self.assertTrue(any(error.startswith(marker) for error in validate_results(data, bundle)), marker)

    def test_assembly_is_validated_hashed_and_immutable(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            results = bundle / "results.json"
            output = bundle / "evidence.json"
            results.write_text(json.dumps(ready_results(bundle)), encoding="utf-8")
            self.assertEqual(assemble(ROOT, results, output), [])
            manifest = json.loads(output.read_text(encoding="utf-8"))
            self.assertTrue(manifest["hardware_verified"])
            self.assertEqual(len(manifest["runs"]), 3)
            self.assertEqual(len(manifest["runs"][0]["artifacts"]), 8)
            self.assertTrue(all(len(item["sha256"]) == 64 for run in manifest["runs"] for item in run["artifacts"]))
            self.assertTrue(any(error.startswith("[HMI-P20-20]") for error in assemble(ROOT, results, output)))


if __name__ == "__main__":
    unittest.main()
