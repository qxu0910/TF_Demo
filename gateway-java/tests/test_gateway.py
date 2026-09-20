"""运行中的网关黑盒测试：python gateway-java/tests/test_gateway.py"""
import concurrent.futures
import json
import os
import unittest
import urllib.error
import urllib.request

BASE = os.environ.get('GATEWAY_TEST_URL', 'http://127.0.0.1:8081')
VALID = {'model': 'factory-mock-v1', 'messages': [{'role': 'user', 'content': '你好 Java 网关'}], 'stream': False}
MODE = os.environ.get('GATEWAY_TEST_MODE', 'cpp-cpu-demo')

def call(path='/v1/chat/completions', body=VALID, method='POST', content_type='application/json'):
    data = body if isinstance(body, bytes) else json.dumps(body, ensure_ascii=False).encode('utf-8')
    request = urllib.request.Request(BASE + path, data=data if method != 'GET' else None,
                                    headers={'Content-Type': content_type}, method=method)
    try:
        response = urllib.request.urlopen(request, timeout=10)
    except urllib.error.HTTPError as error:
        response = error
    with response:
        payload = response.read()
        return response.status, response.headers, json.loads(payload) if 'application/json' in response.headers.get('Content-Type', '') else payload

class GatewayTests(unittest.TestCase):
    def test_success(self):
        status, headers, body = call()
        self.assertEqual(status, 200)
        self.assertEqual(headers['X-Request-Id'], body['request_id'])
        self.assertIn('C++' if MODE == 'cpp-cpu-demo' else 'Java', body['choices'][0]['message']['content'])
        self.assertEqual(body['metadata']['runtime'], MODE)
        self.assertGreaterEqual(body['metadata']['queue_ms'], 0)
        self.assertGreaterEqual(body['metadata']['compute_ms'], 0)
        self.assertGreaterEqual(body['metadata']['server_ms'], body['metadata']['runtime_ms'])
        self.assertNotIn('usage', body)

    def test_validation(self):
        invalid = [b'{', b'{} garbage', b'null', b'[]', b'{"model":}', b'\xff',
                   {**VALID, 'model': 42}, {**VALID, 'model': 'unknown'},
                   {**VALID, 'stream': True}, {**VALID, 'stream': 'false'},
                   {**VALID, 'max_tokens': 64}, {**VALID, 'messages': []},
                   {**VALID, 'messages': VALID['messages'] * 2}]
        invalid += [{**VALID, 'messages': [{'role': 'user', 'content': value}]} for value in ['  \n', 42, 'x' * 2001]]
        for value in invalid:
            with self.subTest(value=str(value)[:60]):
                status, headers, body = call(body=value)
                self.assertEqual(status, 400)
                self.assertEqual(body['error']['code'], 'invalid_request')
                self.assertEqual(headers['X-Request-Id'], body['request_id'])

    def test_boundaries(self):
        self.assertEqual(call(body=b'x' * 16385)[0], 413)
        self.assertEqual(call(content_type='text/plain')[0], 415)
        status, headers, _ = call(method='GET')
        self.assertEqual(status, 405)
        self.assertEqual(headers['Allow'], 'POST')
        for path in ['/v1/chat/completions/extra', '/.git/config', '/unknown']:
            self.assertEqual(call(path, method='GET')[0], 404)

    def test_page_and_health(self):
        self.assertEqual(call('/health', method='GET')[2]['status'], 'ok')
        self.assertEqual(call('/ready', method='GET')[2]['status'], 'ready')
        self.assertEqual(call('/metrics', method='GET')[2]['runtime'], MODE)
        for path in ['/', '/index.html', '/app.js', '/styles.css']:
            self.assertEqual(call(path, method='GET')[0], 200)

    def test_unicode_and_parallel(self):
        body = {**VALID, 'messages': [{'role': 'user', 'content': '你好 "Java" \\ 😀\n网关'}]}
        self.assertEqual(call(body=body)[0], 200)
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            responses = list(pool.map(lambda _: call(), range(20)))
        self.assertTrue(all(response[0] == 200 for response in responses))
        self.assertEqual(len({response[2]['request_id'] for response in responses}), 20)

    def test_optional_stream_and_exact_limit(self):
        body = {'model': 'factory-mock-v1', 'messages': [{'role': 'user', 'content': '中' * 2000}]}
        self.assertEqual(call(body=body, content_type='application/json; charset=utf-8')[0], 200)
        body['messages'][0]['role'] = 'system'
        self.assertEqual(call(body=body)[0], 400)

if __name__ == '__main__':
    unittest.main(verbosity=2)
