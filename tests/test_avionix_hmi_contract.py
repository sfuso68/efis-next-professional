import copy
import json
import unittest
from pathlib import Path

from tools.ui.validate_hmi_contract import validate, validate_repository


ROOT = Path(__file__).resolve().parents[1]
BASE = json.loads((ROOT / "config/ui/avionix-hmi-v1.json").read_text(encoding="utf-8"))


class AvionixHmiContractTests(unittest.TestCase):
    def test_baseline_passes(self):
        self.assertEqual(validate(copy.deepcopy(BASE)), [])

    def test_repository_components_are_registered(self):
        self.assertEqual(validate_repository(ROOT), [])

    def test_platform_swap_fails(self):
        data = copy.deepcopy(BASE)
        data["targets"][0]["platform"] = "RPi4"
        self.assertTrue(validate(data))

    def test_missing_sim_state_fails(self):
        data = copy.deepcopy(BASE)
        data["data_states"].remove("SIM")
        self.assertTrue(validate(data))

    def test_small_touch_target_fails(self):
        data = copy.deepcopy(BASE)
        data["design_tokens"]["minimum_touch_px"] = 32
        self.assertTrue(validate(data))

    def test_missing_vne_fails(self):
        data = copy.deepcopy(BASE)
        data["skyview"]["v_speed_labels"].remove("VNE")
        self.assertTrue(validate(data))

    def test_command_authority_fails(self):
        data = copy.deepcopy(BASE)
        data["degraded_mode"]["no_control_authority"] = False
        self.assertTrue(validate(data))


if __name__ == "__main__":
    unittest.main()
