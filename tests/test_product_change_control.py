import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_product_gate_disposition import ready_disposition, reviewed_session
from tools.product.manage_product_gate_disposition import finalize_disposition, init_disposition
from tools.product.manage_product_change_control import (
    CHANGE_SPECS,
    ChangeControlError,
    finalize_request,
    init_request,
    load,
    validate_record,
    validate_request,
    validate_templates,
)


ROOT = Path(__file__).resolve().parents[1]


def accepted_disposition(domain: str, session: Path) -> None:
    reviewed_session(domain, session)
    init_disposition(ROOT, session)
    disposition = ready_disposition(domain, session)
    (session / "gate-disposition.json").write_text(json.dumps(disposition), encoding="utf-8")
    finalize_disposition(ROOT, session)


def ready_request(domain: str, session: Path) -> dict:
    data = load(session / "change-control-request.json")
    data["status"] = "READY_FOR_REQUEST_VALIDATION"
    data["request_id"] = f"CCR-20260928-{domain.upper()}"
    data["proposer"] = "synthetic-change-proposer"
    data["submitted_utc"] = "2026-09-28T10:00:00+00:00"
    data["title"] = f"Synthetic {domain} change request"
    data["change_summary"] = "Validator-only proposed change with no implementation authority."
    data["affected_requirements"] = [CHANGE_SPECS[domain]["requirement"]]
    data["affected_artifacts"] = [CHANGE_SPECS[domain]["template"]]
    data["affected_nodes"] = [f"synthetic-{domain}-node"]
    data["exclusions"] = ["production deployment", "flight use"]
    data["impact_analysis"] = {
        "safety": "No safety authority is granted by request intake.",
        "architecture": "The proposed boundary remains within the declared domain.",
        "configuration": "All future artifacts require configuration control.",
        "cybersecurity": "No new network trust is created by request intake.",
        "human_factors": "Operator effects require a separate review before implementation.",
        "operations": "No operational use is authorized.",
        "verification": "A separate verification plan and evidence gate remain mandatory.",
    }
    data["rollback_strategy"] = "Restore the preceding immutable release after a separate approved implementation."
    data["verification_plan"] = "Run unit, integration, target and negative tests under a separately approved plan."
    data["intake_authorities"] = []
    for index, role in enumerate(data["required_intake_roles"], start=1):
        signature = session / f"{domain}-{role}-intake-signature.txt"
        signature.write_text(f"synthetic intake signature {role}\n", encoding="utf-8")
        data["intake_authorities"].append({
            "role": role,
            "name": f"synthetic-intake-authority-{index}",
            "reviewed_utc": f"2026-09-28T10:0{index}:00+00:00",
            "signature_artifact": signature.name,
            "signature_sha256": hashlib.sha256(signature.read_bytes()).hexdigest(),
        })
    data["request_complete"] = True
    data["decision"] = "CHANGE_CONTROL_REQUEST_RECORDED_PENDING_CCB_DECISION"
    return data


