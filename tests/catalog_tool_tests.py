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

    def test_operational_dictionary_has_complete_control_domains(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        definitions, errors = MODULE.flatten_parameter_dictionary(dictionary)
        self.assertEqual([], errors)
        self.assertGreaterEqual(len(definitions), 400)
        domains = {definition["domain"] for definition in definitions}
        self.assertTrue({"speeds", "performance", "engine", "electrical", "emergency_procedures", "maintenance", "data_governance", "custom"}.issubset(domains))
        self.assertEqual(len(definitions), len({definition["key"] for definition in definitions}))

    def test_operational_profiles_never_silently_omit_parameters(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        seed = MODULE.read_operational_seed(ROOT / "data/catalog/seed/operational-seed-v2.json")
        definitions, _ = MODULE.flatten_parameter_dictionary(dictionary)
        root = seed["profiles"][0]
        child = seed["profiles"][1]
        root_values, errors = MODULE.materialize_operational_profile(dictionary, root)
        self.assertEqual([], errors)
        self.assertEqual(len(definitions), len(root_values))
        self.assertEqual("UNKNOWN", root_values["speeds.vne"]["disposition"])
        child_values, errors = MODULE.materialize_operational_profile(dictionary, child)
        self.assertEqual([], errors)
        self.assertEqual(len(definitions), len(child_values))
        self.assertEqual("INHERIT", child_values["speeds.vne"]["disposition"])
        self.assertEqual("UNKNOWN", child_values["identity.registration"]["disposition"])

    def test_parameter_register_lists_every_dictionary_key(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        definitions, _ = MODULE.flatten_parameter_dictionary(dictionary)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "register.md"
            self.assertEqual(len(definitions), MODULE.write_parameter_register(dictionary, output))
            register = output.read_text(encoding="utf-8")
            for definition in definitions:
                self.assertIn(f"`{definition['key']}`", register)

    def test_operational_profile_rejects_unknown_key_and_bad_na(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        seed = MODULE.read_operational_seed(ROOT / "data/catalog/seed/operational-seed-v2.json")
        profile = json.loads(json.dumps(seed["profiles"][0]))
        profile["values"]["invented.unsafe"] = {"disposition": "VALUE", "value": 1}
        profile["values"]["flaps.installed"] = {"disposition": "NOT_INSTALLED"}
        errors, report = MODULE.validate_operational_profile(dictionary, profile)
        self.assertTrue(any("unknown operational parameter" in error for error in errors))
        self.assertTrue(any("requires rationale" in error for error in errors))
        self.assertEqual("FAIL", report["result"])

    def test_simulation_profile_cannot_be_promoted_with_unresolved_limits(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        seed = MODULE.read_operational_seed(ROOT / "data/catalog/seed/operational-seed-v2.json")
        profile = json.loads(json.dumps(seed["profiles"][0]))
        profile["lifecycle_state"] = "VERIFIED"
        errors, report = MODULE.validate_operational_profile(dictionary, profile)
        self.assertTrue(any("unresolved required parameter" in error for error in errors))
        self.assertGreater(report["safety_blockers"], 0)
        self.assertFalse(report["release_ready"])

    def test_complete_simulation_profile_is_explicit_and_cannot_be_promoted(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        definitions, _ = MODULE.flatten_parameter_dictionary(dictionary)
        seed = MODULE.read_operational_seed(ROOT / "data/catalog/seed/operational-seed-v2.json")
        profile = next(item for item in seed["profiles"] if item["profile_key"] == "simulation.asso-v-jolly-912-ul")
        self.assertEqual({item["key"] for item in definitions}, set(profile["values"]))
        self.assertTrue(all(item["disposition"] == "VALUE" for item in profile["values"].values()))
        self.assertTrue(all(item["simulated"] is True for item in profile["values"].values()))
        self.assertTrue(all(item["approval_status"] == "INDICATIVE" for item in profile["values"].values()))
        errors, report = MODULE.validate_operational_profile(dictionary, profile, seed["profiles"][0])
        self.assertEqual([], errors)
        self.assertEqual(len(definitions), report["simulated_values"])
        self.assertEqual(0, report["safety_blockers"])
        self.assertFalse(report["release_ready"])
        promoted = json.loads(json.dumps(profile))
        promoted["lifecycle_state"] = "VERIFIED"
        errors, report = MODULE.validate_operational_profile(dictionary, promoted, seed["profiles"][0])
        self.assertIn("simulation-only profile cannot enter VERIFIED", errors)
        self.assertTrue(any("simulated value cannot enter VERIFIED" in error for error in errors))
        self.assertFalse(report["release_ready"])

    def test_v2_build_materializes_every_profile_field(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        definitions, _ = MODULE.flatten_parameter_dictionary(dictionary)
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "catalog.db"
            MODULE.build_catalog(
                ROOT / "data/catalog/schema/001_initial.sql",
                ROOT / "data/catalog/seed/catalog-seed.json",
                output,
                operational_schema=ROOT / "data/catalog/schema/002_operational_parameters.sql",
                dictionary_path=ROOT / "data/catalog/operational-parameter-dictionary-v2.json",
                operational_seed_path=ROOT / "data/catalog/seed/operational-seed-v2.json",
            )
            index_path = MODULE.write_catalog_index(output)
            db = sqlite3.connect(output)
            try:
                self.assertEqual("ok", db.execute("PRAGMA integrity_check").fetchone()[0])
                self.assertEqual(len(definitions), db.execute("SELECT count(*) FROM operational_parameter_definitions").fetchone()[0])
                self.assertEqual(3, db.execute("SELECT count(*) FROM operational_profiles").fetchone()[0])
                self.assertEqual(3 * len(definitions), db.execute("SELECT count(*) FROM operational_parameter_values").fetchone()[0])
                self.assertEqual(3, db.execute("SELECT count(*) FROM operational_validation_runs WHERE result='PASS'").fetchone()[0])
                self.assertEqual(1, db.execute("SELECT count(*) FROM operational_validation_runs WHERE safety_blockers=0").fetchone()[0])
                self.assertEqual(len(definitions), db.execute("SELECT count(*) FROM operational_parameter_values WHERE is_simulated=1").fetchone()[0])
                self.assertEqual(1, db.execute("SELECT count(*) FROM operational_parameter_values v JOIN operational_profiles p ON p.id=v.profile_id WHERE p.profile_key='aircraft.asso-v-jolly-owner' AND v.parameter_key='identity.registration' AND v.disposition='UNKNOWN'").fetchone()[0])
            finally:
                db.close()
            index = json.loads(index_path.read_text(encoding="utf-8"))
            self.assertEqual(28, len(index["operational_domains"]))
            self.assertEqual(len(definitions), len(index["operational_parameters"]))
            self.assertEqual(3, len(index["operational_profiles"]))
            self.assertTrue(all(len(profile["parameters"]) == len(definitions) for profile in index["operational_profiles"]))
            installed = next(profile for profile in index["operational_profiles"] if profile["profile_key"] == "aircraft.asso-v-jolly-owner")
            registration = next(value for value in installed["parameters"] if value["parameter_key"] == "identity.registration")
            self.assertEqual("UNKNOWN", registration["effective_disposition"])
            self.assertFalse(installed["validation"]["release_ready"])
            simulation = next(profile for profile in index["operational_profiles"] if profile["profile_key"] == "simulation.asso-v-jolly-912-ul")
            self.assertEqual(len(definitions), simulation["validation"]["simulated_values"])
            self.assertTrue(all(value["effective_simulated"] for value in simulation["parameters"]))
            self.assertFalse(simulation["validation"]["release_ready"])

    def test_failed_v2_build_preserves_previous_catalog(self):
        dictionary = json.loads((ROOT / "data/catalog/operational-parameter-dictionary-v2.json").read_text(encoding="utf-8"))
        dictionary["policy"]["no_silent_omissions"] = False
        with tempfile.TemporaryDirectory() as directory:
            temporary = Path(directory)
            output = temporary / "catalog.db"
            invalid_dictionary = temporary / "invalid-dictionary.json"
            output.write_bytes(b"PREVIOUS-CATALOG")
            invalid_dictionary.write_text(json.dumps(dictionary), encoding="utf-8")
            with self.assertRaises(ValueError):
                MODULE.build_catalog(
                    ROOT / "data/catalog/schema/001_initial.sql",
                    ROOT / "data/catalog/seed/catalog-seed.json",
                    output,
                    operational_schema=ROOT / "data/catalog/schema/002_operational_parameters.sql",
                    dictionary_path=invalid_dictionary,
                    operational_seed_path=ROOT / "data/catalog/seed/operational-seed-v2.json",
                )
            self.assertEqual(b"PREVIOUS-CATALOG", output.read_bytes())
            self.assertFalse((temporary / "catalog.db.building").exists())


if __name__ == "__main__":
    unittest.main()
