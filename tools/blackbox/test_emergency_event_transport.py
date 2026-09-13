#!/usr/bin/env python3

import json
import sys
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from http.server import ThreadingHTTPServer
from pathlib import Path
from unittest import mock

sys.path.insert(0, str(Path(__file__).parent))
import emergency_event_forwarder as forwarder
import emergency_event_hub as hub_module
import emergency_event_replay as replay


def event(sequence=1, detail="test"):
    return {
        "schema": hub_module.SCHEMA,
        "session_id": "session-a",
        "event_sequence": sequence,
        "written_utc": "2026-09-13T10:00:00.000Z",
        "code": "PILOT_ACTIVATED",
        "detail": detail,
        "assist_state": "PILOT ACTIVATED",
        "emergency_kind": "ENGINE POWER LOSS",
        "simulation": True,
    }


class EmergencyTransportTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        root = Path(self.temporary.name)
        self.hub = hub_module.EmergencyEventHub(root / "events.sqlite3", {"PFD-01": "secret"})
        self.server = ThreadingHTTPServer(("127.0.0.1", 0), hub_module.make_handler(self.hub))
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = "http://127.0.0.1:" + str(self.server.server_address[1])

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join(timeout=2)
        self.hub.close()
        self.temporary.cleanup()

    def test_authenticated_ack_is_stable_and_persisted(self):
        first = forwarder.post_event(self.url, "PFD-01", "secret", event())
        second = forwarder.post_event(self.url, "PFD-01", "secret", event())
        self.assertTrue(first["persisted"])
        self.assertTrue(first["inserted"])
        self.assertFalse(second["inserted"])
        self.assertEqual(first["id"], second["id"])
        self.assertEqual(self.hub.count(), 1)

    def test_sequence_conflict_and_unauthorized_source_are_rejected(self):
        forwarder.post_event(self.url, "PFD-01", "secret", event())
        with self.assertRaises(urllib.error.HTTPError) as conflict:
            forwarder.post_event(self.url, "PFD-01", "secret", event(detail="different"))
        self.assertEqual(conflict.exception.code, 409)
        with self.assertRaises(urllib.error.HTTPError) as denied:
            forwarder.post_event(self.url, "PFD-01", "wrong", event(2))
        self.assertEqual(denied.exception.code, 401)
        self.assertEqual(self.hub.count(), 1)

    def test_forwarder_advances_only_after_verified_ack(self):
        root = Path(self.temporary.name)
        journal = root / "events.jsonl"
        journal.write_text(json.dumps(event()) + "\n" + json.dumps(event(2)) + "\n",
                           encoding="utf-8")
        state_path = root / "forwarder-state.json"
        sent, state = forwarder.forward_once(
            journal, state_path, self.url, "PFD-01", "secret")
        self.assertEqual(sent, 2)
        self.assertEqual(state["last_event_sequence"], 2)
        self.assertEqual(state["offset"], journal.stat().st_size)
        sent_again, _ = forwarder.forward_once(
            journal, state_path, self.url, "PFD-01", "secret")
        self.assertEqual(sent_again, 0)
        self.assertEqual(self.hub.count(), 2)

    def test_forwarder_does_not_advance_when_ack_validation_fails(self):
        root = Path(self.temporary.name)
        journal = root / "unacknowledged.jsonl"
        journal.write_text(json.dumps(event()) + "\n", encoding="utf-8")
        state_path = root / "unacknowledged-state.json"
        with mock.patch.object(forwarder, "post_event",
                               side_effect=ValueError("invalid ack")):
            with self.assertRaises(ValueError):
                forwarder.forward_once(
                    journal, state_path, self.url, "PFD-01", "secret")
        self.assertFalse(state_path.exists())

    def test_committed_record_survives_database_reopen(self):
        database = Path(self.temporary.name) / "reopen.sqlite3"
        first = hub_module.EmergencyEventHub(database, {"PFD-01": "secret"})
        digest, inserted, _ = first.insert("PFD-01", event())
        self.assertTrue(inserted)
        first.close()
        reopened = hub_module.EmergencyEventHub(database, {"PFD-01": "secret"})
        try:
            self.assertEqual(reopened.count(), 1)
            self.assertEqual(reopened.recent(1)[0]["id"], digest)
        finally:
            reopened.close()

    def test_health_exposes_no_command_api(self):
        with urllib.request.urlopen(self.url + "/health") as response:
            health = json.load(response)
        self.assertFalse(health["command_api"])
        request = urllib.request.Request(self.url + "/v1/command", b"{}", method="POST")
        with self.assertRaises(urllib.error.HTTPError) as missing:
            urllib.request.urlopen(request)
        self.assertEqual(missing.exception.code, 404)

    def test_replay_detects_gaps_without_executing_actions(self):
        report = replay.inspect([(1, event(1)), (2, event(3))])
        self.assertFalse(report["valid"])
        self.assertEqual(len(report["sequence_gaps"]), 1)
        self.assertTrue(report["advisory_only"])


if __name__ == "__main__":
    unittest.main()
