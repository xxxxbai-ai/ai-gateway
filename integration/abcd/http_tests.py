"""Exercise actual A/B/C/D together; only the external model server is simulated."""
import argparse
import copy
import http.client
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time
import unittest

parser = argparse.ArgumentParser()
for name in ('gateway', 'provider', 'config', 'work'):
    parser.add_argument('--' + name, required=True)
options, remaining = parser.parse_known_args()


class ABCDIntegrationTests(unittest.TestCase):
    def setUp(self):
        Path(options.work).mkdir(parents=True, exist_ok=True)
        self.temp = tempfile.TemporaryDirectory(dir=options.work)
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.children = []
        self.addCleanup(self.stop)
        with socket.socket() as reservation:
            reservation.bind(('127.0.0.1', 0))
            self.provider_port = reservation.getsockname()[1]
        self.start_process('provider', [options.provider, '--port', str(self.provider_port), '--delay-ms', '1'])
        for _ in range(100):
            try:
                if self.http(self.provider_port, 'GET', '/health')[0] == 200:
                    break
            except OSError:
                time.sleep(.03)
        else:
            self.fail('D model simulator did not start')
        self.config = json.loads(Path(options.config).read_text(encoding='utf-8'))
        self.config['models'] = [m for m in self.config['models'] if m['is_mock']]
        providers = {'mock_code': 'demo-code', 'mock_math': 'demo-language', 'mock': 'demo-general'}
        for model in self.config['models']:
            model.update(base_url=f'http://127.0.0.1:{self.provider_port}/v1',
                         provider_model=providers[model['id']], credential_ref='')
        self.config_path = self.work / 'models.json'
        self.log_path = self.work / 'requests.jsonl'
        self.save()
        output = self.start_process('gateway', [options.gateway, '--port', '0', '--config',
                                    str(self.config_path), '--log', str(self.log_path)])
        for _ in range(100):
            text = output.read_text(encoding='utf-8', errors='replace')
            match = re.search(r'Gateway ready: http://127\.0\.0\.1:(\d+)', text)
            if match:
                self.gateway_port = int(match.group(1))
                break
            if self.children[-1].poll() is not None:
                self.fail(text)
            time.sleep(.03)
        else:
            self.fail('Actual gateway did not start')

    def start_process(self, name, arguments):
        output = self.work / (name + '.txt')
        with output.open('wb') as stream:
            self.children.append(subprocess.Popen(arguments, stdout=stream, stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0)))
        return output

    def stop(self):
        for child in reversed(self.children):
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=5)

    def save(self):
        self.config_path.write_text(json.dumps(self.config, ensure_ascii=False), encoding='utf-8')

    def model(self, identifier):
        return next(m for m in self.config['models'] if m['id'] == identifier)

    def scenario(self, identifier, scenario):
        self.model(identifier)['base_url'] = f'http://127.0.0.1:{self.provider_port}/scenario/{scenario}/v1'
        self.save()

    def http(self, port, method, path, value=None):
        connection = http.client.HTTPConnection('127.0.0.1', port, timeout=8)
        try:
            body = None if value is None else json.dumps(value, ensure_ascii=False).encode('utf-8')
            connection.request(method, path, body, {'Content-Type': 'application/json; charset=utf-8'})
            response = connection.getresponse()
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def chat(self, message='解释 C++ 指针', model=None):
        return self.http(self.gateway_port, 'POST', '/api/chat',
            {'mode': 'auto' if model is None else 'manual', 'model': model, 'message': message})

    def record(self):
        return json.loads(self.log_path.read_text(encoding='utf-8').splitlines()[-1])

    def stats(self):
        return self.http(self.provider_port, 'GET', '/stats')[1]['counts']

    def test_normal_automatic_and_manual(self):
        self.assertEqual(self.http(self.gateway_port, 'GET', '/health')[0], 200)
        models = self.http(self.gateway_port, 'GET', '/api/models')[1]['models']
        self.assertEqual({m['id'] for m in models}, {'mock_code', 'mock_math', 'mock'})
        self.assertTrue(all(set(m) == {'id', 'name', 'is_mock'} for m in models))
        for message, task, model, upstream in [
            ('解释 C++ 指针', 'programming', 'mock_code', 'demo-code'),
            ('计算256的平方根', 'math', 'mock_math', 'demo-language'),
            ('把你好翻译成英文', 'translation', 'mock', 'demo-general')]:
            status, response = self.chat(message)
            self.assertEqual(status, 200)
            self.assertEqual((response['task'], response['selected_model']), (task, model))
            self.assertIn(upstream, response['answer'])
            self.assertTrue(response['is_mock'])
            self.assertEqual(self.record()['request_id'], response['request_id'])
        status, response = self.chat(model='mock_math')
        self.assertEqual((status, response['selected_model']), (200, 'mock_math'))
        self.assertEqual(sum(self.stats().values()), 4)

    def test_auto_fallback_and_manual_single_call(self):
        self.scenario('mock_code', 'reject503')
        status, response = self.chat()
        self.assertEqual((status, response['selected_model'], response['fallback_used']), (200, 'mock', True))
        self.assertEqual([a['model_id'] for a in self.record()['attempts']], ['mock_code', 'mock'])
        self.assertEqual(self.stats()['demo-code'], 1)
        self.assertEqual(self.stats()['demo-general'], 1)
        status, response = self.chat(model='mock_code')
        self.assertEqual((status, response['error_code'], response['fallback_used']), (503, 'upstream_rejected', False))
        self.assertEqual(self.stats()['demo-code'], 2)
        self.assertEqual(self.stats()['demo-general'], 1)

    def test_two_failures_are_bounded(self):
        for model in self.config['models']:
            self.scenario(model['id'], 'reject503')
        status, response = self.chat()
        self.assertEqual((status, response['success'], response['fallback_used']), (503, False, True))
        self.assertEqual(sum(self.stats().values()), 2)

    def test_invalid_upstream_response_does_not_fallback(self):
        self.scenario('mock_code', 'length')
        status, response = self.chat()
        self.assertEqual((status, response['error_code']), (502, 'incomplete_response'))
        self.assertEqual(sum(self.stats().values()), 1)

    def test_real_model_without_local_credential_is_an_explicit_failure(self):
        real = copy.deepcopy(self.model('mock_code'))
        real.update(id='deepseek_flash', name='DeepSeek', is_mock=False,
                    base_url='https://api.deepseek.com', provider_model='deepseek-flash',
                    credential_ref='env:GATEWAY_INTEGRATION_INTENTIONALLY_ABSENT_KEY')
        self.config['models'] = [real]
        self.save()
        status, response = self.chat()
        self.assertEqual(response['error_code'], 'missing_credential')
        self.assertFalse(response['success'])
        self.assertNotEqual(status, 200)
        self.assertEqual(sum(self.stats().values()), 0)


if __name__ == '__main__':
    unittest.main(argv=[__file__, *remaining])
