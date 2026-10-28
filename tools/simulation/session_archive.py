"""Bounded SIL run archive/restore. No models, credentials, network or overwrite.

Checksums attest integrity, NOT authenticity, aircraft fidelity or power-loss
durability. Failed restores remain incomplete evidence in their new directory.
"""
import argparse
import hashlib
import io
import json
import os
from pathlib import Path
import stat
import tarfile

from tools.simulation import session_contract as c
from tools.simulation.session_engine import encode
from tools.simulation.session_trace import verify

FILES = ("truth.jsonl", "result.json", "probe-report.json")
MEMBERS = ("archive-manifest.json", *FILES, "archive-complete.json")
MAX_ARCHIVE = 128 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024


def sha(data): return hashlib.sha256(data).hexdigest()


def fingerprint(info):
    # On this Windows Python, lstat and fstat expose different ctime values
    # for the same unchanged file. Identity/size/mtime remain comparable;
    # POSIX additionally binds ctime. Full payload equality is checked too.
    base = info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns
    return base if os.name == "nt" else (*base,info.st_ctime_ns)


def regular(path):
    info = path.lstat()
    c.require(stat.S_ISREG(info.st_mode) and not path.is_symlink(), "regular file required")
    c.require(info.st_size <= MAX_FILE, "file size limit")
    return info


def read_file(path):
    before = regular(path)
    with path.open("rb") as stream:
        c.require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before), "source changed before read")
        data = stream.read(MAX_FILE+1)
        c.require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before), "source changed during read")
    c.require(len(data) == before.st_size and len(data) <= MAX_FILE, "source length changed")
    return data


def validate_manifest(m):
    c.exact(m, "schema simulated flight_approval session_id source_commit config_sha256 model_sha256 "
            "profile_status included_assets license_review files")
    c.require(m["schema"] == "fuso-avionics.sil-archive.v1" and m["simulated"] is True
              and m["flight_approval"] is False, "archive boundary")
    c.token(m["session_id"])
    c.require(isinstance(m["source_commit"],str) and len(m["source_commit"])==40
              and all(ch in "0123456789abcdef" for ch in m["source_commit"]), "source revision")
    for key in ("config_sha256", "model_sha256"): c.digest(m[key])
    c.require(m["profile_status"] in ("SYNTHETIC_FIXTURE", "REFERENCE_UNVALIDATED")
              and m["included_assets"] == [] and m["license_review"] == "MODEL_ASSETS_NOT_INCLUDED",
              "model redistribution/validation not authorized")
    c.require(isinstance(m["files"],dict) and set(m["files"]) == set(FILES), "archive file allowlist")
    for name, item in m["files"].items():
        c.exact(item,"bytes sha256")
        c.integer(item["bytes"],1,MAX_FILE if name=="truth.jsonl" else 65536)
        c.digest(item["sha256"])
    return m


def run_binding(data, manifest):
    first = data["truth.jsonl"].split(b"\n",1)[0]
    config = c.decode(first)
    c.require(config["kind"] == "RUN_CONFIGURATION", "missing run configuration")
    source = c.manifest(config["manifest"])
    for key in ("session_id","source_commit","config_sha256","model_sha256","profile_status"):
        c.require(source[key] == manifest[key], "archive/run binding")
    result = c.decode(data["result.json"])
    probe = c.decode(data["probe-report.json"])
    c.require(result["session_id"] == source["session_id"] and result["simulated"] is True
              and result["flight_approval"] is False and result["status"] == "COMPLETE_SOFTWARE_ONLY",
              "archive result binding")
    c.require(probe["simulated"] is True and probe["flight_approval"] is False
              and probe["status"] == "COMPLETE" and probe["total_ticks"] == result["total_ticks"]
              and probe["frames"] == result["frames"], "probe/result binding")


def pack(run, archive):
    root, destination = Path(run), Path(archive)
    c.require(root.is_dir() and not root.is_symlink(), "source directory required")
    # Read only the explicit allowlist; no private writer paths or assets.
    files = {name:read_file(root/name) for name in FILES}
    verification = verify(root)
    # Bind the exact bytes archived to the successful verifier observation.
    c.require(all(read_file(root/name) == data for name,data in files.items()), "run changed during verification")
    source = c.manifest(c.decode(files["truth.jsonl"].split(b"\n",1)[0])["manifest"])
    m = {key:source[key] for key in ("session_id","source_commit","config_sha256","model_sha256","profile_status")}
    m.update(schema="fuso-avionics.sil-archive.v1",simulated=True,flight_approval=False,
             included_assets=[],license_review="MODEL_ASSETS_NOT_INCLUDED",
             files={name:dict(bytes=len(data),sha256=sha(data)) for name,data in files.items()})
    validate_manifest(m); run_binding(files,m)
    manifest = encode(m)
    complete = encode(dict(schema="fuso-avionics.sil-archive-complete.v1",manifest_sha256=sha(manifest)))
    with destination.open("xb") as output:
        with tarfile.open(fileobj=output,mode="w",format=tarfile.USTAR_FORMAT) as tar:
            for name,data in (("archive-manifest.json",manifest),*files.items(),("archive-complete.json",complete)):
                info = tarfile.TarInfo(name); info.size=len(data); info.mode=0o600
                tar.addfile(info,io.BytesIO(data))
        output.flush(); os.fsync(output.fileno())
    c.require(destination.stat().st_size <= MAX_ARCHIVE,"archive size limit")
    return dict(status="PASS_SOFTWARE_ONLY",session_id=m["session_id"],trace=verification,
                archive_sha256=hash_file(destination),flight_approval=False)


