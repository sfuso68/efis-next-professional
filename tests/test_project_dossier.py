import json
from pathlib import Path
import tempfile
import unittest
from tools.project_status import project_dossier as d


class DossierTests(unittest.TestCase):
    def test_snapshot_matches_canonical_matrix_and_sim_boundary(self):
        s=d.collect()
        self.assertEqual(len(s["requirements"]),s["status"]["requirements"])
        self.assertEqual(sum(s["status"]["status_counts"].values()),len(s["requirements"]))
        self.assertFalse(s["flight_approval"])
        self.assertEqual(s["simulation"]["completion"],"NOT_COMPLETE")
        self.assertEqual(s["scope"],"DOCUMENTARY_SNAPSHOT_NOT_LIVE_NODE_AUDIT")

    def test_all_requirement_details_are_preserved_in_snapshot(self):
        s=d.collect()
        for row in s["requirements"]:
            self.assertTrue(all(k in row for k in ("id","status","evidence","risk","next_step","acceptance")))
        self.assertEqual(len({r["id"] for r in s["requirements"]}),len(s["requirements"]))
        self.assertEqual({r["id"] for r in s["requirements"] if r["status"]=="BLOCKED"},{"PAN-002","PAN-003"})

    def test_sources_are_hash_bound_and_security_block_not_hidden(self):
        s=d.collect()
        self.assertTrue(all(len(x["sha256"])==64 for x in s["sources"]))
        self.assertEqual(s["evidence"]["ctest"]["status"],"FAIL")
        self.assertFalse(s["evidence"]["ctest"]["policy_changed"])
        self.assertEqual(s["evidence"]["nas_replication"],"NOT_PERFORMED")

    def test_stale_generated_status_fails_before_publication(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);(root/"docs/status").mkdir(parents=True)
            (root/d.SOURCES[1]).write_bytes((d.ROOT/d.SOURCES[1]).read_bytes())
            (root/"docs/status/efis-next-status.json").write_text(json.dumps({"requirements":0}),encoding="utf-8")
            with self.assertRaisesRegex(ValueError,"stale"):d.collect(root)


if __name__=="__main__":unittest.main()
