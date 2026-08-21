import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import edge_health

class EdgeHealthTest(unittest.TestCase):
    def test_snapshot_is_explicitly_non_transmitting(self):
        with patch.object(edge_health, "service_state", return_value="active"), \
             patch.object(edge_health, "network_link", return_value={"present":True,"operstate":"UP","flags":[],"rx":{},"tx":{},"details":{}}):
            status=edge_health.snapshot()
        self.assertFalse(status["policy"]["can_transmit_allowed"])
        self.assertFalse(status["policy"]["flight_critical_dependency"])

    def test_atomic_status_file(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/"health.json"
            edge_health.atomic_json(target,{"health":"OK"})
            self.assertIn('"OK"',target.read_text())

if __name__=="__main__": unittest.main()
