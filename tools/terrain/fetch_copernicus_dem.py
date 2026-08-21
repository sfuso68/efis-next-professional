#!/usr/bin/env python3
"""Discover and download COP-DEM GLO-30 products for a bounded operating area.

Authentication is accepted only through CDSE_ACCESS_TOKEN. The script never
prompts for, stores or logs an account password. Downloads remain staged and are
never activated by this tool.
"""
import argparse, json, math, os, re, sys, zipfile
from pathlib import Path
from urllib.error import HTTPError
from urllib.parse import urlencode
from urllib.request import Request, urlopen

CATALOGUE="https://catalogue.dataspace.copernicus.eu/odata/v1/Products"
DOWNLOAD="https://download.dataspace.copernicus.eu/odata/v1/Products({})/$value"
PRODUCT_MARKER="/COP-DEM_GLO-30-DGED/"
DEFAULT_DATASET="COP-DEM_GLO-30-DGED/2024_1"
PRODUCT_TYPE="SAR_DGE_30_A4AD"
SAFE_NAME=re.compile(r"[^A-Za-z0-9._-]+")

def bbox(value):
    parts=[float(v) for v in value.split(",")]
    if len(parts)!=4: raise argparse.ArgumentTypeError("bbox must be min_lon,min_lat,max_lon,max_lat")
    west,south,east,north=parts
    if not (-180<=west<east<=180 and -90<=south<north<=90): raise argparse.ArgumentTypeError("invalid bbox")
    return parts

def catalogue_url(area,limit,dataset):
    west,south,east,north=area
    polygon=f"POLYGON(({west} {south},{east} {south},{east} {north},{west} {north},{west} {south}))"
    filters=[f"Attributes/OData.CSC.StringAttribute/any(att:att/Name eq 'dataset' and att/OData.CSC.StringAttribute/Value eq '{dataset}')",f"Attributes/OData.CSC.StringAttribute/any(att:att/Name eq 'productType' and att/OData.CSC.StringAttribute/Value eq '{PRODUCT_TYPE}')",f"OData.CSC.Intersects(area=geography'SRID=4326;{polygon}')"]
    query={"$filter":" and ".join(filters),"$select":"Id,Name,S3Path,GeoFootprint,PublicationDate","$orderby":"PublicationDate desc","$top":str(limit)}
    return CATALOGUE+"?"+urlencode(query)

def request_json(url):
    with urlopen(Request(url,headers={"Accept":"application/json","User-Agent":"efis-next-terrain/1"}),timeout=60) as response:
        return json.load(response)

def runtime_key(lat,lon):
    return f"{'N' if lat>=0 else 'S'}{abs(lat):02d}{'E' if lon>=0 else 'W'}{abs(lon):03d}"

def select_products(payload,area):
    west,south,east,north=area; selected={}
    for item in payload.get("value",[]):
        if PRODUCT_MARKER not in (item.get("S3Path") or ""): continue
        rings=(item.get("GeoFootprint") or {}).get("coordinates") or []
        if not rings or not rings[0]: continue
        points=rings[0]; lons=[point[0] for point in points]; lats=[point[1] for point in points]
        tile_west,tile_east=min(lons),max(lons); tile_south,tile_north=min(lats),max(lats)
        if min(east,tile_east)<=max(west,tile_west) or min(north,tile_north)<=max(south,tile_south): continue
        key=runtime_key(math.floor(tile_south),math.floor(tile_west))
        product={field:item.get(field) for field in ("Id","Name","S3Path","GeoFootprint","PublicationDate")}; product["TileKey"]=key
        current=selected.get(key)
        if current is None or (product.get("PublicationDate") or "")>(current.get("PublicationDate") or ""): selected[key]=product
    return [selected[key] for key in sorted(selected)]

