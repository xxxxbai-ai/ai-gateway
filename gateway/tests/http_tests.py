"""A HTTP boundary checks; the host links fixed B/C/D test doubles, not team code."""
import argparse
import http.client
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time
import unittest

parser = argparse.ArgumentParser()
parser.add_argument('--gateway', required=True)
parser.add_argument('--work', required=True)
options, arguments = parser.parse_known_args()

class HttpTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Path(options.work).mkdir(parents=True, exist_ok=True)
        cls.temporary = tempfile.TemporaryDirectory(dir=options.work)
        cls.addClassCleanup(cls.temporary.cleanup)
        cls.directory = Path(cls.temporary.name)
        cls.log = cls.directory / '中文日志.jsonl'
        output = cls.directory / 'startup.txt'
        with output.open('wb') as stream:
            cls.process = subprocess.Popen([options.gateway, '--port', '0', '--config',
                str(cls.directory / 'C提供的配置.json'), '--log', str(cls.log)], stdout=stream,
                stderr=subprocess.STDOUT, creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        cls.addClassCleanup(cls.stop)
        for _ in range(100):
            match = re.search(r'Gateway ready: http://127\.0\.0\.1:(\d+)', output.read_text(encoding='utf-8', errors='replace'))
            if match:
                cls.port = int(match.group(1))
                return
            if cls.process.poll() is not None:
                raise RuntimeError(output.read_text(encoding='utf-8', errors='replace'))
            time.sleep(.05)
        raise RuntimeError('A test host startup timed out')

    @classmethod
    def stop(cls):
        if cls.process.poll() is None:
            cls.process.terminate()
            cls.process.wait(timeout=5)

    def request(self, method, path, value=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=5)
        try:
            body = json.dumps(value, ensure_ascii=False).encode('utf-8') if value is not None else None
            connection.request(method, path, body, {'Content-Type':'application/json'})
            response = connection.getresponse()
            self.assertIn('application/json', response.getheader('Content-Type'))
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def test_health(self):
        self.assertEqual(self.request('GET', '/health'), (200, {'success':True,'status':'ok'}))

    def test_models_from_contract(self):
        status, result = self.request('GET', '/api/models')
        self.assertEqual(status, 200)
        self.assertEqual([x['id'] for x in result['models']], ['a_test_primary','a_test_backup'])

    def test_chat_forwards_unicode_and_serializes_results(self):
        status, result = self.request('POST', '/api/chat', {'mode':'auto','model':None,'message':' 中文 "引号"\n🙂 '})
        self.assertEqual(status, 200)
        self.assertEqual(result['answer'], 'TEST DOUBLE ONLY: 中文 "引号"\n🙂')
        self.assertEqual(result['selected_model'], 'a_test_primary')
        self.assertTrue(result['is_mock'])

    def test_empty_question(self):
        status, result = self.request('POST', '/api/chat', {'mode':'auto','model':None,'message':' '})
        self.assertEqual((status,result['error_code']), (400,'invalid_request'))

    def test_missing_field(self):
        self.assertEqual(self.request('POST','/api/chat',{'message':'x'})[0], 400)

    def test_settings_safe_and_update_acknowledged(self):
        status, result = self.request('GET', '/api/config')
        self.assertEqual(status, 200)
        self.assertNotIn('credential_ref', result['models'][0])
        self.assertEqual(self.request('POST', '/api/config', {'models':result['models']})[0], 200)

    def test_reject_old_fault_switch(self):
        self.assertEqual(self.request('POST', '/api/chat', {'mode':'auto','model':None,'message':'x','demo_failure':True})[0], 400)

if __name__ == '__main__':
    unittest.main(argv=['http_tests.py', *arguments], verbosity=2)
