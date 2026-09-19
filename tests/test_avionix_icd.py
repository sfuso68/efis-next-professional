import copy
import importlib.util
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/protocol/validate_avionix_icd.py"
BUNDLE_PATH = ROOT / "docs/status/evidence/g2-icd-freeze-workstation-20260917/evidence.json"
SPEC = importlib.util.spec_from_file_location("validate_avionix_icd", MODULE_PATH)
assert SPEC and SPEC.loader
icd = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(icd)


class AvionixIcdTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.contract = icd.load_json(icd.DEFAULT_SPEC)
        cls.vectors = icd.load_json(icd.DEFAULT_VECTORS)
        cls.evidence = icd.load_json(icd.DEFAULT_EVIDENCE_SCHEMA)
        cls.bundle = icd.load_json(BUNDLE_PATH)

    def test_crc32c_known_vector(self):
        self.assertEqual(icd.crc32c(b"123456789"), 0xE3069283)

    def test_frozen_contract_is_structurally_valid(self):
        self.assertEqual(icd.validate_spec(self.contract), [])

    def test_canonical_vectors_match_frozen_digests(self):
        errors, actual = icd.validate_vectors(self.contract, self.vectors)
        self.assertEqual(errors, [])
        self.assertEqual(actual["DDP_V2"]["wire_size"], 529)
        self.assertEqual(actual["EDP_V1"]["wire_size"], 694)

    def test_vector_mutation_fails_closed(self):
        altered = copy.deepcopy(self.vectors)
        altered["vectors"]["DDP_V2"]["wire_sha256"] = "0" * 64
        errors, _ = icd.validate_vectors(self.contract, altered)
        self.assertTrue(any("wire_sha256" in item for item in errors))

    def test_layout_gap_fails_closed(self):
        altered = copy.deepcopy(self.contract)
        altered["protocols"]["EDP_V1"]["sample"][1][1] = 3
        self.assertTrue(any("offset" in item for item in icd.validate_spec(altered)))

    def test_enum_renumbering_fails_closed(self):
        altered = copy.deepcopy(self.contract)
        altered["enumerations"]["unit"]["pascal"] = 20
        self.assertTrue(any("enumerations" in item for item in icd.validate_spec(altered)))

    def test_evidence_schema_is_fail_closed(self):
        self.assertEqual(icd.validate_evidence_schema(self.evidence), [])
        altered = copy.deepcopy(self.evidence)
        altered["properties"]["flight_approval"]["const"] = True
        self.assertTrue(icd.validate_evidence_schema(altered))

    def test_reference_evidence_bundle_and_artifact_digest(self):
        self.assertEqual(icd.validate_evidence_bundle(self.bundle), [])

    def test_evidence_digest_mutation_fails_closed(self):
        altered = copy.deepcopy(self.bundle)
        altered["artifacts"][0]["sha256"] = "0" * 64
        self.assertTrue(any("sha256 mismatch" in item for item in icd.validate_evidence_bundle(altered)))

    def test_evidence_path_escape_fails_closed(self):
        altered = copy.deepcopy(self.bundle)
        altered["artifacts"][0]["path"] = "../secret"
        self.assertTrue(any("escapes repository" in item for item in icd.validate_evidence_bundle(altered)))

    def test_malformed_requirement_ids_fail_closed_without_exception(self):
        altered = copy.deepcopy(self.bundle)
        altered["requirement_ids"] = [{"not": "an id"}]
        self.assertTrue(any("requirement_ids" in item for item in icd.validate_evidence_bundle(altered)))


if __name__ == "__main__":
    unittest.main()
