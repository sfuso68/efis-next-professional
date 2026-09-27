from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/product/validate_product_review_gates.py"
SPEC = importlib.util.spec_from_file_location("validate_product_review_gates", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ProductReviewGateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.documents = {name: json.loads(path.read_text(encoding="utf-8")) for name, path in MODULE.GATES.items()}

    def mutation(self, name: str, mutate) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            paths = dict(MODULE.GATES)
            value = copy.deepcopy(self.documents[name])
            mutate(value)
            path = Path(directory) / f"{name}.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            paths[name] = path
            return MODULE.validate(paths)

    def test_all_five_review_gates_pass_but_remain_no_go(self) -> None:
        report = MODULE.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["review_gates"], 5)
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertFalse(report["deployment_allowed"])
        self.assertFalse(report["flight_approval"])

    def test_intake_digest_drift_and_path_escape_are_rejected(self) -> None:
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-EMS-02"):
            self.mutation("engine", lambda value: value.__setitem__("intake_sha256", "0" * 64))
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-SKU-02"):
            self.mutation("sku", lambda value: value.__setitem__("intake_path", "../sku.json"))

    def test_evidence_and_reviewers_cannot_be_claimed_early(self) -> None:
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-FPV-01"):
            self.mutation("fpv", lambda value: value.__setitem__("evidence_bundle_sha256", "a" * 64))
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-VOI-01"):
            self.mutation("voice", lambda value: value["reviewed_by"].append("pilot"))

    def test_engine_activation_is_fail_closed(self) -> None:
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-EMS-04"):
            self.mutation("engine", lambda value: value.__setitem__("activation_allowed", True))

    def test_sku_deployment_is_fail_closed(self) -> None:
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-SKU-04"):
            self.mutation("sku", lambda value: value.__setitem__("deployment_allowed", True))

    def test_fpv_cannot_bridge_domains_or_release_video(self) -> None:
        for key in ("network_bridge_allowed", "video_domain_release_allowed"):
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ReviewGateError, "PR-FPV-04"):
                self.mutation("fpv", lambda value, key=key: value.__setitem__(key, True))

    def test_voice_cannot_transmit_or_enable_operation(self) -> None:
        for key in ("automatic_transmission_allowed", "operational_enable_allowed"):
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ReviewGateError, "PR-VOI-04"):
                self.mutation("voice", lambda value, key=key: value.__setitem__(key, True))

    def test_hil_cannot_reach_production_or_actuators(self) -> None:
        for key in ("execution_allowed", "production_network_allowed", "production_storage_allowed", "actuator_output_allowed", "flight_control_commands_allowed"):
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.ReviewGateError, "PR-HIL-04"):
                self.mutation("hil", lambda value, key=key: value.__setitem__(key, True))

    def test_subject_sets_are_frozen_to_upstream_intakes(self) -> None:
        with self.assertRaisesRegex(MODULE.ReviewGateError, "PR-HIL-03"):
            self.mutation("hil", lambda value: value["subjects"].pop())


if __name__ == "__main__":
    unittest.main()
