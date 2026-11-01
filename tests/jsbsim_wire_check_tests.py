"""Black-box canonical adapter tests. Executable argument mandatory, no false skips."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from tests.test_jsbsim_session import fixture

EXECUTABLE = Path(sys.argv.pop(1)).resolve() if len(sys.argv) > 1 else None


class WireCheckTests(unittest.TestCase):
    def invoke(self, mutation=None):
        self.assertIsNotNone(EXECUTABLE)
        self.assertTrue(EXECUTABLE.is_file())
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            fixture(root, mutation)
            return subprocess.run([str(EXECUTABLE), str(root / "frames.jsonl")],
                                  capture_output=True, text=True, timeout=20)

    def test_complete_fixture_and_canonical_consumer_guards(self):
        result = self.invoke()
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("SIL_CANONICAL_CONSUMERS=PASS frames=21", result.stdout)
        self.assertIn("CRC_DUP_ID_TIMEOUT=PASS", result.stdout)
        self.assertIn("AIRDATA_HEADING_MAPPING=UNAVAILABLE SOCKETS=NONE", result.stdout)

    def test_non_sim_data_and_approval_rejected(self):
        for mutation in (lambda f: f.update(simulated=False), lambda f: f.update(flight_approval=True)):
            result = self.invoke(mutation)
            self.assertEqual(result.returncode, 2)
            self.assertIn("FAIL", result.stderr)

    def test_wrong_clock_or_profile_rejected(self):
        for mutation in (lambda f: f.update(sim_time_ms=1000000), lambda f: f.update(profile_status="VALIDATED"),
                         lambda f: f.update(model="pioneer300")):
            self.assertEqual(self.invoke(mutation).returncode, 2)

    def test_wrong_unit_and_missing_sample_rejected(self):
        def unit(f):
            f["engine"]["rpm"]["unit"] = "percent"
        def missing(f):
            f["engine"].pop("rpm")
        for mutation in (unit, missing):
            self.assertEqual(self.invoke(mutation).returncode, 2)

    def test_unavailable_cylinder_cannot_become_zero_valid(self):
        def fake(f):
            f["engine"]["cht1"]["value"] = 0
        self.assertEqual(self.invoke(fake).returncode, 2)


if __name__ == "__main__":
    if EXECUTABLE is None:
        raise SystemExit("usage: jsbsim_wire_check_tests.py PATH_TO_EXECUTABLE")
    unittest.main(verbosity=2)
