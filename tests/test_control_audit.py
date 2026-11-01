import copy
import hashlib
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from tools.simulation import session_contract as c
from tools.simulation.control_audit import verify_bytes,FILE,MAX_BYTES
from tools.simulation.session_engine import encode
from tools.simulation.session_trace import verify
from tests.live_control_fixture import run_live


class ControlAuditTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name)/"live"
        self.context=run_live(self.root)
        self.data=(self.root/FILE).read_bytes()
        self.rows=[c.decode(line) for line in self.data.splitlines()]

    def check(self,rows=None,**kwargs):
        data=self.data if rows is None else b"".join(encode(r) for r in rows)
        meta=dict(self.context["metadata"],sha256=hashlib.sha256(data).hexdigest(),
                  bytes=len(data),records=len(data.splitlines()))
        meta.update(kwargs)
        return verify_bytes(data,meta,self.context["manifest"],self.context["policy"],epoch_end_tick=7)

    def test_complete_audit_and_truth_lifecycle_correlation(self):
        report=self.check()
        self.assertEqual((report["applied"],report["rejected"]),(2,1))
        full=verify(self.root,allow_control_audit=True)
        self.assertEqual(full["control_execution"]["status"],"PASS_INTEGRITY_ONLY")
        self.assertEqual(full["epochs"],2)
        with self.assertRaises(ValueError):verify(self.root)

    def test_resealed_field_and_type_mutations_fail_closed(self):
        for kind in ("index","epoch","sim","approval","extra","binding","readback","ack","tick"):
            rows=copy.deepcopy(self.rows)
            applied=next(r for r in rows if r["event"]=="APPLIED")
            if kind=="index":rows[1]["index"]=True
            elif kind=="epoch":rows[1]["epoch"]=False
            elif kind=="sim":rows[1]["simulated"]=False
            elif kind=="approval":rows[1]["flight_approval"]=True
            elif kind=="extra":rows[1]["extension"]=0
            elif kind=="binding":rows[0]["binding"]["manifest"]["source_commit"]="1"*40
            elif kind=="readback":applied["readback"]=-.9
            elif kind=="ack":applied["ack"]["applied_tick"]+=1
            else:rows[-1]["tick"]=8
            with self.subTest(kind=kind),self.assertRaises(ValueError):self.check(rows)

    def test_incomplete_duplicate_uncertain_or_out_of_order_audit_rejected(self):
        for kind in ("missing-intent","missing-applied","duplicate","no-release","uncertain","second-bound"):
            rows=copy.deepcopy(self.rows)
            if kind=="missing-intent":rows.pop(1)
            elif kind=="missing-applied":rows.pop(next(i for i,r in enumerate(rows) if r["event"]=="APPLIED"))
            elif kind=="duplicate":rows.insert(-1,copy.deepcopy(rows[2]))
            elif kind=="no-release":rows.pop()
            elif kind=="uncertain":rows[-1]["uncertain_commands"]=["live-1"]
            else:rows.insert(1,copy.deepcopy(rows[0]))
            for index,row in enumerate(rows):row["index"]=index
            with self.subTest(kind=kind),self.assertRaises(ValueError):self.check(rows)

    def test_metadata_allowlist_hash_and_filename(self):
        for kwargs in (dict(file="../control-audit.jsonl"),dict(file="other.jsonl"),
                       dict(records=True),dict(bytes=0),dict(sha256="0"*64),dict(scope="legacy")):
            with self.subTest(kwargs=kwargs),self.assertRaises(ValueError):self.check(**kwargs)

    def test_frozen_policy_is_exact_not_boolean_equivalent(self):
        for kind in ("missing","authority","extra","bool"):
            policy=copy.deepcopy(self.context["policy"])
            if kind=="missing":policy=None
            elif kind=="authority":policy["authority_id"]="other"
            elif kind=="extra":policy["extension"]=1
            else:policy["lease_until_tick"]=True
            with self.subTest(kind=kind),self.assertRaises(ValueError):
                verify_bytes(self.data,self.context["metadata"],self.context["manifest"],policy,epoch_end_tick=7)

    def test_duplicate_sequence_and_impossible_same_step_order_rejected(self):
        rows=copy.deepcopy(self.rows)
        for row in rows:
            if row.get("request",{}).get("command_id")=="live-2":
                row["request"]["sequence"]=1
                if "ack" in row:row["ack"]["sequence"]=1
        with self.assertRaisesRegex(ValueError,"sequence"):self.check(rows)
        other=Path(self.tmp.name)/"multi";run_live(other,multi=True)
        rows=[c.decode(line) for line in (other/FILE).read_bytes().splitlines()]
        self.assertEqual(self.check(rows)["applied"],3)
        first=next(i for i,r in enumerate(rows) if r["event"]=="INTENT" and r["tick"]==6)
        applied=next(i for i,r in enumerate(rows) if r["event"]=="APPLIED" and r["tick"]==6)
        rows[first+1],rows[applied]=rows[applied],rows[first+1]
        for index,row in enumerate(rows):row["index"]=index
        with self.assertRaisesRegex(ValueError,"phase"):self.check(rows)

    def test_size_truncation_and_duplicate_json_keys(self):
        for data in (self.data[:-1],b"x"*(MAX_BYTES+1),self.data.replace(b'"event":"BOUND"',
                     b'"event":"BOUND","event":"BOUND"',1)):
            meta=dict(self.context["metadata"],bytes=len(data),records=len(data.splitlines()),
                      sha256=hashlib.sha256(data).hexdigest())
            with self.assertRaises(ValueError):verify_bytes(data,meta,self.context["manifest"],
                                                          self.context["policy"],epoch_end_tick=7)

    def test_removing_result_marker_cannot_fall_back_to_legacy(self):
        path=self.root/"result.json";result=c.decode(path.read_bytes())
        result.pop("control_execution");path.write_bytes(encode(result))
        with self.assertRaisesRegex(ValueError,"policy/result"):verify(self.root)

    def test_null_configuration_marker_cannot_be_ignored(self):
        result=c.decode((self.root/"result.json").read_bytes());result.pop("control_execution")
        (self.root/"result.json").write_bytes(encode(result))
        rows=[c.decode(line) for line in (self.root/"truth.jsonl").read_bytes().splitlines()]
        rows[0]["configuration"]["control_policy"]=None
        (self.root/"truth.jsonl").write_bytes(b"".join(encode(r) for r in rows))
        with self.assertRaisesRegex(ValueError,"policy/result"):verify(self.root)

    def test_cli_verifier_file_limits_apply_before_decoding(self):
        with patch("tools.simulation.session_trace.MAX_TRUTH",10):
            with self.assertRaisesRegex(ValueError,"type/size"):
                verify(self.root,allow_control_audit=True)
        (self.root/"result.json").write_bytes(b"x"*65537)
        with self.assertRaisesRegex(ValueError,"type/size"):
            verify(self.root,allow_control_audit=True)


if __name__=="__main__":unittest.main()
