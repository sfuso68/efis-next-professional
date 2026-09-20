import hashlib
import json
import pathlib
import tempfile
import unittest

from tools.deployment.validate_release_manifest import ManifestError, validate


class ReleaseManifestTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp.name)
        binary = self.root / "bin" / "efis-test"
        binary.parent.mkdir()
        binary.write_bytes(b"immutable release fixture\n")
        self.manifest = {
            "schema": "efis-next.release-manifest.v1",
            "release_id": "test-0123456",
            "source_commit": "0" * 40,
            "built_utc": "2026-09-20T04:00:00Z",
            "roles": ["EMS"],
            "platform": "RPI4_AARCH64",
            "repository": "sfuso68/efis-next-professional",
            "flight_approval": False,
            "simulation_mode": True,
            "artifacts": [{
                "path": "bin/efis-test",
                "sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
                "executable": False,
            }],
        }

    def tearDown(self):
        self.temp.cleanup()

    def test_valid_manifest_checks_artifact(self):
        result = validate(self.manifest, self.root, "EMS", "RPI4_AARCH64", True)
        self.assertEqual(result["status"], "PASS")
        self.assertEqual(result["artifacts_checked"], 1)
        self.assertFalse(result["flight_approval"])

    def test_rejects_path_escape(self):
        self.manifest["artifacts"][0]["path"] = "../outside"
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root)

    def test_rejects_hash_mismatch(self):
        self.manifest["artifacts"][0]["sha256"] = "f" * 64
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root, check_files=True)

    def test_rejects_symlink_artifact(self):
        target = self.root / "bin" / "real"
        target.write_bytes(b"real\n")
        link = self.root / "bin" / "linked"
        try:
            link.symlink_to(target)
        except OSError as error:
            self.skipTest(f"symlink unavailable: {error}")
        self.manifest["artifacts"][0] = {
            "path": "bin/linked",
            "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            "executable": False,
        }
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root, check_files=True)

    def test_rejects_wrong_role_or_platform(self):
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root, "PFD", "RPI4_AARCH64")
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root, "EMS", "RPI5_AARCH64")

    def test_rejects_flight_approval(self):
        self.manifest["flight_approval"] = True
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root)

    def test_rejects_unknown_or_missing_fields(self):
        self.manifest["unexpected"] = 1
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root)
        del self.manifest["unexpected"]
        del self.manifest["repository"]
        with self.assertRaises(ManifestError):
            validate(self.manifest, self.root)

    def test_schema_file_is_json(self):
        schema = pathlib.Path(__file__).parents[1] / "config" / "deployment" / "efis-release-manifest-v1.schema.json"
        self.assertEqual(json.loads(schema.read_text(encoding="utf-8"))["type"], "object")


if __name__ == "__main__":
    unittest.main()
