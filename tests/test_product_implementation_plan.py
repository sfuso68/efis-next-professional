import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from tests.test_product_ccb_decision import completed_request, ready_ccb
from tools.product.manage_product_ccb_decision import finalize_decision, init_decision
from tools.product.manage_product_implementation_plan import (
    PLAN_SPECS,
    ImplementationPlanError,
    finalize_plan,
    init_plan,
    load,
    validate_plan,
    validate_record,
    validate_templates,
)


ROOT = Path(__file__).resolve().parents[1]


def approved_ccb(domain: str, session: Path) -> None:
    completed_request(domain, session)
    init_decision(ROOT, session)
    decision = ready_ccb(domain, session)
    (session / "ccb-decision.json").write_text(json.dumps(decision), encoding="utf-8")
    finalize_decision(ROOT, session)


def ready_plan(domain: str, session: Path) -> dict:
    data = load(session / "implementation-plan.json")
    data["status"] = "READY_FOR_PLAN_VALIDATION"
    data["plan_id"] = f"IMP-20260929-{domain.upper()}"
    data["owner"] = "synthetic-plan-owner"
    data["prepared_utc"] = "2026-09-29T08:00:00+00:00"
    data["baseline_branch"] = "work/synthetic-plan"
    data["baseline_commit"] = "a" * 40
    data["objective"] = f"Prepare a bounded synthetic {domain} implementation for validator testing only."
    data["in_scope_requirements"] = [PLAN_SPECS[domain]["requirement"]]
    data["in_scope_artifacts"] = [PLAN_SPECS[domain]["template"]]
    data["in_scope_nodes"] = [f"synthetic-{domain}-node"]
    data["out_of_scope"] = ["production deployment", "flight use", "unreviewed hardware changes"]
    data["work_packages"] = [{
        "id": f"WP-{domain.upper()}-01",
        "description": "Implement the bounded change only after separate authorization.",
        "artifacts": [PLAN_SPECS[domain]["template"]],
        "verification": "Run deterministic unit and negative tests.",
        "evidence": "Store logs, hashes and review outcome in an immutable bundle.",
        "exit_criteria": "All planned tests pass and no operational authority is claimed.",
    }]
    data["hazard_controls"] = ["Fail closed on missing data", "Keep simulation and live provenance visible"]
    data["verification_plan"] = "Execute unit, integration, target and rollback tests under separate authorization."
    data["rollback_plan"] = "Restore the preceding immutable release and verify its manifest and health."
    data["deployment_plan"] = "No deployment is permitted by this plan; a later deployment gate is mandatory."
    data["plan_reviewers"] = []
    for index, role in enumerate(data["required_plan_roles"], start=1):
        signature = session / f"{domain}-{role}-plan-signature.txt"
        signature.write_text(f"synthetic plan signature {role}\n", encoding="utf-8")
        data["plan_reviewers"].append({
            "role": role,
            "name": f"synthetic-plan-reviewer-{index}",
            "reviewed_utc": f"2026-09-29T08:0{index}:00+00:00",
            "signature_artifact": signature.name,
            "signature_sha256": hashlib.sha256(signature.read_bytes()).hexdigest(),
        })
    data["plan_complete"] = True
    data["decision"] = "IMPLEMENTATION_PLAN_RECORDED_PENDING_AUTHORIZATION"
    return data


