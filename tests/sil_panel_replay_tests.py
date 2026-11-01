"""Native bench adapter contract; check-only paths never send UDP."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import time
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


@unittest.skipIf(sys.platform == "win32", "POSIX pipe deadline tested on Linux/ARM64")
class NativeLiveTests(unittest.TestCase):
    def run_exchange(self, fault=None):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            c = config()
            world = World(c)
            fixture(root, lambda f: f.update(world=world.frame(f["flight"], f["sim_time_ms"],
                    {"pressure_hpa": 850, "temperature_c": 8, "density_kg_m3": 1.05})))
            canonical = json.dumps(c, sort_keys=True, separators=(",", ":"))
            digest = hashlib.sha256(canonical.encode()).hexdigest()
            hello = dict(schema="fuso-avionics.sil-live.v1", kind="HELLO", session_id="fixture",
                         seconds=1, simulated=True, flight_approval=False,
                         world_config_canonical=canonical, world_config_sha256=digest)
            with subprocess.Popen([str(EXE), digest, "--live-check-stdio"], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True) as process:
                try:
                    process.stdin.write(json.dumps(hello)+"\n")
                    process.stdin.flush()
                    start = time.monotonic()
                    for index, line in enumerate((root / "frames.jsonl").read_text().splitlines()):
                        request_line = process.stdout.readline()
                        if not request_line:
                            break
                        request = json.loads(request_line)
                        frame = json.loads(line)
                        if fault == "late":
                            time.sleep(.35)
                        elif fault != "burst":
                            time.sleep(max(0, start + index * .05 - time.monotonic()))
                        response = dict(schema=hello["schema"], kind="FRAME", nonce=request["nonce"], frame=frame)
                        if fault == "nonce":
                            response["nonce"] = "0" * 32
                        if fault == "non-sim":
                            frame["simulated"] = False
                        try:
                            process.stdin.write(json.dumps(response)+"\n")
                            process.stdin.flush()
                        except BrokenPipeError:
                            break
                        if fault in ("nonce", "non-sim", "late"):
                            break
                    process.wait(timeout=4)
                    return process.returncode, process.stdout.read(), process.stderr.read()
                finally:
                    if process.poll() is None:
                        process.kill()
                        process.wait(timeout=2)
                    try:
                        process.stdin.close()
                    except BrokenPipeError:
                        pass
                    process.stdin = None

    def test_live_positive_challenges(self):
        code, out, err = self.run_exchange()
        self.assertEqual(code, 0, err)
        self.assertIn('"frames":21', out)
        self.assertIn('"mode":"LIVE_CHECK_ONLY"', out)

    def test_live_nonce_and_sim_fail_closed(self):
        for fault in ("nonce", "non-sim"):
            code, _, _ = self.run_exchange(fault)
            self.assertEqual(code, 2)

    def test_live_late_response_rejected(self):
        code, _, err = self.run_exchange("late")
        self.assertEqual(code, 2)
        self.assertIn("timeout", err)

    def test_live_burst_replay_not_accepted_as_live(self):
        code, _, _ = self.run_exchange("burst")
        self.assertEqual(code, 2)


if __name__ == "__main__":
    if EXE is None:
        raise SystemExit("native executable required")
    unittest.main(verbosity=2)
