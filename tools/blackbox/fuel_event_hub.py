#!/usr/bin/env python3
"""Authenticated, deduplicating collector for EFIS fuel blackbox events."""
import argparse, hashlib, hmac, json, os, sqlite3, subprocess, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

def canonical(value): return json.dumps(value, sort_keys=True, separators=(",", ":"))

class Hub:
    def __init__(self, database, tokens, maintenance_script, root, key, export_root):
        self.tokens=tokens; self.lock=threading.Lock(); Path(database).parent.mkdir(parents=True,exist_ok=True)
        self.db=sqlite3.connect(database,check_same_thread=False)
        self.db.execute("PRAGMA journal_mode=WAL")
        self.db.execute("CREATE TABLE IF NOT EXISTS events(id TEXT PRIMARY KEY,source TEXT NOT NULL,received_utc TEXT NOT NULL,event_utc TEXT,state TEXT,payload TEXT NOT NULL)")
        self.db.commit()
        self.database=database;self.maintenance_script=maintenance_script;self.root=root;self.key=key;self.export_root=export_root;self.last_maintenance={"state":"READY"}
    def authorized(self,source,token): return source in self.tokens and hmac.compare_digest(self.tokens[source],token)
    def insert(self,source,event):
        digest=hashlib.sha256((source+"\n"+canonical(event)).encode()).hexdigest()
        received=time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime())
        with self.lock: cur=self.db.execute("INSERT OR IGNORE INTO events VALUES(?,?,?,?,?,?)",(digest,source,received,event.get("utc"),event.get("state"),canonical(event)));self.db.commit()
        return digest,cur.rowcount==1
    def recent(self,limit):
        with self.lock: rows=self.db.execute("SELECT id,source,received_utc,payload FROM events ORDER BY rowid DESC LIMIT ?",(limit,)).fetchall()
        return [{"id":r[0],"source":r[1],"received_utc":r[2],"event":json.loads(r[3])} for r in rows]
    def count(self):
        with self.lock: return self.db.execute("SELECT COUNT(*) FROM events").fetchone()[0]
    def summary(self):
        with self.lock: rows=self.db.execute("SELECT source,COUNT(*),MAX(received_utc) FROM events GROUP BY source ORDER BY source").fetchall()
        return [{"source":r[0],"records":r[1],"last_contact_utc":r[2]} for r in rows]
    def reader_authorized(self,token): return any(hmac.compare_digest(value,token) for value in self.tokens.values())
    def run_maintenance(self,source,action,request_id):
        commands={"CREATE_INCIDENT":["incident","--hours","2"],"VERIFY_SIGNATURES":["verify"]}
        if action=="EXPORT_USB":
            if not self.export_root or not os.path.ismount(self.export_root):
                self.last_maintenance={"state":"WAITING","action":action,"source":source,"request_id":request_id,"utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"result":"USB_NOT_MOUNTED"}
                raise RuntimeError("USB_NOT_MOUNTED")
            command=["export-usb","--usb",self.export_root]
        elif action in commands: command=commands[action]
        else: raise ValueError("unsupported_action")
        args=["/usr/bin/python3",self.maintenance_script,*command,"--database",self.database,"--root",self.root,"--key",self.key]
        result=subprocess.run(args,capture_output=True,text=True,timeout=60,check=True)
        self.last_maintenance={"state":"COMPLETE","action":action,"source":source,"request_id":request_id,"utc":time.strftime("%Y-%m-%dT%H:%M:%SZ",time.gmtime()),"result":result.stdout.strip()};return self.last_maintenance

def handler(hub):
    class Handler(BaseHTTPRequestHandler):
        def reply(self,status,data):
            body=canonical(data).encode();self.send_response(status);self.send_header("Content-Type","application/json");self.send_header("Content-Length",str(len(body)));self.end_headers();self.wfile.write(body)
        def do_GET(self):
            parsed=urlparse(self.path)
            if parsed.path=="/health": return self.reply(200,{"ok":True,"records":hub.count(),"nodes":hub.summary(),"maintenance":hub.last_maintenance})
            if parsed.path=="/v1/events":
                token=self.headers.get("Authorization","").removeprefix("Bearer ")
                if not hub.reader_authorized(token): return self.reply(401,{"error":"unauthorized"})
                return self.reply(200,{"events":hub.recent(min(200,max(1,int(parse_qs(parsed.query).get("limit",[50])[0]))))})
            self.reply(404,{"error":"not_found"})
        def do_POST(self):
            if self.path not in ("/v1/events","/v1/maintenance"): return self.reply(404,{"error":"not_found"})
            try:
                data=json.loads(self.rfile.read(min(int(self.headers.get("Content-Length","0")),65536)))
                source=str(data["source"]); token=self.headers.get("Authorization","").removeprefix("Bearer ")
                if not hub.authorized(source,token): return self.reply(401,{"error":"unauthorized"})
                if self.path=="/v1/maintenance": return self.reply(200,hub.run_maintenance(source,str(data["action"]),str(data.get("request_id",""))))
                digest,inserted=hub.insert(source,data["event"]);self.reply(201 if inserted else 200,{"id":digest,"inserted":inserted})
            except Exception as exc: self.reply(400,{"error":"bad_request","detail":str(exc)})
        def log_message(self,fmt,*args): pass
    return Handler

def main():
    p=argparse.ArgumentParser();p.add_argument("--bind",default="0.0.0.0");p.add_argument("--port",type=int,default=18093);p.add_argument("--database",required=True);p.add_argument("--tokens",required=True);p.add_argument("--maintenance-script",required=True);p.add_argument("--root",required=True);p.add_argument("--key",required=True);p.add_argument("--export-root",default="/media/blackbox/EFIS_EXPORT");a=p.parse_args()
    tokens=json.loads(Path(a.tokens).read_text());hub=Hub(a.database,tokens,a.maintenance_script,a.root,a.key,a.export_root);ThreadingHTTPServer((a.bind,a.port),handler(hub)).serve_forever()
if __name__=="__main__": main()