def hash_file(path):
    h = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda:stream.read(1024*1024),b""):h.update(block)
    return h.hexdigest()


def restore(archive, directory):
    source, destination = Path(archive), Path(directory)
    info = source.lstat()
    c.require(stat.S_ISREG(info.st_mode) and not source.is_symlink() and info.st_size <= MAX_ARCHIVE,
              "archive type/size")
    with source.open("rb") as raw:
        c.require(fingerprint(os.fstat(raw.fileno())) == fingerprint(info), "archive changed before read")
        files = {}; expected_offset=0
        # Uncompressed tar only; never extractall, never restore mode/uid/path metadata.
        with tarfile.open(fileobj=raw,mode="r:") as tar:
            for index, member in enumerate(tar):
                c.require(index<len(MEMBERS) and member.name==MEMBERS[index]
                          and member.isreg() and not member.issparse() and not member.pax_headers,
                          "archive member/order/type")
                c.require(member.offset==expected_offset and member.offset_data==expected_offset+512,
                          "archive hidden header")
                limit = MAX_FILE if member.name=="truth.jsonl" else 65536
                c.integer(member.size,1,limit)
                data = tar.extractfile(member).read(limit+1)
                c.require(len(data)==member.size,"archive member truncated")
                files[member.name]=data
                expected_offset=member.offset_data+((member.size+511)//512)*512
        raw.seek(expected_offset)
        tail=raw.read(MAX_ARCHIVE+1)
        c.require(1024<=len(tail)<=10240 and not any(tail),"archive footer/trailing payload")
        raw.seek(0); archive_digest=hashlib.sha256()
        for block in iter(lambda:raw.read(1024*1024),b""):archive_digest.update(block)
        c.require(fingerprint(os.fstat(raw.fileno())) == fingerprint(info), "archive changed during read")
    c.require(tuple(files)==MEMBERS,"archive incomplete")
    m = validate_manifest(c.decode(files.pop("archive-manifest.json")))
    complete = c.decode(files.pop("archive-complete.json"))
    c.exact(complete,"schema manifest_sha256")
    c.require(complete["schema"]=="fuso-avionics.sil-archive-complete.v1"
              and complete["manifest_sha256"]==sha(encode(m)),"archive completion digest")
    for name,data in files.items():
        c.require(len(data)==m["files"][name]["bytes"] and sha(data)==m["files"][name]["sha256"],
                  "archive payload digest")
    run_binding(files,m)
    # All archive/path/hash checks precede any write. Existing target is NEVER reused.
    destination.mkdir(parents=False,exist_ok=False)
    for name,data in files.items():
        with (destination/name).open("xb") as output:
            c.require(output.write(data)==len(data),"short restore write")
            output.flush(); os.fsync(output.fileno())
    verification = verify(destination)
    report = dict(schema="fuso-avionics.sil-restore.v1",status="PASS_SOFTWARE_ONLY",
                  archive_sha256=archive_digest.hexdigest(),session_id=m["session_id"],trace=verification,
                  simulated=True,flight_approval=False,hardware_acceptance=False)
    with (destination/"restore-complete.json").open("xb") as output:
        data=encode(report); c.require(output.write(data)==len(data),"short restore receipt")
        output.flush();os.fsync(output.fileno())
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    sub=parser.add_subparsers(dest="action",required=True)
    p=sub.add_parser("pack");p.add_argument("--run",type=Path,required=True);p.add_argument("--archive",type=Path,required=True)
    p=sub.add_parser("restore");p.add_argument("--archive",type=Path,required=True);p.add_argument("--directory",type=Path,required=True)
    args=parser.parse_args()
    try:print(json.dumps(pack(args.run,args.archive) if args.action=="pack" else restore(args.archive,args.directory)))
    except (ValueError,OSError,KeyError,TypeError,tarfile.TarError) as exc:parser.exit(2,f"SIL archive FAILED: {exc}\n")


if __name__=="__main__":main()
