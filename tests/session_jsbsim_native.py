"""Explicit JSBSim tests. Missing real backend fails, no skip/mocking."""
import tempfile
from pathlib import Path
import unittest
from tools.simulation import session_contract as c
from tools.simulation import session_jsbsim as s
from tools.simulation.session_trace import verify


class RealSessionTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)
        self.world=c.decode((c.ROOT/"config/simulation/treviso-belluno-world-reference.json").read_bytes())

    def test_native_timed_session_and_posix_result(self):
        report=s.run(self.root/"run",self.world,session_id="native-run",source_commit="0"*40,seconds=1)
        self.assertEqual(report["status"],"COMPLETE")
        self.assertEqual(report["total_ticks"],100)
        self.assertEqual(report["frames"],21)
        self.assertEqual(verify(self.root/"run")["status"],"PASS")

    def test_native_pause_resume_preserves_physical_clock(self):
        report=s.run(self.root/"pause",self.world,session_id="native-pause",source_commit="0"*40,seconds=2,pause_tick=25)
        self.assertEqual(report["epoch"],1)
        self.assertEqual(report["total_ticks"],200)
        self.assertEqual(verify(self.root/"pause")["epochs"],2)

    def test_native_corruption_is_not_replay_success(self):
        s.run(self.root/"bad",self.world,session_id="native-bad",source_commit="0"*40,seconds=1)
        path=self.root/"bad"/"truth.jsonl"
        with path.open("ab") as stream:stream.write(b'{"truncated":')
        with self.assertRaises(ValueError):verify(self.root/"bad")

    def test_native_gnss_fault_does_not_replace_truth_or_stop_physics(self):
        backend=s.Backend(self.world)
        manifest=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        manifest["model_sha256"]=backend.model_digest["sha256"]
        for _ in range(2000):backend.step("turn")
        raw=backend.frame("turn",manifest["session_id"])
        frame=s.normalize(raw,manifest,2000,0)
        self.assertIsNotNone(raw["flight"]["latitude_deg"]["value"])
        self.assertIsNone(frame["groups"]["flight"]["latitude_deg"]["value"])
        self.assertIsNotNone(frame["groups"]["flight"]["tas_kt"]["value"])
        for _ in range(1000):backend.step("turn")
        frame=s.normalize(backend.frame("turn",manifest["session_id"]),manifest,3000,0)
        self.assertIsNotNone(frame["groups"]["flight"]["latitude_deg"]["value"])

    def test_native_missing_cylinders_are_not_aggregate_copies(self):
        backend=s.Backend(self.world)
        manifest=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        raw=backend.frame("cruise",manifest["session_id"])
        frame=s.normalize(raw,manifest,0,0)
        for name in [f"{kind}{i}" for kind in ("cht","egt") for i in range(1,5)]:
            self.assertIsNone(frame["groups"]["engine"][name]["value"])
        self.assertIsNone(frame["groups"]["airframe"]["gear_down_locked"]["value"])


if __name__=="__main__":unittest.main()
