import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_product_change_control import accepted_disposition, ready_request
from tools.product.manage_product_change_control import finalize_request, init_request
from tools.product.manage_product_ccb_decision import (
    CCB_SPECS,
    CcbDecisionError,
    finalize_decision,
    init_decision,
    load,
    validate_decision,
    validate_record,
    validate_templates,
)


ROOT = Path(__file__).resolve().parents[1]


def completed_request(domain: str, session: Path) -> None:
    accepted_disposition(domain, session)
    init_request(ROOT, session)
    request = ready_request(domain, session)
    (session / "change-control-request.json").write_text(json.dumps(request), encoding="utf-8")
    finalize_request(ROOT, session)


def ready_ccb(domain: str, session: Path, ccb_decision: str = "APPROVE_IMPLEMENTATION_PLANNING") -> dict:
    data = load(session / "ccb-decision.json")
    data["status"] = "READY_FOR_CCB_VALIDATION"
    data["meeting_id"] = f"CCB-20260928-{domain.upper()}"
    data["chair"] = "synthetic-ccb-member-1"
    data["decided_utc"] = "2026-09-28T12:00:00+00:00"
    data["ccb_decision"] = ccb_decision
    data["rationale"] = "Synthetic unanimous CCB decision for validator testing only."
    data["conditions"] = ["A separate implementation plan remains mandatory"] if ccb_decision == "APPROVE_IMPLEMENTATION_PLANNING" else []
    data["residual_risks"] = ["No implementation or deployment authority has been granted"]
    vote = {"APPROVE_IMPLEMENTATION_PLANNING": "APPROVE", "REJECT": "REJECT", "DEFER": "DEFER"}[ccb_decision]
    data["board_members"] = []
    for index, role in enumerate(data["required_board_roles"], start=1):
        signature = session / f"{domain}-{role}-ccb-signature.txt"
        signature.write_text(f"synthetic CCB signature {role}\n", encoding="utf-8")
        data["board_members"].append({
            "role": role,
            "name": f"synthetic-ccb-member-{index}",
            "voted_utc": f"2026-09-28T12:0{index}:00+00:00",
            "vote": vote,
            "signature_artifact": signature.name,
            "signature_sha256": hashlib.sha256(signature.read_bytes()).hexdigest(),
        })
    data["quorum_complete"] = True
    data["decision"] = {
        "APPROVE_IMPLEMENTATION_PLANNING": "ELIGIBLE_FOR_SEPARATE_IMPLEMENTATION_PLAN",
        "REJECT": "NO_GO_CCB_REJECTED",
        "DEFER": "NO_GO_CCB_DEFERRED",
    }[ccb_decision]
    return data


