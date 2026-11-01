"""Explicit real-JSBSim integration: missing runtime is an error, never a skip."""
import hashlib
import json
import math
import os
from pathlib import Path
import shutil
import tempfile
import unittest
import io

from tools.simulation import jsbsim_session as s


class NativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import jsbsim
        assert jsbsim.__version__ == s.VERSION
        cls.temp = tempfile.TemporaryDirectory(dir=os.environ.get("SIL_TEST_ROOT"))
        cls.root = Path(cls.temp.name)
        cls.runs = {}
        for name, scenario in (("cruise-a", "cruise"), ("cruise-b", "cruise"),
                               ("turn", "turn"), ("engine-out", "engine-out"),
                               ("sensor-loss", "sensor-loss")):
            p = cls.root / name
            s.run_session(p, scenario, 60, "repeatable-reference")
            s.verify_session(p)
            cls.runs[name] = [json.loads(line) for line in (p / "frames.jsonl").read_text().splitlines()]

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def test_01_repeatable_complete_sessions(self):
        a = (self.root / "cruise-a" / "frames.jsonl").read_bytes()
        b = (self.root / "cruise-b" / "frames.jsonl").read_bytes()
        self.assertEqual(hashlib.sha256(a).hexdigest(), hashlib.sha256(b).hexdigest())
        self.assertEqual(len(self.runs["cruise-a"]), 1201)

    def test_02_cruise_and_turn_are_dynamically_distinct(self):
        nominal = self.runs["cruise-a"]
        turn = self.runs["turn"]
        self.assertLess(max(abs(f["flight"]["altitude_msl_ft"]["value"]-5000) for f in nominal), 30)
        self.assertGreater(turn[700]["flight"]["roll_deg"]["value"], 10)
        self.assertLess(abs(turn[-1]["flight"]["roll_deg"]["value"]), 5)
        self.assertGreater(abs(turn[-1]["flight"]["heading_true_deg"]["value"]
                               -nominal[-1]["flight"]["heading_true_deg"]["value"]), 5)

    def test_03_engine_out_changes_fuel_and_trajectory(self):
        nominal, failed = self.runs["cruise-a"], self.runs["engine-out"]
        self.assertLess(failed[-1]["engine"]["flow_gph"]["value"], .05)
        self.assertLess(failed[-1]["flight"]["altitude_msl_ft"]["value"],
                        nominal[-1]["flight"]["altitude_msl_ft"]["value"] - 30)
        self.assertEqual(failed[0]["events"], [])
        self.assertIn("ENGINE_MIXTURE_CUTOFF", failed[-1]["events"])

    def test_04_sensor_loss_and_recovery(self):
        run = self.runs["sensor-loss"]
        self.assertIsNone(run[600]["flight"]["cas_kt"]["value"])
        self.assertIsNotNone(run[900]["flight"]["cas_kt"]["value"])
        self.assertTrue(all(f["flight"]["tas_kt"]["value"] > 0 for f in run))

    def test_05_altitude_derivative_matches_vertical_speed(self):
        for scenario in ("cruise-a", "turn", "engine-out"):
            run = self.runs[scenario]
            for a, b in zip(run, run[1:]):
                delta = (b["flight"]["altitude_msl_ft"]["value"]-a["flight"]["altitude_msl_ft"]["value"]) / .05
                vs = (b["flight"]["vs_fpm"]["value"]+a["flight"]["vs_fpm"]["value"]) / 120
                self.assertLess(abs(delta-vs), 1.0)

    def test_06_fuel_conservation(self):
        for scenario in ("cruise-a", "engine-out"):
            run = self.runs[scenario]
            def total(f):
                return f["engine"]["left_gal"]["value"]+f["engine"]["right_gal"]["value"]
            consumed = total(run[0])-total(run[-1])
            integral = sum((a["engine"]["flow_gph"]["value"]+b["engine"]["flow_gph"]["value"])*.05/7200
                           for a,b in zip(run,run[1:]))
            self.assertLess(abs(consumed-integral), .03)
            self.assertGreater(consumed, 0)

    def test_07_three_offline_views_share_exact_clock(self):
        for frame in self.runs["turn"]:
            views = [s.role_view(frame, r) for r in ("PFD", "MFD", "EMS")]
            self.assertEqual(len({(v["session_id"],v["sequence"],v["sim_time_ms"]) for v in views}), 1)

    def test_08_tampered_replay_not_accepted(self):
        p = self.root / "tampered"
        shutil.copytree(self.root / "cruise-a", p)
        frames = p / "frames.jsonl"
        text = frames.read_text()
        frames.write_text(text.replace('"simulated":true', '"simulated":false', 1))
        with self.assertRaises(ValueError):
            s.verify_session(p)

    def test_09_live_exchange_uses_actual_fdm_and_source_does_not_claim_display(self):
        from tools.simulation.jsbsim_live import Exchange
        from tests.test_sil_world import config
        requests = iter(json.dumps({"schema": "fuso-avionics.sil-live.v1",
                                    "sequence": i, "nonce": f"{i:032x}"}) for i in range(21))
        output = io.StringIO()
        world = config()
        path = self.root / "live-one-second"
        result = s.run_session(path, seconds=1, world_config=world, realtime=True,
                               exchange=Exchange(world, output, lambda: next(requests)))
        self.assertIsNone(result["efis_nodes_connected"])
        self.assertEqual(result["live_exchange"]["requests"], 21)
        self.assertEqual(len(output.getvalue().splitlines()), 22)
        self.assertEqual(s.verify_session(path)["frames"], 21)

    def test_10_partial_request_times_out_without_newline(self):
        from tools.simulation.jsbsim_live import bounded_line
        read_fd, write_fd = os.pipe()
        try:
            os.write(write_fd, b"{")
            with os.fdopen(read_fd, "rb", buffering=0) as stream:
                with self.assertRaisesRegex(ValueError, "timeout"):
                    bounded_line(stream, .02)
        finally:
            os.close(write_fd)


if __name__ == "__main__":
    unittest.main(verbosity=2)
