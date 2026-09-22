"""启动隔离 Java 进程和可控下游，验证跨服务错误映射。先运行 start.ps1 编译。"""
import http.server
import json
import os
import pathlib
import shutil
import socket
import subprocess
import threading
import time
import unittest
import urllib.error
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[2]
JAVA = os.environ.get('JAVA_BIN') or shutil.which('java')
if not JAVA:
    JAVA = str(next((ROOT / 'tmp/toolchains/jdk').glob('*/bin/java.exe')))

class Backend(http.server.BaseHTTPRequestHandler):
    mode = 'ok'
    def log_message(self, *_):
        pass
    def respond(self, status, body, request_id=''):
        try:
            self.send_response(status)
            self.send_header('Content-Type', 'application/json')
            self.send_header('X-Request-Id', request_id)
            self.end_headers()
            self.wfile.write(json.dumps(body).encode())
        except ConnectionError:
            pass
    def do_GET(self):
        self.respond(503 if self.mode == 'unready' else 200, {'status': 'ok', 'backend': 'cpp-cuda-demo' if self.mode == 'cuda' else 'unknown' if self.mode == 'unknown' else 'cpp-cpu-demo'})
    def do_POST(self):
        self.rfile.read(int(self.headers['Content-Length']))
        request_id = self.headers['X-Request-Id']
        mode = self.mode
        if mode == 'slow':
            time.sleep(.6)
        if mode == 'disconnect':
            self.close_connection = True
            return
        if mode in ('503', '504', '500'):
            self.respond(int(mode), {'error': {'message': 'fixture'}}, request_id)
            return
        body = {'request_id': 'wrong-id' if mode == 'wrong-id' else request_id,
                'backend': 'cpp-cuda-demo' if mode == 'cuda' else 'unknown' if mode == 'unknown' else 'cpp-cpu-demo', 'content': 'fixture response', 'queue_ms': 0, 'compute_ms': 1, 'demo_work_ms': 0}
        if mode == 'malformed':
            body = {'unexpected': True}
        self.respond(200, body, request_id)

class GatewayFailures(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.backend = http.server.ThreadingHTTPServer(('127.0.0.1', 0), Backend)
        cls.thread = threading.Thread(target=cls.backend.serve_forever, daemon=True)
        cls.thread.start()
        with socket.socket() as sock:
            sock.bind(('127.0.0.1', 0))
            cls.port = sock.getsockname()[1]
        classpath = os.pathsep.join([str(ROOT/'gateway-java/target/classes'), str(ROOT/'tmp/java-libs/gson-2.14.0.jar')])
        cls.process = subprocess.Popen([JAVA, '--add-modules', 'jdk.httpserver',
            f'-Djdk.net.unixdomain.tmpdir={ROOT / "tmp"}', f'-Dfactory.frontend={ROOT / "frontend"}',
            f'-Dfactory.runtime.url=http://127.0.0.1:{cls.backend.server_port}',
            '-Dfactory.runtime.timeoutMs=100', '-cp', classpath, 'factory.Gateway', str(cls.port)],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        cls.addClassCleanup(cls.cleanup)
        for _ in range(100):
            try:
                if cls.call('/health', None)[0] == 200:
                    return
            except OSError:
                time.sleep(.05)
        raise RuntimeError('Isolated gateway failed to start')

    @classmethod
    def cleanup(cls):
        cls.process.terminate()
        cls.process.wait(timeout=10)
        cls.backend.shutdown()
        cls.backend.server_close()
        cls.thread.join()

    @classmethod
    def call(cls, path='/v1/chat/completions', body=True):
        payload = {'model': 'factory-mock-v1', 'messages': [{'role': 'user', 'content': 'hello'}]}
        req = urllib.request.Request(f'http://127.0.0.1:{cls.port}{path}',
            data=json.dumps(payload).encode() if body else None, headers={'Content-Type': 'application/json'})
        try:
            response = urllib.request.urlopen(req, timeout=4)
        except urllib.error.HTTPError as error:
            response = error
        with response:
            return response.status, response.headers, json.loads(response.read())

    def test_errors(self):
        for mode, expected in [('503', 503), ('504', 504), ('500', 502), ('wrong-id', 502),
                               ('malformed', 502), ('unknown', 502), ('disconnect', 502), ('slow', 504)]:
            with self.subTest(mode=mode):
                Backend.mode = mode
                status, headers, body = self.call()
                self.assertEqual(status, expected)
                self.assertEqual(headers['X-Request-Id'], body['request_id'])
                self.assertIn('error', body)
        Backend.mode = 'ok'
        self.assertEqual(self.call()[0], 200)

    def test_backend_identity(self):
        for mode, expected in [('cuda', 'cpp-cuda-demo'), ('ok', 'cpp-cpu-demo')]:
            Backend.mode = mode
            self.assertEqual(self.call('/ready', None)[2]['runtime'], expected)
            status, _, body = self.call()
            self.assertEqual(status, 200)
            self.assertEqual(body['metadata']['runtime'], expected)
        Backend.mode = 'unknown'
        self.assertEqual(self.call('/ready', None)[0], 503)
        Backend.mode = 'ok'

    def test_readiness(self):
        Backend.mode = 'unready'
        self.assertEqual(self.call('/health', None)[0], 200)
        self.assertEqual(self.call('/ready', None)[0], 503)
        Backend.mode = 'ok'
        self.assertEqual(self.call('/ready', None)[0], 200)

if __name__ == '__main__':
    unittest.main(verbosity=2)
