from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools/network/validate_g1_upstream_proposal.py"
SPEC = importlib.util.spec_from_file_location("validate_g1_upstream_proposal", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class G1UpstreamProposalTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.proposal = MODULE.load_json(MODULE.DEFAULT_PROPOSAL)
        cls.network = MODULE.load_json(MODULE.NETWORK_PLAN)
        cls.vertical = MODULE.load_json(MODULE.G2_VERTICAL)
        cls.readiness = MODULE.load_json(MODULE.READINESS)
        cls.icd = MODULE.load_json(MODULE.ICD)
        cls.types = MODULE.TYPES.read_text(encoding="utf-8")

    def validate(self, proposal: dict | None = None, **overrides: object) -> dict:
        return MODULE.validate(
            copy.deepcopy(proposal if proposal is not None else self.proposal),
            copy.deepcopy(overrides.get("network", self.network)),
            copy.deepcopy(overrides.get("vertical", self.vertical)),
            copy.deepcopy(overrides.get("readiness", self.readiness)),
            copy.deepcopy(overrides.get("icd", self.icd)),
            str(overrides.get("types_text", self.types)),
        )

    def test_repository_proposal_validates_and_remains_no_go(self) -> None:
        report = self.validate()
        self.assertEqual(report["controls_validated"], 20)
        self.assertEqual(report["proposal_status"], "PROPOSED_NOT_APPROVED")
        self.assertFalse(report["execution_authorized"])
        self.assertFalse(report["flight_approval"])

    def test_all_control_mutations_fail_closed(self) -> None:
        for index, control in enumerate(MODULE.expected_controls()):
            with self.subTest(control=control["id"]):
                invalid = copy.deepcopy(self.proposal)
                invalid["controls"][index]["state"] = "PASS"
                with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-01"):
                    self.validate(invalid)

    def test_authorization_or_flight_promotion_is_rejected(self) -> None:
        for key in ("execution_authorized", "flight_approval"):
            invalid = copy.deepcopy(self.proposal)
            invalid[key] = True
            with self.assertRaises(MODULE.ProposalError):
                self.validate(invalid)

    def test_edp_reuse_or_command_path_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.proposal)
        invalid["transport"]["port"] = 49003
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-05"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.proposal)
        invalid["transport"]["command_path"] = "PRESENT"
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-05"):
            self.validate(invalid)

    def test_legacy_alias_or_runtime_source_id_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.proposal)
        invalid["source_id"]["legacy_alias_allowed"] = True
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-10"):
            self.validate(invalid)
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-10"):
            self.validate(types_text=self.types + "\nasso5_engine\n")

    def test_invented_rate_or_freshness_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.proposal)
        invalid["transport"]["publish_rate_hz"] = 20
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-05"):
            self.validate(invalid)
        invalid = copy.deepcopy(self.proposal)
        invalid["transport"]["freshness_thresholds"] = {"stale_ms": 250}
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-05"):
            self.validate(invalid)

    def test_network_activation_or_capacity_hiding_is_rejected(self) -> None:
        network = copy.deepcopy(self.network)
        network["flows"].append({"port": 49004})
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-18"):
            self.validate(network=network)
        invalid = copy.deepcopy(self.proposal)
        invalid["network_capacity"]["status"] = "READY"
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-18"):
            self.validate(invalid)

    def test_unresolved_g2_and_r100_boundaries_are_required(self) -> None:
        vertical = copy.deepcopy(self.vertical)
        vertical["topology"]["source_to_blackbox"]["transport"] = "ERAP_V1"
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-19"):
            self.validate(vertical=vertical)
        readiness = copy.deepcopy(self.readiness)
        readiness["unresolved"]["real_source_id"] = "asso5_engine"
        with self.assertRaisesRegex(MODULE.ProposalError, "G1-U20-19"):
            self.validate(readiness=readiness)


if __name__ == "__main__":
    unittest.main()
