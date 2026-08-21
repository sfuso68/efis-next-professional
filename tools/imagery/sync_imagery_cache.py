#!/usr/bin/env python3
"""Synchronise an EFIS imagery download plan into an atomic local cache."""
import argparse
import base64
import concurrent.futures
import json
import os
import tempfile
import urllib.request
import urllib.parse
from pathlib import Path


def valid_image(path):
    with path.open("rb") as stream:
        signature = stream.read(12)
    return (signature.startswith(b"RIFF") and signature[8:12] == b"WEBP") or signature.startswith(b"\x89PNG")


def authorization_header(username, password):
    if not username and not password:
        return None
    if not username or not password:
        raise ValueError("both imagery username and password are required")
    token = base64.b64encode(f"{username}:{password}".encode("utf-8")).decode("ascii")
    return f"Basic {token}"


def resolved_url(url, base_url=None):
    parsed = urllib.parse.urlparse(url)
    if parsed.scheme in ("http", "https") and parsed.netloc:
        return url
    if not base_url:
        raise ValueError("relative tile URL requires --base-url")
    return urllib.parse.urljoin(base_url.rstrip("/") + "/", url.lstrip("/"))


def entry_cache_path(entry):
    value = entry.get("cache_path") or entry.get("path")
    if not value:
        raise ValueError("tile entry requires path or cache_path")
    candidate = Path(value)
    if candidate.is_absolute() or ".." in candidate.parts:
        raise ValueError("unsafe tile cache path")
    return candidate


def fetch(entry, cache_root, timeout, authorization=None, base_url=None):
    target = cache_root / entry_cache_path(entry)
    if target.exists() and target.stat().st_size > 256 and valid_image(target):
        return {"status": "reused", "path": str(target), "bytes": target.stat().st_size}
    target.parent.mkdir(parents=True, exist_ok=True)
    headers = {"User-Agent": "EFIS-NEXT-Imagery/1"}
    if authorization:
        headers["Authorization"] = authorization
    request = urllib.request.Request(resolved_url(entry["url"], base_url), headers=headers)
    descriptor, temporary_name = tempfile.mkstemp(prefix=target.name + ".", suffix=".partial", dir=target.parent)
    os.close(descriptor)
    temporary = Path(temporary_name)
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response, temporary.open("wb") as output:
            while block := response.read(1024 * 1024):
                output.write(block)
        if temporary.stat().st_size <= 256 or not valid_image(temporary):
            raise ValueError("download is not a supported imagery tile")
        temporary.replace(target)
        return {"status": "downloaded", "path": str(target), "bytes": target.stat().st_size}
    finally:
        temporary.unlink(missing_ok=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", type=Path, required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--workers", type=int, default=4)
    parser.add_argument("--timeout", type=int, default=45)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--username-env", default="EFIS_IMAGERY_USER")
    parser.add_argument("--password-env", default="EFIS_IMAGERY_PASSWORD")
    parser.add_argument("--base-url", help="HTTPS origin used for relative tile URLs")
    args = parser.parse_args()
    plan = json.loads(args.plan.read_text(encoding="utf-8"))
    entries = plan["tiles"]
    authorization = authorization_header(
        os.environ.get(args.username_env), os.environ.get(args.password_env)
    )
    if args.dry_run:
        print(json.dumps({"dataset_id": plan["dataset_id"], "tiles": len(entries), "dry_run": True}))
        return
    results, failures = [], []
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, min(args.workers, 8))) as executor:
        futures = {
            executor.submit(fetch, entry, args.cache, args.timeout, authorization, args.base_url): entry
            for entry in entries
        }
        for future in concurrent.futures.as_completed(futures):
            try:
                results.append(future.result())
            except Exception as error:
                failures.append({"tile": futures[future], "error": str(error)})
    report = {
        "schema": "efis-next.imagery-sync-report.v1",
        "dataset_id": plan["dataset_id"],
        "requested": len(entries),
        "downloaded": sum(item["status"] == "downloaded" for item in results),
        "reused": sum(item["status"] == "reused" for item in results),
        "failed": len(failures),
        "bytes": sum(item["bytes"] for item in results),
        "failures": failures[:50],
    }
    report_path = args.cache / "last-sync-report.json"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    report_path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report))
    if failures:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
