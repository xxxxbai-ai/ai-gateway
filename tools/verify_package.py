"""Run the real Qt integration checks using only the packaged runtime DLLs."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--package', required=True, type=Path)
parser.add_argument('--test-bin', required=True, type=Path)
parser.add_argument('--work', required=True, type=Path)
options = parser.parse_args()
work = options.work.resolve()
if work.exists():
    raise SystemExit('Choose a fresh work folder; existing files are preserved.')
shutil.copytree(options.package, work)
for name in ['client_tests.exe', 'Qt6Test.dll']:
    shutil.copy2(options.test_bin / name, work / name)
environment = dict(os.environ)
environment['PATH'] = environment.get('SystemRoot', r'C:\Windows') + r'\System32'
environment['QT_QPA_PLATFORM'] = 'offscreen'
environment['QT_QPA_PLATFORM_PLUGIN_PATH'] = str(work / 'platforms')
for name in ['QT_PLUGIN_PATH', 'QML_IMPORT_PATH', 'QML2_IMPORT_PATH']:
    environment.pop(name, None)
report = work / 'package-verification.txt'
result = subprocess.run([str(work / 'client_tests.exe'), '-o', str(report) + ',txt', '-v1'],
                        cwd=work, env=environment, timeout=60, capture_output=True)
if report.exists():
    print(report.read_text(encoding='utf-8', errors='replace'))
print('Packaged runtime check exit:', result.returncode)
raise SystemExit(result.returncode)
