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

    def test_all_policy_executables_use_canonical_non_symlink_bin_paths(self):
        for profile in self.policy["nodes"]:
            self.assertTrue(profile["executable"].startswith("bin/"), profile)
            self.assertNotIn("build-rpi", profile["executable"])

        combined = "\n".join(
            path.read_text(encoding="utf-8") for path in self.units.glob("*.conf")
        )
        self.assertNotIn("/build-rpi", combined)

    def test_blackbox_candidate_remains_explicitly_simulated(self):
        content = (self.units / "efis-blackbox-release-v1.override.conf").read_text(encoding="utf-8")
        self.assertIn("--engine-simulation", content)

    def test_mfd_does_not_write_logs_below_immutable_release(self):
        content = (self.units / "efis-mfd-release-v1.override.conf").read_text(encoding="utf-8")
        self.assertIn("StandardOutput=journal", content)
        self.assertIn("StandardError=journal", content)
        self.assertNotIn("efis-next-current/logs", content)

    def test_pfd_and_recorder_share_one_validated_release_without_runtime_writes(self):
        pfd = (self.units / "efis-pfd-release-v1.override.conf").read_text(
            encoding="utf-8"
        )
        recorder = (self.units / "efis-recorder-release-v1.override.conf").read_text(
            encoding="utf-8"
        )
        for content, role, executable in (
            (pfd, "PFD", "bin/efis-pfd-qt"),
            (recorder, "RECORDER", "bin/efis-recorder"),
        ):
            self.assertIn("WorkingDirectory=/home/efis/efis-next-current", content)
            self.assertIn(f"--expected-role {role}", content)
            self.assertIn(f"/home/efis/efis-next-current/{executable}", content)
            self.assertIn("StandardOutput=journal", content)
            self.assertIn("StandardError=journal", content)
            self.assertNotIn("efis-next-current/logs", content)
        self.assertIn(
            "/home/efis/.local/state/efis-next/records",
            recorder,
        )
        self.assertIn(
            "/home/efis/.local/state/efis-next/recorder.status",
            recorder,
        )
        self.assertIn("--max-storage-bytes 25769803776", recorder)
        self.assertIn("--min-free-bytes 12884901888", recorder)
        self.assertIn("--minimum-completed-segments 120", recorder)

    def test_pfd_recorder_rollout_assets_are_guarded_and_dual_role(self):
        scripts = ROOT / "deploy" / "scripts"
        stage = (scripts / "stage_pfd_recorder_immutable_releases.sh").read_text(
            encoding="utf-8"
        )
        activate = (scripts / "activate_pfd_recorder_immutable_release.sh").read_text(
            encoding="utf-8"
        )
        reboot = (scripts / "verify_pfd_recorder_reboot_acceptance.sh").read_text(
            encoding="utf-8"
        )
        self.assertIn("--role PFD", stage)
        self.assertIn("--role RECORDER", stage)
        self.assertIn("EXECUTION_AUTHORIZED=false", stage)
        self.assertIn("pfdrec-d52e377-primary-v1", stage)
        self.assertIn("pfdrec-60b3526-rollback-v1", stage)
        self.assertIn("ROLLBACK_DRILL=PASS", activate)
        self.assertIn("PFD_RECORDER_IMMUTABLE_ACTIVATION=PASS", activate)
        self.assertIn("restore_dropin", activate)
        self.assertIn("mv -Tf", activate)
        self.assertIn("get_throttled", activate)
        self.assertIn("PFD_RECORDER_REBOOT_ACCEPTANCE=PASS", reboot)
        self.assertIn("RECORDER_STATUS_MTIME_AFTER", reboot)
        self.assertIn("for role in PFD RECORDER", reboot)
        self.assertIn("--expected-role \"$role\"", reboot)

    def test_pfd_weather_async_rollout_is_pinned_and_fail_closed(self):
        scripts = ROOT / "deploy" / "scripts"
        prepare = (scripts / "prepare_pfd_weather_async_release.sh").read_text(
            encoding="utf-8"
        )
        activate = (scripts / "activate_pfd_weather_async_release.sh").read_text(
            encoding="utf-8"
        )
        reboot = (scripts / "verify_pfd_weather_async_reboot.sh").read_text(
            encoding="utf-8"
        )
        release = "pfdrec-3c7bb4d-weather-async-v1"
        rollback = "pfdrec-7051a47-retention-v1"
        commit = "3c7bb4d5495e1ce9f582dd185be48b9563993e48"
        archive_sha = "beb4cce4eed80e950dda6e245ff3bae42842f96242f4836cddeb5fe7310c61f3"

        for content in (prepare, activate, reboot):
            self.assertIn(release, content)
            self.assertIn("RPI5_AARCH64", content)
        self.assertIn(commit, prepare)
        self.assertIn(archive_sha, prepare)
        self.assertIn("sha256sum --check --strict", prepare)
        self.assertIn("--role PFD --role RECORDER", prepare)
        self.assertIn("efis_weather_service_tests", prepare)
        self.assertIn("EXECUTION_AUTHORIZED=false", prepare)
        self.assertIn(rollback, activate)
        self.assertIn("ROLLBACK_DRILL=PASS", activate)
        self.assertIn("mv -Tf", activate)
        self.assertIn("EFIS_WEATHER_SIMULATION_IO_DELAY_MS=400", activate)
        self.assertIn("event=weather_simulation_worker_slow", activate)
        self.assertIn("event=callback_stall callback=weather_simulation", activate)
        self.assertIn("PFD_WEATHER_ASYNC_ACTIVATION=PASS", activate)
        self.assertIn("PFD_WEATHER_ASYNC_REBOOT_ACCEPTANCE=PASS", reboot)
        self.assertIn("event=callback_stall callback=weather_simulation", reboot)

    def test_ems_runtime_writes_are_outside_immutable_release(self):
        content = (ROOT / "deploy/systemd/release-v1/efis-ems-release-v1.override.conf").read_text(
            encoding="utf-8"
        )
        self.assertIn(
            "EFIS_ENGINE_RUNTIME_STATE=/home/pi/.local/state/efis-next/ems/engine-runtime.state",
            content,
        )
        self.assertIn(
            "EFIS_BLACKBOX_FUEL_EVENTS=/home/pi/.local/state/efis-next/ems/blackbox-fuel-events.jsonl",
            content,
        )
        self.assertIn(
            "EFIS_CAPTURE_DIR=/home/pi/.local/state/efis-next/ems/captures",
            content,
        )
        self.assertIn("Environment=EFIS_KIOSK=1", content)
        self.assertIn("Environment=QSG_RENDER_LOOP=threaded", content)
        self.assertIn("Environment=EFIS_EDP_BIND=10.77.10.12", content)
        self.assertIn("Environment=EFIS_EDP_PORT=49003", content)
        self.assertIn("Environment=EFIS_EDP_SOURCE_NODE=47", content)
        self.assertNotIn("EFIS_ENGINE_RUNTIME_STATE=/home/pi/efis-next-current", content)
        self.assertNotIn("EFIS_CAPTURE_DIR=/home/pi/efis-next-current", content)


if __name__ == "__main__":
    unittest.main()
