import copy
import hashlib
import io
import tarfile
import tempfile
from pathlib import Path
import unittest

from tools.simulation import session_archive as a
from tools.simulation import session_contract as c
from tools.simulation.session_engine import SessionEngine, TruthJournal, encode, TICK_NS
from tools.simulation.session_jsbsim import PhysicalAdapter
from tests.test_session_jsbsim import raw_fixture
from tests.test_sil_session_engine import Clock


def fixture_run(root):
    """Synthetic virtual-clock trace, NOT a JSBSim performance assertion."""
    manifest=c.decode((c.ROOT/"tests/data/protocols/sil-session-v1-vectors.json").read_bytes())["manifest"]
    config={"test_fixture":True}
    manifest["config_sha256"]=hashlib.sha256(encode(config)).hexdigest()
    class Backend:
        ticks=0
        def step(self, _):self.ticks+=1
        def frame(self, scenario, session_id):
            raw=raw_fixture();raw.update(sim_time_ms=self.ticks*10,session_id=session_id)
            return raw
    journal=TruthJournal(root)
    journal.append(dict(kind="RUN_CONFIGURATION",configuration=config,manifest=manifest))
    clock=Clock();engine=SessionEngine(manifest,PhysicalAdapter(Backend(),"cruise",journal),journal,
                                     clock=clock,max_ticks=10)
    engine.validate();engine.arm_software_test();engine.start()
    for _ in range(10):clock.now+=TICK_NS;engine.pump()
    (root/"probe-report.json").write_bytes(encode(dict(status="COMPLETE",frames=engine.frames,
        total_ticks=engine.total_ticks,simulated=True,flight_approval=False)))


