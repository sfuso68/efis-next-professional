#!/usr/bin/env python3
import json
import tempfile
import threading
import unittest
import urllib.error
import urllib.request
from pathlib import Path

from imagery_server import Handler, ImageryServer


class ImageryServerTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        base = Path(self.temporary.name)
        self.root = base / "root"
        (self.root / "manifests").mkdir(parents=True)
        (self.root / "dataset/14/1").mkdir(parents=True)
        (self.root / "manifests/demo.json").write_text('{"dataset_id":"demo"}', encoding="utf-8")
        (self.root / "dataset/14/1/2.webp").write_bytes(b"RIFF" + (12).to_bytes(4, "little") + b"WEBP" + b"12345678")
        catalog = base / "catalog.json"
        catalog.write_text(json.dumps({"datasets": [{"dataset_id": "demo", "manifest_path": "manifests/demo.json"}]}), encoding="utf-8")
        self.server = ImageryServer(("127.0.0.1", 0), Handler, self.root, catalog)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.base_url = f"http://127.0.0.1:{self.server.server_port}"

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.temporary.cleanup()

    def read(self, path, headers=None):
        request = urllib.request.Request(self.base_url + path, headers=headers or {})
        with urllib.request.urlopen(request) as response:
            return response, response.read()

    def test_health_and_catalogue(self):
        _, body = self.read("/v1/health")
        self.assertEqual(json.loads(body)["status"], "ok")
        _, body = self.read("/v1/catalog")
        self.assertEqual(json.loads(body)["datasets"][0]["dataset_id"], "demo")

    def test_manifest_and_range(self):
        _, body = self.read("/v1/datasets/demo/manifest")
        self.assertEqual(json.loads(body)["dataset_id"], "demo")
        response, body = self.read("/tiles/dataset/14/1/2.webp", {"Range": "bytes=4-11"})
        self.assertEqual(response.status, 206)
        self.assertEqual(len(body), 8)

    def test_traversal_is_not_served(self):
        with self.assertRaises(urllib.error.HTTPError) as error:
            self.read("/tiles/%2e%2e/catalog.json")
        self.assertEqual(error.exception.code, 404)


if __name__ == "__main__":
    unittest.main()
