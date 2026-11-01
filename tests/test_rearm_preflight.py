import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from tools.simulation import rearm_preflight as r
from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode


def fixture():
    p=dict(schema=r.SCHEMA,kind="POLICY",mode=r.MODE,session_id="bench",authority_id="local-console",
        model_sha256="1"*64,source_commit="2"*40,
        leases=[dict(lease_id=f"lease-{i}",epoch=i,wall_limit_ns=5_000_000_000,tick_limit=500) for i in range(3)])
    s=dict(schema=r.SCHEMA,kind="SNAPSHOT",session_id="bench",authority_id="local-console",epoch=0,
        state="PAUSED",revision=7,config_sha256="3"*64,model_sha256=p["model_sha256"],source_commit=p["source_commit"],
        current_lease_id="lease-0",used_leases=["lease-0"],revoked=True,audit_closed=True,audit_sha256="4"*64,
        pending=0,dispatched=0,uncertain=0,history_ids=["old-input"],history_bytes=1024,
        history_sha256="5"*64,simulated=True,flight_approval=False)
    req=dict(schema=r.SCHEMA,kind="PLAN_REARM",command_id="plan-1",resume_command_id="resume-1",
        session_id="bench",expected_epoch=0,expected_revision=7,previous_lease_id="lease-0",next_lease_id="lease-1",
        expected_config_sha256=s["config_sha256"],expected_history_sha256=s["history_sha256"],
        policy_sha256=r.sha(p),confirmed=True)
    return p,s,req


