#!/usr/bin/env python3
"""Create a staged Caddyfile with one additional basic-auth machine user."""
import argparse
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(); parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True); parser.add_argument("--username",required=True)
    parser.add_argument("--hash-file",type=Path,required=True); args=parser.parse_args()
    text=args.source.read_text(); password_hash=args.hash_file.read_text().strip()
    if not password_hash.startswith("$2") or len(password_hash)<50: raise SystemExit("invalid bcrypt hash")
    if any(line.strip().startswith(args.username+" ") for line in text.splitlines()):
        raise SystemExit("username already present")
    marker="\tbasic_auth {"
    if marker not in text: raise SystemExit("basic_auth block not found")
    staged=text.replace(marker,marker+f"\n\t\t{args.username} {password_hash}",1)
    args.output.write_text(staged)

if __name__=="__main__": main()
