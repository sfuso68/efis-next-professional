#!/usr/bin/env python3
"""Safely unpack and audit downloaded Copernicus DEM products."""
import argparse, hashlib, json, os, shutil, stat, sys, zipfile
from pathlib import Path

MAX_MEMBER_BYTES=512*1024*1024
MAX_ARCHIVE_BYTES=2*1024*1024*1024

def safe_members(archive):
    total=0; result=[]
    for member in archive.infolist():
        path=Path(member.filename)
        if path.is_absolute() or ".." in path.parts: raise ValueError(f"unsafe archive path: {member.filename}")
        if stat.S_ISLNK(member.external_attr>>16): raise ValueError(f"archive symlink rejected: {member.filename}")
        if member.file_size>MAX_MEMBER_BYTES: raise ValueError(f"archive member too large: {member.filename}")
        total+=member.file_size
        if total>MAX_ARCHIVE_BYTES: raise ValueError("archive expanded size limit exceeded")
        result.append(member)
    return result

def sha256(path):
    digest=hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda:source.read(1024*1024),b""): digest.update(block)
    return digest.hexdigest()

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--discovery",type=Path,required=True); parser.add_argument("--downloads",type=Path,required=True); parser.add_argument("--output",type=Path,required=True); args=parser.parse_args()
    discovery=json.loads(args.discovery.read_text(encoding="utf-8")); expected={p["TileKey"]:p for p in discovery["products"]}
    args.output.mkdir(parents=True,exist_ok=True); dem_dir=args.output/"dem"; dem_dir.mkdir(exist_ok=True)
    records=[]; missing=[]
    for key,product in expected.items():
        archive=args.downloads/(product["Name"]+".zip")
        if not archive.is_file(): missing.append(key); continue
        with zipfile.ZipFile(archive) as bundle:
            members=safe_members(bundle)
            candidates=[m for m in members if not m.is_dir() and m.filename.lower().endswith(("_dem.tif","_dem.tiff"))]
            if len(candidates)!=1: raise ValueError(f"{key}: expected one DEM GeoTIFF, found {len(candidates)}")
            target=dem_dir/f"{key}.tif"; temporary=target.with_suffix(".tif.part")
            with bundle.open(candidates[0]) as source,temporary.open("wb") as destination: shutil.copyfileobj(source,destination,1024*1024)
            os.replace(temporary,target)
        records.append({"key":key,"product_id":product["Id"],"product_name":product["Name"],"source_member":candidates[0].filename,"path":str(target),"bytes":target.stat().st_size,"sha256":sha256(target)})
    report={"schema_version":1,"collection":discovery["collection"],"bbox":discovery["bbox"],"expected_tiles":len(expected),"prepared_tiles":len(records),"missing_tiles":missing,"tiles":records,"ready":not missing and len(records)==len(expected)}
    (args.output/"stage-report.json").write_text(json.dumps(report,indent=2),encoding="utf-8")
    print(json.dumps({"report":str(args.output/"stage-report.json"),"ready":report["ready"],"prepared":len(records),"missing":len(missing)},sort_keys=True))
    if not report["ready"]: raise SystemExit(3)

if __name__=="__main__":
    try: main()
    except (ValueError,KeyError,zipfile.BadZipFile) as exc:
        print(f"error: {exc}",file=sys.stderr); raise SystemExit(2)
