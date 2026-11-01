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
from tools.simulation.control_audit import FILE as CONTROL_FILE, MAX_BYTES as MAX_CONTROL

FILES = ("truth.jsonl", "result.json", "probe-report.json")
MEMBERS = ("archive-manifest.json", *FILES, "archive-complete.json")
LIVE_FILES = (*FILES, CONTROL_FILE)
LIVE_MEMBERS = ("archive-manifest.json", *LIVE_FILES, "archive-complete.json")
MAX_ARCHIVE = 128 * 1024 * 1024
MAX_FILE = 64 * 1024 * 1024


def sha(data): return hashlib.sha256(data).hexdigest()


def fingerprint(info):
    # On this Windows Python, lstat and fstat expose different ctime values
    # for the same unchanged file. Identity/size/mtime remain comparable;
    # POSIX additionally binds ctime. Full payload equality is checked too.
    base = info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns
    return base if os.name == "nt" else (*base,info.st_ctime_ns)


def regular(path, limit=MAX_FILE):
    info = path.lstat()
    c.require(stat.S_ISREG(info.st_mode) and not path.is_symlink(), "regular file required")
    c.require(info.st_size <= limit, "file size limit")
    return info


def read_file(path, limit=MAX_FILE):
    before = regular(path,limit)
    with path.open("rb") as stream:
        c.require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before), "source changed before read")
        data = stream.read(limit+1)
        c.require(fingerprint(os.fstat(stream.fileno())) == fingerprint(before), "source changed during read")
    c.require(len(data) == before.st_size and len(data) <= limit, "source length changed")
    return data


def file_limit(name, live):
    return MAX_FILE if name=="truth.jsonl" else MAX_CONTROL if live and name==CONTROL_FILE else 65536


def validate_manifest(m, *, live=False):
    c.exact(m, "schema simulated flight_approval session_id source_commit config_sha256 model_sha256 "
            "profile_status included_assets license_review files")
    version=2 if live else 1
    c.require(m["schema"] == f"fuso-avionics.sil-archive.v{version}" and m["simulated"] is True
              and m["flight_approval"] is False, "archive boundary")
    c.token(m["session_id"])
    c.require(isinstance(m["source_commit"],str) and len(m["source_commit"])==40
              and all(ch in "0123456789abcdef" for ch in m["source_commit"]), "source revision")
    for key in ("config_sha256", "model_sha256"): c.digest(m[key])
    c.require(m["profile_status"] in ("SYNTHETIC_FIXTURE", "REFERENCE_UNVALIDATED")
              and m["included_assets"] == [] and m["license_review"] == "MODEL_ASSETS_NOT_INCLUDED",
              "model redistribution/validation not authorized")
    c.require(isinstance(m["files"],dict) and set(m["files"]) == set(LIVE_FILES if live else FILES),
              "archive file allowlist")
    for name, item in m["files"].items():
        c.exact(item,"bytes sha256")
        c.integer(item["bytes"],1,file_limit(name,live))
        c.digest(item["sha256"])
    return m


def run_binding(data, manifest, *, live=False):
    first = data["truth.jsonl"].split(b"\n",1)[0]
    config = c.decode(first)
    c.require(config["kind"] == "RUN_CONFIGURATION", "missing run configuration")
    source = c.manifest(config["manifest"])
    for key in ("session_id","source_commit","config_sha256","model_sha256","profile_status"):
        c.require(source[key] == manifest[key], "archive/run binding")
    result = c.decode(data["result.json"])
    c.require(("control_execution" in result)==live
              and ("control_policy" in config["configuration"])==live,"archive control mode mismatch")
    probe = c.decode(data["probe-report.json"])
    c.require(result["session_id"] == source["session_id"] and result["simulated"] is True
              and result["flight_approval"] is False and result["status"] == "COMPLETE_SOFTWARE_ONLY",
              "archive result binding")
    c.require(probe["simulated"] is True and probe["flight_approval"] is False
              and probe["status"] == "COMPLETE" and probe["total_ticks"] == result["total_ticks"]
              and probe["frames"] == result["frames"], "probe/result binding")


