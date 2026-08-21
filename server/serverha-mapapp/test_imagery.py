import tempfile
import unittest
from pathlib import Path

import imagery


class ImageryMissionTest(unittest.TestCase):
    def test_treviso_area(self):
        result = imagery.area_plan({"name":"Treviso","center":[45.648399,12.1944],"radius_nm":25,"zooms":[11,12]})
        self.assertEqual(result["selection_mode"], "AREA")
        self.assertGreater(result["tile_count"], 0)

    def test_route(self):
        result = imagery.route_plan({"name":"LIPH-LIDB","points":[
            {"id":"LIPH","lat":45.648399,"lon":12.1944},
            {"id":"LIDB","lat":46.166549,"lon":12.250389}],"zooms":[11,12],"corridor_nm":8})
        self.assertEqual(result["selection_mode"], "FLIGHT_PLAN")
        self.assertEqual(result["corridor_nm"], 8)

    def test_outside_coverage_rejected(self):
        self.assertIn("error", imagery.area_plan({"bbox":[1,1,2,2]}))

    def test_unknown_dataset_is_not_served(self):
        self.assertIsNone(imagery.manifest_file("../secret"))
        self.assertIsNone(imagery.tile_file("../secret", 12, 1, 1))

    def test_hybrid_style_orders_raster_below_aeronautical_overlay(self):
        style = imagery.hybrid_style()
        identifiers = [layer["id"] for layer in style["layers"]]
        self.assertLess(identifiers.index("sentinel"), identifiers.index("airspace-boundary"))
        self.assertEqual(style["sources"]["sentinel"]["tileSize"], 512)

    def test_plan_excludes_tiles_missing_from_active_inventory(self):
        with tempfile.TemporaryDirectory() as directory:
            previous = imagery.IMAGERY_ROOT
            imagery.IMAGERY_ROOT = directory
            try:
                tile = Path(directory) / "tiles" / imagery.DEFAULT_DATASET / "11" / "1093" / "727.webp"
                tile.parent.mkdir(parents=True)
                tile.write_bytes(b"RIFF....WEBP")
                result = imagery._finish("TEST", "inventory", {(11, 1093, 727), (11, 1092, 727)}, [11])
                self.assertEqual(result["requested_tile_count"], 2)
                self.assertEqual(result["tile_count"], 1)
                self.assertEqual(result["coverage_gap_tile_count"], 1)
                self.assertFalse(result["coverage_complete"])
            finally:
                imagery.IMAGERY_ROOT = previous

    def test_atomic_save_and_list(self):
        with tempfile.TemporaryDirectory() as directory:
            previous = imagery.PACKAGES
            imagery.PACKAGES = directory
            try:
                package = imagery.area_plan({"name":"Test Area","center":[45.65,12.19],"radius_nm":2,"zooms":[11]})
                saved = imagery.save(package)
                self.assertEqual(saved["package_id"], "test-area")
                self.assertEqual(len(imagery.packages()), 1)
                self.assertTrue((Path(directory)/"test-area.json").is_file())
            finally:
                imagery.PACKAGES = previous


if __name__ == "__main__": unittest.main()
