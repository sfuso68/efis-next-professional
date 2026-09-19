import copy
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.ui.validate_hmi_hardware_acceptance import validate


ROOT = Path(__file__).resolve().parents[1]


class HmiHardwareAcceptanceTests(unittest.TestCase):
    def test_repository_plan_is_prepared_but_not_run(self):
        self.assertEqual(validate(ROOT), [])

    def test_all_twenty_controls_fail_closed(self):
        plan_source = ROOT / "config/ui/avionix-hmi-hardware-acceptance-v1.json"
        evidence_source = ROOT / "config/ui/avionix-hmi-hardware-evidence-not-run-v1.json"
        original_plan = json.loads(plan_source.read_text(encoding="utf-8"))
        original_evidence = json.loads(evidence_source.read_text(encoding="utf-8"))

        def mutate(control, plan, evidence):
            if control == 1: plan["schema"] = "unknown"
            elif control == 2: plan["unknown"] = True
            elif control == 3: plan["status"] = "PASS"
            elif control == 4: plan["authority"] = "CONTROL"
            elif control == 5: plan["targets"][2]["platform"] = "RPi5"
            elif control == 6: plan["scenarios"].pop()
            elif control == 7: plan["lighting_modes"] = ["DAY"]
            elif control == 8: plan["acceptance"]["unknown"] = True
            elif control == 9: plan["acceptance"]["minimum_touch_px"] = 43
            elif control == 10: plan["acceptance"]["minimum_fps"] = 39
            elif control == 11: plan["acceptance"]["required_throttled_value"] = "0x50000"
            elif control == 12: plan["duration_minutes"] = 59
            elif control == 13: plan["acceptance"]["transport_replay_required"] = False
            elif control == 14: plan["acceptance"]["ethernet_fault_recovery_required"] = False
            elif control == 15: plan["acceptance"]["operator_visual_signoff_required"] = False
            elif control == 16: plan["acceptance"]["operator_touch_signoff_required"] = False
            elif control == 17: plan["evidence"]["digest"] = "MD5"
            elif control == 18: evidence["schema"] = "unknown"
            elif control == 19: evidence["runs"].pop()
            elif control == 20: evidence["hardware_verified"] = True

        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "config/ui").mkdir(parents=True)
            for control in range(1, 21):
                plan = copy.deepcopy(original_plan)
                evidence = copy.deepcopy(original_evidence)
                mutate(control, plan, evidence)
                plan_path = root / "config/ui/avionix-hmi-hardware-acceptance-v1.json"
                evidence_path = root / "config/ui/avionix-hmi-hardware-evidence-not-run-v1.json"
                plan_path.write_text(json.dumps(plan), encoding="utf-8")
                evidence_path.write_text(json.dumps(evidence), encoding="utf-8")
                marker = f"[HMI-A20-{control:02d}]"
                self.assertTrue(any(error.startswith(marker) for error in validate(root)), marker)

    def test_complete_synthetic_bundle_exercises_pass_validation(self):
        """This verifies the validator, not any real hardware."""
        kinds = ["SCREENSHOT", "FRAME_TIMING", "THERMAL", "TOUCH_LOG", "BOOT_LOG", "TRANSPORT_REPLAY", "FAULT_RECOVERY", "OPERATOR_SIGNOFF"]
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            stamp = datetime.now(timezone.utc).isoformat()
            runs = []
            for role in ("PFD", "MFD", "EMS"):
                artifacts = []
                for kind in kinds:
                    path = bundle / f"{role.lower()}-{kind.lower()}.txt"
                    path.write_text(f"synthetic validator fixture {role} {kind}\n", encoding="utf-8")
                    artifacts.append({"kind": kind, "file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "generated_utc": stamp})
                runs.append({
                    "role": role, "node": f"synthetic-{role.lower()}", "release": "1" * 40, "result": "PASS",
                    "measurements": {
                        "duration_minutes": 60, "minimum_fps": 40,
                        "p95_frame_time_ms": 25.0, "maximum_temperature_c": 55.0,
                        "throttled": "0x0", "cold_boot_pass": True,
                        "reboot_pass": True,
                        "scenarios_passed": ["LIVE", "STALE", "DEGRADED", "LOST", "FAIL", "SIM"],
                        "lighting_modes_passed": ["DAY", "NIGHT"],
                        "minimum_touch_px": 44, "transport_replay_pass": True,
                        "ethernet_fault_recovery_pass": True,
                    }, "artifacts": artifacts,
                })
            manifest = {"schema": "avionix.hmi-hardware-evidence.v1", "status": "PASS", "hardware_verified": True, "flight_approval": False, "generated_utc": stamp, "operator": "synthetic-test-only", "runs": runs}
            path = bundle / "manifest.json"
            path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertEqual(validate(ROOT, path), [])

            manifest["runs"][0]["measurements"]["minimum_fps"] = 39
            path.write_text(json.dumps(manifest), encoding="utf-8")
            self.assertTrue(any(error.startswith("[HMI-A20-20]") for error in validate(ROOT, path)))


if __name__ == "__main__":
    unittest.main()
