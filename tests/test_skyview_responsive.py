import copy
import json
import unittest
from pathlib import Path

from tools.ui.validate_skyview_responsive import validate, validate_repository


ROOT = Path(__file__).resolve().parents[1]
BASE = json.loads((ROOT / "config/ui/avionix-skyview-responsive-v1.json").read_text(encoding="utf-8"))


class SkyViewResponsiveTests(unittest.TestCase):
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
        case(2, lambda d: d["resolutions"].reverse())
        case(3, lambda d: d.update(compact_max_width_px=1024))
        case(4, lambda d: d.update(fma_height_logical_px=57))
        case(5, lambda d: d.update(tape_vertical_margin_logical_px=8))
        case(6, lambda d: d.update(weather_right_margin_logical_px=4))
        case(7, lambda d: d["compact_weather"].update(width_logical_px=160))
        case(8, lambda d: d["compact_weather"].update(top_margin_logical_px=9))
        case(9, lambda d: d["compact_weather"].update(badge_height_logical_px=34))
        case(10, lambda d: d["compact_weather"].update(legend_visible=True))
        case(11, lambda d: d["compact_weather"].update(no_data_integrated=False))
        case(12, lambda d: d["regular_weather"].update(height_logical_px=64))
        case(13, lambda d: d["regular_weather"].update(top_margin_logical_px=9))
        case(14, lambda d: d["regular_weather"].update(badge_height_logical_px=44))
        case(15, lambda d: d["regular_weather"].update(legend_visible=False))
        case(16, lambda d: d["regular_weather"].update(no_data_gap_logical_px=0))
        case(17, lambda d: d.update(minimum_heading_clearance_logical_px=0))
        case(18, lambda d: d.update(attribution_visible=False))
        case(19, lambda d: d.update(golden_interactive=True))
        case(20, lambda d: d.update(no_control_authority=False))

        self.assertEqual(len(mutations), 20)
        for control, data in mutations:
            with self.subTest(control=control):
                marker = f"[HMI-R20-{control:02d}]"
                self.assertTrue(any(error.startswith(marker) for error in validate(data)))

    def test_non_object_fails_closed(self):
        self.assertTrue(validate([]))


if __name__ == "__main__":
    unittest.main()