def inspect_archive(path):
    try:
        with zipfile.ZipFile(path) as archive:
            files=[member for member in archive.infolist() if not member.is_dir()]
            dem=[member for member in files if member.filename.lower().endswith(("_dem.tif","_dem.tiff"))]
            return {"payload_status":"DEM_PRESENT" if len(dem)==1 else "REJECTED_METADATA_ONLY" if not dem else "REJECTED_AMBIGUOUS_DEM","file_count":len(files),"dem_count":len(dem),"dem_bytes":dem[0].file_size if len(dem)==1 else 0}
    except zipfile.BadZipFile:
        return {"payload_status":"REJECTED_INVALID_ARCHIVE","file_count":0,"dem_count":0,"dem_bytes":0}

def archive_path(product,target):
    return target/(SAFE_NAME.sub("_",product["Name"] or product["Id"])+".zip")

def download(product,target,token):
    final=archive_path(product,target); partial=final.with_suffix(final.suffix+".part")
    if final.is_file():
        return {"product_id":product["Id"],"file":str(final),"bytes":final.stat().st_size,"reused":True,**inspect_archive(final)}
    offset=partial.stat().st_size if partial.exists() else 0
    headers={"Authorization":f"Bearer {token}","User-Agent":"efis-next-terrain/1"}
    if offset: headers["Range"]=f"bytes={offset}-"
    request=Request(DOWNLOAD.format(product["Id"]),headers=headers)
    try:
        with urlopen(request,timeout=120) as response:
            append=offset>0 and response.status==206
            with partial.open("ab" if append else "wb") as output:
                while True:
                    block=response.read(1024*1024)
                    if not block: break
                    output.write(block)
    except HTTPError as exc:
        if exc.code in (401,403): raise RuntimeError("CDSE token rejected or CCM licence/access not enabled") from exc
        raise
    os.replace(partial,final)
    return {"product_id":product["Id"],"file":str(final),"bytes":final.stat().st_size,"reused":False,**inspect_archive(final)}

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument("--bbox",type=bbox,default=bbox("10.6,44.8,13.2,46.7"),help="west,south,east,north; default Veneto envelope")
    parser.add_argument("--limit",type=int,default=250)
    parser.add_argument("--dataset",default=DEFAULT_DATASET)
    parser.add_argument("--output",type=Path,required=True)
    parser.add_argument("--download",action="store_true")
    parser.add_argument("--audit-existing",action="store_true")
    args=parser.parse_args()
    if not 1<=args.limit<=1000: parser.error("limit must be 1..1000")
    products=select_products(request_json(catalogue_url(args.bbox,args.limit,args.dataset)),args.bbox)
    args.output.mkdir(parents=True,exist_ok=True)
    discovery={"schema_version":1,"collection":"COP-DEM_GLO-30-DGED","dataset":args.dataset,"product_type":PRODUCT_TYPE,"bbox":args.bbox,"products":products}
    (args.output/"discovery.json").write_text(json.dumps(discovery,indent=2),encoding="utf-8")
    result={"products":len(products),"discovery":str(args.output/"discovery.json"),"downloads":[]}
    if args.download:
        token=os.environ.get("CDSE_ACCESS_TOKEN")
        if not token: raise RuntimeError("CDSE_ACCESS_TOKEN is required for download")
        for product in products: result["downloads"].append(download(product,args.output,token))
    if args.audit_existing:
        audit=[]
        for product in products:
            path=archive_path(product,args.output)
            audit.append({"product_id":product["Id"],"file":str(path),**(inspect_archive(path) if path.is_file() else {"payload_status":"MISSING","file_count":0,"dem_count":0,"dem_bytes":0})})
        result["audit"]=audit
        (args.output/"payload-audit.json").write_text(json.dumps(audit,indent=2),encoding="utf-8")
    print(json.dumps(result,sort_keys=True))
    if args.download and any(item["payload_status"]!="DEM_PRESENT" for item in result["downloads"]): raise SystemExit(4)
    if args.audit_existing and any(item["payload_status"]!="DEM_PRESENT" for item in result["audit"]): raise SystemExit(4)

if __name__=="__main__":
    try: main()
    except Exception as exc:
        print(f"error: {exc}",file=sys.stderr); raise SystemExit(2)
