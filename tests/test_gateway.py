"""Exercise the actual C++ HTTP gateway, B/C/D modules and owned mock service."""
import argparse
import concurrent.futures
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
parser.add_argument('--config', required=True)
parser.add_argument('--work', required=True)
options, remaining = parser.parse_known_args()


class GatewayTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Path(options.work).mkdir(parents=True, exist_ok=True)
        cls.work = tempfile.TemporaryDirectory(prefix='http-', dir=options.work)
        cls.addClassCleanup(cls.work.cleanup)
        cls.directory = Path(cls.work.name)
        cls.config = cls.directory / 'models.json'
        cls.log = cls.directory / 'requests.jsonl'
        cls.original = json.loads(Path(options.config).read_text(encoding='utf-8'))
        cls.config.write_text(json.dumps(cls.original, ensure_ascii=False), encoding='utf-8')
        cls.process, cls.port = cls.start_gateway('primary')
        cls.addClassCleanup(cls.stop_gateway, cls.process)

    @classmethod
    def start_gateway(cls, suffix):
        output_path = cls.directory / (suffix + '.log')
        with output_path.open('wb') as output:
            process = subprocess.Popen([options.gateway, '--demo', '--port', '0', '--config', str(cls.config),
                '--log', str(cls.log)], stdout=output, stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        deadline = time.monotonic() + 12
        while time.monotonic() < deadline:
            text = output_path.read_text(encoding='utf-8', errors='replace')
            match = re.search(r'Gateway ready: http://127\.0\.0\.1:(\d+)', text)
            if match:
                return process, int(match.group(1))
            if process.poll() is not None:
                raise RuntimeError('Gateway startup failed: ' + text)
            time.sleep(0.05)
        cls.stop_gateway(process)
        raise RuntimeError('Gateway startup timed out')

    @staticmethod
    def stop_gateway(process):
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=4)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait(timeout=4)

    def setUp(self):
        self.config.write_text(json.dumps(self.original, ensure_ascii=False), encoding='utf-8')

    def request(self, method, path, body=None, raw=None, port=None):
        payload = raw if raw is not None else (None if body is None else json.dumps(body, ensure_ascii=False).encode('utf-8'))
        connection = http.client.HTTPConnection('127.0.0.1', port or self.port, timeout=6)
        try:
            connection.request(method, path, body=payload, headers={'Content-Type': 'application/json; charset=utf-8'})
            response = connection.getresponse()
            self.assertTrue(response.getheader('Content-Type', '').startswith('application/json'))
            return response.status, json.loads(response.read().decode('utf-8'))
        finally:
            connection.close()

    def chat(self, text='解释 C++ 指针', **extra):
        body = {'mode': 'auto', 'model': None, 'message': text}
        body.update(extra)
        return self.request('POST', '/api/chat', body)

    def records(self):
        return [json.loads(line) for line in self.log.read_text(encoding='utf-8').splitlines() if line]

    def record_for(self, result):
        return next(value for value in self.records() if value['request_id'] == result['request_id'])

    def write_config(self, change):
        value = json.loads(json.dumps(self.original))
        change(value['models'])
        self.config.write_text(json.dumps(value, ensure_ascii=False), encoding='utf-8')

    def test_health_and_dynamic_models(self):
        self.assertEqual(self.request('GET', '/health'), (200, {'success': True, 'status': 'ok'}))
        status, result = self.request('GET', '/api/models')
        self.assertEqual(status, 200)
        self.assertEqual({model['id'] for model in result['models']}, {'mock', 'mock_code', 'mock_math'})
        self.assertTrue(result['demo_mode'])
        for model in result['models']:
            self.assertEqual(set(model), {'id', 'name', 'is_mock'})

    def test_five_tasks_reach_expected_models(self):
        for text, task, model in [('解释 C++ 指针', 'programming', 'mock_code'),
                ('计算256的平方根', 'math', 'mock_math'), ('把你好翻译成英文', 'translation', 'mock'),
                ('帮我写一篇环保作文', 'writing', 'mock'), ('为什么天空是蓝色的', 'general', 'mock')]:
            with self.subTest(task=task):
                status, result = self.chat(text)
                self.assertEqual(status, 200)
                self.assertEqual((result['task'], result['selected_model']), (task, model))
                self.assertTrue(result['is_mock'])
                self.assertFalse(result['fallback_used'])
                self.assertIn(text, result['answer'])
                self.assertIn('模拟回答', result['answer'])
                self.assertTrue(result['reason'])
                self.assertEqual(len(self.record_for(result)['attempts']), 1)

    def test_manual_overrides_automatic_choice(self):
        status, result = self.chat(mode='manual', model='mock_math')
        self.assertEqual(status, 200)
        self.assertEqual(result['selected_model'], 'mock_math')
        self.assertEqual(result['task'], 'programming')
        self.assertIn('手动', result['reason'])

    def test_unknown_and_disabled_manual_models(self):
        status, result = self.chat(mode='manual', model='missing')
        self.assertEqual((status, result['error_code']), (400, 'unknown_model'))
        self.write_config(lambda models: models[0].update(enabled=False))
        status, result = self.chat(mode='manual', model='mock_code')
        self.assertEqual((status, result['error_code']), (400, 'disabled_model'))
        self.assertEqual(self.record_for(result)['attempts'], [])

    def test_invalid_inputs_do_not_call_models(self):
        valid = {'mode': 'auto', 'model': None, 'message': '你好'}
        cases = [[], None, 'text', {}, {**valid, 'mode': 'bad'}, {**valid, 'mode': 1},
            {**valid, 'message': ''}, {**valid, 'message': ' \t\r\n'}, {**valid, 'message': 42},
            {**valid, 'model': []}, {**valid, 'model': 'mock'}, {**valid, 'mode': 'manual'},
            {**valid, 'mode': 'manual', 'model': ''}, {**valid, 'demo_failure': 'yes'}]
        cases += [{key: value for key, value in valid.items() if key != missing} for missing in valid]
        for body in cases:
            with self.subTest(body=body):
                status, result = self.request('POST', '/api/chat', raw=json.dumps(body).encode('utf-8'))
                self.assertEqual(status, 400)
                self.assertFalse(result['success'])
                self.assertTrue(result['error'])
                self.assertEqual(self.record_for(result)['attempts'], [])
        for raw in [b'{broken', b'{"mode":"auto","model":null,"message":"\xff"}']:
            self.assertEqual(self.request('POST', '/api/chat', raw=raw)[0], 400)

    def test_uncertain_and_explicit_translation(self):
        self.assertTrue(self.chat('你好')[1]['uncertain'])
        self.assertEqual(self.chat('C++ 方程')[1]['task'], 'general')
        result = self.chat('翻译这段 C++ 报错')[1]
        self.assertEqual(result['task'], 'translation')
        self.assertEqual(result['selected_model'], 'mock')

    def test_unicode_quotes_and_whitespace(self):
        text = '解释 C++ 的 "字符串"\\路径\n第二行 🚀'
        status, result = self.chat(' \t' + text + '\r\n')
        self.assertEqual(status, 200)
        self.assertIn(text, result['answer'])

    def test_automatic_failure_switches_once(self):
        status, result = self.chat(demo_failure=True)
        self.assertEqual(status, 200)
        self.assertTrue(result['fallback_used'])
        self.assertEqual(result['selected_model'], 'mock')
        attempts = self.record_for(result)['attempts']
        self.assertEqual([item['model_id'] for item in attempts], ['mock_code', 'mock'])
        self.assertEqual([item['http_status'] for item in attempts], [503, 200])

    def test_manual_failure_does_not_switch(self):
        status, result = self.chat(mode='manual', model='mock_code', demo_failure=True)
        self.assertEqual(status, 503)
        self.assertFalse(result['fallback_used'])
        self.assertEqual(len(self.record_for(result)['attempts']), 1)

    def test_two_failures_do_not_try_third(self):
        self.write_config(lambda models: models[2].update(base_url='http://127.0.0.1:18080/scenario/reject429/v1'))
        status, result = self.chat(demo_failure=True)
        self.assertEqual(status, 503)
        attempts = self.record_for(result)['attempts']
        self.assertEqual(len(attempts), 2)
        self.assertEqual([item['http_status'] for item in attempts], [503, 429])

    def test_no_backup_and_no_available_model(self):
        self.write_config(lambda models: [model.update(enabled=False) for model in models if model['id'] != 'mock_code'])
        status, result = self.chat(demo_failure=True)
        self.assertEqual(status, 503)
        self.assertFalse(result['fallback_used'])
        self.assertEqual(len(self.record_for(result)['attempts']), 1)
        self.write_config(lambda models: [model.update(enabled=False) for model in models])
        status, result = self.chat()
        self.assertEqual((status, result['error_code']), (503, 'no_available_model'))

    def test_invalid_upstream_response_does_not_switch(self):
        self.write_config(lambda models: models[0].update(base_url='http://127.0.0.1:18080/scenario/bad_json/v1'))
        status, result = self.chat()
        self.assertEqual((status, result['error_code']), (502, 'invalid_response'))
        self.assertEqual(len(self.record_for(result)['attempts']), 1)

    def test_config_read_error_and_recovery(self):
        self.config.write_text('{broken', encoding='utf-8')
        self.assertEqual(self.chat()[0], 500)
        self.config.write_text(json.dumps(self.original), encoding='utf-8')
        self.assertEqual(self.chat()[0], 200)

    def test_settings_saved_and_used_after_restart(self):
        status, config = self.request('GET', '/api/config')
        self.assertEqual(status, 200)
        for model in config['models']:
            self.assertNotIn('credential_ref', model)
            self.assertNotIn('base_url', model)
            if model['id'] == 'mock_math':
                model['scores']['programming'] = 100
        saved_status, saved_result = self.request('POST', '/api/config', {'models': config['models']})
        self.assertEqual(saved_status, 200, saved_result)
        self.assertEqual(self.chat()[1]['selected_model'], 'mock_math')
        process, port = self.start_gateway('restart')
        try:
            status, result = self.request('POST', '/api/chat', {'mode':'auto','model':None,'message':'解释 C++ 指针'}, port=port)
            self.assertEqual(status, 200)
            self.assertEqual(result['selected_model'], 'mock_math')
        finally:
            self.stop_gateway(process)

    def test_rejected_settings_preserve_original_file(self):
        before = self.config.read_bytes()
        _, config = self.request('GET', '/api/config')
        config['models'][0]['scores']['math'] = 101
        self.assertEqual(self.request('POST', '/api/config', {'models':config['models']})[0], 400)
        self.assertEqual(self.config.read_bytes(), before)

    def test_concurrent_requests_have_unique_ids_and_valid_logs(self):
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(lambda _: self.chat(), range(12)))
        self.assertTrue(all(status == 200 for status, _ in results))
        ids = {value['request_id'] for _, value in results}
        self.assertEqual(len(ids), 12)
        records = self.records()
        self.assertTrue(ids.issubset({record['request_id'] for record in records}))
        self.assertTrue(all('elapsed_ms' in record and 'attempts' in record for record in records))


if __name__ == '__main__':
    unittest.main(argv=['test_gateway.py', *remaining], verbosity=2)
