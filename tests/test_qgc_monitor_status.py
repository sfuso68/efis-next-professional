from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "groundstation" / "qgc_monitor_status.py"
SPEC = importlib.util.spec_from_file_location("qgc_monitor_status", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


PREFLIGHT = {
    "status": "PASS",
    "checks": {"artifact_integrity": {"pass": True}},
}
QGC = {
    "LoadState": "loaded",
    "ActiveState": "active",
    "SubState": "running",
    "NRestarts": "0",
}
FIREWALL = {"LoadState": "loaded", "ActiveState": "active"}
SPEECH = {"LoadState": "loaded", "ActiveState": "active", "SubState": "running"}
SPEECH_MUTED = {"LoadState": "masked", "ActiveState": "inactive", "SubState": "dead"}
LISTENER = [
    'UNCONN 0 0 0.0.0.0:14550 0.0.0.0:* users:(("QGroundControl",pid=1,fd=1))'
]


class QgcMonitorStatusTests(unittest.TestCase):
    def evaluate(self, **overrides: object) -> dict[str, object]:
        arguments = {
            "preflight": PREFLIGHT,
            "qgc_state": QGC,
            "qgc_enabled": True,
            "listeners": LISTENER,
            "firewall_state": FIREWALL,
            "firewall_enabled": True,
            "speech_state": SPEECH_MUTED,
            "speech_socket_state": SPEECH_MUTED,
            "speech_enablement": "masked",
            "speech_socket_enablement": "masked",
            "qgc_audio_is_muted": True,
            "speech_processes": [],
            "qgc_runtime_warnings": [],
        }
        arguments.update(overrides)
        return MODULE.evaluate(**arguments)

    def test_all_gates_pass(self) -> None:
        self.assertEqual(self.evaluate()["status"], "PASS")

    def test_missing_firewall_is_partial(self) -> None:
        report = self.evaluate(
            firewall_state={"LoadState": "not-found", "ActiveState": "inactive"},
            firewall_enabled=False,
        )
        self.assertEqual(report["status"], "PARTIAL")

    def test_failed_preflight_is_failure(self) -> None:
        report = self.evaluate(preflight={"status": "BLOCKED", "checks": {}})
        self.assertEqual(report["status"], "FAIL")

    def test_unmasked_speech_backend_is_partial(self) -> None:
        report = self.evaluate(
            speech_state=SPEECH,
            speech_socket_state={
                "LoadState": "loaded",
                "ActiveState": "active",
                "SubState": "listening",
            },
            speech_enablement="enabled",
            speech_socket_enablement="enabled",
        )
        self.assertEqual(report["status"], "PARTIAL")
        self.assertFalse(report["checks"]["speech_output_muted"])

    def test_spawned_speech_process_is_partial(self) -> None:
        report = self.evaluate(
            speech_processes=["123 /usr/bin/speech-dispatcher --spawn"]
        )
        self.assertEqual(report["status"], "PARTIAL")
        self.assertFalse(report["checks"]["speech_output_muted"])

    def test_qgc_audio_setting_is_required_and_parsed(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            settings = Path(directory) / "QGroundControl.ini"
            settings.write_text("[General]\naudioMuted=true\n", encoding="utf-8")
            self.assertTrue(MODULE.qgc_audio_muted(settings))
            settings.write_text("[General]\naudioMuted=false\n", encoding="utf-8")
            self.assertFalse(MODULE.qgc_audio_muted(settings))
        self.assertEqual(
            self.evaluate(qgc_audio_is_muted=False)["status"], "PARTIAL"
        )

    def test_qgc_speech_plugin_error_is_informational_when_muted(self) -> None:
        report = self.evaluate(
            qgc_runtime_warnings=['Error loading text-to-speech plug-in "speechd"']
        )
        self.assertEqual(report["status"], "PASS")
        self.assertFalse(report["checks"]["qgc_speech_plugin_clean"])

    def test_restart_or_competing_listener_is_failure(self) -> None:
        restarted = dict(QGC)
        restarted["NRestarts"] = "1"
        self.assertEqual(self.evaluate(qgc_state=restarted)["status"], "FAIL")
        self.assertEqual(
            self.evaluate(listeners=[*LISTENER, "other :14550"])["status"],
            "FAIL",
        )


if __name__ == "__main__":
    unittest.main()
