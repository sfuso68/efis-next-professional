from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).parents[1]
SCRIPT = ROOT / "tools" / "network" / "validate_mavlink_observer_gateway.py"
SPEC = importlib.util.spec_from_file_location("validate_mavlink_observer_gateway", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class MavlinkObserverGatewayPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.policy = json.loads((
            ROOT / "config" / "network" / "mavlink-observer-gateway-v1.json"
        ).read_text(encoding="utf-8"))

    def test_repository_policy_is_safe(self) -> None:
        report = MODULE.validate(self.policy)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual(report["observers"], 2)
        self.assertFalse(report["return_path"])

    def test_ip_forwarding_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["controls"]["ip_forward_required"] = True
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_command_path_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["controls"]["mavlink_command_forwarding_allowed"] = True
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_observer_on_flight_data_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["observers"][0]["address"] = "10.77.10.50"
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_duplicate_observer_is_rejected(self) -> None:
        invalid = copy.deepcopy(self.policy)
        invalid["observers"][1]["address"] = invalid["observers"][0]["address"]
        with self.assertRaises(MODULE.PolicyError):
            MODULE.validate(invalid)

    def test_server_firewall_is_narrow_and_has_no_forward_or_nat_hook(self) -> None:
        firewall = (
            ROOT / "deploy" / "network" / "efis-mavlink-observer-server.nft"
        ).read_text(encoding="utf-8")
        normalized = " ".join(
            line.split("#", 1)[0].strip()
            for line in firewall.splitlines()
            if line.split("#", 1)[0].strip()
        )
        self.assertIn("table inet efis_mavlink_observer", normalized)
        self.assertIn("type filter hook input priority -10; policy accept;", normalized)
        self.assertIn("ip daddr 192.168.1.6 udp dport 14550", normalized)
        self.assertIn("ip saddr != 192.168.1.47 counter drop", normalized)
        self.assertNotIn("hook forward", normalized)
        self.assertNotIn("hook output", normalized)
        self.assertNotIn("type nat", normalized)

    def test_ground_station_firewall_is_monitor_only_and_narrow(self) -> None:
        firewall = (
            ROOT / "deploy" / "network" / "efis-qgc-monitor-firewall.nft"
        ).read_text(encoding="utf-8")
        normalized = " ".join(
            line.split("#", 1)[0].strip()
            for line in firewall.splitlines()
            if line.split("#", 1)[0].strip()
        )
        self.assertIn("table inet efis_qgc_monitor", normalized)
        self.assertIn("hook input priority -10; policy accept;", normalized)
        self.assertIn("hook output priority -10; policy accept;", normalized)
        self.assertIn("ip daddr 192.168.1.39 udp dport 14550", normalized)
        self.assertIn("ip saddr != 192.168.1.47 counter drop", normalized)
        self.assertIn(
            "ip daddr 192.168.1.47 meta l4proto udp counter drop", normalized
        )
        self.assertNotIn("ip daddr 192.168.1.47 udp counter", normalized)
        self.assertIn("ip daddr 10.77.10.0/24 counter drop", normalized)
        self.assertNotIn("hook forward", normalized)
        self.assertNotIn("type nat", normalized)
        self.assertNotIn("counter accept", normalized)

    def test_ground_station_firewall_installer_checks_before_enable(self) -> None:
        installer = (
            ROOT / "deploy" / "scripts" / "install_qgc_monitor_firewall.sh"
        ).read_text(encoding="utf-8")
        self.assertIn('if [ "$(id -u)" -ne 0 ]', installer)
        self.assertLess(installer.index("nft --check"), installer.index("enable --now"))
        self.assertLess(
            installer.index("systemd-analyze verify"),
            installer.index("enable --now"),
        )

    def test_ground_station_firewall_arms_rollback_before_enable(self) -> None:
        installer = (
            ROOT / "deploy" / "scripts" / "install_qgc_monitor_firewall.sh"
        ).read_text(encoding="utf-8")
        arm = installer.index("systemd-run")
        verify_timer = installer.index(
            "systemctl is-active --quiet efis-qgc-firewall-rollback.timer"
        )
        enable = installer.index(
            "systemctl enable --now efis-qgc-monitor-firewall.service"
        )
        self.assertLess(arm, verify_timer)
        self.assertLess(verify_timer, enable)
        self.assertIn("--on-active=300s", installer)

    def test_firewall_commit_verifies_before_cancelling_rollback(self) -> None:
        script = (
            ROOT / "deploy" / "scripts" / "commit_qgc_monitor_firewall.sh"
        ).read_text(encoding="utf-8")
        verify_service = script.index(
            "systemctl is-active --quiet efis-qgc-monitor-firewall.service"
        )
        verify_table = script.index("nft list table inet efis_qgc_monitor")
        verify_qgc = script.index("grep -Fq 'QGroundControl'")
        cancel = script.index("systemctl stop efis-qgc-firewall-rollback.timer")
        self.assertLess(verify_service, cancel)
        self.assertLess(verify_table, cancel)
        self.assertLess(verify_qgc, cancel)

    def test_firewall_rollback_is_narrow(self) -> None:
        script = (
            ROOT / "deploy" / "scripts" / "rollback_qgc_monitor_firewall.sh"
        ).read_text(encoding="utf-8")
        self.assertIn("delete table inet efis_qgc_monitor", script)
        self.assertIn("disable --now efis-qgc-monitor-firewall.service", script)
        self.assertNotIn("flush ruleset", script)

    def test_ground_station_firewall_unit_has_no_forwarding_side_effect(self) -> None:
        unit = (
            ROOT / "deploy" / "systemd" / "efis-qgc-monitor-firewall.service"
        ).read_text(encoding="utf-8")
        self.assertIn("/etc/efis/qgc-monitor-firewall.nft", unit)
        self.assertIn("RemainAfterExit=yes", unit)
        self.assertNotIn("ip_forward", unit)
        self.assertNotIn("NetworkNamespacePath", unit)


if __name__ == "__main__":
    unittest.main()
