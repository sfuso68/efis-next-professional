import json
import pathlib
import tempfile
import unittest

from tools.deployment.build_release_manifest import atomic_write, build_manifest
from tools.deployment.prepare_release_transition import prepare


ROOT = pathlib.Path(__file__).parents[1]
POLICY = ROOT / "config" / "deployment" / "node-release-policy-v1.json"


class ReleasePreparationTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.base = pathlib.Path(self.temp.name)
        self.candidate = self.base / "candidate"
        self.previous = self.base / "previous"
        self.candidate.mkdir()
        self.previous.mkdir()
        artifact = self.candidate / "build-rpi4" / "efis-ems-qt"
        artifact.parent.mkdir()
        artifact.write_bytes(b"candidate fixture\n")

    def tearDown(self):
        self.temp.cleanup()

    def manifest(self):
        return build_manifest(
            self.candidate,
            "ems-test-0123456",
            "a" * 40,
            "2026-09-20T06:00:00Z",
            ["EMS"],
            "RPI4_AARCH64",
            True,
            ["build-rpi4/efis-ems-qt:data"],
        )

    def test_build_is_deterministic_and_fail_closed(self):
        first = self.manifest()
        second = self.manifest()
        self.assertEqual(first, second)
        self.assertFalse(first["flight_approval"])
        self.assertTrue(first["simulation_mode"])
        self.assertEqual(len(first["artifacts"]), 1)

    def test_build_rejects_duplicate_and_escape(self):
        common = (
            self.candidate, "ems-test-0123456", "a" * 40,
            "2026-09-20T06:00:00Z", ["EMS"], "RPI4_AARCH64", True,
        )
        with self.assertRaises(ValueError):
            build_manifest(*common, ["build-rpi4/efis-ems-qt:data"] * 2)
        with self.assertRaises(ValueError):
            build_manifest(*common, ["../escape:data"])
        with self.assertRaises(ValueError):
            build_manifest(
                self.candidate, "ems-test-0123456", "a" * 40,
                "2026-09-20T06:00:00Z", ["EMS", "EMS"],
                "RPI4_AARCH64", True, ["build-rpi4/efis-ems-qt:data"],
            )

    def test_atomic_write_refuses_overwrite(self):
        output = self.candidate / "release-manifest.json"
        atomic_write(output, self.manifest())
        with self.assertRaises(ValueError):
            atomic_write(output, self.manifest())

    def test_transition_plan_is_non_executable(self):
        manifest_path = self.candidate / "release-manifest.json"
        atomic_write(manifest_path, self.manifest())
        plan = prepare(
            POLICY, manifest_path, self.candidate, self.previous, "efis", "EMS",
        )
        self.assertEqual(plan["status"], "PREPARED_NOT_EXECUTED")
        self.assertFalse(plan["execution_authorized"])
        self.assertFalse(plan["flight_approval"])
        self.assertEqual(plan["mutating_commands"], [])
        self.assertEqual(len(plan["required_gates"]), 9)

    def test_transition_rejects_same_target_and_wrong_role(self):
        manifest_path = self.candidate / "release-manifest.json"
        atomic_write(manifest_path, self.manifest())
        with self.assertRaises(ValueError):
            prepare(POLICY, manifest_path, self.candidate, self.candidate, "efis", "EMS")
        with self.assertRaises(ValueError):
            prepare(POLICY, manifest_path, self.candidate, self.previous, "efis", "PFD")

    def test_transition_output_round_trip(self):
        manifest_path = self.candidate / "release-manifest.json"
        atomic_write(manifest_path, self.manifest())
        plan = prepare(POLICY, manifest_path, self.candidate, self.previous, "efis", "EMS")
        encoded = json.dumps(plan, sort_keys=True)
        self.assertEqual(json.loads(encoded)["release_id"], "ems-test-0123456")
        self.assertEqual(len(plan["manifest_sha256"]), 64)
        self.assertEqual(len(plan["policy_sha256"]), 64)
        schema = json.loads((ROOT / "config" / "deployment" / "efis-release-transition-plan-v1.schema.json").read_text(encoding="utf-8"))
        self.assertEqual(schema["properties"]["execution_authorized"]["const"], False)

    def test_transition_rejects_manifest_outside_candidate(self):
        outside = self.base / "release-manifest.json"
        atomic_write(outside, self.manifest())
        with self.assertRaises(ValueError):
            prepare(POLICY, outside, self.candidate, self.previous, "efis", "EMS")


if __name__ == "__main__":
    unittest.main()
