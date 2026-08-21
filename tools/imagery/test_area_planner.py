#!/usr/bin/env python3
import importlib.util
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT))
spec = importlib.util.spec_from_file_location("area", ROOT / "plan_area_cache.py")
area = importlib.util.module_from_spec(spec)
spec.loader.exec_module(area)


class AreaPlannerTest(unittest.TestCase):
    def test_circle_bbox_contains_centre(self):
        west, south, east, north = area.circle_bbox(45.65, 12.19, 25)
        self.assertLess(west, 12.19)
        self.assertGreater(east, 12.19)
        self.assertLess(south, 45.65)
        self.assertGreater(north, 45.65)

    def test_larger_bounds_produce_more_tiles(self):
        small = set(area.tile_bounds(12.0, 45.5, 12.2, 45.7, 12))
        large = set(area.tile_bounds(12.0, 45.5, 12.5, 46.0, 12))
        self.assertGreater(len(large), len(small))


if __name__ == "__main__":
    unittest.main()
