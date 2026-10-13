"""Restore a bundle into a disposable directory; never replace a live repository."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args], stderr=subprocess.PIPE, timeout=300)


def verify(bundle, expected_sha256, temp_parent=None):
    bundle = Path(bundle).resolve(strict=True)
    digest = hashlib.sha256(bundle.read_bytes()).hexdigest()
    if digest != expected_sha256:
        raise ValueError('Bundle SHA-256 mismatch')
    with tempfile.TemporaryDirectory(prefix='bundle-restore-', dir=temp_parent) as tmp:
        repo = Path(tmp) / 'restore.git'
        subprocess.run(['git', 'init', '--bare', str(repo)], check=True,
                       capture_output=True, timeout=300)
        git(repo, 'fetch', '--no-tags', str(bundle), 'refs/*:refs/*')
        git(repo, 'symbolic-ref', 'HEAD', 'refs/heads/main')
        git(repo, 'fsck', '--full')
        main = git(repo, 'rev-parse', 'refs/heads/main').decode().strip()
        hashes = {}
        for name in ['docs/status/EFIS_NEXT_MASTER_MATRIX.md',
                     'docs/status/EFIS_NEXT_STATE.md', 'CMakeLists.txt']:
            hashes[name] = hashlib.sha256(git(repo, 'show', main + ':' + name)).hexdigest()
        recovered_refs = git(repo, 'for-each-ref', '--format=%(objectname) %(refname)').decode().splitlines()
        advertised = subprocess.check_output(['git', 'bundle', 'list-heads', str(bundle)], timeout=30).decode().splitlines()
        refs = dict(line.split(maxsplit=1)[::-1] for line in recovered_refs)
        for line in advertised:
            oid, ref = line.split(maxsplit=1)
            if ref.startswith('refs/') and refs.get(ref) != oid:
                raise ValueError('Restored bundle ref mismatch: ' + ref)
        return dict(status='PASS', bundle_sha256=digest, main=main,
                    refs_checked=len(refs), file_sha256=hashes, live_repositories_modified=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('bundle')
    parser.add_argument('--sha256', required=True)
    parser.add_argument('--temp-parent')
    args = parser.parse_args()
    print(json.dumps(verify(args.bundle, args.sha256, args.temp_parent), indent=2))
