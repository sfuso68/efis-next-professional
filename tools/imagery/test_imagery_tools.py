#!/usr/bin/env python3
import base64
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location("planner", ROOT / "plan_route_cache.py")
planner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(planner)
sync_spec = importlib.util.spec_from_file_location("sync", ROOT / "sync_imagery_cache.py")
sync = importlib.util.module_from_spec(sync_spec)
sync_spec.loader.exec_module(sync)
finalize_spec = importlib.util.spec_from_file_location("finalize", ROOT / "finalize_imagery_package.py")
finalize = importlib.util.module_from_spec(finalize_spec)
finalize_spec.loader.exec_module(finalize)


class ImageryPlannerTest(unittest.TestCase):
    def test_machine_credentials_create_basic_auth_header(self):
        header = sync.authorization_header("efis-pfd-01", "secret")
        scheme, encoded = header.split(" ", 1)
        self.assertEqual(scheme, "Basic")
        self.assertEqual(base64.b64decode(encoded).decode(), "efis-pfd-01:secret")
        self.assertIsNone(sync.authorization_header(None, None))
        with self.assertRaises(ValueError):
            sync.authorization_header("efis-pfd-01", None)

    def test_relative_tile_url_is_resolved_against_server(self):
        self.assertEqual(
            sync.resolved_url("/api/imagery/tile/dataset/14/1/2.webp", "https://mappe.casa.lan"),
            "https://mappe.casa.lan/api/imagery/tile/dataset/14/1/2.webp",
        )
        with self.assertRaises(ValueError):
            sync.resolved_url("/api/imagery/tile/dataset/14/1/2.webp")

    def test_server_path_is_accepted_as_safe_cache_path(self):
        entry = {"path": "dataset/14/1/2.webp"}
        self.assertEqual(str(sync.entry_cache_path(entry)).replace("\\", "/"), entry["path"])
        with self.assertRaises(ValueError):
            sync.entry_cache_path({"path": "../secret"})

    def test_complete_cache_is_published_offline_ready(self):
        import tempfile
        with tempfile.TemporaryDirectory() as directory:
            cache = Path(directory)
            tile = cache / "dataset/14/1/2.webp"
            tile.parent.mkdir(parents=True)
            tile.write_bytes(b"RIFF0000WEBPpayload")
            plan = {"dataset_id": "dataset", "selection_id": "route", "tiles": [{"path": "dataset/14/1/2.webp"}]}
            report = {"requested": 1, "failed": 0, "bytes": tile.stat().st_size}
            status = finalize.build_status(plan, report, cache)
            self.assertTrue(status["offline_ready"])
            self.assertEqual(status["state"], "OFFLINE_READY")

    def test_web_mercator_known_origin(self):
        self.assertEqual(planner.tile_xy(0.0, 0.0, 1), (1, 1))

    def test_route_plan_includes_endpoints(self):
        route = [(45.648399, 12.1944), (46.166549, 12.250389)]
        tiles = planner.route_tiles(route, 12, 2.0)
        self.assertIn((12, *planner.tile_xy(*route[0], 12)), tiles)
        self.assertIn((12, *planner.tile_xy(*route[-1], 12)), tiles)

    def test_corridor_expands_plan(self):
        route = [(45.65, 12.19), (46.16, 12.25)]
        self.assertGreater(len(planner.route_tiles(route, 13, 8.0)),
                           len(planner.route_tiles(route, 13, 2.0)))


if __name__ == "__main__":
    unittest.main()
