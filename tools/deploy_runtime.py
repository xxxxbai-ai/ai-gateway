"""Copy a Windows DLL and its non-system dependencies into the runnable folder."""
import argparse
from pathlib import Path
import re
import shutil
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--dll', required=True)
parser.add_argument('--search', action='append', default=[])
parser.add_argument('--out', required=True)
parser.add_argument('--objdump', required=True)
args = parser.parse_args()
output = Path(args.out).resolve()
output.mkdir(parents=True, exist_ok=True)
search = [Path(folder) for folder in args.search]
queue = [Path(args.dll).resolve()]
seen = set()
while queue:
    dll = queue.pop()
    key = dll.name.lower()
    if key in seen:
        continue
    seen.add(key)
    destination = output / dll.name
    if dll != destination:
        shutil.copy2(dll, destination)
    result = subprocess.run([args.objdump, '-p', str(dll)], capture_output=True, check=True)
    for name in re.findall(r'DLL Name:\s*(\S+)', result.stdout.decode('utf-8', errors='replace')):
        for directory in search:
            candidate = directory / name
            if candidate.is_file():
                queue.append(candidate.resolve())
                break
print(f'Deployed {len(seen)} runtime libraries.')
