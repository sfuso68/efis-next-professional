import copy
import json
import unittest

from tools.simulation import check_development_plan as plan


class DevelopmentPlanTests(unittest.TestCase):
    def setUp(self):
        self.baseline = plan.load()

    def test_baseline_is_planning_not_acceptance(self):
        result = plan.validate(self.baseline)
        self.assertEqual(result["domains"], 14)
        self.assertEqual(result["required_scenarios"], 16)
        self.assertEqual(result["simulator_acceptance"], "NOT_COMPLETE")
        self.assertTrue((plan.ROOT / self.baseline["specification"]).is_file())

    def test_authority_escalation_rejected(self):
        for key in ("flight_approval", "deployment_authorized"):
            for value in (True, 0, "false", None):
                with self.subTest(key=key, value=value):
                    mutated = copy.deepcopy(self.baseline)
                    mutated[key] = value
                    with self.assertRaises(ValueError):
                        plan.validate(mutated)

    def test_readiness_claims_rejected(self):
        for key, value in (("completion", "COMPLETE"), ("model_validation", "PASS"),
                           ("hardware_acceptance", "PASS")):
            mutated = copy.deepcopy(self.baseline)
            mutated[key] = value
            with self.assertRaises(ValueError):
                plan.validate(mutated)
        self.baseline["packages"][0]["state"] = "DONE"
        with self.assertRaises(ValueError):
            plan.validate(self.baseline)

    def test_cycles_missing_and_duplicate_dependencies_rejected(self):
        for deps in (["P02"], ["MISSING"], ["P01", "P01"], []):
            mutated = copy.deepcopy(self.baseline)
            mutated["packages"][1]["depends_on"] = deps
            with self.assertRaises(ValueError):
                plan.validate(mutated)

    def test_module_or_acceptance_omission_rejected(self):
        mutated = copy.deepcopy(self.baseline)
        del mutated["coverage"]["EMS"]
        with self.assertRaises(ValueError):
            plan.validate(mutated)
        self.baseline["coverage"]["EMS"].remove("P12")
        with self.assertRaises(ValueError):
            plan.validate(self.baseline)

    def test_missing_scenario_or_open_gate_rejected(self):
        for key in ("scenario_ids", "open_gates"):
            mutated = copy.deepcopy(self.baseline)
            mutated[key].pop()
            with self.assertRaises(ValueError):
                plan.validate(mutated)

    def test_unknown_fields_and_duplicate_json_keys_rejected(self):
        self.baseline["auto_deploy"] = True
        with self.assertRaises(ValueError):
            plan.validate(self.baseline)
        with self.assertRaises(ValueError):
            json.loads('{"state":"TODO","state":"DONE"}', object_pairs_hook=plan.unique_object)

    def test_bad_shapes_rejected(self):
        for key in ("packages", "coverage", "scenario_ids", "open_gates"):
            mutated = copy.deepcopy(self.baseline)
            mutated[key] = None
            with self.assertRaises(ValueError):
                plan.validate(mutated)


if __name__ == "__main__":
    unittest.main()
