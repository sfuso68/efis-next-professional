import importlib.util
import json
import sqlite3
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("catalog_tool", ROOT / "tools/catalog/catalog_tool.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class CatalogTests(unittest.TestCase):
    def test_active_profile_is_valid(self):
        profile = json.loads((ROOT / "config/active/aircraft-profile.json").read_text(encoding="utf-8"))
        self.assertEqual([], MODULE.validate_profile(profile))

    def test_invalid_speed_order_is_rejected(self):
        profile = json.loads((ROOT / "config/active/aircraft-profile.json").read_text(encoding="utf-8"))
        profile["speeds_kt"]["VNE"] = 100
        self.assertTrue(any("VNO < VNE" in error for error in MODULE.validate_profile(profile)))

    def test_seed_builds_integral_catalog(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "catalog.db"
            MODULE.build_catalog(ROOT / "data/catalog/schema/001_initial.sql", ROOT / "data/catalog/seed/catalog-seed.json", output)
            index_path = MODULE.write_catalog_index(output)
            db = sqlite3.connect(output)
            try:
                self.assertEqual("ok", db.execute("PRAGMA integrity_check").fetchone()[0])
                self.assertEqual(1, db.execute("SELECT count(*) FROM aircraft").fetchone()[0])
                self.assertEqual(10, db.execute("SELECT count(*) FROM aircraft_speeds").fetchone()[0])
            finally:
                db.close()
            index = json.loads(index_path.read_text(encoding="utf-8"))
            self.assertEqual(1, len(index["aircraft"]))
            self.assertEqual(140, index["aircraft"][0]["speeds_kt"]["VNE"])

    def test_legacy_catalog_is_normalized(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "catalog.db"
            report = MODULE.build_catalog(ROOT / "data/catalog/schema/001_initial.sql", ROOT / "data/catalog/seed/catalog-seed.json", output, ROOT / "data/catalog/seed/legacy-catalog-import.json")
            self.assertEqual(69, report["aircraft"])
            db = sqlite3.connect(output)
            try:
                self.assertEqual(70, db.execute("SELECT count(*) FROM aircraft").fetchone()[0])
                self.assertGreater(db.execute("SELECT count(*) FROM engines").fetchone()[0], 16)
                self.assertEqual(69, db.execute("SELECT count(*) FROM aircraft WHERE status='INDICATIVE' AND model!='LSA SIM'").fetchone()[0])
            finally:
                db.close()


if __name__ == "__main__":
    unittest.main()
