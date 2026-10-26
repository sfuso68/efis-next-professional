"""Offline tests require no JSBSim; native integration is a separate explicit run."""
import io
import hashlib
import json
import math
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from tools.simulation import jsbsim_session as s


class FakeFdm(dict):
    def get_sim_time(self):
        return .01

    def run(self):
        return True


def backend():
    b = s.Backend.__new__(s.Backend)
    b.f = FakeFdm({p: 1.0 for p, _, _ in s.FLIGHT.values()})
    b.f.update({p: 100.0 for p, _, _ in s.ENGINE.values()})
    b.f.update({"position/h-sl-ft": 5000, "velocities/q-rad_sec": 0,
                "velocities/p-rad_sec": 0, "fcs/flap-pos-deg": 0})
    b.catalog = set(b.f)
    b.base_e, b.base_t, b.base_p, b.base_v, b.ticks = 0, .7, 1, 90, 0
    return b


def fixture(directory, mutate=None):
    frames = []
    b = backend()
    for i in range(21):
        b.ticks = i * 5
        frame = b.frame("cruise", "fixture")
        if mutate:
            mutate(frame)
        frames.append(json.dumps(frame, allow_nan=False)+"\n")
    content = "".join(frames).encode()
    (directory / "frames.jsonl").write_bytes(content)
    manifest = {"schema": s.SCHEMA, "status": "COMPLETE_OFFLINE_REFERENCE", "simulated": True,
                "flight_approval": False, "duration_seconds": 1, "frames": 21,
                "integration_hz": 100, "output_hz": 20, "model": "c172p", "backend": "JSBSim-"+s.VERSION,
                "profile_status": "REFERENCE_UNVALIDATED", "session_id": "fixture",
                "frames_sha256": hashlib.sha256(content).hexdigest()}
    (directory / "manifest.json").write_text(json.dumps(manifest))


