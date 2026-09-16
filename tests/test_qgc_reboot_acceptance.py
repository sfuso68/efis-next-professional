from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_reboot_acceptance.py"
SPEC = importlib.util.spec_from_file_location("qgc_reboot_acceptance", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def status(*, speech_muted: bool = True) -> dict[str, object]:
    checks = {name: True for name in MODULE.REQUIRED_STATUS_CHECKS}
    checks["speech_output_muted"] = speech_muted
    checks.update(
        {"firewall_loaded": False, "firewall_enabled": False, "firewall_active": False}
    )
    return {"status": "PARTIAL", "checks": checks}


class QgcRebootAcceptanceTests(unittest.TestCase):
    def test_arm_passes_despite_separate_firewall_gate(self) -> None:
        report = MODULE.evaluate(
            phase="arm",
            baseline_boot_id="boot-a",
            current_boot_id="boot-a",
            status=status(),
            is_linger_disabled=True,
        )
        self.assertEqual(report["status"], "PASS")
        self.assertNotIn("reboot_observed", report["checks"])

    def test_check_requires_changed_boot_id(self) -> None:
        report = MODULE.evaluate(
            phase="check",
            baseline_boot_id="boot-a",
            current_boot_id="boot-a",
            status=status(),
            is_linger_disabled=True,
        )
        self.assertEqual(report["status"], "FAIL")
        self.assertFalse(report["checks"]["reboot_observed"])

    def test_check_passes_after_reboot_with_all_operational_gates(self) -> None:
        report = MODULE.evaluate(
            phase="check",
            baseline_boot_id="boot-a",
            current_boot_id="boot-b",
            status=status(),
            is_linger_disabled=True,
        )
        self.assertEqual(report["status"], "PASS")

    def test_unmuted_speech_or_linger_fails(self) -> None:
        speech = MODULE.evaluate(
            phase="check",
            baseline_boot_id="boot-a",
            current_boot_id="boot-b",
            status=status(speech_muted=False),
            is_linger_disabled=True,
        )
        linger = MODULE.evaluate(
            phase="check",
            baseline_boot_id="boot-a",
            current_boot_id="boot-b",
            status=status(),
            is_linger_disabled=False,
        )
        self.assertEqual(speech["status"], "FAIL")
        self.assertEqual(linger["status"], "FAIL")


if __name__ == "__main__":
    unittest.main()
