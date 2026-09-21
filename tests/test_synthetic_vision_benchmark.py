import copy
import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from tools.terrain.validate_synthetic_vision_benchmark import (
    ARTIFACT_TYPES, CONTRACT, ROOT, digest, validate_contract, validate_results,
)


BASE = json.loads(CONTRACT.read_text(encoding="utf-8"))


class SyntheticVisionBenchmarkTests(unittest.TestCase):
    def test_canonical_contract_passes(self):
        self.assertEqual(validate_contract(copy.deepcopy(BASE), ROOT), [])

    def test_contract_fails_closed_on_safety_mutations(self):
        mutations = []

        def case(change):
            data = copy.deepcopy(BASE)
            change(data)
            mutations.append(data)

        case(lambda d: d.update(status="PASS"))
        case(lambda d: d.update(flight_approval=True))
        case(lambda d: d.update(platform="X86_64"))
        case(lambda d: d["required_tiles"].pop())
        case(lambda d: d["checkpoints"].pop())
        case(lambda d: d["checkpoints"][1].update(latitude_deg=45.81))
        case(lambda d: d["run_requirements"].update(duration_seconds=60))
        case(lambda d: d["acceptance"].update(minimum_fps=30.0))
        case(lambda d: d["required_artifacts"].pop())
        case(lambda d: d["data_policy"].update(navigation_use=True))
        for data in mutations:
            with self.subTest(data=data):
                self.assertTrue(validate_contract(data, ROOT))

    def _fixture(self, directory: Path):
        artifacts = {}
        for kind in ARTIFACT_TYPES:
            path = directory / f"{kind.lower()}.txt"
            path.write_text(f"real evidence placeholder for fixture {kind}\n", encoding="utf-8")
            artifacts[kind] = {"path": path.name, "sha256": digest(path)}
        results = {
            "schema": "avionix.synthetic-vision-benchmark-results.v1",
            "benchmark_id": BASE["benchmark_id"],
            "contract_sha256": digest(CONTRACT),
            "source_commit": "a" * 40,
            "platform": "RPI5_AARCH64",
            "role": "PFD",
            "release_id": "pfd-test-release",
            "status": "PASS",
            "flight_approval": False,
            "duration_seconds": 3600,
            "modes": ["DAY", "NIGHT"],
            "measurements": {
                "minimum_fps": 40.0,
                "p95_frame_ms": 25.0,
                "maximum_frame_ms": 100.0,
                "maximum_temperature_c": 60.0,
                "throttled": "0x0",
                "boundary_discontinuity_m": 1.0,
                "missing_samples": 0,
                "unlabelled_fallbacks": 0,
                "finite_projection": True,
                "near_field_visual_pass": True,
                "horizon_continuity_pass": True,
                "tile_boundary_crossed": True,
                "missing_tile_fallback_explicit": True,
            },
            "checkpoints": [
                {"id": item["id"], "day_observed": True, "night_observed": True}
                for item in BASE["checkpoints"]
            ],
            "artifacts": artifacts,
            "operator": "Test Operator",
        }
        return results

    def test_complete_fixture_passes(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.assertEqual(validate_results(self._fixture(root), BASE, root), [])

    def test_each_threshold_fails_closed(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            changes = {
                "minimum_fps": 39.9,
                "p95_frame_ms": 25.1,
                "maximum_frame_ms": 100.1,
                "boundary_discontinuity_m": 1.1,
                "missing_samples": 1,
                "unlabelled_fallbacks": 1,
                "maximum_temperature_c": float("nan"),
                "throttled": "0x50000",
                "finite_projection": False,
                "near_field_visual_pass": False,
                "horizon_continuity_pass": False,
                "tile_boundary_crossed": False,
                "missing_tile_fallback_explicit": False,
            }
            for name, value in changes.items():
                results = self._fixture(root)
                results["measurements"][name] = value
                with self.subTest(name=name):
                    self.assertTrue(validate_results(results, BASE, root))

    def test_artifact_escape_symlink_and_digest_mutation_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            results = self._fixture(root)
            results["artifacts"]["THERMAL"]["path"] = "../escape.txt"
            self.assertTrue(validate_results(results, BASE, root))

            results = self._fixture(root)
            results["artifacts"]["THERMAL"]["sha256"] = "0" * 64
            self.assertTrue(validate_results(results, BASE, root))

            target = root / "target.txt"
            target.write_text("target", encoding="utf-8")
            link = root / "link.txt"
            try:
                link.symlink_to(target)
            except OSError:
                return
            results = self._fixture(root)
            results["artifacts"]["THERMAL"] = {"path": link.name, "sha256": digest(target)}
            self.assertTrue(validate_results(results, BASE, root))

    def test_incomplete_observation_and_forged_contract_fail(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            results = self._fixture(root)
            results["checkpoints"][0]["night_observed"] = False
            self.assertTrue(validate_results(results, BASE, root))
            results = self._fixture(root)
            results["contract_sha256"] = hashlib.sha256(b"forged").hexdigest()
            self.assertTrue(validate_results(results, BASE, root))


if __name__ == "__main__":
    unittest.main()