class ProductImplementationPlanTests(unittest.TestCase):
    def test_five_templates_are_valid_only_as_not_run(self):
        report = validate_templates(ROOT)
        self.assertEqual(report["templates"], 5)
        self.assertEqual(report["decision"], "NO_GO_PENDING_IMPLEMENTATION_PLAN")
        for domain, spec in PLAN_SPECS.items():
            with self.subTest(domain=domain):
                path = ROOT / spec["template"]
                data = load(path)
                self.assertEqual(validate_plan(data, ROOT, path.parent, allow_not_run=True)["execution_status"], "NOT_RUN")
                with self.assertRaises(ImplementationPlanError):
                    validate_plan(data, ROOT, path.parent)

    def test_init_links_approved_ccb_for_all_domains(self):
        for domain in PLAN_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                approved_ccb(domain, session)
                report = init_plan(ROOT, session)
                self.assertEqual(report["execution_status"], "PREPARED_NOT_RUN")
                self.assertEqual(report["decision"], "NO_GO_PENDING_IMPLEMENTATION_PLAN")
                with self.assertRaises(ImplementationPlanError):
                    init_plan(ROOT, session)

    def test_five_complete_plans_stop_before_authorization(self):
        for domain in PLAN_SPECS:
            with self.subTest(domain=domain), tempfile.TemporaryDirectory() as temporary:
                session = Path(temporary) / domain
                approved_ccb(domain, session)
                init_plan(ROOT, session)
                plan = ready_plan(domain, session)
                (session / "implementation-plan.json").write_text(json.dumps(plan), encoding="utf-8")
                report = validate_plan(plan, ROOT, session)
                self.assertEqual(report["decision"], "IMPLEMENTATION_PLAN_RECORDED_PENDING_AUTHORIZATION")
                record = finalize_plan(ROOT, session)
                self.assertEqual(record["decision"], "NO_GO_IMPLEMENTATION_AUTHORIZATION_NOT_CREATED")
                self.assertEqual(record["next_action"], "CONVENE_SEPARATE_IMPLEMENTATION_AUTHORIZATION_REVIEW")
                self.assertFalse(record["implementation_authorized"])
                self.assertFalse(record["deployment_allowed"])
                self.assertFalse(record["flight_approval"])

    def test_prepared_plan_rejects_premature_scope(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "fpv"
            approved_ccb("fpv", session)
            init_plan(ROOT, session)
            prepared = load(session / "implementation-plan.json")
            prepared["in_scope_nodes"] = ["premature-node"]
            with self.assertRaises(ImplementationPlanError):
                validate_plan(prepared, ROOT, session, allow_not_run=True)

    def test_incomplete_scope_work_package_and_baseline_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "engine"
            approved_ccb("engine", session)
            init_plan(ROOT, session)
            baseline = ready_plan("engine", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["baseline_commit"] = "not-a-commit"
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["in_scope_requirements"] = []
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["work_packages"][0]["artifacts"] = ["../escape"]
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["work_packages"][0]["exit_criteria"] = None
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["rollback_plan"] = None
            mutations.append(item)
            for index, candidate in enumerate(mutations):
                with self.subTest(index=index), self.assertRaises(ImplementationPlanError):
                    validate_plan(candidate, ROOT, session)

    def test_reviewers_are_independent_from_prior_people(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "voice"
            approved_ccb("voice", session)
            init_plan(ROOT, session)
            duplicate = ready_plan("voice", session)
            duplicate["plan_reviewers"][1]["name"] = duplicate["plan_reviewers"][0]["name"]
            with self.assertRaises(ImplementationPlanError):
                validate_plan(duplicate, ROOT, session)
            overlap = ready_plan("voice", session)
            overlap["plan_reviewers"][0]["name"] = "synthetic-ccb-member-1"
            with self.assertRaises(ImplementationPlanError):
                validate_plan(overlap, ROOT, session)

    def test_digests_signatures_and_authority_claims_fail_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "hil"
            approved_ccb("hil", session)
            init_plan(ROOT, session)
            baseline = ready_plan("hil", session)
            mutations = []
            item = copy.deepcopy(baseline)
            item["ccb_record_sha256"] = "0" * 64
            mutations.append(item)
            item = copy.deepcopy(baseline)
            item["plan_reviewers"][0]["signature_sha256"] = "0" * 64
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
                with self.subTest(index=index), self.assertRaises(ImplementationPlanError):
                    validate_plan(candidate, ROOT, session)

    def test_record_is_immutable_and_tamper_evident(self):
        with tempfile.TemporaryDirectory() as temporary:
            session = Path(temporary) / "sku"
            approved_ccb("sku", session)
            init_plan(ROOT, session)
            plan = ready_plan("sku", session)
            (session / "implementation-plan.json").write_text(json.dumps(plan), encoding="utf-8")
            record = finalize_plan(ROOT, session)
            with self.assertRaises(ImplementationPlanError):
                finalize_plan(ROOT, session)
            changed = copy.deepcopy(record)
            changed["implementation_authorized"] = True
            with self.assertRaises(ImplementationPlanError):
                validate_record(ROOT, session, changed)
            changed = copy.deepcopy(record)
            changed["work_packages"][0]["description"] = "tampered"
            with self.assertRaises(ImplementationPlanError):
                validate_record(ROOT, session, changed)
            plan_path = session / "implementation-plan.json"
            plan_path.write_text(plan_path.read_text(encoding="utf-8") + "\n", encoding="utf-8")
            with self.assertRaises(ImplementationPlanError):
                validate_record(ROOT, session)


if __name__ == "__main__":
    unittest.main()
