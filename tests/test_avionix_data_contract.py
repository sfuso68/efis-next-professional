from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
MODULE_PATH = ROOT / "tools/protocol/validate_data_contract.py"
SPEC = importlib.util.spec_from_file_location("validate_data_contract", MODULE_PATH)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DataContractTests(unittest.TestCase):
    def setUp(self) -> None:
        self.path = ROOT / "config/protocol/avionix-data-contract-v1.json"
        self.document = json.loads(self.path.read_text(encoding="utf-8"))

    def validate_document(self, document: dict) -> dict:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "contract.json"
            path.write_text(json.dumps(document), encoding="utf-8")
            return MODULE.validate(ROOT, path)

    def test_repository_contract_passes(self) -> None:
        self.assertEqual(MODULE.validate(ROOT, self.path)["status"], "PASS")

    def test_wrong_port_fails_closed(self) -> None:
        document = copy.deepcopy(self.document)
        document["contracts"][0]["port"] = 49003
        with self.assertRaisesRegex(MODULE.ContractError, "missing contract flows"):
            self.validate_document(document)

    def test_schema_drift_fails_closed(self) -> None:
        document = copy.deepcopy(self.document)
        document["contracts"][1]["schema_version"] = 2
        with self.assertRaisesRegex(MODULE.ContractError, "schema_version"):
            self.validate_document(document)

    def test_invalid_freshness_fails_closed(self) -> None:
        document = copy.deepcopy(self.document)
        document["contracts"][0]["lost_after_ms"] = 250
        with self.assertRaisesRegex(MODULE.ContractError, "lost_after_ms"):
            self.validate_document(document)

    def test_command_capability_fails_closed(self) -> None:
        document = copy.deepcopy(self.document)
        document["contracts"][1]["command_capability"] = True
        with self.assertRaisesRegex(MODULE.ContractError, "read-only"):
            self.validate_document(document)


if __name__ == "__main__":
    unittest.main()
