import copy
import hashlib
import tempfile
import tarfile
from pathlib import Path
import threading
import unittest
from unittest.mock import patch

from tests import test_control_execution as fixture
from tests.lifecycle_fixture import run_lifecycle
from tools.simulation import session_contract as c
from tools.simulation.console_lifecycle import TraceAudit, request
from tools.simulation.local_console import LocalConsole
from tools.simulation.session_engine import encode, TICK_NS
from tools.simulation.session_trace import verify
from tools.simulation.session_archive import pack, restore, LIVE_FILES


class LifecycleTests(unittest.TestCase):
    def setUp(self):
        self.fixture=fixture.ControlExecutionTests();self.fixture.setUp()
        self.addCleanup(self.fixture.doCleanups)
        self.engine=self.fixture.engine
        self.console=LocalConsole(self.engine,self.fixture.control,lifecycle=True)

    def obj(self,action="PAUSE",identity="pause-1",epoch=0):
        return dict(action=action,command_id=identity,
                    session_id=self.engine.manifest["session_id"],expected_epoch=epoch)

    def send(self,obj):
        self.assertEqual(self.console.offer(encode(obj)),"QUEUED_NOT_VALIDATED")
        self.console.pump()
        replies=c.decode(self.console.latest())
        return replies[0] if isinstance(replies,list) else replies

    def test_pause_resume_cached_across_epoch_never_reapply_or_rearm(self):
        pause=self.obj();resume=self.obj("RESUME","resume-1")
        first=self.send(pause)
        self.assertEqual(first["outcome"],"APPLIED");self.assertEqual(self.engine.state,"PAUSED")
        self.assertEqual(self.send(resume)["after"]["epoch"],1)
        self.assertEqual(self.send(pause),first)
        self.assertEqual(self.engine.state,"RUNNING")
        self.assertEqual(self.send(resume)["outcome"],"APPLIED")
        self.assertEqual(self.engine.manifest["epoch"],1)
        self.assertTrue(self.fixture.control.commands.closed)
        self.assertTrue(self.fixture.control.executor.released)
        self.assertEqual(self.send(dict(action="SET_CONTROL",command_id="new-input",sequence=2,
                                      control="aileron",value=.5))["console"],"NOT_ADMITTED")

    def test_stale_epoch_session_and_wrong_state_rejections_are_retained(self):
        for obj,reason in ((self.obj(epoch=3),"REJECTED_EPOCH"),
                           (dict(self.obj(),session_id="another"),"REJECTED_SESSION"),
                           (self.obj("RESUME"),"REJECTED_STATE")):
            with self.subTest(reason=reason):
                if self.engine.manifest["epoch"]!=0:self.setUp()
                obj["command_id"]=reason
                reply=self.send(obj);self.assertEqual(reply["outcome"],reason)
                self.send(self.obj(identity="pause-actual"))
                self.assertEqual(self.send(obj),reply)
                self.send(self.obj("RESUME","resume-actual",self.engine.manifest["epoch"]))

    def test_cross_action_identity_conflicts_share_history(self):
        self.send(self.obj())
        reply=self.send(dict(action="SET_CONTROL",command_id="pause-1",sequence=1,control="aileron",value=.2))
        self.assertEqual(reply["console"],"DUPLICATE_CONFLICT")
        self.assertEqual(self.send(dict(self.obj(),action="RESUME"))["console"],"DUPLICATE_CONFLICT")
        self.assertEqual(self.engine.state,"PAUSED")

    def test_exact_fields_and_types_rejected_before_audit_or_mutation(self):
        for obj in (dict(self.obj(),expected_epoch=True),dict(self.obj(),extra=1),
                    dict(self.obj(),command_id="bad id"),dict(self.obj(),session_id=[])):
            self.assertEqual(self.send(obj)["console"],"INVALID")
        self.assertEqual(self.engine.state,"RUNNING")
        self.assertFalse(any(r.get("kind")=="CONSOLE_LIFECYCLE" for r in self.fixture.journal.records))

    def test_shared_history_exhaustion_stops_without_transition(self):
        self.console.MAX_HISTORY=0
        self.send(self.obj());self.assertEqual(self.engine.state,"COMPLETE")
        self.assertEqual(self.engine.manifest["epoch"],0)

    def test_intent_write_failure_cannot_pause_or_claim_success(self):
        with patch.object(self.fixture.journal,"append",side_effect=OSError("intent fsync")):
            with self.assertRaises((OSError,RuntimeError)):self.send(self.obj())
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.console._lookup("pause-1")["console"],"UNCERTAIN_NO_RETRY")
        self.assertFalse(hasattr(self.fixture.journal,"result"))

    def test_result_write_failure_never_becomes_invalid_input_or_retried(self):
        original=self.fixture.journal.append
        def append(row):
            if row.get("stage")=="RESULT":raise ValueError("result failure")
            original(row)
        with patch.object(self.fixture.journal,"append",side_effect=append):
            with self.assertRaisesRegex(RuntimeError,"uncertain"):self.send(self.obj())
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.console._lookup("pause-1")["console"],"UNCERTAIN_NO_RETRY")

    def test_stop_latched_during_intent_prevents_resume(self):
        self.send(self.obj())
        original=self.fixture.journal.append
        def append(row):
            original(row)
            if row.get("stage")=="INTENT":self.console.request_stop()
        with patch.object(self.fixture.journal,"append",side_effect=append):
            with self.assertRaisesRegex(RuntimeError,"uncertain"):
                self.send(self.obj("RESUME","resume-1"))
        self.assertEqual(self.engine.state,"FAILED")
        self.assertEqual(self.engine.manifest["epoch"],0)

    def test_default_console_still_refuses_lifecycle(self):
        console=LocalConsole(self.engine,self.fixture.control)
        console.offer(encode(self.obj()));console.pump()
        self.assertEqual(c.decode(console.latest())[0]["console"],"INVALID")
        self.assertEqual(self.engine.state,"RUNNING")


class LifecycleTraceTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.parent=Path(self.tmp.name);self.root=self.parent/"run"
        run_lifecycle(self.root)

    def rows(self):return [c.decode(line) for line in (self.root/"truth.jsonl").read_bytes().splitlines()]

    def reseal(self,rows):
        data=b''.join(encode(r) for r in rows);(self.root/"truth.jsonl").write_bytes(data)
        result=c.decode((self.root/"result.json").read_bytes())
        result.update(truth_records=len(rows),truth_sha256=hashlib.sha256(data).hexdigest())
        (self.root/"result.json").write_bytes(encode(result))

    def test_complete_trace_and_v2_restore_preserve_lifecycle(self):
        report=verify(self.root,allow_control_audit=True)
        self.assertEqual(report["epochs"],2)
        self.assertEqual(report["console_lifecycle"]["completed"],3)
        pack(self.root,self.parent/"run.tar",live=True)
        restore(self.parent/"run.tar",self.parent/"restored",live=True)
        for name in LIVE_FILES:
            self.assertEqual((self.root/name).read_bytes(),(self.parent/"restored"/name).read_bytes())

    def test_own_tar_record_boundary_footer_is_valid_but_trailing_data_is_not(self):
        archive=self.parent/"boundary.tar";pack(self.root,archive,live=True)
        with tarfile.open(archive,"r:") as tar:
            last=list(tar)[-1]
            end=last.offset_data+((last.size+511)//512)*512
        tail=archive.stat().st_size-end
        self.assertEqual(tail,tarfile.RECORDSIZE+512)
        data=archive.read_bytes();archive.write_bytes(data[:-1]+b'x')
        with self.assertRaisesRegex(ValueError,"footer"):
            restore(archive,self.parent/"bad-footer",live=True)
        self.assertFalse((self.parent/"bad-footer").exists())

    def test_resealed_result_or_tick_changes_fail_closed(self):
        original=self.rows()
        for field,value in (("outcome","REJECTED_STATE"),("epoch",True),("index",99)):
            with self.subTest(field=field):
                rows=copy.deepcopy(original)
                row=next(r for r in rows if r.get("stage")=="RESULT")
                row[field]=value;self.reseal(rows)
                with self.assertRaises(ValueError):verify(self.root,allow_control_audit=True)

    def test_missing_result_duplicate_identity_and_reordered_intent_rejected(self):
        original=self.rows()
        variants=[]
        rows=copy.deepcopy(original);rows.remove(next(r for r in rows if r.get("stage")=="RESULT"));variants.append(rows)
        rows=copy.deepcopy(original)
        intent=next(r for r in rows if r.get("stage")=="INTENT")
        at=rows.index(intent);rows[at],rows[at+1]=rows[at+1],rows[at];variants.append(rows)
        rows=copy.deepcopy(original)
        for row in rows:
            if row.get("kind")=="CONSOLE_LIFECYCLE" and row["request"]["command_id"]=="resume-1":
                row["request"]["command_id"]="pause-1"
        variants.append(rows)
        for rows in variants:
            self.reseal(rows)
            with self.assertRaises(ValueError):verify(self.root,allow_control_audit=True)

    def test_missing_policy_or_unresolved_intent_cannot_be_accepted(self):
        rows=self.rows();intent=next(r for r in rows if r.get("stage")=="INTENT")
        with self.assertRaisesRegex(ValueError,"policy"):
            TraceAudit(False).accept(intent,phase="RUNNING",epoch=0,
                  session_id=intent["session_id"],last_total=5,unpaired=False)
        audit=TraceAudit(True)
        audit.accept(intent,phase="RUNNING",epoch=0,session_id=intent["session_id"],last_total=5,unpaired=False)
        with self.assertRaisesRegex(ValueError,"incomplete"):audit.finish()

    def test_removing_complete_pair_cannot_hide_unaudited_pause_or_resume(self):
        original=self.rows()
        for identity in ("pause-1","resume-1"):
            rows=[r for r in copy.deepcopy(original) if not (
                  r.get("kind")=="CONSOLE_LIFECYCLE" and r["request"]["command_id"]==identity)]
            index=0
            for row in rows:
                if row.get("kind")=="CONSOLE_LIFECYCLE":row["index"]=index;index+=1
            self.reseal(rows)
            with self.assertRaisesRegex(ValueError,"unaudited"):verify(self.root,allow_control_audit=True)

    def test_lifecycle_cannot_reuse_executed_control_identity(self):
        rows=self.rows()
        for row in rows:
            if row.get("kind")=="CONSOLE_LIFECYCLE" and row["request"]["command_id"]=="pause-1":
                row["request"]["command_id"]="input-1"
        self.reseal(rows)
        with self.assertRaisesRegex(ValueError,"shared console identity"):verify(self.root,allow_control_audit=True)

    def test_observed_shared_history_capacity_is_bounded(self):
        audit=TraceAudit(True)
        data=(self.root/"control-audit.jsonl").read_bytes()
        audit.identities={f"lifecycle-{i}" for i in range(256)}
        with self.assertRaisesRegex(ValueError,"capacity"):audit.controls(data)
        audit.identities=set();audit.bytes=524288
        with self.assertRaisesRegex(ValueError,"capacity"):audit.controls(data)


if __name__=="__main__":unittest.main()
