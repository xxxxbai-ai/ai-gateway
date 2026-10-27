"""Actual A+B+C over HTTP; only D is replaced with a clearly marked test double."""
import argparse
import concurrent.futures
import copy
import csv
import http.client
import json
from pathlib import Path
import re
import subprocess
import tempfile
import time
import unittest

parser = argparse.ArgumentParser()
for name in ('gateway', 'analyzer', 'fixture', 'config', 'work'):
    parser.add_argument('--' + name, required=True)
options, arguments = parser.parse_known_args()


class BCIntegrationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        Path(options.work).mkdir(parents=True, exist_ok=True)
        report = Path(options.work) / 'standalone_b_results.tsv'
        subprocess.run([options.analyzer, options.fixture, str(report)], check=True,
                       stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=15)
        with report.open(encoding='utf-8', newline='') as stream:
            cls.standalone = list(csv.DictReader(stream, delimiter='\t'))
        cls.base_config = json.loads(Path(options.config).read_text(encoding='utf-8'))

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(dir=options.work)
        self.addCleanup(self.temporary.cleanup)
        self.directory = Path(self.temporary.name)
        self.config_path = self.directory / '模型配置.json'
        self.log_path = self.directory / '请求日志.jsonl'
        self.configuration = copy.deepcopy(self.base_config)
        for model in self.configuration['models']:
            if model['is_mock']:
                model['provider_model'] = 'bc-test-ok'
        self.save_fixture()
        self.process = None
        self.addCleanup(self.stop)
        self.start()

    def save_fixture(self):
        self.config_path.write_text(json.dumps(self.configuration, ensure_ascii=False), encoding='utf-8')

    def model(self, model_id):
        return next(model for model in self.configuration['models'] if model['id'] == model_id)

    def start(self):
        output = self.directory / 'startup.txt'
        with output.open('wb') as stream:
            self.process = subprocess.Popen([
                options.gateway, '--port', '0', '--config', str(self.config_path),
                '--log', str(self.log_path)], stdout=stream, stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        for _ in range(100):
            text = output.read_text(encoding='utf-8', errors='replace')
            match = re.search(r'Gateway ready: http://127\.0\.0\.1:(\d+)', text)
            if match:
                self.port = int(match.group(1))
                self.assertIn('actual A/B/C, D test double only', text)
                return
            if self.process.poll() is not None:
                raise RuntimeError(text)
            time.sleep(.05)
        raise RuntimeError('BC integration test host startup timed out')

    def stop(self):
        if self.process is not None and self.process.poll() is None:
            self.process.terminate()
            self.process.wait(timeout=5)

    def request(self, method, path, value=None):
        connection = http.client.HTTPConnection('127.0.0.1', self.port, timeout=8)
        try:
            body = json.dumps(value, ensure_ascii=False).encode('utf-8') if value is not None else None
            connection.request(method, path, body, {'Content-Type': 'application/json; charset=utf-8'})
            response = connection.getresponse()
            self.assertIn('application/json', response.getheader('Content-Type'))
            return response.status, json.loads(response.read())
        finally:
            connection.close()

    def chat(self, message, model=None):
        return self.request('POST', '/api/chat', {
            'mode': 'manual' if model is not None else 'auto', 'model': model, 'message': message})

    def records(self):
        return [json.loads(line) for line in self.log_path.read_text(encoding='utf-8').splitlines()]

    def test_frozen_30_matches_standalone_B_and_routes_using_actual_C(self):
        models = {'programming': 'mock_code', 'math': 'mock_math', 'translation': 'mock',
                  'writing': 'mock', 'general': 'mock'}
        self.assertEqual(len(self.standalone), 30)
        for row in self.standalone:
            with self.subTest(row=row['id']):
                status, result = self.chat(row['question'])
                self.assertEqual(status, 200)
                self.assertEqual(result['task'], row['actual_task'])
                self.assertEqual(result['uncertain'], row['actual_uncertain'] == 'true')
                self.assertTrue(result['reason'].startswith(row['reason'] + '；'))
                self.assertEqual(result['selected_model'], models[row['expected_task']])
                self.assertEqual(result['answer'],
                    f"BC TEST DOUBLE ONLY [{result['selected_model']}]: {row['question']}")
                self.assertTrue(result['is_mock'])
                self.assertFalse(result['fallback_used'])
                record = self.records()[-1]
                self.assertEqual(record['request_id'], result['request_id'])
                self.assertEqual(record['task'], result['task'])
                self.assertEqual(record['attempts'][0]['model_id'], result['selected_model'])

    def test_quotes_negation_and_newlines_preserve_classification(self):
        for message, task in [
            ('将“帮我写一篇作文”翻译成英文', 'translation'),
            ('不要翻译，请解释 C++ 报错', 'programming'),
            ('不要润色，请翻译这段 C++ 报错', 'translation'),
            ('WRITE AN EMAIL ABOUT C++', 'writing'),
            ('翻译这段\nC++ 报错', 'translation')]:
            with self.subTest(message=message):
                status, result = self.chat(message)
                self.assertEqual((status, result['task']), (200, task))

    def test_automatic_fallback_uses_actual_C_candidates(self):
        for failure in ('429', '503'):
            with self.subTest(failure=failure):
                self.model('mock_code')['provider_model'] = 'bc-test-reject-' + failure
                self.save_fixture()
                status, result = self.chat('解释 C++ 指针')
                self.assertEqual((status, result['selected_model']), (200, 'mock'))
                self.assertTrue(result['fallback_used'])
                self.assertIn('HTTP ' + failure, result['reason'])
                self.assertEqual([a['model_id'] for a in self.records()[-1]['attempts']], ['mock_code', 'mock'])

    def test_manual_failure_never_switches(self):
        self.model('mock_code')['provider_model'] = 'bc-test-reject-503'
        self.save_fixture()
        status, result = self.chat('计算平方根', model='mock_code')
        self.assertEqual((status, result['error_code']), (503, 'upstream_rejected'))
        self.assertFalse(result['fallback_used'])
        self.assertEqual(len(self.records()[-1]['attempts']), 1)

    def test_two_failures_stop_after_two_distinct_models(self):
        self.model('mock_code')['provider_model'] = 'bc-test-reject-503'
        self.model('mock')['provider_model'] = 'bc-test-reject-429'
        self.save_fixture()
        status, result = self.chat('解释 C++ 指针')
        self.assertEqual(status, 503)
        self.assertTrue(result['fallback_used'])
        self.assertEqual([a['model_id'] for a in self.records()[-1]['attempts']], ['mock_code', 'mock'])

    def test_manual_unknown_disabled_and_empty_do_not_call_D(self):
        for message, model, code in [('你好', 'missing', 'unknown_model'),
                                     ('你好', 'real_a', 'disabled_model'),
                                     (' ', None, 'invalid_request')]:
            with self.subTest(code=code):
                status, result = self.chat(message, model)
                self.assertEqual((status, result['error_code']), (400, code))
                self.assertEqual(self.records()[-1]['attempts'], [])

    def test_no_enabled_models_does_not_call_D(self):
        for model in self.configuration['models']:
            model['enabled'] = False
        self.save_fixture()
        status, result = self.chat('解释 C++ 指针')
        self.assertEqual((status, result['error_code']), (503, 'no_available_model'))
        self.assertEqual(self.records()[-1]['attempts'], [])

    def test_score_change_disable_and_save_survive_restart(self):
        status, settings = self.request('GET', '/api/config')
        self.assertEqual(status, 200)
        self.assertNotIn('credential_ref', settings['models'][0])
        changes = settings['models']
        code = next(m for m in changes if m['id'] == 'mock_code')
        math = next(m for m in changes if m['id'] == 'mock_math')
        code['scores']['programming'] = 10
        math['scores']['programming'] = 99
        self.assertEqual(self.request('POST', '/api/config', {'models': changes})[0], 200)
        self.assertEqual(self.chat('解释 C++ 指针')[1]['selected_model'], 'mock_math')
        math['enabled'] = False
        self.assertEqual(self.request('POST', '/api/config', {'models': changes})[0], 200)
        self.assertEqual(self.chat('解释 C++ 指针')[1]['selected_model'], 'mock')
        self.stop()
        self.start()
        self.assertEqual(self.chat('解释 C++ 指针')[1]['selected_model'], 'mock')
        ids = [m['id'] for m in self.request('GET', '/api/models')[1]['models']]
        self.assertNotIn('mock_math', ids)
        self.assertNotIn('real_a', ids)

    def test_invalid_save_preserves_previous_configuration(self):
        before = self.config_path.read_bytes()
        changes = self.request('GET', '/api/config')[1]['models']
        changes[0]['scores']['programming'] = 101
        self.assertEqual(self.request('POST', '/api/config', {'models': changes})[0], 400)
        self.assertEqual(self.config_path.read_bytes(), before)
        self.assertEqual(self.chat('解释 C++ 指针')[1]['selected_model'], 'mock_code')

    def test_bad_configuration_is_reported_without_D_call(self):
        self.config_path.write_text('{broken', encoding='utf-8')
        status, result = self.chat('解释 C++ 指针')
        self.assertEqual((status, result['error_code']), (500, 'invalid_config'))
        self.assertEqual(self.records()[-1]['attempts'], [])

    def test_timeout_and_network_error_do_not_trigger_extra_calls(self):
        for scenario, status, code in [('timeout', 504, 'total_timeout'),
                                       ('network', 503, 'connection_failed')]:
            with self.subTest(scenario=scenario):
                self.model('mock_code')['provider_model'] = 'bc-test-' + scenario
                self.save_fixture()
                actual_status, result = self.chat('解释 C++ 指针')
                self.assertEqual((actual_status, result['error_code']), (status, code))
                self.assertFalse(result['fallback_used'])
                self.assertEqual(len(self.records()[-1]['attempts']), 1)
        self.model('mock_code')['provider_model'] = 'bc-test-ok'
        self.save_fixture()
        self.assertEqual(self.chat('解释 C++ 指针')[0], 200)

    def test_concurrent_requests_keep_B_C_and_logs_consistent(self):
        messages = ['解释 C++ 指针', '计算256的平方根', '把你好翻译成英文'] * 4
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            results = list(pool.map(self.chat, messages))
        self.assertTrue(all(status == 200 for status, _ in results))
        self.assertEqual(len({r['request_id'] for _, r in results}), len(messages))
        self.assertEqual(len(self.records()), len(messages))


if __name__ == '__main__':
    unittest.main(argv=['bc_http_tests.py', *arguments], verbosity=2)
