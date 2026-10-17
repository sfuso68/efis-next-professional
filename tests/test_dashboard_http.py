from http.server import HTTPServer
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
from urllib.error import HTTPError
from urllib.request import urlopen, Request
from tools.project_status import serve_dashboard as service


class HttpTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.patch_root = patch.object(service, 'ROOT', self.root)
        self.patch_backup = patch.object(service, 'BACKUP_STATUS', self.root / 'backup.json')
        self.patch_root.start()
        self.patch_backup.start()
        self.server = HTTPServer(('127.0.0.1', 0), service.Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = 'http://127.0.0.1:' + str(self.server.server_port)

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()
        self.patch_root.stop()
        self.patch_backup.stop()
        self.tmp.cleanup()

    def test_view_includes_monitors_and_no_cache(self):
        (self.root / 'index.html').write_text('<h1>Test</h1>')
        with urlopen(self.url) as response:
            self.assertEqual('no-store', response.headers['Cache-Control'])
            body = response.read()
            self.assertIn(b'backup-status.json', body)
            self.assertIn(b'MONITOR OBSOLETO', body)

    def test_backup_redacts_internal_details(self):
        (self.root / 'backup.json').write_text(json.dumps({'status': 'PASS', 'secret': 'hidden'}))
        with urlopen(self.url + '/backup-status.json') as response:
            self.assertEqual({'status': 'PASS'}, json.load(response))

    def test_missing_or_corrupt_backup_is_503(self):
        for payload in [None, '{bad', '[]']:
            if payload is not None:
                (self.root / 'backup.json').write_text(payload)
            with self.assertRaises(HTTPError) as error:
                urlopen(self.url + '/backup-status.json')
            self.assertEqual(503, error.exception.code)

    def test_arbitrary_files_not_served(self):
        for path in ['/../secret', '/github.git/config', '/status.json?extra=1']:
            with self.assertRaises(HTTPError) as error:
                urlopen(self.url + path)
            self.assertEqual(404, error.exception.code)

    def test_write_methods_not_accepted(self):
        with self.assertRaises(HTTPError) as error:
            urlopen(Request(self.url, data=b'change', method='POST'))
        self.assertEqual(501, error.exception.code)
