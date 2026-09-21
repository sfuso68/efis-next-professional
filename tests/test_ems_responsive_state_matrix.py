import copy
import json
from pathlib import Path
import shutil
import tempfile
import unittest

from tools.ui.validate_ems_responsive_state_matrix import validate
from tools.ui.verify_ems_golden_rerender import compare_manifests


ROOT = Path(__file__).resolve().parents[1]


class EmsResponsiveStateMatrixTests(unittest.TestCase):
    def test_rerender_manifest_requires_the_same_semantic_matrix(self):
        reference = {
            "images": [{"file": "ems-800x480-live.png", "width": 800, "height": 480,
                        "state": "LIVE", "pixel_sha256": "a" * 64}]
        }
        rerendered = copy.deepcopy(reference)
        rerendered["images"][0]["sha256"] = "b" * 64
        self.assertEqual(compare_manifests(reference, rerendered), [])
        rerendered["images"][0]["state"] = "STALE"
        self.assertTrue(compare_manifests(reference, rerendered))

    def test_repository_matrix_passes(self):
        self.assertEqual(validate(ROOT), [])

    def test_all_twenty_controls_fail_closed(self):
        source_config = ROOT / "config/ui/avionix-ems-responsive-state-matrix-v1.json"
        source_golden = ROOT / "tests/golden/ems-responsive-states"

        def mutate(control, config, manifest, root):
            image_dir = root / "tests/golden/ems-responsive-states"
            if control == 1: config["schema"] = "unknown"
            elif control == 2: config["unknown"] = True
            elif control == 3: config["status"] = "HARDWARE_ACCEPTED"
            elif control == 4: config["role"] = "pfd"
            elif control == 5: config["resolutions"].reverse()
            elif control == 6: config["states"].reverse()
            elif control == 7: config["states"][0]["unknown"] = True
            elif control == 8: config["states"][1]["age_ms"] = 0
            elif control == 9: config["states"][3]["severity"] = "NORMAL"
            elif control == 10: config["invariants"]["captures"] = 17
            elif control == 11: config["golden"]["interactive"] = True
            elif control == 12: manifest["schema"] = "unknown"
            elif control == 13: manifest["status"] = "HARDWARE_ACCEPTED"
            elif control == 14: manifest["platform"] = "unknown"
            elif control == 15: manifest["generated_utc"] = "2026-09-18T00:00:00"
            elif control == 16: manifest["images"].pop()
            elif control == 17: manifest["images"].reverse()
            elif control == 18: manifest["images"][0]["file"] = "wrong.png"
            elif control == 19: (image_dir / manifest["images"][0]["file"]).unlink()
            elif control == 20: manifest["images"][0]["sha256"] = "0" * 64

        with tempfile.TemporaryDirectory() as temporary:
            baseline_root = Path(temporary) / "baseline"
            (baseline_root / "config/ui").mkdir(parents=True)
            shutil.copy2(source_config, baseline_root / "config/ui" / source_config.name)
            shutil.copytree(source_golden, baseline_root / "tests/golden/ems-responsive-states")
            baseline_config = json.loads(source_config.read_text(encoding="utf-8"))
            baseline_manifest = json.loads((source_golden / "manifest.json").read_text(encoding="utf-8"))
            for control in range(1, 21):
                with self.subTest(control=control):
                    case_root = Path(temporary) / f"case-{control}"
                    shutil.copytree(baseline_root, case_root)
                    config = copy.deepcopy(baseline_config)
                    manifest = copy.deepcopy(baseline_manifest)
                    mutate(control, config, manifest, case_root)
                    (case_root / "config/ui" / source_config.name).write_text(
                        json.dumps(config), encoding="utf-8"
                    )
                    (case_root / "tests/golden/ems-responsive-states/manifest.json").write_text(
                        json.dumps(manifest), encoding="utf-8"
                    )
                    marker = f"[HMI-M20-{control:02d}]"
                    self.assertTrue(
                        any(error.startswith(marker) for error in validate(case_root)),
                        f"control {control} did not fail closed",
                    )


if __name__ == "__main__":
    unittest.main()
