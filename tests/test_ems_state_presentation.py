import copy
import json
import unittest
from pathlib import Path

from tools.ui.validate_ems_state_presentation import validate, validate_repository


ROOT = Path(__file__).resolve().parents[1]
BASE = json.loads((ROOT / "config/ui/avionix-ems-state-presentation-v1.json").read_text(encoding="utf-8"))


class EmsStatePresentationTests(unittest.TestCase):
    def test_baseline_and_repository_pass(self):
        self.assertEqual(validate(copy.deepcopy(BASE)), [])
        self.assertEqual(validate_repository(ROOT), [])

    def test_all_twenty_controls_fail_closed(self):
        mutations = []

        def case(control, mutate):
            data = copy.deepcopy(BASE)
            mutate(data)
            mutations.append((control, data))

        case(1, lambda d: d.update(schema="wrong"))
        case(2, lambda d: d.update(status="HARDWARE_ACCEPTED"))
        case(3, lambda d: d.update(platform="RPi5"))
        case(4, lambda d: d.update(resolution=[1024, 600]))
        case(5, lambda d: d["states"].reverse())
        case(6, lambda d: d["states"][0].update(extra=True))
        case(7, lambda d: d["states"][1].update(age_ms=250))
        case(8, lambda d: d["states"][2].update(severity="NORMAL"))
        case(9, lambda d: d["states"][2].update(show_age=False))
        case(10, lambda d: d["states"][1].update(normal_green_allowed=True))
        case(11, lambda d: d["presentation"].update(extra=True))
        case(12, lambda d: d["presentation"]["normal_ranges_hidden_for"].remove("STALE"))
        case(13, lambda d: d["presentation"].update(last_value_policy="HIDE"))
        case(14, lambda d: d["presentation"]["cas_warning_states"].remove("FAIL"))
        case(15, lambda d: d["presentation"]["derived_functions_blocked_for"].remove("FAIL"))
        case(16, lambda d: d["presentation"].update(colour_is_not_only_cue=False))
        case(17, lambda d: d["presentation"].update(sim_always_visible=False))
        case(18, lambda d: d["presentation"].update(no_control_authority=False))
        case(19, lambda d: d["golden"].update(interactive=True))
        case(20, lambda d: d["golden"].update(directory="output"))

        self.assertEqual(len(mutations), 20)
        for control, data in mutations:
            with self.subTest(control=control):
                marker = f"[HMI-E20-{control:02d}]"
                errors = validate(data) if control != 20 else validate_repository(ROOT, data)
                self.assertTrue(any(error.startswith(marker) for error in errors), errors)

    def test_non_object_fails_closed(self):
        self.assertTrue(validate([]))


if __name__ == "__main__":
    unittest.main()
