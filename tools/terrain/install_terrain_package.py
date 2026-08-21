#!/usr/bin/env python3
import argparse, hashlib, json, os, re, shutil, tempfile
from datetime import datetime, timezone
from pathlib import Path

EXPECTED=3601*3601*2
PACKAGE_RE=re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$")
def validate(package: Path):
    manifest=json.loads((package/"manifest.json").read_text(encoding="utf-8"))
    if manifest.get("schema_version")!=1 or not manifest.get("package_id") or not manifest.get("tiles"): raise ValueError("invalid terrain manifest")
    if not PACKAGE_RE.fullmatch(manifest["package_id"]): raise ValueError("invalid package id")
    source=manifest.get("source")
    if not isinstance(source,dict) or not all(source.get(k) for k in ("title","edition","attribution","status")): raise ValueError("incomplete source metadata")
    if not manifest.get("vertical_datum"): raise ValueError("missing vertical datum")
    package_root=package.resolve(); keys=set()
    for tile in manifest["tiles"]:
        key=tile.get("key")
        if not key or key in keys: raise ValueError(f"invalid or duplicate tile key: {key}")
        keys.add(key)
        path=(package/tile.get("path","")).resolve()
        if package_root not in path.parents: raise ValueError(f"tile path escapes package: {key}")
        if not path.is_file() or path.stat().st_size!=EXPECTED or tile.get("bytes")!=EXPECTED: raise ValueError(f"invalid tile: {key}")
        digest=hashlib.sha256(path.read_bytes()).hexdigest()
        if digest!=tile.get("sha256"): raise ValueError(f"checksum mismatch: {key}")
    return manifest

def main():
    p=argparse.ArgumentParser(); p.add_argument("package",type=Path); p.add_argument("--store",type=Path,required=True); a=p.parse_args()
    manifest=validate(a.package); a.store.mkdir(parents=True,exist_ok=True); packages=a.store/"packages"; packages.mkdir(exist_ok=True)
    target=packages/manifest["package_id"]
    if not target.exists():
        temporary=Path(tempfile.mkdtemp(prefix=".terrain-install-",dir=packages))
        try: shutil.copytree(a.package,temporary/"payload",dirs_exist_ok=True); os.replace(temporary/"payload",target)
        finally: shutil.rmtree(temporary,ignore_errors=True)
    active={"schema_version":1,"package_id":manifest["package_id"],"package_path":str(target.resolve()),"tiles_path":str((target/"tiles").resolve()),"activated_utc":datetime.now(timezone.utc).isoformat(),"source":manifest["source"],"vertical_datum":manifest["vertical_datum"]}
    fd,temp=tempfile.mkstemp(prefix=".active-terrain-",dir=a.store,text=True)
    try:
        with os.fdopen(fd,"w",encoding="utf-8") as f: json.dump(active,f,ensure_ascii=False,indent=2); f.flush(); os.fsync(f.fileno())
        os.replace(temp,a.store/"active-package.json")
    finally:
        if os.path.exists(temp): os.unlink(temp)
    print(json.dumps(active,sort_keys=True))
if __name__=="__main__": main()
