from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/product/validate_product_tools_batch.py"
SPEC = importlib.util.spec_from_file_location("validate_product_tools_batch", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class ProductToolsBatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.documents = {name: json.loads(path.read_text(encoding="utf-8")) for name, path in MODULE.DEFAULT_PATHS.items()}

    def validate_mutation(self, name: str, mutate) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            paths = dict(MODULE.DEFAULT_PATHS)
            value = copy.deepcopy(self.documents[name])
            mutate(value)
            path = Path(directory) / f"{name}.json"
            path.write_text(json.dumps(value), encoding="utf-8")
            paths[name] = path
            return MODULE.validate(paths)

    def test_repository_registries_pass_but_remain_not_run(self) -> None:
        report = MODULE.validate()
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["execution_status"], "NOT_RUN")
        self.assertEqual(report["registries"], 5)
        self.assertEqual(report["engines"], 2)
        self.assertEqual(report["profiles"], 2)
        self.assertEqual(report["fpv_feeds"], 4)
        self.assertEqual(report["approved_phraseology_packs"], 0)
        self.assertEqual(report["hil_scenarios"], 6)

    def test_unreviewed_engine_cannot_be_activated(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-EMS-01"):
            self.validate_mutation("engine", lambda value: value.__setitem__("activation_allowed", True))

    def test_engine_limits_cannot_exist_without_sources(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-EMS-05"):
            self.validate_mutation("engine", lambda value: value["engines"][0]["limits"].append({"id": "RPM", "max": 5800}))

    def test_product_profile_cannot_switch_in_flight_or_allow_unknown_features(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-SKU-03"):
            self.validate_mutation("sku", lambda value: value.__setitem__("inflight_profile_switch_allowed", True))
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-SKU-03"):
            self.validate_mutation("sku", lambda value: value.__setitem__("unknown_capability_policy", "ALLOW"))

    def test_product_profile_requires_shared_health_logging_and_setup(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-SKU-05"):
            self.validate_mutation("sku", lambda value: value["profiles"][0]["capabilities"].remove("HEALTH_CAS"))

    def test_fpv_cannot_bridge_or_claim_unmeasured_performance(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-FPV-03"):
            self.validate_mutation("fpv", lambda value: value.__setitem__("network_bridge_allowed", True))
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-FPV-06"):
            self.validate_mutation("fpv", lambda value: value["feeds"][0].__setitem__("measured_bitrate_mbps", 8.0))

    def test_voice_cannot_transmit_or_claim_unreviewed_pack(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-VOI-03"):
            self.validate_mutation("voice", lambda value: value.__setitem__("automatic_transmission_allowed", True))
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-VOI-04"):
            self.validate_mutation("voice", lambda value: value["approved_packs"].append({"pack_id": "fake"}))

    def test_hil_cannot_target_production_or_actuators(self) -> None:
        for key in ("production_network_allowed", "production_storage_allowed", "actuator_output_allowed", "flight_control_command_allowed"):
            with self.subTest(key=key), self.assertRaisesRegex(MODULE.RegistryError, "PT-HIL-03"):
                self.validate_mutation("hil", lambda value, key=key: value.__setitem__(key, True))

    def test_hil_scenario_ids_and_seeds_are_unique(self) -> None:
        with self.assertRaisesRegex(MODULE.RegistryError, "PT-HIL-05"):
            self.validate_mutation("hil", lambda value: value["scenarios"][1].__setitem__("seed", value["scenarios"][0]["seed"]))


if __name__ == "__main__":
    unittest.main()
