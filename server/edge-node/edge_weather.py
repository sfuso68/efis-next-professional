#!/usr/bin/env python3
"""Fail-safe local aviation-weather cache for EFIS EDGE V2."""
import argparse, base64, contextlib, json, os, socket, ssl, tempfile, time, urllib.request
from datetime import datetime, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

MAX_AGE_SECONDS = 2 * 3600

def parse_resolve(value):
    if not value:
        return None
    try:
        hostname, address = value.rsplit("=", 1)
        socket.inet_pton(socket.AF_INET6 if ":" in address else socket.AF_INET, address)
    except (ValueError, OSError) as exc:
        raise ValueError("--resolve must be HOSTNAME=IP") from exc
    if not hostname or any(character.isspace() for character in hostname):
        raise ValueError("--resolve must be HOSTNAME=IP")
    return hostname.lower(), address

@contextlib.contextmanager
def temporary_resolver(resolve):
    """Pin one hostname to an IP while preserving HTTPS SNI and Host."""
    if not resolve:
        yield
        return
    expected_host, address = resolve
    original = socket.getaddrinfo
    def mapped(host, port, *args, **kwargs):
        return original(address if str(host).lower() == expected_host else host, port, *args, **kwargs)
    socket.getaddrinfo = mapped
    try:
        yield
    finally:
        socket.getaddrinfo = original

def atomic_json(path, payload):
    path=Path(path); path.parent.mkdir(parents=True,exist_ok=True)
    fd,tmp=tempfile.mkstemp(prefix=path.name+".",dir=path.parent)
    try:
        with os.fdopen(fd,"w",encoding="utf-8") as stream:
            json.dump(payload,stream,indent=2); stream.write("\n"); stream.flush(); os.fsync(stream.fileno())
        os.replace(tmp,path)
    finally: Path(tmp).unlink(missing_ok=True)

def cached(cache,icao):
    path=Path(cache)/(icao.upper()+".json")
    if not path.is_file(): return {"icao":icao.upper(),"state":"NO_DATA","stale":True,"age_seconds":None}
    try: data=json.loads(path.read_text())
    except (OSError,ValueError): return {"icao":icao.upper(),"state":"INVALID","stale":True,"age_seconds":None}
    age=max(0,int(time.time()-path.stat().st_mtime)); data=dict(data)
    report=data.get("provider_payload") or {}
    has_report=bool(report.get("metar") or report.get("taf"))
    state="STALE" if age>MAX_AGE_SECONDS else ("AVAILABLE" if has_report else "NO_REPORT")
    data.update({"icao":icao.upper(),"state":state,
                 "stale":age>MAX_AGE_SECONDS,"age_seconds":age,"has_report":has_report,
                 "edge_source":"CENTRAL_CACHE","edge_cached_utc":datetime.fromtimestamp(path.stat().st_mtime,timezone.utc).isoformat()})
    return data

def sync_one(cache,base_url,icao,username=None,password=None,ca_file=None,resolve=None):
    headers={"User-Agent":"EFIS-EDGE-V2/0.1"}
    if username or password:
        if not username or not password: raise ValueError("both EDGE weather credentials are required")
        token=base64.b64encode(f"{username}:{password}".encode()).decode(); headers["Authorization"]="Basic "+token
    request=urllib.request.Request(base_url.rstrip("/")+"/api/metar/"+icao.upper(),headers=headers)
    context=ssl.create_default_context(cafile=ca_file) if ca_file else None
    with temporary_resolver(resolve):
        with urllib.request.urlopen(request,timeout=20,context=context) as response: payload=json.load(response)
    payload={"schema":"efis-next.edge-weather-cache.v1","provider_payload":payload,
             "provider_url":base_url,"fetched_utc":datetime.now(timezone.utc).isoformat()}
    atomic_json(Path(cache)/(icao.upper()+".json"),payload); return payload

class Handler(BaseHTTPRequestHandler):
    cache=None
    def do_GET(self):
        if self.path=="/v1/weather/status":
            files=list(Path(self.cache).glob("*.json")); payload={"mode":"SHADOW_CACHE","stations":len(files)}
        elif self.path.startswith("/v1/weather/") and len(self.path.split("/"))==4:
            payload=cached(self.cache,self.path.rsplit("/",1)[-1])
        else: self.send_error(404); return
        body=json.dumps(payload,separators=(",",":")).encode(); self.send_response(200)
        self.send_header("Content-Type","application/json"); self.send_header("Cache-Control","no-store")
        self.send_header("Content-Length",str(len(body))); self.end_headers(); self.wfile.write(body)
    def log_message(self,fmt,*args): return

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--cache",default="cache/weather")
    parser.add_argument("--sync-url"); parser.add_argument("--icao",action="append",default=[])
    parser.add_argument("--bind",default="127.0.0.1"); parser.add_argument("--port",type=int,default=18092)
    parser.add_argument("--serve",action="store_true"); parser.add_argument("--ca-file")
    parser.add_argument("--resolve",help="pin HTTPS hostname without changing system DNS: HOSTNAME=IP")
    args=parser.parse_args()
    resolve=parse_resolve(args.resolve)
    if args.sync_url:
        for icao in args.icao or ["LIPH","LIDB"]:
            sync_one(args.cache,args.sync_url,icao,os.getenv("EFIS_EDGE_USER"),os.getenv("EFIS_EDGE_PASSWORD"),args.ca_file,resolve)
    if args.serve:
        Handler.cache=args.cache; ThreadingHTTPServer((args.bind,args.port),Handler).serve_forever()
    else: print(json.dumps({icao:cached(args.cache,icao) for icao in args.icao or ["LIPH","LIDB"]}))

if __name__=="__main__": main()