class RearmPreflightTests(unittest.TestCase):
    def setUp(self):self.p,self.s,self.req=fixture()
    def run_plan(self):return r.plan(self.p,self.s,self.req)
    def blocked(self,reason):
        out=self.run_plan();self.assertIn(reason,out["blockers"])
        self.assertIsNone(out["plan"]);self.assertIsNone(out["plan_sha256"])
        self.assertEqual(out["execution"],"NOT_ARMED")

    def test_valid_plan_is_deterministic_nonmutating_and_never_activation(self):
        before=copy.deepcopy((self.p,self.s,self.req));out=self.run_plan()
        self.assertEqual(out["validation"],"PASS_PLAN_ONLY")
        self.assertEqual(out["execution"],"NOT_ARMED")
        self.assertEqual(out,self.run_plan());self.assertEqual(before,(self.p,self.s,self.req))
        self.assertEqual(out["plan_sha256"],r.sha(out["plan"]))
        self.assertEqual(out["plan"]["next_lease"]["epoch"],1)
        self.assertEqual(out["plan"]["additional_history_ids"],2)

    def test_versioned_reference_vector_is_offline_only(self):
        v=c.decode((c.ROOT/"tests/data/protocols/sil-rearm-preflight-v1-vectors.json").read_bytes())
        self.assertFalse(v["flight_approval"])
        self.assertEqual(v["scope"],"OFFLINE_PLAN_ONLY_NOT_ACTIVATION")
        self.assertEqual((v["policy"],v["snapshot"],v["request"]),fixture())
        self.assertEqual(r.plan(v["policy"],v["snapshot"],v["request"])["validation"],v["expected_validation"])

    def test_confirmation_and_paused_state_are_mandatory(self):
        self.req["confirmed"]=False;self.s["state"]="RUNNING"
        self.blocked("EXPLICIT_CONFIRMATION_REQUIRED");self.blocked("NOT_PAUSED")

    def test_open_authority_or_audit_cannot_plan(self):
        for name in ("revoked","audit_closed"):
            self.s[name]=False;self.blocked("OLD_AUTHORITY_OR_AUDIT_OPEN");self.s[name]=True

    def test_pending_dispatched_or_uncertain_blocks(self):
        for name in ("pending","dispatched","uncertain"):
            self.s[name]=1;self.blocked("UNRESOLVED_COMMANDS");self.s[name]=0

    def test_session_and_authority_do_not_come_from_untrusted_request(self):
        self.req["session_id"]="other";self.blocked("SESSION_MISMATCH")
        self.s["authority_id"]="other";self.blocked("AUTHORITY_MISMATCH")

    def test_source_model_config_and_policy_changes_block(self):
        self.s["source_commit"]="6"*40;self.blocked("SOURCE_OR_MODEL_CHANGED")
        self.s["source_commit"]=self.p["source_commit"];self.s["model_sha256"]="6"*64
        self.blocked("SOURCE_OR_MODEL_CHANGED")
        self.s["config_sha256"]="6"*64;self.blocked("CONFIG_CHANGED")
        self.req["policy_sha256"]="6"*64;self.blocked("POLICY_CHANGED")

    def test_changed_revision_history_and_epoch_block_stale_plan(self):
        self.req["expected_revision"]+=1;self.blocked("STALE_SNAPSHOT")
        self.req["expected_revision"]=self.s["revision"];self.req["expected_history_sha256"]="9"*64
        self.blocked("STALE_SNAPSHOT");self.req["expected_epoch"]+=1;self.blocked("EPOCH_CHANGED")

    def test_reuse_skip_or_broken_lease_chain_blocks(self):
        self.req["next_lease_id"]="lease-0";self.blocked("LEASE_NOT_NEXT_UNUSED")
        self.req["next_lease_id"]="lease-2";self.blocked("LEASE_NOT_NEXT_UNUSED")
        self.s["used_leases"]=["lease-1"];self.blocked("LEASE_CHAIN_MISMATCH")

    def test_catalogue_exhaustion_never_indexes_past_end(self):
        self.s.update(used_leases=["lease-0","lease-1","lease-2"],current_lease_id="lease-2",epoch=2)
        self.req.update(previous_lease_id="lease-2",expected_epoch=2)
        self.blocked("LEASE_CATALOGUE_EXHAUSTED")

    def test_global_ids_not_reused_between_plan_and_resume(self):
        for name in ("command_id","resume_command_id"):
            old=self.req[name];self.req[name]="old-input";self.blocked("COMMAND_ID_ALREADY_USED");self.req[name]=old
        self.req["resume_command_id"]=self.req["command_id"];self.blocked("COMMAND_ID_ALREADY_USED")

    def test_history_capacity_counts_both_required_identities_and_bytes(self):
        self.s["history_ids"]=[f"old-{i}" for i in range(255)];self.s["history_bytes"]=10000
        self.blocked("HISTORY_CAPACITY")
        self.s["history_ids"]=[];self.s["history_bytes"]=524000;self.blocked("HISTORY_CAPACITY")

    def test_wrong_types_duplicates_unknown_fields_and_non_sim_fail(self):
        mutations=[("s","epoch",True),("s","revoked",1),("s","simulated",False),
            ("s","flight_approval",True),("s","history_ids",["dup","dup"]),
            ("s","history_sha256","bad"),("req","confirmed",1),("req","extra",1)]
        for obj,key,value in mutations:
            with self.subTest(obj=obj,key=key):
                self.p,self.s,self.req=fixture();target=self.s if obj=="s" else self.req;target[key]=value
                with self.assertRaises(ValueError):self.run_plan()

    def test_policy_exact_catalogue_epoch_and_hard_lease_limits(self):
        for key,value in (("epoch",True),("epoch",2),("wall_limit_ns",5_000_000_001),("tick_limit",501)):
            p=copy.deepcopy(self.p);p["leases"][0][key]=value
            with self.assertRaises(ValueError):r.policy(p)
        self.p["leases"][1]["lease_id"]="lease-0"
        with self.assertRaises(ValueError):r.policy(self.p)
        self.p=fixture()[0];self.p["leases"]*=3
        with self.assertRaises(ValueError):r.policy(self.p)

    def test_pure_plan_does_not_consume_revision_or_create_dedup_state(self):
        a=self.run_plan();b=self.run_plan()
        self.assertEqual(a,b);self.assertEqual(self.s["used_leases"],["lease-0"])
        self.s["revision"]+=1;self.blocked("STALE_SNAPSHOT")

    def test_cli_three_bounded_files_and_invalid_json_exit_codes(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name,obj in (("policy",self.p),("snapshot",self.s),("request",self.req)):
                (root/name).write_bytes(encode(obj))
            args=[sys.executable,"-B","-m","tools.simulation.rearm_preflight"]
            for name in ("policy","snapshot","request"):args.extend(["--"+name,str(root/name)])
            result=subprocess.run(args,capture_output=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stderr)
            self.assertEqual(json.loads(result.stdout)["execution"],"NOT_ARMED")
            self.req["confirmed"]=False;(root/"request").write_bytes(encode(self.req))
            self.assertEqual(subprocess.run(args,capture_output=True,timeout=20).returncode,3)
            (root/"request").write_bytes(b'x'*65537)
            self.assertEqual(subprocess.run(args,capture_output=True,timeout=20).returncode,2)
            for raw in (encode(self.req).rstrip()[:-1]+b',"confirmed":true}',b'{"confirmed":NaN}',b'\xff'):
                (root/"request").write_bytes(raw)
                self.assertEqual(subprocess.run(args,capture_output=True,timeout=20).returncode,2)


if __name__=="__main__":unittest.main()
