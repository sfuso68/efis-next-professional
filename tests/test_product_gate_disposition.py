import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_package_product_evidence import ready_results
from tests.test_product_evidence_review import completed_review
from tools.product.manage_product_evidence_session import finalize_evidence, finalize_review, init_review, init_session
from tools.product.manage_product_gate_disposition import DISPOSITION_SPECS, GateDispositionError, finalize_disposition, init_disposition, load, validate_disposition, validate_record, validate_templates


ROOT = Path(__file__).resolve().parents[1]


def reviewed_session(domain: str, session: Path, accepted: bool = True) -> None:
    init_session(ROOT, domain, session)
    (session / "results.json").write_text(json.dumps(ready_results(domain, session)), encoding="utf-8")
    finalize_evidence(ROOT, session)
    init_review(ROOT, session)
    review = completed_review(domain, session, session / "evidence-bundle.json")
    if not accepted:
        finding = review["required_findings"][0]
        review["findings"][finding]["result"] = "FAIL"
        review["decision"] = "NO_GO_REVIEW_FINDINGS"
    (session / "independent-review.json").write_text(json.dumps(review), encoding="utf-8")
    finalize_review(ROOT, session)


def ready_disposition(domain: str, session: Path, accepted: bool = True) -> dict:
    data = load(session / "gate-disposition.json")
    data["status"] = "READY_FOR_DISPOSITION_VALIDATION"
    data["coordinator"] = "synthetic-disposition-coordinator"
    data["captured_utc"] = "2026-09-28T08:00:00+00:00"
    data["decision_authorities"] = []
    for index, role in enumerate(data["required_authority_roles"], start=1):
        signature = session / f"{domain}-{role}-decision-signature.txt"
        signature.write_text(f"synthetic decision signature {role}\n", encoding="utf-8")
        data["decision_authorities"].append({
            "role": role,
            "name": f"synthetic-authority-{index}",
            "decided_utc": f"2026-09-28T08:0{index}:00+00:00",
            "signature_artifact": signature.name,
            "signature_sha256": hashlib.sha256(signature.read_bytes()).hexdigest(),
        })
    data["disposition"] = "ACCEPT_FOR_CHANGE_CONTROL" if accepted else "REJECT"
    data["justification"] = "synthetic disposition for validator testing only"
    data["conditions"] = ["separate change control remains mandatory"] if accepted else []
    data["residual_risks"] = ["NONE_IDENTIFIED_IN_SYNTHETIC_VALIDATOR_FIXTURE"] if accepted else ["review finding remains open"]
    data["disposition_complete"] = True
    data["decision"] = "ELIGIBLE_FOR_SEPARATE_CHANGE_CONTROL_REQUEST" if accepted else "NO_GO_GATE_REJECTED"
    return data


class ProductGateDispositionTests(unittest.TestCase):
    def test_five_templates_are_valid_only_as_not_run(self):
        report = validate_templates(ROOT)
        self.assertEqual(report["templates"], 5)
        self.assertEqual(report["decision"], "NO_GO_PENDING_GATE_DISPOSITION")
        for domain, spec in DISPOSITION_SPECS.items():
            path = ROOT / spec["template"]
            with self.subTest(domain=domain):
                data = load(path)
                self.assertEqual(validate_disposition(data, ROOT, path.parent, allow_not_run=True)["execution_status"], "NOT_RUN")
                with self.assertRaises(GateDispositionError):
                    validate_disposition(data, ROOT, path.parent)

    def test_init_links_immutable_outcome_for_all_domains(self):
        for domain in DISPOSITION_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                reviewed_session(domain, session)
                report = init_disposition(ROOT, session)
                self.assertEqual(report["execution_status"], "PREPARED_NOT_RUN")
                self.assertEqual(report["decision"], "NO_GO_PENDING_GATE_DISPOSITION")
                with self.assertRaises(GateDispositionError):
                    init_disposition(ROOT, session)

    def test_five_positive_dispositions_stop_before_change_control(self):
        for domain in DISPOSITION_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                reviewed_session(domain, session)
                init_disposition(ROOT, session)
                data = ready_disposition(domain, session)
                (session / "gate-disposition.json").write_text(json.dumps(data), encoding="utf-8")
                report = validate_disposition(data, ROOT, session)
                self.assertEqual(report["decision"], "ELIGIBLE_FOR_SEPARATE_CHANGE_CONTROL_REQUEST")
                record = finalize_disposition(ROOT, session)
                self.assertEqual(record["decision"], "NO_GO_CHANGE_CONTROL_NOT_CREATED")
                self.assertEqual(record["next_action"], "CREATE_SEPARATE_CHANGE_CONTROL_REQUEST")
                self.assertFalse(record["deployment_allowed"])
                self.assertFalse(record["flight_approval"])
                self.assertEqual(validate_record(ROOT, session)["domain"], domain)

    def test_failed_review_cannot_be_accepted_but_can_be_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "voice"
            reviewed_session("voice", session, accepted=False)
            init_disposition(ROOT, session)
            accepted = ready_disposition("voice", session, accepted=True)
            with self.assertRaises(GateDispositionError):
                validate_disposition(accepted, ROOT, session)
            rejected = ready_disposition("voice", session, accepted=False)
            self.assertEqual(validate_disposition(rejected, ROOT, session)["decision"], "NO_GO_GATE_REJECTED")

    def test_authorities_are_distinct_from_each_other_and_reviewers(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "engine"
            reviewed_session("engine", session)
            init_disposition(ROOT, session)
            duplicate = ready_disposition("engine", session)
            duplicate["decision_authorities"][1]["name"] = duplicate["decision_authorities"][0]["name"]
            with self.assertRaises(GateDispositionError):
                validate_disposition(duplicate, ROOT, session)
            overlap = ready_disposition("engine", session)
            overlap["decision_authorities"][0]["name"] = "synthetic-reviewer-1"
            with self.assertRaises(GateDispositionError):
                validate_disposition(overlap, ROOT, session)

    def test_digest_signature_change_control_and_authority_claims_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "hil"
            reviewed_session("hil", session)
            init_disposition(ROOT, session)
            baseline = ready_disposition("hil", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["outcome_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["decision_authorities"][0]["signature_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["change_control_id"] = "CC-UNAUTHORIZED"
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["deployment_allowed"] = True
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["flight_approval"] = True
            mutations.append(item)
            for index, candidate in enumerate(mutations):
                with self.subTest(index=index), self.assertRaises(GateDispositionError):
                    validate_disposition(candidate, ROOT, session)

    def test_record_is_immutable_and_tamper_evident(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "sku"
            reviewed_session("sku", session)
            init_disposition(ROOT, session)
            data = ready_disposition("sku", session)
            (session / "gate-disposition.json").write_text(json.dumps(data), encoding="utf-8")
            record = finalize_disposition(ROOT, session)
            with self.assertRaises(GateDispositionError):
                finalize_disposition(ROOT, session)
            changed = copy.deepcopy(record)
            changed["deployment_allowed"] = True
            with self.assertRaises(GateDispositionError):
                validate_record(ROOT, session, changed)
            changed = copy.deepcopy(record)
            changed["conditions"] = ["tampered condition"]
            with self.assertRaises(GateDispositionError):
                validate_record(ROOT, session, changed)
            disposition_path = session / "gate-disposition.json"
            disposition_path.write_text(disposition_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(GateDispositionError):
                validate_record(ROOT, session)


if __name__ == "__main__":
    unittest.main()
