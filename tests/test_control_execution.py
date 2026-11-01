import copy
import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

from tools.simulation import session_contract as c
from tools.simulation.control_execution import SyncControlAudit
from tools.simulation.pilot_fixture import INPUTS
from tools.simulation.session_control import SessionAdmission
from tools.simulation.session_engine import SessionEngine, TICK_NS, MAX_LAG_NS, encode
from tools.simulation.session_trace import verify
from tests.test_sil_session_engine import Clock, FixtureBackend, MemoryJournal


class Fdm:
    """Input-property fixture, not aerodynamics."""
    def __init__(self):
        self.catalog = set(INPUTS.values())
        self.overrides, self.values = {}, {}
        self.writes = self.steps = 0

    def set_test_control(self, control, value):
        self.writes += 1
        self.overrides[control] = value

    def clear_test_controls(self):self.overrides.clear()

    def step(self):
        self.steps += 1
        self.values.update({INPUTS[k]:v for k,v in self.overrides.items()})

    def read(self, prop):return self.values.get(prop)


class Adapter(FixtureBackend):
    def __init__(self, backend):
        super().__init__()
        self.backend, self.pilot = backend, None

    def step(self):
        self.backend.step()
        super().step()


class ControlExecutionTests(unittest.TestCase):
    def setUp(self):
        vectors = c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())
        self.manifest, self.request = vectors["manifest"], vectors["command"]
        self.tmp = tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name)/"control.jsonl"
        self.clock, self.fdm, self.journal = Clock(), Fdm(), MemoryJournal()
        self.adapter = Adapter(self.fdm)
        self.engine = SessionEngine(self.manifest,self.adapter,self.journal,clock=self.clock)
        self.control = SessionAdmission(self.manifest,principal="instructor",lease_id="lease-001",
                         lease_until_tick=100,lease_duration_ns=1_000_000_000,clock=self.clock)
        self.control.enable_execution(self.fdm,self.path)
        self.audit = self.control.executor.audit
        self.addCleanup(self.audit.close)
        self.engine.attach_admission(self.control)
        self.engine.validate();self.engine.arm_software_test();self.engine.start()

    def raw(self, number=1, control="aileron", value=.25, **extra):
        request = copy.deepcopy(self.request)
        request.update(command_id=f"cmd-{number}",sequence=number,
                       action="SET_CONTROL",args=dict(control=control,value=value))
        request.update(extra)
        return encode(request)

    def offer(self, **kwargs):
        raw = self.raw(**kwargs)
        self.control.offer(raw,principal="instructor")
        self.engine.pump()
        return raw

    def step(self):
        self.clock.now += TICK_NS
        return self.engine.pump()

    def rows(self):return [c.decode(line) for line in self.path.read_bytes().splitlines()]

    def ack(self, raw):return self.control.lookup_outcome(raw,principal="instructor")

    def assert_stopped(self, steps=0):
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.fdm.steps,steps)
        self.assertEqual(self.fdm.overrides,{})
        self.assertFalse(self.engine.pump())

    def test_applied_only_after_step_readback_and_fsync_then_cached_retry(self):
        raw = self.offer()
        self.assertEqual(self.ack(raw)["status"],"QUEUED")
        self.step()
        self.assertEqual(self.ack(raw)["status"],"APPLIED")
        self.control.offer(raw,principal="instructor");self.engine.pump();self.step()
        self.assertEqual(self.fdm.writes,1)
        self.engine.stop()
        rows = self.rows()
        self.assertEqual([r["event"] for r in rows],["BOUND","INTENT","APPLIED","RELEASED"])
        self.assertEqual(rows[2]["readback"],.25)
        self.assertEqual(rows[0]["binding"]["manifest"],self.manifest)
        meta = self.journal.result["control_execution"]
        self.assertEqual(meta["sha256"],hashlib.sha256(self.path.read_bytes()).hexdigest())
        self.assertEqual(meta["records"],4)

    def test_three_supported_controls_and_conflicting_same_tick_rejected(self):
        for n,name in enumerate(INPUTS,1):self.offer(number=n,control=name,value=.2)
        conflict = self.offer(number=4,control="aileron",value=-.2)
        self.step()
        self.assertEqual(self.fdm.writes,3)
        self.assertEqual(self.ack(conflict)["reason"],"INVALID")
        self.assertEqual(self.control.executor.applied,3)

    def test_unsupported_action_control_and_missing_property_do_not_mutate(self):
        for case in ("action","control","property"):
            with self.subTest(case=case):
                self.setUp()
                if case=="action":raw=self.offer(action="PAUSE",args={})
                elif case=="control":raw=self.offer(control="rudder",value=.1)
                else:
                    self.fdm.catalog.remove(INPUTS["aileron"])
                    raw=self.offer()
                self.step()
                self.assertEqual(self.fdm.writes,0)
                self.assertEqual(self.ack(raw)["reason"],"UNSUPPORTED")

    def test_slow_intent_cannot_mutate_or_step_after_deadline(self):
        self.offer()
        original = self.audit.record
        def slow(event,*args,**kwargs):
            original(event,*args,**kwargs)
            if event=="INTENT":self.clock.now+=MAX_LAG_NS+1
        self.audit.record = slow
        with self.assertRaises(ValueError):self.step()
        self.assert_stopped()
        self.assertEqual(self.fdm.writes,0)

    def test_expiry_and_revoke_during_intent_cannot_mutate(self):
        for revoke in (False,True):
            with self.subTest(revoke=revoke):
                self.setUp();self.offer()
                original=self.audit.record
                def interrupt(event,*args,**kwargs):
                    original(event,*args,**kwargs)
                    if event=="INTENT":
                        if revoke:self.control.signal_revoke()
                        else:self.control.commands._deadline=self.clock.now
                self.audit.record=interrupt
                with self.assertRaises(ValueError):self.step()
                self.assert_stopped()
                self.assertEqual(self.fdm.writes,0)

    def test_setter_revoke_is_rechecked_immediately_before_physics(self):
        self.offer()
        original = self.fdm.set_test_control
        def revoke(**kwargs):original(**kwargs);self.control.signal_revoke()
        self.fdm.set_test_control = revoke
        with self.assertRaises(ValueError):self.step()
        self.assert_stopped()

    def test_final_writer_check_expiry_cannot_apply_batch(self):
        self.offer()
        def watchdog():
            if self.fdm.overrides:self.control.commands._deadline=self.clock.now
        self.journal.check = watchdog
        with self.assertRaises(ValueError):self.step()
        self.assert_stopped()

    def test_expiry_with_no_new_due_command_clears_held_overrides(self):
        raw=self.offer();self.step()
        self.control.commands._deadline=self.clock.now+1
        self.step()
        self.assertEqual(self.fdm.overrides,{})
        self.assertEqual(self.ack(raw)["status"],"APPLIED")
        self.assertEqual(self.engine.state,"RUNNING")

    def test_release_io_after_final_authority_check_cannot_hide_lateness(self):
        self.offer();self.step()
        original=self.audit.record
        def slow(event,*args,**kwargs):
            original(event,*args,**kwargs)
            if event=="RELEASED":self.clock.now+=MAX_LAG_NS+1
        self.audit.record=slow
        checks=0
        def watchdog():
            nonlocal checks
            checks+=1
            if checks==3:self.control.commands._deadline=self.clock.now
        self.journal.check=watchdog
        with self.assertRaises(ValueError):self.step()
        self.assert_stopped(steps=1)

    def test_partial_set_failure_clears_entire_batch_before_physics(self):
        self.offer();self.offer(number=2,control="elevator",value=.1)
        original=self.fdm.set_test_control
        def fail(control,value):
            if control=="elevator":raise OSError("second setter failed")
            original(control,value)
        self.fdm.set_test_control=fail
        with self.assertRaises(OSError):self.step()
        self.assert_stopped()

    def test_readback_failure_never_claims_applied(self):
        raw=self.offer();self.fdm.read=lambda prop:-.99
        with self.assertRaises(ValueError):self.step()
        self.assert_stopped(steps=1)
        with self.assertRaisesRegex(RuntimeError,"uncertain"):self.ack(raw)
        with self.assertRaisesRegex(RuntimeError,"retry forbidden"):
            self.control.commands.submit(raw,authenticated_principal="instructor",current_tick=1)
        self.assertFalse(any(r["event"]=="APPLIED" for r in self.rows()))

    def test_result_fsync_crossing_lease_retains_known_applied(self):
        raw=self.offer()
        self.control.commands._deadline=TICK_NS+10
        original=self.audit.record
        def slow(event,*args,**kwargs):
            original(event,*args,**kwargs)
            if event=="APPLIED":self.clock.now+=20
        self.audit.record=slow
        self.step()
        self.assertEqual(self.ack(raw)["status"],"APPLIED")
        self.assertTrue(self.control.commands.closed)
        self.assertEqual(self.fdm.overrides,{})

    def test_revoke_during_physics_preserves_completed_outcome(self):
        raw=self.offer();original=self.fdm.step
        def revoke():original();self.control.signal_revoke()
        self.fdm.step=revoke
        self.step()
        self.assertEqual(self.ack(raw)["status"],"APPLIED")
        self.assertEqual(self.fdm.overrides,{})

    def test_pause_clears_before_release_io_and_resume_never_rearms(self):
        self.offer();self.step()
        original=self.audit.record
        def observe(event,*args,**kwargs):
            if event=="RELEASED":self.assertEqual(self.fdm.overrides,{})
            original(event,*args,**kwargs)
        self.audit.record=observe
        self.engine.pause();self.engine.resume();self.step()
        self.assertEqual(self.engine.manifest["epoch"],1)
        self.assertEqual(self.fdm.writes,1)
        self.assertEqual(self.fdm.overrides,{})

    def test_audit_record_capacity_and_fsync_failure_stop_before_input(self):
        for fault in ("capacity","bytes","fsync"):
            with self.subTest(fault=fault):
                self.setUp();self.offer()
                if fault=="capacity":self.audit.MAX_RECORDS=1
                if fault=="bytes":self.audit.MAX_BYTES=self.audit.bytes
                if fault=="fsync":
                    with patch("tools.simulation.control_execution.os.fsync",side_effect=OSError("fsync failed")):
                        with self.assertRaises((ValueError,OSError)):self.step()
                else:
                    with self.assertRaises(ValueError):self.step()
                self.assert_stopped()
                self.assertTrue(self.audit.closed)

    def test_clear_failure_closes_sidecar_and_never_claims_release(self):
        self.offer();self.step()
        def fail():raise OSError("clear failed")
        self.fdm.clear_test_controls=fail
        with self.assertRaises(OSError):self.engine.pause()
        self.assertEqual(self.engine.state,"FAILED")
        self.assertTrue(self.audit.closed)
        self.assertFalse(any(r["event"]=="RELEASED" for r in self.rows()))
        self.assertFalse(self.engine.pump())
        self.assertEqual(self.fdm.steps,1)

    def test_wrong_target_and_fixture_conflict_rejected_before_bind(self):
        fresh=SessionAdmission(self.manifest,principal="instructor",lease_id="lease-001",
              lease_until_tick=100,lease_duration_ns=1_000_000_000,clock=self.clock)
        fresh.enable_execution(self.fdm,Path(self.tmp.name)/"other-audit.jsonl")
        self.addCleanup(fresh.executor.audit.close)
        other=SessionEngine(self.manifest,Adapter(Fdm()),MemoryJournal(),clock=self.clock)
        with self.assertRaises(ValueError):other.attach_admission(fresh)
        self.adapter.pilot=object()
        other=SessionEngine(self.manifest,self.adapter,MemoryJournal(),clock=self.clock)
        with self.assertRaises(ValueError):other.attach_admission(fresh)

    def test_result_audit_failure_clears_inputs_without_success_ack(self):
        raw=self.offer();original=self.audit.record
        def fail(event,*args,**kwargs):
            if event=="APPLIED":raise OSError("result fsync failed")
            original(event,*args,**kwargs)
        self.audit.record=fail
        with self.assertRaises(OSError):self.step()
        self.assert_stopped(steps=1)
        with self.assertRaisesRegex(RuntimeError,"uncertain"):self.ack(raw)
        self.assertFalse(hasattr(self.journal,"result"))

    def test_short_write_and_close_failure_are_not_completion(self):
        for close_failure in (False,True):
            with self.subTest(close_failure=close_failure):
                self.setUp();self.offer();underlying=self.audit.stream
                class BrokenStream:
                    def write(self,data):
                        return underlying.write(data if close_failure else data[:-1])
                    def fileno(self):return underlying.fileno()
                    def close(self):
                        underlying.close()
                        if close_failure:raise OSError("close failed")
                self.audit.stream=BrokenStream()
                if close_failure:
                    self.step()
                    with self.assertRaises(OSError):self.engine.stop()
                    self.assert_stopped(steps=1)
                else:
                    with self.assertRaises(OSError):self.step()
                    self.assert_stopped()
                self.assertTrue(self.audit.closed)
                self.assertFalse(hasattr(self.journal,"result"))

    def test_exclusive_audit_and_legacy_trace_refuses_live_sidecar_result(self):
        with self.assertRaises(FileExistsError):SyncControlAudit(self.path,self.control.commands.binding)
        result_path=Path(self.tmp.name)/"result.json"
        result_path.write_bytes(encode(dict(control_execution={})))
        with self.assertRaisesRegex(ValueError,"sidecar replay/archive unsupported"):
            verify(Path(self.tmp.name))


if __name__=="__main__":unittest.main()
