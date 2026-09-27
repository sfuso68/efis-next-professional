from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/product/validate_product_evidence_batch.py"
SPEC = importlib.util.spec_from_file_location("validate_product_evidence_batch", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ProductEvidenceBatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.documents = {name: json.loads(path.read_text(encoding="utf-8")) for name, path in MODULE.INTAKES.items()}

    def mutation(self, name: str, mutate) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            paths = dict(MODULE.INTAKES)
            value = copy.deepcopy(self.documents[name])
            mutate(value)
            path = Path(directory) / f"{name}.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            paths[name] = path
            return MODULE.validate(paths)

    def test_all_five_intakes_pass_but_remain_not_run(self) -> None:
        report = MODULE.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["intakes"], 5)
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertFalse(report["hardware_verified"])
        self.assertFalse(report["flight_approval"])

    def test_registry_digest_drift_is_rejected(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-EMS-02"):
            self.mutation("engine", lambda value: value.__setitem__("registry_sha256", "0" * 64))

    def test_engine_cannot_claim_artifacts_or_activation(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-EMS-03"):
            self.mutation("engine", lambda value: value["engines"][0]["artifacts"].append("photo.jpg"))
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-EMS-05"):
            self.mutation("engine", lambda value: value.__setitem__("activation_allowed", True))

    def test_sku_cannot_claim_approval(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-SKU-05"):
            self.mutation("sku", lambda value: value.__setitem__("approved", True))

    def test_fpv_requires_four_empty_feeds_and_isolation(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-FPV-03"):
            self.mutation("fpv", lambda value: value["feeds"].pop())
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-FPV-06"):
            self.mutation("fpv", lambda value: value.__setitem__("network_bridge_enabled", True))

    def test_voice_stays_offline_and_unapproved(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-VOI-05"):
            self.mutation("voice", lambda value: value.__setitem__("automatic_transmission_allowed", True))
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-VOI-06"):
            self.mutation("voice", lambda value: value.__setitem__("approved", True))

    def test_hil_cannot_use_production_or_actuators(self) -> None:
        for key in ("production_network_used", "production_storage_used", "actuator_output_enabled", "flight_control_commands_enabled"):
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-HIL-05"):
                self.mutation("hil", lambda value, key=key: value.__setitem__(key, True))

    def test_hil_input_digest_cannot_be_asserted_in_template(self) -> None:
        with self.assertRaisesRegex(MODULE.EvidenceIntakeError, "PE-HIL-04"):
            self.mutation("hil", lambda value: value["scenarios"][0].__setitem__("input_sha256", "a" * 64))


if __name__ == "__main__":
    unittest.main()
