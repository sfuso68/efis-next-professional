"""Black-box tests of the built offline CLI; absence of the binary is a failure."""
import json
import subprocess
import sys
import unittest

BINARY = sys.argv.pop(1)


class MissionCliTests(unittest.TestCase):
    def run_cli(self, *args):
        return subprocess.run([BINARY, *args], capture_output=True, text=True, timeout=30)

    def frames(self, scenario="full-flight"):
        result = self.run_cli("--step-ms", "60000", "--scenario", scenario)
        self.assertEqual(result.returncode, 0, result.stderr)
        return [json.loads(line) for line in result.stdout.splitlines()]

    def test_describe_is_explicitly_synthetic_and_offline(self):
        result = self.run_cli("--describe")
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["profile_status"], "SYNTHETIC_UNVALIDATED")
        self.assertIs(data["flight_approval"], False)
        self.assertIs(data["network"], False)
        self.assertGreater(data["duration_ms"], 600000)
        self.assertEqual(len(data["parameters"]), 18)

    def test_profile_overrides_are_exported_and_validated(self):
        result = self.run_cli("--describe", "--ambient-c", "5", "--wind-east-kt", "12")
        self.assertEqual(result.returncode, 0, result.stderr)
        data = json.loads(result.stdout)
        self.assertEqual(data["parameters"]["ambient-c"], 5)
        self.assertEqual(data["parameters"]["wind-east-kt"], 12)
        for args in [("--cruise-ft", "NaN"), ("--climb-fpm", "-1"),
                     ("--fuel-left-gal", "-1"), ("--ambient-c", "2junk")]:
            with self.subTest(args=args):
                self.assertEqual(self.run_cli(*args).returncode, 2)

    def test_frames_cover_mission_with_validity_units_and_provenance(self):
        rows = self.frames()
        self.assertEqual(rows[0]["time_ms"], 0)
        self.assertEqual(rows[-1]["phase"], "COMPLETE")
        self.assertEqual(rows[-1]["time_ms"], rows[-1]["duration_ms"])
        self.assertEqual(rows[-1]["engine"]["flow_gph"]["value"], 0)
        self.assertEqual(rows[-1]["engine"]["bus_v"]["value"], 0)
        for i, row in enumerate(rows):
            self.assertEqual(row["sequence"], i + 1)
            self.assertIs(row["flight_approval"], False)
            self.assertIs(row["simulated"], True)
            self.assertEqual(len(row["engine"]), 18)
            for sample in [*row["flight"].values(), *row["engine"].values()]:
                self.assertIs(sample["simulated"], True)
                self.assertGreater(sample["signal"], 0)
                self.assertGreater(sample["unit"], 0)
                self.assertIn("validity", sample)

    def test_same_inputs_produce_identical_output(self):
        a = self.run_cli("--step-ms", "60000", "--seed", "42")
        b = self.run_cli("--step-ms", "60000", "--seed", "42")
        self.assertEqual(a.returncode, 0)
        self.assertEqual(b.returncode, 0)
        self.assertEqual(a.stdout, b.stdout)

    def test_sensor_failure_and_recovery_are_exposed(self):
        rows = self.frames("sensor-degradation")
        states = [row["flight"]["ias_kt"]["validity"] for row in rows]
        self.assertGreater(len(set(states)), 1)
        self.assertEqual(states[0], states[-1])

    def test_bad_options_fail_closed(self):
        for args in [("--step-ms", "0"), ("--step-ms", "60001"),
                     ("--step-ms", "20junk"), ("--seed", "-1"),
                     ("--seed", "NaN"), ("--scenario", "unknown"),
                     ("--output", "not-supported"), ("--step-ms",)]:
            with self.subTest(args=args):
                result = self.run_cli(*args)
                self.assertEqual(result.returncode, 2)
                self.assertEqual(result.stdout, "")


if __name__ == "__main__":
    unittest.main()