def pack(run, archive, *, live=False):
    root, destination = Path(run), Path(archive)
    c.require(root.is_dir() and not root.is_symlink(), "source directory required")
    # Read only the explicit allowlist; no private writer paths or assets.
    files = {name:read_file(root/name,file_limit(name,live)) for name in (LIVE_FILES if live else FILES)}
    verification = verify(root,allow_control_audit=live)
    # Bind the exact bytes archived to the successful verifier observation.
    c.require(all(read_file(root/name,file_limit(name,live)) == data for name,data in files.items()),
              "run changed during verification")
    source = c.manifest(c.decode(files["truth.jsonl"].split(b"\n",1)[0])["manifest"])
    m = {key:source[key] for key in ("session_id","source_commit","config_sha256","model_sha256","profile_status")}
    version=2 if live else 1
    m.update(schema=f"fuso-avionics.sil-archive.v{version}",simulated=True,flight_approval=False,
             included_assets=[],license_review="MODEL_ASSETS_NOT_INCLUDED",
             files={name:dict(bytes=len(data),sha256=sha(data)) for name,data in files.items()})
    validate_manifest(m,live=live); run_binding(files,m,live=live)
    manifest = encode(m)
    complete = encode(dict(schema=f"fuso-avionics.sil-archive-complete.v{version}",manifest_sha256=sha(manifest)))
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


def restore(archive, directory, *, live=False):
    source, destination = Path(archive), Path(directory)
    info = source.lstat()
    c.require(stat.S_ISREG(info.st_mode) and not source.is_symlink() and info.st_size <= MAX_ARCHIVE,
              "archive type/size")
    members=LIVE_MEMBERS if live else MEMBERS
    with source.open("rb") as raw:
        c.require(fingerprint(os.fstat(raw.fileno())) == fingerprint(info), "archive changed before read")
        files = {}; expected_offset=0
        # Uncompressed tar only; never extractall, never restore mode/uid/path metadata.
        with tarfile.open(fileobj=raw,mode="r:") as tar:
            for index, member in enumerate(tar):
                c.require(index<len(members) and member.name==members[index]
                          and member.isreg() and not member.issparse() and not member.pax_headers,
                          "archive member/order/type")
                c.require(member.offset==expected_offset and member.offset_data==expected_offset+512,
                          "archive hidden header")
                limit = file_limit(member.name,live)
                c.integer(member.size,1,limit)
                data = tar.extractfile(member).read(limit+1)
                c.require(len(data)==member.size,"archive member truncated")
                files[member.name]=data
                expected_offset=member.offset_data+((member.size+511)//512)*512
        raw.seek(expected_offset)
        tail=raw.read(MAX_ARCHIVE+1)
        # Two mandatory zero blocks can cross a 10-KiB record boundary.
        # tarfile's canonical maximum padding is therefore 10 KiB + 512 B.
        c.require(1024<=len(tail)<=tarfile.RECORDSIZE+512 and len(tail)%512==0
                  and not any(tail),"archive footer/trailing payload")
        raw.seek(0); archive_digest=hashlib.sha256()
        for block in iter(lambda:raw.read(1024*1024),b""):archive_digest.update(block)
        c.require(fingerprint(os.fstat(raw.fileno())) == fingerprint(info), "archive changed during read")
    c.require(tuple(files)==members,"archive incomplete")
    m = validate_manifest(c.decode(files.pop("archive-manifest.json")),live=live)
    complete = c.decode(files.pop("archive-complete.json"))
    c.exact(complete,"schema manifest_sha256")
    version=2 if live else 1
    c.require(complete["schema"]==f"fuso-avionics.sil-archive-complete.v{version}"
              and complete["manifest_sha256"]==sha(encode(m)),"archive completion digest")
    for name,data in files.items():
        c.require(len(data)==m["files"][name]["bytes"] and sha(data)==m["files"][name]["sha256"],
                  "archive payload digest")
    run_binding(files,m,live=live)
    # All archive/path/hash checks precede any write. Existing target is NEVER reused.
    destination.mkdir(parents=False,exist_ok=False)
    for name,data in files.items():
        with (destination/name).open("xb") as output:
            c.require(output.write(data)==len(data),"short restore write")
            output.flush(); os.fsync(output.fileno())
    verification = verify(destination,allow_control_audit=live)
    report = dict(schema=f"fuso-avionics.sil-restore.v{version}",status="PASS_SOFTWARE_ONLY",
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
    p.add_argument("--live-control",action="store_true",help="explicit v2 including complete control audit")
    p=sub.add_parser("restore");p.add_argument("--archive",type=Path,required=True);p.add_argument("--directory",type=Path,required=True)
    p.add_argument("--live-control",action="store_true",help="explicit v2, never fallback to legacy")
    args=parser.parse_args()
    try:print(json.dumps(pack(args.run,args.archive,live=args.live_control) if args.action=="pack"
                         else restore(args.archive,args.directory,live=args.live_control)))
    except (ValueError,OSError,KeyError,TypeError,tarfile.TarError) as exc:parser.exit(2,f"SIL archive FAILED: {exc}\n")


if __name__=="__main__":main()
