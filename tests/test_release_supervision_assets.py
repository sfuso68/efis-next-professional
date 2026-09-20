import json
import pathlib
import unittest


ROOT = pathlib.Path(__file__).parents[1]


class ReleaseSupervisionAssetTests(unittest.TestCase):
    def setUp(self):
        policy_path = ROOT / "config" / "deployment" / "node-release-policy-v1.json"
        self.policy = json.loads(policy_path.read_text(encoding="utf-8"))
        self.units = ROOT / "deploy" / "systemd" / "release-v1"

    def test_policy_has_exactly_one_profile_per_node_service(self):
        profiles = self.policy["nodes"]
        self.assertEqual(len(profiles), 5)
        keys = {(item["node"], item["service"]) for item in profiles}
        self.assertEqual(len(keys), len(profiles))

    def test_candidate_units_are_fail_closed_and_immutable(self):
        files = sorted(self.units.glob("*.conf"))
        self.assertEqual(len(files), 5)
        combined = "\n".join(path.read_text(encoding="utf-8") for path in files)
        directives = "\n".join(
            line for line in combined.splitlines() if line and not line.startswith("#")
        )
        self.assertNotIn("build/dev", directives)
        self.assertNotIn("flight_approval=true", combined.lower())
        for path in files:
            content = path.read_text(encoding="utf-8")
            self.assertIn("ExecCondition=", content, path.name)
            self.assertIn("validate_release_manifest.py", content, path.name)
            self.assertIn("--check-files", content, path.name)

    def test_each_policy_role_has_matching_gate(self):
        combined = "\n".join(
            path.read_text(encoding="utf-8") for path in self.units.glob("*.conf")
        )
        for profile in self.policy["nodes"]:
            self.assertIn(f"--expected-role {profile['role']}", combined)
            self.assertIn(f"--expected-platform {profile['platform']}", combined)
            self.assertIn(profile["current"], combined)

    def test_blackbox_candidate_remains_explicitly_simulated(self):
        content = (self.units / "efis-blackbox-release-v1.override.conf").read_text(encoding="utf-8")
        self.assertIn("--engine-simulation", content)


if __name__ == "__main__":
    unittest.main()
