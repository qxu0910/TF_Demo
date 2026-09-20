"""在 Linux/WSL 执行；自启隔离 Runtime，测试后终止，不依赖主服务。"""
import concurrent.futures
import json
import pathlib
import signal
import socket
import subprocess
import time
import unittest
import urllib.error
import urllib.request

BIN = pathlib.Path(__file__).resolve().parents[1] / 'build/runtime_server'

def free_port():
    with socket.socket() as sock:
        sock.bind(('127.0.0.1', 0))
        return sock.getsockname()[1]

def call(port, path='/internal/completions', body=None, content_type='application/json'):
    headers = {'Content-Type': content_type, 'X-Request-Id': 'test-request'}
    data = body if isinstance(body, bytes) else json.dumps(body).encode() if body is not None else None
    req = urllib.request.Request(f'http://127.0.0.1:{port}{path}', data=data, headers=headers)
    try:
        response = urllib.request.urlopen(req, timeout=6)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        return response.status, response.headers, json.loads(response.read())

class HttpTests(unittest.TestCase):
    def setUp(self):
        self.port = free_port()
        self.process = subprocess.Popen([str(BIN), str(self.port), '1', '1', '150'],
                                        stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
        self.addCleanup(self.cleanup)
        deadline = time.monotonic() + 5
        while time.monotonic() < deadline:
            try:
                if call(self.port, '/health')[0] == 200:
                    return
            except OSError:
                time.sleep(.03)
        self.fail('Runtime failed to start')

    def cleanup(self):
        if self.process.poll() is None:
            self.process.send_signal(signal.SIGTERM)
            try:
                self.process.wait(timeout=7)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
                raise AssertionError('Runtime did not stop gracefully')
        self.process.stderr.close()

    def request(self, **changes):
        return {'request_id': 'test-request', 'prompt': 'hello 中文', 'timeout_ms': 2000, **changes}

    def test_success(self):
        code, headers, data = call(self.port, body=self.request())
        self.assertEqual(code, 200)
        self.assertEqual(data['request_id'], headers['X-Request-Id'])
        self.assertEqual(data['backend'], 'cpp-cpu-demo')
        self.assertGreaterEqual(data['compute_ms'], 100)
        self.assertGreaterEqual(data['queue_ms'], 0)

    def test_validation(self):
        for body in [b'{', b'null', self.request(prompt=''), self.request(prompt=' \n'),
                     self.request(request_id='mismatch'), self.request(timeout_ms=0),
                     self.request(timeout_ms=1.5), self.request(timeout_ms=10001),
                     self.request(prompt='x' * 8193), self.request(extra=True)]:
            with self.subTest(body=str(body)[:60]):
                self.assertEqual(call(self.port, body=body)[0], 400)
        self.assertEqual(call(self.port, body=self.request(), content_type='text/plain')[0], 415)
        self.assertEqual(call(self.port, '/unknown')[0], 404)

    def test_timeout_and_queue_full(self):
        self.assertEqual(call(self.port, body=self.request(timeout_ms=5))[0], 504)
        time.sleep(.03)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda _: call(self.port, body=self.request()), range(8)))
        codes = [result[0] for result in results]
        self.assertIn(200, codes)
        self.assertIn(503, codes)
        metrics = call(self.port, '/metrics')[2]
        self.assertGreater(metrics['rejected'], 0)
        self.assertGreater(metrics['timed_out'], 0)

    def test_shutdown_with_active_request(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=1) as pool:
            future = pool.submit(call, self.port, body=self.request())
            deadline = time.monotonic() + 2
            while time.monotonic() < deadline and call(self.port, '/metrics')[2]['active'] == 0:
                time.sleep(.002)
            self.process.send_signal(signal.SIGTERM)
            self.assertEqual(future.result()[0], 503)
            self.assertEqual(self.process.wait(timeout=7), 0)

if __name__ == '__main__':
    unittest.main(verbosity=2)