class ProductChangeControlTests(unittest.TestCase):
    def test_five_templates_are_valid_only_as_not_run(self):
        report = validate_templates(ROOT)
        self.assertEqual(report["templates"], 5)
        self.assertEqual(report["decision"], "NO_GO_PENDING_CHANGE_CONTROL_REQUEST")
        for domain, spec in CHANGE_SPECS.items():
            with self.subTest(domain=domain):
                path = ROOT / spec["template"]
                data = load(path)
                self.assertEqual(validate_request(data, ROOT, path.parent, allow_not_run=True)["execution_status"], "NOT_RUN")
                with self.assertRaises(ChangeControlError):
                    validate_request(data, ROOT, path.parent)

    def test_init_links_accepted_disposition_for_all_domains(self):
        for domain in CHANGE_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                accepted_disposition(domain, session)
                report = init_request(ROOT, session)
                self.assertEqual(report["execution_status"], "PREPARED_NOT_RUN")
                self.assertEqual(report["decision"], "NO_GO_PENDING_CHANGE_CONTROL_REQUEST")
                with self.assertRaises(ChangeControlError):
                    init_request(ROOT, session)

    def test_prepared_request_rejects_premature_scope_and_impact_claims(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "fpv"
            accepted_disposition("fpv", session)
            init_request(ROOT, session)
            prepared = load(session / "change-control-request.json")
            prepared["affected_nodes"] = ["premature-node-claim"]
            with self.assertRaises(ChangeControlError):
                validate_request(prepared, ROOT, session, allow_not_run=True)
            prepared = load(session / "change-control-request.json")
            prepared["impact_analysis"]["safety"] = "premature impact claim"
            with self.assertRaises(ChangeControlError):
                validate_request(prepared, ROOT, session, allow_not_run=True)

    def test_five_complete_requests_stop_before_ccb_decision(self):
        for domain in CHANGE_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                accepted_disposition(domain, session)
                init_request(ROOT, session)
                request = ready_request(domain, session)
                (session / "change-control-request.json").write_text(json.dumps(request), encoding="utf-8")
                report = validate_request(request, ROOT, session)
                self.assertEqual(report["decision"], "CHANGE_CONTROL_REQUEST_RECORDED_PENDING_CCB_DECISION")
                record = finalize_request(ROOT, session)
                self.assertEqual(record["decision"], "NO_GO_IMPLEMENTATION_NOT_AUTHORIZED")
                self.assertEqual(record["next_action"], "CONVENE_SEPARATE_CCB_DECISION")
                self.assertFalse(record["implementation_authorized"])
                self.assertFalse(record["deployment_allowed"])
                self.assertFalse(record["flight_approval"])

    def test_incomplete_impact_scope_and_rollback_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "engine"
            accepted_disposition("engine", session)
            init_request(ROOT, session)
            baseline = ready_request("engine", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["impact_analysis"]["safety"] = None
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["affected_requirements"] = []
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["affected_artifacts"] = ["../escape"]
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["rollback_strategy"] = None
            mutations.append(item)
            for index, candidate in enumerate(mutations):
                with self.subTest(index=index), self.assertRaises(ChangeControlError):
                    validate_request(candidate, ROOT, session)

    def test_intake_authorities_are_independent_from_prior_people(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "voice"
            accepted_disposition("voice", session)
            init_request(ROOT, session)
            duplicate = ready_request("voice", session)
            duplicate["intake_authorities"][1]["name"] = duplicate["intake_authorities"][0]["name"]
            with self.assertRaises(ChangeControlError):
                validate_request(duplicate, ROOT, session)
            reviewer_overlap = ready_request("voice", session)
            reviewer_overlap["intake_authorities"][0]["name"] = "synthetic-reviewer-1"
            with self.assertRaises(ChangeControlError):
                validate_request(reviewer_overlap, ROOT, session)
            disposition_overlap = ready_request("voice", session)
            disposition_overlap["intake_authorities"][0]["name"] = "synthetic-authority-1"
            with self.assertRaises(ChangeControlError):
                validate_request(disposition_overlap, ROOT, session)

    def test_digests_signatures_and_authority_claims_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "hil"
            accepted_disposition("hil", session)
            init_request(ROOT, session)
            baseline = ready_request("hil", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["disposition_record_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["intake_authorities"][0]["signature_sha256"] = "0" * 64
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
                with self.subTest(index=index), self.assertRaises(ChangeControlError):
                    validate_request(candidate, ROOT, session)

    def test_record_is_immutable_and_tamper_evident(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "sku"
            accepted_disposition("sku", session)
            init_request(ROOT, session)
            request = ready_request("sku", session)
            (session / "change-control-request.json").write_text(json.dumps(request), encoding="utf-8")
            record = finalize_request(ROOT, session)
            with self.assertRaises(ChangeControlError):
                finalize_request(ROOT, session)
            changed = copy.deepcopy(record)
            changed["implementation_authorized"] = True
            with self.assertRaises(ChangeControlError):
                validate_record(ROOT, session, changed)
            changed = copy.deepcopy(record)
            changed["verification_plan"] = "tampered plan"
            with self.assertRaises(ChangeControlError):
                validate_record(ROOT, session, changed)
            request_path = session / "change-control-request.json"
            request_path.write_text(request_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(ChangeControlError):
                validate_record(ROOT, session)


if __name__ == "__main__":
    unittest.main()
