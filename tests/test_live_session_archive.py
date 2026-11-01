import io
import tarfile
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from tools.simulation import session_archive as a,session_contract as c
from tools.simulation.control_audit import FILE
from tools.simulation.session_engine import encode
from tools.simulation.session_trace import verify
from tests.live_control_fixture import run_live


class LiveArchiveTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.run=self.root/"run";run_live(self.run)
        self.archive=self.root/"live.tar"

    def rewrite(self,mutate):
        a.pack(self.run,self.archive,live=True)
        with tarfile.open(self.archive,"r:") as tar:
            files={m.name:tar.extractfile(m).read() for m in tar}
        mutate(files)
        m=c.decode(files["archive-manifest.json"])
        for name in a.LIVE_FILES:
            if name in files:m["files"][name]=dict(bytes=len(files[name]),sha256=a.sha(files[name]))
        files["archive-manifest.json"]=encode(m)
        files["archive-complete.json"]=encode(dict(schema="fuso-avionics.sil-archive-complete.v2",
                                                  manifest_sha256=a.sha(encode(m))))
        changed=self.root/"changed.tar"
        with tarfile.open(changed,"w",format=tarfile.USTAR_FORMAT) as tar:
            for name,data in files.items():
                info=tarfile.TarInfo(name);info.size=len(data);tar.addfile(info,io.BytesIO(data))
        return changed

    def test_v2_exact_four_payload_roundtrip(self):
        a.pack(self.run,self.archive,live=True)
        target=self.root/"restored"
        report=a.restore(self.archive,target,live=True)
        self.assertEqual(report["schema"],"fuso-avionics.sil-restore.v2")
        self.assertEqual(report["trace"]["control_execution"]["applied"],2)
        for name in a.LIVE_FILES:self.assertEqual((target/name).read_bytes(),(self.run/name).read_bytes())

    def test_explicit_mode_never_falls_back_to_legacy(self):
        with self.assertRaises(ValueError):a.pack(self.run,self.archive)
        self.assertFalse(self.archive.exists())
        a.pack(self.run,self.archive,live=True)
        target=self.root/"restore"
        with self.assertRaises(ValueError):a.restore(self.archive,target)
        self.assertFalse(target.exists())

    def test_existing_targets_are_never_overwritten(self):
        a.pack(self.run,self.archive,live=True);original=self.archive.read_bytes()
        with self.assertRaises(FileExistsError):a.pack(self.run,self.archive,live=True)
        self.assertEqual(self.archive.read_bytes(),original)
        target=self.root/"keep";target.mkdir();(target/"keep.txt").write_bytes(b"keep")
        with self.assertRaises(FileExistsError):a.restore(self.archive,target,live=True)
        self.assertEqual((target/"keep.txt").read_bytes(),b"keep")

    def test_missing_sidecar_never_exports(self):
        (self.run/FILE).unlink()
        with self.assertRaises(OSError):a.pack(self.run,self.archive,live=True)
        self.assertFalse(self.archive.exists())

    def test_resealed_false_readback_fails_without_restore_receipt(self):
        def mutate(files):
            rows=[c.decode(line) for line in files[FILE].splitlines()]
            next(r for r in rows if r["event"]=="APPLIED")["readback"]=-.9
            files[FILE]=b"".join(encode(r) for r in rows)
            result=c.decode(files["result.json"])
            result["control_execution"].update(bytes=len(files[FILE]),sha256=a.sha(files[FILE]))
            files["result.json"]=encode(result)
        changed=self.rewrite(mutate);target=self.root/"bad-restore"
        with self.assertRaisesRegex(ValueError,"readback"):a.restore(changed,target,live=True)
        self.assertFalse((target/"restore-complete.json").exists())

    def test_v1_schema_in_v2_container_cannot_be_accepted(self):
        def mutate(files):
            result=c.decode(files["archive-manifest.json"]);result["schema"]="fuso-avionics.sil-archive.v1"
            files["archive-manifest.json"]=encode(result)
        changed=self.rewrite(mutate)
        with self.assertRaises(ValueError):a.restore(changed,self.root/"target",live=True)

    def test_missing_or_unknown_payload_cannot_fall_back(self):
        for missing in (True,False):
            with self.subTest(missing=missing):
                if self.archive.exists():
                    # New fixture owns each candidate; no deletion of source evidence.
                    self.archive=self.root/"second.tar"
                def mutate(files):
                    if missing:files.pop(FILE)
                    else:files["../outside"]=b"bad"
                changed=self.rewrite(mutate)
                with self.assertRaises(ValueError):a.restore(changed,self.root/f"target-{missing}",live=True)

    def test_changed_source_after_verification_never_exports(self):
        original=a.verify
        def mutate(root,**kwargs):
            result=original(root,**kwargs)
            with (root/FILE).open("ab") as stream:stream.write(b"\n")
            return result
        with patch.object(a,"verify",side_effect=mutate):
            with self.assertRaisesRegex(ValueError,"changed"):a.pack(self.run,self.archive,live=True)
        self.assertFalse(self.archive.exists())

    def test_appended_tar_payload_is_rejected(self):
        a.pack(self.run,self.archive,live=True)
        with self.archive.open("ab") as stream:stream.write(b"hidden")
        with self.assertRaises(ValueError):a.restore(self.archive,self.root/"target",live=True)

    def test_sidecar_size_cap_checked_before_export(self):
        with (self.run/FILE).open("ab") as stream:stream.write(b"x"*1048577)
        with self.assertRaises(ValueError):a.pack(self.run,self.archive,live=True)
        self.assertFalse(self.archive.exists())


if __name__=="__main__":unittest.main()