class SessionArchiveTests(unittest.TestCase):
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup)
        self.root=Path(self.tmp.name);self.run=self.root/"run";fixture_run(self.run)
        self.archive=self.root/"run.tar"

    def rewrite(self, mutate):
        a.pack(self.run,self.archive)
        with tarfile.open(self.archive,"r:") as tar:
            members=[(copy.copy(m),tar.extractfile(m).read()) for m in tar]
        members=mutate(members)
        changed=self.root/"changed.tar"
        with tarfile.open(changed,"w",format=tarfile.USTAR_FORMAT) as tar:
            for info,data in members:
                if info.isreg():info.size=len(data)
                tar.addfile(info,io.BytesIO(data) if info.isreg() else None)
        return changed

    def reject(self, archive):
        destination=self.root/"restore"
        with self.assertRaises((ValueError,OSError,tarfile.TarError)):a.restore(archive,destination)
        self.assertFalse((destination/"restore-complete.json").exists())

    def test_roundtrip_exact_bytes_and_receipt(self):
        report=a.pack(self.run,self.archive)
        destination=self.root/"restored"
        restored=a.restore(self.archive,destination)
        self.assertEqual(restored["archive_sha256"],report["archive_sha256"])
        self.assertEqual(restored["trace"]["status"],"PASS")
        self.assertFalse(restored["flight_approval"])
        for name in a.FILES:self.assertEqual((self.run/name).read_bytes(),(destination/name).read_bytes())
        self.assertTrue((destination/"restore-complete.json").is_file())

    def test_existing_archive_and_destination_are_preserved(self):
        a.pack(self.run,self.archive);original=self.archive.read_bytes()
        with self.assertRaises(FileExistsError):a.pack(self.run,self.archive)
        self.assertEqual(self.archive.read_bytes(),original)
        destination=self.root/"exists";destination.mkdir()
        (destination/"keep").write_bytes(b"existing")
        with self.assertRaises(FileExistsError):a.restore(self.archive,destination)
        self.assertEqual((destination/"keep").read_bytes(),b"existing")

    def test_missing_source_probe_or_corrupt_result_never_exports(self):
        (self.run/"probe-report.json").unlink()
        with self.assertRaises(OSError):a.pack(self.run,self.archive)
        self.assertFalse(self.archive.exists())

    def test_probe_mismatch_never_exports(self):
        probe=c.decode((self.run/"probe-report.json").read_bytes());probe["frames"]+=1
        (self.run/"probe-report.json").write_bytes(encode(probe))
        with self.assertRaises(ValueError):a.pack(self.run,self.archive)
        self.assertFalse(self.archive.exists())

    def test_payload_digest_mutation(self):
        def change(items):
            info,data=items[1];items[1]=(info,data+b" ");return items
        self.reject(self.rewrite(change))

    def test_traversal_and_absolute_names_rejected_before_write(self):
        for name in ("../escape","/absolute","C:/outside"):
            with self.subTest(name=name):
                root=Path(self.tmp.name)/name.replace("/","_").replace(":","_")
                # Reuse the immutable source, but a fresh archive per mutation.
                self.archive=self.root/(root.name+".tar")
                def change(items):items[1][0].name=name;return items
                changed=self.rewrite(change)
                self.reject(changed)
                changed.unlink()
        self.assertFalse((self.root/"escape").exists())

    def test_symlink_hardlink_and_device_members_rejected(self):
        for kind in (tarfile.SYMTYPE,tarfile.LNKTYPE,tarfile.CHRTYPE):
            self.archive=self.root/(str(kind)+".tar")
            def change(items):
                items[1][0].type=kind;items[1][0].linkname="result.json";items[1][0].size=0
                return items
            changed=self.rewrite(change);self.reject(changed);changed.unlink()

    def test_duplicate_unknown_and_missing_completion(self):
        for action in ("duplicate","unknown","missing"):
            self.archive=self.root/(action+".tar")
            def change(items):
                if action=="duplicate":items.insert(2,copy.deepcopy(items[1]))
                elif action=="unknown":items[-1][0].name="extra.json"
                else:items.pop()
                return items
            changed=self.rewrite(change);self.reject(changed);changed.unlink()

    def test_non_sim_boundary_rejected_even_with_new_completion_digest(self):
        def change(items):
            m=c.decode(items[0][1]);m["flight_approval"]=True
            manifest=encode(m);items[0]=(items[0][0],manifest)
            end=c.decode(items[-1][1]);end["manifest_sha256"]=a.sha(manifest)
            items[-1]=(items[-1][0],encode(end));return items
        self.reject(self.rewrite(change))

    def test_truncated_and_appended_hidden_payload_rejected(self):
        a.pack(self.run,self.archive);data=self.archive.read_bytes()
        for suffix,value in (("short",data[:-1024]),("extra",data+b"hidden")):
            changed=self.root/(suffix+".tar");changed.write_bytes(value)
            if suffix=="short":
                # Remove all valid footer, not merely optional record padding.
                with tarfile.open(self.archive,"r:") as tar:last=list(tar)[-1]
                end=last.offset_data+((last.size+511)//512)*512
                changed.write_bytes(data[:end])
            self.reject(changed)

    def test_manifest_bound_model_and_asset_policy(self):
        def change(items):
            m=c.decode(items[0][1]);m["model_sha256"]="f"*64
            manifest=encode(m);items[0]=(items[0][0],manifest)
            end=c.decode(items[-1][1]);end["manifest_sha256"]=a.sha(manifest)
            items[-1]=(items[-1][0],encode(end));return items
        self.reject(self.rewrite(change))

    def test_semantic_trace_corruption_even_when_all_archive_hashes_updated(self):
        def change(items):
            info,data=items[1]
            rows=data.splitlines();event=c.decode(rows[-1]);event["event"]="RUNNING"
            rows[-1]=encode(event).rstrip(b"\n");data=b"\n".join(rows)+b"\n";items[1]=(info,data)
            m=c.decode(items[0][1]);m["files"]["truth.jsonl"].update(bytes=len(data),sha256=a.sha(data))
            result=c.decode(items[2][1]);result["truth_sha256"]=a.sha(data);result_data=encode(result)
            items[2]=(items[2][0],result_data)
            m["files"]["result.json"].update(bytes=len(result_data),sha256=a.sha(result_data))
            manifest=encode(m);items[0]=(items[0][0],manifest)
            end=c.decode(items[-1][1]);end["manifest_sha256"]=a.sha(manifest)
            items[-1]=(items[-1][0],encode(end));return items
        self.reject(self.rewrite(change))

    def test_metadata_limits(self):
        m=dict(schema="fuso-avionics.sil-archive.v1",simulated=True,flight_approval=False,
               session_id="fixture",source_commit="0"*40,config_sha256="a"*64,model_sha256="b"*64,
               profile_status="REFERENCE_UNVALIDATED",included_assets=[],license_review="MODEL_ASSETS_NOT_INCLUDED",
               files={name:dict(bytes=1,sha256="c"*64) for name in a.FILES})
        a.validate_manifest(m)
        for changes in ({"included_assets":["c172.xml"]},{"profile_status":"APPROVED"}):
            with self.assertRaises(ValueError):a.validate_manifest(dict(m,**changes))
        m["files"]["truth.jsonl"]["bytes"]=a.MAX_FILE+1
        with self.assertRaises(ValueError):a.validate_manifest(m)

    def test_pax_extended_metadata_is_not_an_extraction_backdoor(self):
        a.pack(self.run,self.archive)
        changed=self.root/"pax.tar"
        with tarfile.open(self.archive,"r:") as src, tarfile.open(changed,"w",format=tarfile.PAX_FORMAT) as out:
            for member in src:
                data=src.extractfile(member).read()
                member.pax_headers={"comment":"untrusted metadata"}
                out.addfile(member,io.BytesIO(data))
        self.reject(changed)

    def test_compressed_input_is_rejected_not_decompressed(self):
        changed=self.root/"compressed.tar.gz"
        with tarfile.open(changed,"w:gz") as out:
            member=tarfile.TarInfo("archive-manifest.json");member.size=2
            out.addfile(member,io.BytesIO(b"{}"))
        self.reject(changed)


if __name__=="__main__":unittest.main()
