"""Explicit real-JSBSim suite: absence of the backend fails, never skips."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tools.simulation import jsbsim_session as s
from tests.test_sil_world import config


class PhysicalWorldTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        cls.addClassCleanup(cls.tmp.cleanup)
        cls.weather = cls.root/"weather"
        cls.repeat = cls.root/"repeat"
        cls.calm = cls.root/"calm"
        cls.config = config()
        s.run_session(cls.weather, "cruise", 60, "weather-test", world_config=cls.config)
        s.run_session(cls.repeat, "cruise", 60, "weather-test", world_config=cls.config)
        calm = copy.deepcopy(cls.config)
        calm["atmosphere"].update(gust_kt=0, vertical_gust_kt=0)
        for layer in calm["atmosphere"]["wind_layers"]:
            layer.update(north_kt=0, east_kt=0)
        s.run_session(cls.calm, "cruise", 60, "calm-test", world_config=calm)
        cls.frames = [json.loads(line) for line in (cls.weather/"frames.jsonl").read_text().splitlines()]
        cls.calm_frames = [json.loads(line) for line in (cls.calm/"frames.jsonl").read_text().splitlines()]

    def test_real_backend_world_and_artifact_verifier(self):
        self.assertEqual(s.verify_session(self.weather)["frames"], 1201)
        self.assertEqual(s.verify_session(self.calm)["frames"], 1201)
        self.assertTrue(all(f["world"]["sim_time_ms"] == f["sim_time_ms"] for f in self.frames))
        self.assertTrue(all(f["world"]["simulated"] and not f["world"]["flight_approval"] for f in self.frames))

    def test_weather_replay_byte_identical(self):
        self.assertEqual((self.weather/"frames.jsonl").read_bytes(), (self.repeat/"frames.jsonl").read_bytes())
        self.assertEqual((self.weather/"sim-radar.png").read_bytes(), (self.repeat/"sim-radar.png").read_bytes())

    def test_wind_changes_actual_fdm_not_only_metadata(self):
        wind, calm = self.frames[1000], self.calm_frames[1000]
        self.assertGreater(abs(wind["flight"]["gs_kt"]["value"]-calm["flight"]["gs_kt"]["value"]), 3)
        self.assertNotEqual(wind["flight"]["longitude_deg"]["value"], calm["flight"]["longitude_deg"]["value"])

    def test_physical_atmosphere_and_altimeter(self):
        for f in self.frames:
            a = f["world"]["truth_weather"]
            self.assertTrue(700 < a["pressure_hpa"] < 950)
            self.assertAlmostEqual(a["temperature_c"], 18-.0065*f["flight"]["altitude_msl_ft"]["value"]*.3048, delta=.1)
            self.assertNotEqual(f["world"]["sensors"]["pressure_altitude_ft"], f["world"]["sensors"]["indicated_altitude_ft"])

    def test_faults_use_same_session_and_radio_survives_gnss_loss(self):
        for index, valid in ((399, True), (400, False), (599, False), (600, True)):
            nav = self.frames[index]["world"]["navigation"]
            self.assertEqual(nav["gnss"]["lat"]["value"] is not None, valid)
            self.assertIsNotNone(nav["radios"][0]["radial_mag_deg"]["value"])
        self.assertFalse(self.frames[800]["world"]["weather_products"]["display_allowed"])
        self.assertEqual(self.frames[900]["world"]["weather_products"]["transport"], "OFFLINE")

    def test_world_mutation_rejected_even_with_rehashed_frames(self):
        broken = self.root/"mutated"
        broken.mkdir()
        for name in ("manifest.json", "sim-radar.png", "sim-radar.json"):
            (broken/name).write_bytes((self.weather/name).read_bytes())
        frames = copy.deepcopy(self.frames)
        frames[1]["world"]["navigation"]["radios"][0]["cdi"]["value"] = 123
        content = b"".join((json.dumps(f, sort_keys=True, separators=(",", ":"))+"\n").encode() for f in frames)
        (broken/"frames.jsonl").write_bytes(content)
        manifest = json.loads((broken/"manifest.json").read_text())
        manifest["frames_sha256"] = hashlib.sha256(content).hexdigest()
        (broken/"manifest.json").write_text(json.dumps(manifest))
        with self.assertRaises(ValueError):
            s.verify_session(broken)


if __name__ == "__main__":
    unittest.main()
