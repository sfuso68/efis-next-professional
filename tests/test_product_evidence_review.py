import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_package_product_evidence import TEMPLATES, ready_results
from tools.product.package_product_evidence import assemble
from tools.product.validate_product_evidence_bundle import BundleValidationError, load_bundle, validate_file
from tools.product.validate_product_independent_review import IndependentReviewError, REVIEW_SPECS, load, validate_review, validate_templates


ROOT = Path(__file__).resolve().parents[1]


def packaged_bundle(domain: str, directory: Path) -> Path:
    results = directory / f"{domain}-results.json"
    output = directory / f"{domain}-evidence.json"
    results.write_text(json.dumps(ready_results(domain, directory)), encoding="utf-8")
    errors = assemble(ROOT, results, output)
    if errors:
        raise AssertionError(errors)
    return output


def completed_review(domain: str, directory: Path, bundle: Path) -> dict:
    template = ROOT / REVIEW_SPECS[domain]["template"]
    data = load(template)
    data["status"] = "READY_FOR_REVIEW_VALIDATION"
    data["coordinator"] = "synthetic-review-coordinator"
    data["captured_utc"] = "2026-09-27T15:00:00+00:00"
    data["evidence_bundle_path"] = bundle.name
    data["evidence_bundle_sha256"] = hashlib.sha256(bundle.read_bytes()).hexdigest()
    data["reviewers"] = []
    for index, role in enumerate(data["required_review_roles"], start=1):
        signature = directory / f"{domain}-{role}-signature.txt"
        signature.write_text(f"synthetic signature for {role}\n", encoding="utf-8")
        data["reviewers"].append({
            "role": role,
            "name": f"synthetic-reviewer-{index}",
            "reviewed_utc": f"2026-09-27T15:0{index}:00+00:00",
            "signature_artifact": signature.name,
            "signature_sha256": hashlib.sha256(signature.read_bytes()).hexdigest(),
        })
    data["findings"] = {finding: {"result": "PASS", "note": f"synthetic review of {finding}"} for finding in data["required_findings"]}
    data["evidence_integrity_verified"] = True
    data["review_complete"] = True
    data["decision"] = "EVIDENCE_ACCEPTED_FOR_NEXT_GATE"
    return data


class ProductEvidenceReviewTests(unittest.TestCase):
    def test_bundle_schema_is_closed_and_fail_closed(self):
        schema = json.loads((ROOT / "config/product/product-evidence-bundle-v1.schema.json").read_text(encoding="utf-8"))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"]["status"]["const"], "PACKAGED_PENDING_REVIEW")
        self.assertEqual(schema["properties"]["hardware_verified"]["const"], False)
        self.assertEqual(schema["properties"]["deployment_allowed"]["const"], False)
        self.assertEqual(schema["properties"]["flight_approval"]["const"], False)

    def test_all_five_packaged_domains_validate_every_artifact(self):
        for domain in TEMPLATES:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                report = validate_file(ROOT, packaged_bundle(domain, Path(temporary)))
                self.assertEqual(report["domain"], domain)
                self.assertEqual(report["review_status"], "PENDING")
                self.assertFalse(report["hardware_verified"])

    def test_bundle_tamper_source_drift_and_authority_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            bundle = packaged_bundle("engine", directory)
            baseline = load_bundle(bundle)
            mutations = (
                lambda data: data.update({"status": "PASS"}),
                lambda data: data["source"].update({"intake_sha256": "0" * 64}),
                lambda data: data["subjects"][0]["artifacts"][0].update({"sha256": "0" * 64}),
                lambda data: data.update({"hardware_verified": True}),
                lambda data: data.update({"deployment_allowed": True}),
                lambda data: data.update({"flight_approval": True}),
            )
            for index, mutation in enumerate(mutations):
                with self.subTest(index=index):
                    changed = copy.deepcopy(baseline)
                    mutation(changed)
                    candidate = directory / f"tampered-{index}.json"
                    candidate.write_text(json.dumps(changed), encoding="utf-8")
                    with self.assertRaises(BundleValidationError):
                        validate_file(ROOT, candidate)

    def test_five_review_templates_pass_only_as_not_run(self):
        report = validate_templates(ROOT)
        self.assertEqual(report["templates"], 5)
        self.assertEqual(report["execution_status"], "NOT_RUN")
        for domain, spec in REVIEW_SPECS.items():
            path = ROOT / spec["template"]
            with self.subTest(domain=domain):
                review = load(path)
                self.assertEqual(validate_review(review, ROOT, path.parent, allow_not_run=True)["decision"], "NO_GO_NOT_RUN")
                with self.assertRaises(IndependentReviewError):
                    validate_review(review, ROOT, path.parent)

    def test_all_five_completed_synthetic_reviews_stop_at_next_gate(self):
        for domain in REVIEW_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                directory = Path(temporary)
                bundle = packaged_bundle(domain, directory)
                report = validate_review(completed_review(domain, directory, bundle), ROOT, directory)
                self.assertEqual(report["decision"], "EVIDENCE_ACCEPTED_FOR_NEXT_GATE")
                self.assertEqual(report["reviewers"], 3)
                self.assertFalse(report["hardware_verified"])
                self.assertFalse(report["deployment_allowed"])
                self.assertFalse(report["flight_approval"])

    def test_failed_finding_requires_no_go(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            bundle = packaged_bundle("fpv", directory)
            review = completed_review("fpv", directory, bundle)
            first = review["required_findings"][0]
            review["findings"][first]["result"] = "FAIL"
            with self.assertRaises(IndependentReviewError):
                validate_review(review, ROOT, directory)
            review["decision"] = "NO_GO_REVIEW_FINDINGS"
            self.assertEqual(validate_review(review, ROOT, directory)["decision"], "NO_GO_REVIEW_FINDINGS")

    def test_reviewer_independence_signature_and_bundle_digest_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            bundle = packaged_bundle("sku", directory)
            baseline = completed_review("sku", directory, bundle)
            cases = []
            duplicate = copy.deepcopy(baseline)
            duplicate["reviewers"][1]["name"] = duplicate["reviewers"][0]["name"]
            cases.append(duplicate)
            signature = copy.deepcopy(baseline)
            signature["reviewers"][0]["signature_sha256"] = "0" * 64
            cases.append(signature)
            digest = copy.deepcopy(baseline)
            digest["evidence_bundle_sha256"] = "0" * 64
            cases.append(digest)
            authority = copy.deepcopy(baseline)
            authority["deployment_allowed"] = True
            cases.append(authority)
            for index, candidate in enumerate(cases):
                with self.subTest(index=index), self.assertRaises(IndependentReviewError):
                    validate_review(candidate, ROOT, directory)


if __name__ == "__main__":
    unittest.main()