class SessionTests(unittest.TestCase):
    def test_realtime_pacing_waits_before_tick_and_records_timing(self):
        now = [10.0]
        def sleep(delay):
            now[0] += delay
        pacer = s.RealtimePacer(lambda: now[0], sleep)
        for tick in range(101):
            pacer.wait(tick)
        report = pacer.report()
        self.assertEqual(report["ticks"], 101)
        self.assertEqual(report["elapsed_seconds"], 1)
        self.assertEqual(report["maximum_lateness_ms"], 0)

    def test_realtime_stall_and_discontinuity_fail_closed(self):
        now = [0.0]
        pacer = s.RealtimePacer(lambda: now[0], lambda delay: None)
        pacer.wait(0)
        now[0] = .2
        with self.assertRaisesRegex(ValueError, "deadline exceeded"):
            pacer.wait(1)
        self.assertEqual(pacer.last_tick, 0)
        with self.assertRaisesRegex(ValueError, "discontinuity"):
            pacer.wait(2)

    def test_realtime_timing_manifest_mutations_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            fixture(p)
            manifest = json.loads((p / "manifest.json").read_text())
            good = {"mode": "MONOTONIC_FIXED_STEP", "ticks": 101,
                    "elapsed_seconds": 1.01, "maximum_lateness_ms": 2,
                    "lateness_limit_ms": 100, "overrun_policy": "STOP_NO_CATCHUP"}
            for key, value in (("ticks", 100), ("maximum_lateness_ms", 101),
                               ("elapsed_seconds", .99), ("elapsed_seconds", True),
                               ("overrun_policy", "CATCHUP")):
                manifest["realtime_timing"] = dict(good, **{key: value})
                (p / "manifest.json").write_text(json.dumps(manifest))
                with self.subTest(key=key), self.assertRaises(ValueError):
                    s.verify_session(p)
            manifest["realtime_timing"] = good
            (p / "manifest.json").write_text(json.dumps(manifest))
            self.assertEqual(s.verify_session(p)["realtime_timing"], good)

    def test_realtime_failure_never_completes_and_nonfinite_clock_rejected(self):
        with self.assertRaises(ValueError):
            s.RealtimePacer(lambda: math.nan)
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "overrun"
            with patch.object(s, "Backend", return_value=backend()), \
                    patch.object(s.RealtimePacer, "wait", side_effect=ValueError("deadline exceeded")):
                with self.assertRaises(ValueError):
                    s.run_session(p, seconds=1, realtime=True)
            self.assertFalse((p / "manifest.json").exists())
            self.assertEqual(json.loads((p / "failure.json").read_text())["status"], "FAILED")

    def test_sample_conversion_and_unavailable(self):
        self.assertEqual(s.sample(212, "degC", "x", None)["value"], 100)
        for value in (None, math.nan, math.inf):
            datum = s.sample(value, "degC", "x")
            self.assertIsNone(datum["value"])
            self.assertEqual(datum["validity"], "UNAVAILABLE")
            self.assertTrue(datum["simulated"])

    def test_controller_finite_and_bounded(self):
        self.assertEqual(s.clip(2, -1, 1), 1)
        self.assertEqual(s.clip(-2, -1, 1), -1)
        with self.assertRaises(ValueError):
            s.clip(math.nan, -1, 1)

    def test_missing_property_not_zero_or_created(self):
        b = backend()
        self.assertIsNone(b.read("propulsion/engine/missing"))
        self.assertNotIn("propulsion/engine/missing", b.f)

    def test_same_frame_clock_and_no_fake_cylinders(self):
        b = backend()
        frame = b.frame("cruise", "shared")
        self.assertEqual(frame["sequence"], 0)
        self.assertEqual(frame["sim_time_ms"], 0)
        self.assertEqual(len(frame["engine"]), 18)
        for i in range(1, 5):
            self.assertIsNone(frame["engine"][f"cht{i}"]["value"])
            self.assertIsNone(frame["engine"][f"egt{i}"]["value"])
        self.assertFalse(frame["flight_approval"])
        self.assertEqual(frame["profile_status"], "REFERENCE_UNVALIDATED")

    def test_fault_changes_validity_not_physics_or_other_fields(self):
        b = backend()
        for ticks, status in ((1995, "SIMULATED"), (2000, "UNAVAILABLE"),
                              (3995, "UNAVAILABLE"), (4000, "SIMULATED")):
            b.ticks = ticks
            frame = b.frame("sensor-loss", "shared")
            self.assertEqual(frame["flight"]["cas_kt"]["validity"], status)
            self.assertEqual(frame["flight"]["tas_kt"]["validity"], "SIMULATED")

    def test_views_share_identity_sequence_time(self):
        frame = backend().frame("cruise", "shared")
        for role in ("PFD", "MFD", "EMS"):
            view = s.role_view(frame, role)
            for key in ("session_id", "sequence", "sim_time_ms"):
                self.assertEqual(frame[key], view[key])
            self.assertTrue(view["offline_test_view"])
            self.assertFalse(view["flight_approval"])
        with self.assertRaises(ValueError):
            s.role_view(frame, "PIXHAWK")

    def test_step_uses_commands_not_pose_setters(self):
        b = backend()
        before = dict(b.f)
        b.step("cruise")
        self.assertEqual(b.ticks, 1)
        for key in before:
            if key.startswith(("position/", "attitude/", "velocities/")):
                self.assertEqual(before[key], b.f[key])
        for key in ("fcs/elevator-cmd-norm", "fcs/aileron-cmd-norm", "fcs/throttle-cmd-norm"):
            self.assertTrue(-1 <= b.f[key] <= 1)

    def test_fdm_clock_drift_and_missing_required_data_fail(self):
        b = backend()
        b.ticks = 10
        with self.assertRaises(ValueError):
            b.step("cruise")
        b = backend()
        b.catalog.remove("position/h-sl-ft")
        with self.assertRaises(ValueError):
            b.frame("cruise", "shared")

    def test_invalid_inputs_do_not_create_session(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "run"
            for scenario, seconds, sid in [("bad", 1, "x"), ("cruise", 0, "x"),
                                           ("cruise", 3601, "x"), ("cruise", 1, "../x")]:
                with self.assertRaises(ValueError):
                    s.run_session(p, scenario, seconds, sid)
                self.assertFalse(p.exists())

    def test_existing_session_preserved(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            marker = p / "keep"
            marker.write_text("original")
            with self.assertRaises(FileExistsError), patch.object(s, "Backend") as factory:
                s.run_session(p)
                factory.assert_not_called()
            self.assertEqual(marker.read_text(), "original")

    def test_failure_never_creates_complete_manifest(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "failed"
            with patch.object(s, "Backend", side_effect=ValueError("trim rejected")):
                with self.assertRaises(ValueError):
                    s.run_session(p)
            self.assertEqual(json.loads((p / "failure.json").read_text())["status"], "FAILED")
            self.assertFalse((p / "manifest.json").exists())

    def test_complete_fixture_verifies_and_byte_mutation_fails(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            fixture(p)
            self.assertEqual(s.verify_session(p)["frames"], 21)
            with (p / "frames.jsonl").open("ab") as stream:
                stream.write(b" ")
            with self.assertRaises(ValueError):
                s.verify_session(p)

    def test_semantic_mutations_rejected_even_with_updated_digest(self):
        def missing(frame):
            frame["engine"].pop("rpm")
        def fake_valid(frame):
            frame["engine"]["cht1"]["value"] = 0
        def bad_unit(frame):
            frame["engine"]["rpm"]["unit"] = ""
        def wrong_clock(frame):
            frame["sim_time_ms"] += 1
        def bool_value(frame):
            frame["engine"]["rpm"]["value"] = True
        for mutation in (missing, fake_valid, bad_unit, wrong_clock, bool_value):
            with self.subTest(mutation=mutation.__name__), tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                fixture(p, mutation)
                with self.assertRaises(ValueError):
                    s.verify_session(p)

    def test_duplicate_and_nonfinite_json_rejected(self):
        for text in ('{"a":1,"a":2}', '{"a":NaN}', '{"a":Infinity}'):
            with self.assertRaises(ValueError):
                s.strict_json(text)

    def test_manifest_not_object_or_missing_data_rejected(self):
        for text in ('[]', '{}', '{"simulated":true,"simulated":false}'):
            with tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                (p / "manifest.json").write_text(text)
                with self.assertRaises(ValueError):
                    s.verify_session(p)

    def test_manifest_duration_and_failure_marker_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp)
            fixture(p)
            (p / "failure.json").write_text('{}')
            with self.assertRaises(ValueError):
                s.verify_session(p)

    def test_empty_model_tree_rejected(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                s.model_tree_digest(Path(tmp))

    def test_empty_or_extra_fields_not_accepted_as_full_flight(self):
        for mutate in (lambda f: f.update(flight={}), lambda f: f["flight"].update(fake=s.sample(0,"x","x"))):
            with tempfile.TemporaryDirectory() as tmp:
                p = Path(tmp)
                fixture(p, mutate)
                with self.assertRaises(ValueError):
                    s.verify_session(p)


if __name__ == "__main__":
    unittest.main()