class ProductCcbDecisionTests(unittest.TestCase):
    def test_five_templates_are_valid_only_as_not_run(self):
        report = validate_templates(ROOT)
        self.assertEqual(report["templates"], 5)
        self.assertEqual(report["decision"], "NO_GO_PENDING_CCB_DECISION")
        for domain, spec in CCB_SPECS.items():
            with self.subTest(domain=domain):
                path = ROOT / spec["template"]
                data = load(path)
                self.assertEqual(validate_decision(data, ROOT, path.parent, allow_not_run=True)["execution_status"], "NOT_RUN")
                with self.assertRaises(CcbDecisionError):
                    validate_decision(data, ROOT, path.parent)

    def test_init_links_completed_request_for_all_domains(self):
        for domain in CCB_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                completed_request(domain, session)
                report = init_decision(ROOT, session)
                self.assertEqual(report["execution_status"], "PREPARED_NOT_RUN")
                self.assertEqual(report["decision"], "NO_GO_PENDING_CCB_DECISION")
                with self.assertRaises(CcbDecisionError):
                    init_decision(ROOT, session)

    def test_five_approvals_stop_before_implementation_plan(self):
        for domain in CCB_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                completed_request(domain, session)
                init_decision(ROOT, session)
                decision = ready_ccb(domain, session)
                (session / "ccb-decision.json").write_text(json.dumps(decision), encoding="utf-8")
                report = validate_decision(decision, ROOT, session)
                self.assertEqual(report["decision"], "ELIGIBLE_FOR_SEPARATE_IMPLEMENTATION_PLAN")
                record = finalize_decision(ROOT, session)
                self.assertEqual(record["decision"], "NO_GO_IMPLEMENTATION_PLAN_NOT_CREATED")
                self.assertEqual(record["next_action"], "CREATE_SEPARATE_IMPLEMENTATION_PLAN")
                self.assertFalse(record["implementation_authorized"])
                self.assertFalse(record["deployment_allowed"])
                self.assertFalse(record["flight_approval"])

    def test_reject_and_defer_remain_no_go(self):
        for ccb_decision, expected in (("REJECT", "NO_GO_CCB_REJECTED"), ("DEFER", "NO_GO_CCB_DEFERRED")):
            with self.subTest(decision=ccb_decision), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / "engine"
                completed_request("engine", session)
                init_decision(ROOT, session)
                decision = ready_ccb("engine", session, ccb_decision)
                (session / "ccb-decision.json").write_text(json.dumps(decision), encoding="utf-8")
                record = finalize_decision(ROOT, session)
                self.assertEqual(record["decision"], expected)
                self.assertEqual(record["status"], "CCB_DECISION_RECORDED_NO_GO")
                self.assertFalse(record["implementation_plan_required"])

    def test_prepared_decision_rejects_premature_claims(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "fpv"
            completed_request("fpv", session)
            init_decision(ROOT, session)
            prepared = load(session / "ccb-decision.json")
            prepared["rationale"] = "premature claim"
            with self.assertRaises(CcbDecisionError):
                validate_decision(prepared, ROOT, session, allow_not_run=True)

    def test_board_is_unanimous_and_independent(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "voice"
            completed_request("voice", session)
            init_decision(ROOT, session)
            split_vote = ready_ccb("voice", session)
            split_vote["board_members"][1]["vote"] = "REJECT"
            with self.assertRaises(CcbDecisionError):
                validate_decision(split_vote, ROOT, session)
            duplicate = ready_ccb("voice", session)
            duplicate["board_members"][1]["name"] = duplicate["board_members"][0]["name"]
            with self.assertRaises(CcbDecisionError):
                validate_decision(duplicate, ROOT, session)
            prior_overlap = ready_ccb("voice", session)
            prior_overlap["board_members"][0]["name"] = "synthetic-intake-authority-1"
            prior_overlap["chair"] = "synthetic-intake-authority-1"
            with self.assertRaises(CcbDecisionError):
                validate_decision(prior_overlap, ROOT, session)

    def test_digests_signatures_and_authority_claims_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "hil"
            completed_request("hil", session)
            init_decision(ROOT, session)
            baseline = ready_ccb("hil", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["request_record_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["board_members"][0]["signature_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["implementation_plan_id"] = "IP-UNAUTHORIZED"
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["implementation_authorized"] = True
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["deployment_allowed"] = True
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["flight_approval"] = True
            mutations.append(item)
            for index, candidate in enumerate(mutations):
                with self.subTest(index=index), self.assertRaises(CcbDecisionError):
                    validate_decision(candidate, ROOT, session)

    def test_record_is_immutable_and_tamper_evident(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "sku"
            completed_request("sku", session)
            init_decision(ROOT, session)
            decision = ready_ccb("sku", session)
            (session / "ccb-decision.json").write_text(json.dumps(decision), encoding="utf-8")
            record = finalize_decision(ROOT, session)
            with self.assertRaises(CcbDecisionError):
                finalize_decision(ROOT, session)
            changed = copy.deepcopy(record)
            changed["implementation_authorized"] = True
            with self.assertRaises(CcbDecisionError):
                validate_record(ROOT, session, changed)
            changed = copy.deepcopy(record)
            changed["conditions"] = ["tampered condition"]
            with self.assertRaises(CcbDecisionError):
                validate_record(ROOT, session, changed)
            decision_path = session / "ccb-decision.json"
            decision_path.write_text(decision_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(CcbDecisionError):
                validate_record(ROOT, session)


if __name__ == "__main__":
    unittest.main()
