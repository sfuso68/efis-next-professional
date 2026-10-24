"""Native bench adapter contract; check-only paths never send UDP."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from tests.test_jsbsim_session import fixture
from tests.test_sil_world import config
from tools.simulation.sil_world import World

EXE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


class PanelReplayTests(unittest.TestCase):
    def invoke(self, mutate=None, digest_override=None, mode="--check"):
        self.assertIsNotNone(EXE)
        self.assertTrue(EXE.is_file())
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            c = config()
            world = World(c)
            def enrich(f):
                f["world"] = world.frame(f["flight"], f["sim_time_ms"],
                    {"pressure_hpa": 850, "temperature_c": 8, "density_kg_m3": 1.05})
                if mutate:
                    mutate(f)
            fixture(root, enrich)
            manifest = json.loads((root/"manifest.json").read_text())
            manifest["world_config"] = c
            (root/"manifest.json").write_text(json.dumps(manifest))
            return subprocess.run([str(EXE), str(root), digest_override or manifest["frames_sha256"], mode],
                                  capture_output=True, text=True, timeout=20)

    def test_positive_loopback_adapter_preflight_without_sending(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("CHECK_ONLY", result.stdout)
        self.assertIn("frames=21", result.stdout)

    def test_wrong_expected_digest_rejected(self):
        self.assertEqual(self.invoke(digest_override="0"*64).returncode, 2)

    def test_no_arbitrary_endpoint_or_sending_mode(self):
        for mode in ("--send", "--destination=192.168.1.45", "--port=49002"):
            self.assertEqual(self.invoke(mode=mode).returncode, 2)

    def test_non_sim_and_approval_rejected(self):
        for mutate in (lambda f: f.update(simulated=False), lambda f: f.update(flight_approval=True),
                       lambda f: f["world"].update(simulated=False)):
            self.assertEqual(self.invoke(mutate).returncode, 2)

    def test_clock_and_missing_world_rejected(self):
        for mutate in (lambda f: f.update(sim_time_ms=999), lambda f: f.pop("world")):
            self.assertEqual(self.invoke(mutate).returncode, 2)

    def test_units_and_unavailable_value_rejected(self):
        def units(f):
            f["world"]["sensors"]["ias_kt"]["unit"] = "m/s"
        def invalid(f):
            f["world"]["sensors"]["ias_kt"].update(validity="UNAVAILABLE", value=0)
        for mutate in (units, invalid):
            self.assertEqual(self.invoke(mutate).returncode, 2)


if __name__ == "__main__":
    if EXE is None:
        raise SystemExit("native executable required")
    unittest.main(verbosity=2)
