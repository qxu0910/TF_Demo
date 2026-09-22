"""验证基准 CLI 契约，不对机器耗时设置脆弱阈值。"""
import json
import math
import pathlib
import subprocess
import unittest

BIN = pathlib.Path(__file__).resolve().parents[2] / 'runtime-cpp/build/byte_sum_benchmark'

class BenchmarkTests(unittest.TestCase):
    def test_report(self):
        for size in (1, 257, 8192):
            with self.subTest(size=size):
                report = json.loads(subprocess.check_output([str(BIN), str(size), '7', '2'], text=True))
                self.assertEqual(report['sum'], sum((i * 37 + 129) % 256 for i in range(size)))
                self.assertTrue(report['correct'])
                self.assertEqual((report['bytes'], report['iterations'], report['warmup']), (size, 7, 2))
                self.assertEqual(report['backend'], 'cpp-cpu-demo')
                for key in ('total', 'cpu_reference'):
                    self.assertTrue(math.isfinite(report[key]['p95_ms']))
                    self.assertGreaterEqual(report[key]['p50_ms'], 0)
                    self.assertGreaterEqual(report[key]['p95_ms'], report[key]['p50_ms'])
                for key in ('h2d_host', 'kernel_event', 'd2h_host'):
                    self.assertIsNone(report[key])

    def test_invalid_arguments(self):
        for args in [('0',), ('8193',), ('-1',), ('1.5',), ('10x',), ('1', '0'),
                     ('1', '10001'), ('1', '2', '1001'), ('1', '2', '3', '4')]:
            with self.subTest(args=args):
                result = subprocess.run([str(BIN), *args], capture_output=True, text=True)
                self.assertNotEqual(result.returncode, 0)
                self.assertEqual(result.stdout, '')

if __name__ == '__main__':
    unittest.main(verbosity=2)
