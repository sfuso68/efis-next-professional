"""Explicit JSBSim tests. Missing real backend fails, no skip/mocking."""
import tempfile
from pathlib import Path
import unittest
from tools.simulation import session_contract as c
from tools.simulation import session_jsbsim as s
from tools.simulation.session_trace import verify


class RealSessionTests(unittest.TestCase):
    def test_native_three_inputs_and_rehashed_false_readback_rejected(self):
        import hashlib
        from tools.simulation.session_engine import encode
        plan=[dict(apply_tick=10,control=name,value=value) for name,value in
              (("aileron",.1),("elevator",.01),("throttle",.6))]
        s.run(self.root/"inputs",self.world,session_id="native-inputs",source_commit="0"*40,
              scenario="cruise",seconds=1,pilot_fixture=plan)
        self.assertEqual(verify(self.root/"inputs")["pilot_commands_applied"],3)
        path=self.root/"inputs"/"truth.jsonl"
        rows=[c.decode(line) for line in path.read_bytes().splitlines()]
        next(r for r in rows if r.get("event")=="APPLIED")["readback"]=-.9
        data=b"".join(encode(r) for r in rows)
        path.write_bytes(data)
        result_path=self.root/"inputs"/"result.json"
        result=c.decode(result_path.read_bytes()); result["truth_sha256"]=hashlib.sha256(data).hexdigest()
        result_path.write_bytes(encode(result))
        with self.assertRaisesRegex(ValueError,"readback"):verify(self.root/"inputs")

    def test_native_pilot_changes_physics_and_is_audited(self):
        plan=[dict(apply_tick=10,control="aileron",value=.25)]
        for name,pilot in (("baseline",None),("pilot",plan)):
            s.run(self.root/name,self.world,session_id="native-pilot",source_commit="0"*40,
                  scenario="cruise",seconds=2,pilot_fixture=pilot)
        rows=[c.decode(line) for line in (self.root/"pilot"/"truth.jsonl").read_bytes().splitlines()]
        applications=[r for r in rows if r.get("event")=="APPLIED"]
        self.assertEqual(len(applications),1)
        self.assertEqual(applications[0]["readback"],.25)
        self.assertEqual(applications[0]["ack"]["applied_tick"],10)
        self.assertTrue(any(r.get("event")=="RELEASED" and r["total_ticks"]==101 for r in rows))
        def roll_at_50(name):
            return next(c.decode(line)["frame"]["flight"]["roll_deg"]["value"]
                        for line in (self.root/name/"truth.jsonl").read_bytes().splitlines()
                        if c.decode(line).get("kind")=="PHYSICS_WORLD" and c.decode(line)["total_ticks"]==50)
        self.assertGreater(abs(roll_at_50("pilot")-roll_at_50("baseline")),.01)
        self.assertEqual(verify(self.root/"pilot")["pilot_commands_applied"],1)

    def test_native_pilot_invalid_options_leave_no_output(self):
        plan=[dict(apply_tick=1,control="aileron",value=.1)]
        with self.assertRaises(ValueError):
            s.run(self.root/"invalid",self.world,session_id="bad",source_commit="0"*40,
                  seconds=1,pause_tick=25,pilot_fixture=plan)
        self.assertFalse((self.root/"invalid").exists())

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

    def test_native_async_recorder_and_pause(self):
        report=s.run(self.root/"async",self.world,session_id="native-async",source_commit="0"*40,
                     seconds=1,pause_tick=25,journal_mode="async-test")
        self.assertEqual(report["status"],"COMPLETE")
        self.assertEqual(report["journal_mode"],"async-test")
        self.assertLessEqual(report["maximum_pending_records"],16)
        self.assertLessEqual(report["maximum_observed_ack_age_ms"],100)
        self.assertEqual(verify(self.root/"async")["status"],"PASS")


if __name__=="__main__":unittest.main()
