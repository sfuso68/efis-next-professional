import copy
import json
import pathlib
import tempfile
import unittest

from tools.deployment.prepare_paired_display_release import atomic_write, prepare


ROOT = pathlib.Path(__file__).parents[1]


def transition(role: str, commit: str = "a" * 40) -> dict:
    profiles = {
        "PFD": ("efis-pfd-01", "user", "efis-pfd.service", "/home/efis/efis-next-current"),
        "MFD": ("pi5", "system", "efis-next-mfd-direct.service", "/home/pi/efis-next-current"),
    }
    node, scope, service, current = profiles[role]
    return {
        "schema": "efis-next.release-transition-plan.v1",
        "status": "PREPARED_NOT_EXECUTED",
        "decision": "NO_GO_UNTIL_SEPARATE_HARDWARE_AUTHORIZATION",
        "flight_approval": False,
        "execution_authorized": False,
        "node": node,
        "role": role,
        "platform": "RPI5_AARCH64",
        "service_scope": scope,
        "service": service,
        "current_link": current,
        "candidate_target": f"/opt/efis/releases/{role.lower()}-candidate",
        "rollback_target": f"/opt/efis/releases/{role.lower()}-rollback",
        "release_id": f"{role.lower()}-c12-0123456",
        "source_commit": commit,
        "manifest_sha256": ("1" if role == "PFD" else "2") * 64,
        "policy_sha256": "3" * 64,
        "artifacts_checked": 2,
        "required_gates": [f"gate-{index}" for index in range(9)],
        "mutating_commands": [],
    }


class PairedDisplayReleaseTests(unittest.TestCase):
    def test_prepares_mfd_first_fail_closed_plan(self):
        plan = prepare(transition("PFD"), transition("MFD"))
        self.assertEqual(plan["activation_order"], ["MFD", "PFD"])
        self.assertEqual(plan["rollback_order"], ["PFD", "MFD"])
        self.assertEqual([node["role"] for node in plan["nodes"]], ["MFD", "PFD"])
        self.assertFalse(plan["execution_authorized"])
        self.assertFalse(plan["flight_approval"])
        self.assertEqual(plan["mutating_commands"], [])
        self.assertEqual(len(plan["required_gates"]), 12)

    def test_rejects_source_commit_mismatch(self):
        with self.assertRaisesRegex(ValueError, "source commits differ"):
            prepare(transition("PFD", "a" * 40), transition("MFD", "b" * 40))

    def test_rejects_policy_mismatch(self):
        mfd = transition("MFD")
        mfd["policy_sha256"] = "4" * 64
        with self.assertRaisesRegex(ValueError, "policies differ"):
            prepare(transition("PFD"), mfd)

    def test_rejects_authorization_or_commands(self):
        for field, value in (("execution_authorized", True), ("flight_approval", True), ("mutating_commands", ["systemctl restart efis-pfd"])):
            with self.subTest(field=field):
                pfd = transition("PFD")
                pfd[field] = value
                with self.assertRaises(ValueError):
                    prepare(pfd, transition("MFD"))

    def test_rejects_wrong_node_and_platform(self):
        for field, value in (("node", "unknown"), ("platform", "RPI4_AARCH64"), ("current_link", "/tmp/current")):
            with self.subTest(field=field):
                mfd = transition("MFD")
                mfd[field] = value
                with self.assertRaises(ValueError):
                    prepare(transition("PFD"), mfd)

    def test_rejects_unsafe_paths_and_identifiers(self):
        mutations = (
            ("candidate_target", "relative/candidate"),
            ("rollback_target", "/opt/releases/../escape"),
            ("release_id", "BAD ID"),
            ("service", "unsafe service"),
        )
        for field, value in mutations:
            with self.subTest(field=field):
                pfd = transition("PFD")
                pfd[field] = value
                with self.assertRaises(ValueError):
                    prepare(pfd, transition("MFD"))

    def test_rejects_identical_release_ids(self):
        pfd = transition("PFD")
        mfd = transition("MFD")
        mfd["release_id"] = pfd["release_id"]
        with self.assertRaisesRegex(ValueError, "role-specific"):
            prepare(pfd, mfd)

    def test_input_digest_detects_mutation(self):
        pfd = transition("PFD")
        first = prepare(pfd, transition("MFD"))
        changed = copy.deepcopy(pfd)
        changed["candidate_target"] += "-new"
        second = prepare(changed, transition("MFD"))
        self.assertNotEqual(first["nodes"][1]["transition_plan_sha256"], second["nodes"][1]["transition_plan_sha256"])

    def test_atomic_write_refuses_overwrite_and_schema_is_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            output = pathlib.Path(directory) / "paired.json"
            atomic_write(output, prepare(transition("PFD"), transition("MFD")))
            with self.assertRaises(ValueError):
                atomic_write(output, prepare(transition("PFD"), transition("MFD")))
            self.assertEqual(json.loads(output.read_text())["status"], "PREPARED_NOT_EXECUTED")
        schema = json.loads((ROOT / "config/deployment/efis-paired-display-release-plan-v1.schema.json").read_text(encoding="utf-8"))
        self.assertFalse(schema["additionalProperties"])
        self.assertEqual(schema["properties"]["execution_authorized"]["const"], False)


if __name__ == "__main__":
    unittest.main()
