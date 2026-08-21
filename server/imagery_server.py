#!/usr/bin/env python3
"""EFIS NEXT read-only imagery catalogue and tile delivery service."""
import argparse
import hashlib
import json
import mimetypes
import os
import re
from datetime import datetime, timezone
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

RANGE = re.compile(r"bytes=(\d*)-(\d*)$")


class ImageryServer(ThreadingHTTPServer):
    def __init__(self, address, handler, root, catalog):
        super().__init__(address, handler)
        self.root = root.resolve()
        self.catalog_path = catalog.resolve()
        self.started = datetime.now(timezone.utc).isoformat()

    def catalog(self):
        return json.loads(self.catalog_path.read_text(encoding="utf-8"))


class Handler(BaseHTTPRequestHandler):
    server_version = "EFIS-Imagery/0.1"

    def log_message(self, form, *args):
        print(json.dumps({"event": "http", "client": self.client_address[0],
                          "request": self.requestline, "message": form % args}))

    def json_response(self, payload, status=HTTPStatus.OK):
        body = json.dumps(payload, separators=(",", ":")).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(body)

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        path = unquote(urlsplit(self.path).path)
        if path == "/v1/health":
            self.json_response({"status": "ok", "service": "efis-imagery",
                                "started_utc": self.server.started})
            return
        if path == "/v1/catalog":
            self.json_response(self.server.catalog())
            return
        if path.startswith("/v1/datasets/") and path.endswith("/manifest"):
            dataset_id = path[len("/v1/datasets/"):-len("/manifest")].strip("/")
            dataset = next((item for item in self.server.catalog()["datasets"]
                            if item["dataset_id"] == dataset_id), None)
            if not dataset:
                self.json_response({"error": "dataset_not_found"}, HTTPStatus.NOT_FOUND)
                return
            self.serve_file(self.safe_path(dataset["manifest_path"]), immutable=False)
            return
        if path.startswith("/tiles/"):
            self.serve_file(self.safe_path(path[len("/tiles/"):]), immutable=True)
            return
        self.json_response({"error": "not_found"}, HTTPStatus.NOT_FOUND)

    def safe_path(self, relative):
        candidate = (self.server.root / relative).resolve()
        try:
            candidate.relative_to(self.server.root)
        except ValueError:
            return None
        return candidate

    def serve_file(self, path, immutable):
        if path is None or not path.is_file():
            self.json_response({"error": "file_not_found"}, HTTPStatus.NOT_FOUND)
            return
        size = path.stat().st_size
        etag = hashlib.sha256(f"{path.name}:{size}:{path.stat().st_mtime_ns}".encode()).hexdigest()[:24]
        if self.headers.get("If-None-Match") == f'"{etag}"':
            self.send_response(HTTPStatus.NOT_MODIFIED)
            self.end_headers()
            return
        start, end, status = 0, size - 1, HTTPStatus.OK
        range_header = self.headers.get("Range")
        if range_header:
            match = RANGE.fullmatch(range_header.strip())
            if not match:
                self.send_error(HTTPStatus.REQUESTED_RANGE_NOT_SATISFIABLE)
                return
            start = int(match.group(1) or 0)
            end = int(match.group(2) or size - 1)
            if start >= size or end < start:
                self.send_error(HTTPStatus.REQUESTED_RANGE_NOT_SATISFIABLE)
                return
            end = min(end, size - 1)
            status = HTTPStatus.PARTIAL_CONTENT
        length = end - start + 1
        self.send_response(status)
        self.send_header("Content-Type", mimetypes.guess_type(path.name)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(length))
        self.send_header("Accept-Ranges", "bytes")
        self.send_header("ETag", f'"{etag}"')
        self.send_header("Cache-Control", "public,max-age=31536000,immutable" if immutable else "no-cache")
        if status == HTTPStatus.PARTIAL_CONTENT:
            self.send_header("Content-Range", f"bytes {start}-{end}/{size}")
        self.end_headers()
        if self.command == "HEAD":
            return
        with path.open("rb") as stream:
            stream.seek(start)
            remaining = length
            while remaining:
                block = stream.read(min(1024 * 1024, remaining))
                if not block:
                    break
                self.wfile.write(block)
                remaining -= len(block)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default=os.environ.get("EFIS_IMAGERY_HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("EFIS_IMAGERY_PORT", "8090")))
    parser.add_argument("--root", type=Path, default=Path(os.environ.get("EFIS_IMAGERY_ROOT", "data/imagery/server")))
    parser.add_argument("--catalog", type=Path, default=Path(os.environ.get("EFIS_IMAGERY_CATALOG", "config/imagery/server-catalog.json")))
    args = parser.parse_args()
    if not args.catalog.is_file():
        raise SystemExit(f"catalog not found: {args.catalog}")
    args.root.mkdir(parents=True, exist_ok=True)
    server = ImageryServer((args.host, args.port), Handler, args.root, args.catalog)
    print(json.dumps({"event": "server_start", "host": args.host, "port": args.port,
                      "root": str(server.root), "catalog": str(server.catalog_path)}), flush=True)
    server.serve_forever()


if __name__ == "__main__":
    main()
