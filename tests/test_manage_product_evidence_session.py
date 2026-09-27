import copy
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_package_product_evidence import ready_results
from tests.test_product_evidence_review import completed_review
from tools.product.manage_product_evidence_session import finalize_evidence, finalize_review, init_review, init_session, session_status, validate_outcome
from tools.product.validate_product_independent_review import load as load_review, validate_review


ROOT = Path(__file__).resolve().parents[1]


class ProductEvidenceSessionTests(unittest.TestCase):
    def test_init_creates_closed_not_run_session_for_all_domains(self):
        for domain in ("engine", "sku", "fpv", "voice", "hil"):
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                report = init_session(ROOT, domain, session)
                self.assertEqual(report["status"], "NOT_RUN")
                self.assertTrue((session / "results.json").is_file())
                self.assertTrue((session / "artifacts").is_dir())
                status = session_status(ROOT, session)
                self.assertEqual(status["status"], "NOT_RUN")
                self.assertTrue(status["missing_artifacts"])
                self.assertFalse(status["hardware_verified"])

    def test_init_never_overwrites_existing_session(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "session"
            init_session(ROOT, "engine", session)
            with self.assertRaises(ValueError):
                init_session(ROOT, "engine", session)

    def test_capture_status_reports_missing_and_unsafe_without_packaging(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "session"
            init_session(ROOT, "engine", session)
            results_path = session / "results.json"
            data = json.loads(results_path.read_text(encoding="utf-8"))
            data["status"] = "CAPTURING"
            data["subjects"][0]["artifacts"]["data-plate-photo"] = "missing.jpg"
            results_path.write_text(json.dumps(data), encoding="utf-8")
            report = session_status(ROOT, session)
            self.assertEqual(report["status"], "CAPTURING")
            self.assertIn("ROTAX_912_UL/data-plate-photo", report["unsafe_artifacts"])
            self.assertFalse(report["ready_for_packaging"])
            self.assertFalse((session / "evidence-bundle.json").exists())

    def test_unconfirmed_session_cannot_finalize(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "session"
            init_session(ROOT, "sku", session)
            with self.assertRaises(ValueError):
                finalize_evidence(ROOT, session)

    def test_complete_five_domain_pipeline_stops_at_gate_disposition(self):
        for domain in ("engine", "sku", "fpv", "voice", "hil"):
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                init_session(ROOT, domain, session)
                (session / "results.json").write_text(json.dumps(ready_results(domain, session)), encoding="utf-8")
                evidence = finalize_evidence(ROOT, session)
                self.assertEqual(evidence["status"], "PACKAGED_PENDING_REVIEW")
                prepared = init_review(ROOT, session)
                self.assertEqual(prepared["status"], "PREPARED_NOT_RUN")
                review_path = session / "independent-review.json"
                prepared_data = load_review(review_path)
                self.assertEqual(validate_review(prepared_data, ROOT, session, allow_not_run=True)["execution_status"], "PREPARED_NOT_RUN")
                bundle = session / "evidence-bundle.json"
                review_path.write_text(json.dumps(completed_review(domain, session, bundle)), encoding="utf-8")
                outcome = finalize_review(ROOT, session)
                self.assertEqual(outcome["status"], "REVIEWED_PENDING_GATE_DISPOSITION")
                self.assertTrue(outcome["next_gate_eligible"])
                self.assertEqual(outcome["decision"], "NO_GO_PENDING_GATE_DISPOSITION")
                self.assertFalse(outcome["hardware_verified"])
                self.assertFalse(outcome["deployment_allowed"])
                self.assertFalse(outcome["flight_approval"])
                self.assertEqual(validate_outcome(ROOT, session)["domain"], domain)

    def test_review_and_outcome_are_immutable_and_tamper_evident(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "session"
            init_session(ROOT, "engine", session)
            (session / "results.json").write_text(json.dumps(ready_results("engine", session)), encoding="utf-8")
            finalize_evidence(ROOT, session)
            init_review(ROOT, session)
            with self.assertRaises(ValueError):
                init_review(ROOT, session)
            review_path = session / "independent-review.json"
            review_path.write_text(json.dumps(completed_review("engine", session, session / "evidence-bundle.json")), encoding="utf-8")
            outcome = finalize_review(ROOT, session)
            with self.assertRaises(ValueError):
                finalize_review(ROOT, session)
            changed = copy.deepcopy(outcome)
            changed["deployment_allowed"] = True
            with self.assertRaises(ValueError):
                validate_outcome(ROOT, session, changed)
            review_path.write_text(review_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(ValueError):
                validate_outcome(ROOT, session)

    def test_failed_review_remains_no_go(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "session"
            init_session(ROOT, "voice", session)
            (session / "results.json").write_text(json.dumps(ready_results("voice", session)), encoding="utf-8")
            finalize_evidence(ROOT, session)
            init_review(ROOT, session)
            review = completed_review("voice", session, session / "evidence-bundle.json")
            finding = review["required_findings"][0]
            review["findings"][finding]["result"] = "FAIL"
            review["decision"] = "NO_GO_REVIEW_FINDINGS"
            (session / "independent-review.json").write_text(json.dumps(review), encoding="utf-8")
            outcome = finalize_review(ROOT, session)
            self.assertFalse(outcome["next_gate_eligible"])
            self.assertEqual(outcome["decision"], "NO_GO_REVIEW_FINDINGS")


if __name__ == "__main__":
    unittest.main()
