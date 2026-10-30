import copy
import unittest
from tools.simulation import session_contract as c
from tools.simulation.pilot_fixture import Audit, INPUTS, PilotFixture, validate_plan


class Journal:
    def __init__(self): self.rows=[]
    def append(self, record): self.rows.append(copy.deepcopy(record))


class Backend:
    def __init__(self):
        self.catalog=set(INPUTS.values()); self.ticks=0; self.values={}; self.writes=0
    def set_test_control(self, control, value):
        self.values[INPUTS[control]]=value; self.writes+=1
    def read(self, prop): return self.values.get(prop)
    def clear_test_controls(self): self.values.clear()


class PilotFixtureTests(unittest.TestCase):
    def setUp(self):
        self.m=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
        self.plan=[dict(apply_tick=1,control="aileron",value=.25)]
        self.backend,self.journal=Backend(),Journal()
        self.now=1000

    def pilot(self):
        return PilotFixture(self.plan,self.m,self.backend,self.journal,clock=lambda:self.now)

    def test_tick_order_readback_and_ack(self):
        p=self.pilot(); self.assertEqual(self.backend.writes,0)
        p.before_step(); self.backend.ticks+=1; p.after_step()
        self.assertEqual(self.journal.rows[-1]["ack"]["status"],"APPLIED")
        self.assertEqual(self.journal.rows[-1]["readback"],.25)
        p.before_step(); self.backend.ticks+=1; p.after_step()
        self.assertEqual(self.backend.writes,1)
        audit=Audit(self.plan,self.m)
        for r in self.journal.rows:audit.record(r,"RUNNING")
        self.assertEqual(audit.finish(),1)

    def test_invalid_plan_limits_and_capabilities(self):
        for plan in ([],self.plan*17,[dict(apply_tick=101,control="aileron",value=0)],
                     [dict(apply_tick=1,control="mixture",value=1)],
                     [dict(apply_tick=True,control="aileron",value=0)],
                     [dict(apply_tick=1,control="aileron",value=float("nan"))],self.plan*2):
            with self.assertRaises(ValueError):validate_plan(plan)
        self.backend.catalog.clear()
        with self.assertRaises(ValueError):self.pilot()
        self.assertEqual(self.backend.writes,0)

    def test_readback_mismatch_never_claims_applied(self):
        p=self.pilot();p.before_step();self.backend.ticks=1
        self.backend.values[INPUTS["aileron"]]=-.25
        with self.assertRaises(ValueError):p.after_step()
        self.assertEqual([r["event"] for r in self.journal.rows],["ADMITTED","INTENT"])

    def test_expired_lease_releases_controls_before_step(self):
        p=self.pilot();p.before_step();self.backend.ticks=1;p.after_step()
        self.now+=5_000_000_000
        p.before_step()
        self.assertEqual(self.backend.values,{})
        self.assertEqual(self.journal.rows[-1]["event"],"RELEASED")
        p.before_step()
        self.assertEqual(sum(r["event"]=="RELEASED" for r in self.journal.rows),1)

    def test_audit_rejects_duplicate_missing_and_modified_application(self):
        p=self.pilot();p.before_step();self.backend.ticks=1;p.after_step()
        for change in ("duplicate","missing","readback","tick","status","request"):
            rows=copy.deepcopy(self.journal.rows)
            if change=="duplicate":rows.append(copy.deepcopy(rows[-1]))
            elif change=="missing":rows.pop(1)
            elif change=="readback":rows[-1]["readback"]=0
            elif change=="tick":rows[-1]["total_ticks"]=2
            elif change=="status":rows[-1]["ack"]["status"]="QUEUED"
            else:rows[-1]["request"]["args"]["value"]=0
            with self.subTest(change=change),self.assertRaises(ValueError):
                audit=Audit(self.plan,self.m)
                for r in rows:audit.record(r,"RUNNING")
                audit.finish()

    def test_journal_failure_prevents_input_write(self):
        p=self.pilot()
        def fail(_):raise OSError("injected audit failure")
        self.journal.append=fail
        with self.assertRaises(OSError):p.before_step()
        self.assertEqual(self.backend.writes,0)


    def test_slow_intent_journal_expires_lease_before_input_write(self):
        p=self.pilot()
        original=self.journal.append
        def slow(record):
            original(record)
            if record["event"]=="INTENT":self.now+=5_000_000_000
        self.journal.append=slow
        with self.assertRaises(ValueError):p.before_step()
        self.assertEqual(self.backend.writes,0)


if __name__=="__main__":unittest.main()
