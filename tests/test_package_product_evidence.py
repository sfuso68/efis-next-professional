import copy
import json
from pathlib import Path
import tempfile
import unittest

from tools.product.package_product_evidence import assemble, load_results, validate_results


ROOT = Path(__file__).resolve().parents[1]
TEMPLATES = {
    "engine": ROOT / "config/product/engine-source-review-results-not-run-v1.json",
    "sku": ROOT / "config/product/sku-release-review-results-not-run-v1.json",
    "fpv": ROOT / "config/video/fpv-benchmark-review-results-not-run-v1.json",
    "voice": ROOT / "config/voice/voice-cabin-review-results-not-run-v1.json",
    "hil": ROOT / "config/hil/hil-execution-review-results-not-run-v1.json",
}


def ready_results(domain: str, bundle: Path) -> dict:
    data = load_results(TEMPLATES[domain])
    data["status"] = "READY_FOR_PACKAGING"
    data["operator_confirmed"] = True
    data["operator"] = "synthetic-test-only"
    data["captured_utc"] = "2026-09-27T10:30:00+00:00"
    data["decision"] = "NO_GO_PENDING_INDEPENDENT_REVIEW"
    for subject in data["subjects"]:
        subject["artifacts"] = {}
        for kind in subject["required_artifacts"]:
            name = f"{subject['id'].lower()}-{kind}.txt"
            (bundle / name).write_text(f"synthetic fixture {subject['id']}/{kind}\n", encoding="utf-8")
            subject["artifacts"][kind] = name
    for kind in data["global_required_artifacts"]:
        name = f"global-{kind}.txt"
        (bundle / name).write_text(f"synthetic global fixture {kind}\n", encoding="utf-8")
        data["global_artifacts"][kind] = name
    return data


class ProductEvidencePackagerTests(unittest.TestCase):
    def test_five_not_run_templates_are_safe_but_not_packagable(self):
        for domain, path in TEMPLATES.items():
            with self.subTest(domain=domain):
                data = load_results(path)
                self.assertEqual(validate_results(data, ROOT, path.parent, allow_not_run=True), [])
                self.assertTrue(any(item.startswith("[PRODUCT-PKG-04]") for item in validate_results(data, ROOT, path.parent)))

    def test_all_five_domains_package_hashed_pending_review_bundles(self):
        for domain in TEMPLATES:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                bundle = Path(temporary)
                results = bundle / "results.json"
                output = bundle / "evidence.json"
                results.write_text(json.dumps(ready_results(domain, bundle)), encoding="utf-8")
                self.assertEqual(assemble(ROOT, results, output), [])
                packaged = json.loads(output.read_text(encoding="utf-8"))
                self.assertEqual(packaged["status"], "PACKAGED_PENDING_REVIEW")
                self.assertEqual(packaged["review_status"], "PENDING")
                self.assertFalse(packaged["hardware_verified"])
                self.assertFalse(packaged["deployment_allowed"])
                self.assertFalse(packaged["flight_approval"])
                artifacts = [item for row in packaged["subjects"] for item in row["artifacts"]] + packaged["global_artifacts"]
                self.assertTrue(artifacts)
                self.assertTrue(all(item["bytes"] > 0 and len(item["sha256"]) == 64 for item in artifacts))

    def test_output_is_immutable(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            results = bundle / "results.json"
            output = bundle / "evidence.json"
            results.write_text(json.dumps(ready_results("engine", bundle)), encoding="utf-8")
            self.assertEqual(assemble(ROOT, results, output), [])
            self.assertTrue(any(item.startswith("[PRODUCT-PKG-19]") for item in assemble(ROOT, results, output)))

    def test_missing_empty_and_escaping_artifacts_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            baseline = ready_results("engine", bundle)
            first = baseline["subjects"][0]["artifacts"]
            kind = next(iter(first))
            cases = ["missing.txt", "empty.txt", "../escape.txt"]
            (bundle / "empty.txt").write_bytes(b"")
            for value in cases:
                with self.subTest(value=value):
                    data = copy.deepcopy(baseline)
                    data["subjects"][0]["artifacts"][kind] = value
                    self.assertTrue(any(item.startswith("[PRODUCT-PKG-12]") for item in validate_results(data, ROOT, bundle)))

    def test_subject_and_artifact_sets_are_frozen(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            subject = ready_results("voice", bundle)
            subject["subjects"][0]["id"] = "unknown"
            self.assertTrue(any(item.startswith("[PRODUCT-PKG-08]") for item in validate_results(subject, ROOT, bundle)))
            artifacts = ready_results("voice", bundle)
            artifacts["subjects"][0]["artifacts"].pop("phrase-review")
            self.assertTrue(any(item.startswith("[PRODUCT-PKG-11]") for item in validate_results(artifacts, ROOT, bundle)))

    def test_confirmation_identity_and_approval_claims_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            mutations = (
                ("operator_confirmed", False, 4),
                ("captured_utc", "local-time", 5),
                ("hardware_evidence_claimed", True, 17),
                ("flight_approval", True, 17),
                ("decision", "GO", 18),
            )
            for key, value, control in mutations:
                with self.subTest(key=key):
                    data = ready_results("sku", bundle)
                    data[key] = value
                    self.assertTrue(any(item.startswith(f"[PRODUCT-PKG-{control:02d}]") for item in validate_results(data, ROOT, bundle)))

    def test_source_digest_drift_fails_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            data = ready_results("hil", bundle)
            data["review_gate_sha256"] = "0" * 64
            self.assertTrue(any(item.startswith("[PRODUCT-PKG-07]") for item in validate_results(data, ROOT, bundle)))

    def test_symlink_artifact_is_rejected_when_supported(self):
        with tempfile.TemporaryDirectory() as temporary:
            bundle = Path(temporary)
            data = ready_results("engine", bundle)
            kind = next(iter(data["subjects"][0]["artifacts"]))
            target = bundle / "real.txt"
            link = bundle / "linked.txt"
            target.write_text("fixture\n", encoding="utf-8")
            try:
                link.symlink_to(target)
            except OSError:
                self.skipTest("symlink creation unavailable on this host")
            data["subjects"][0]["artifacts"][kind] = link.name
            self.assertTrue(any(item.startswith("[PRODUCT-PKG-12]") for item in validate_results(data, ROOT, bundle)))


if __name__ == "__main__":
    unittest.main()
