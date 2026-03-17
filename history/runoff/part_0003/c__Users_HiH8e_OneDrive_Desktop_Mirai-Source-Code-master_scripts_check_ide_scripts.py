import re
import subprocess
import tempfile
from pathlib import Path

path = Path(__file__).resolve().parents[1] / 'IDEre2.html'
text = path.read_text(encoding='utf-8')
pattern = re.compile(r'<script\b[^>]*>(.*?)</script>', re.S | re.I)
errors = []
for idx, match in enumerate(pattern.finditer(text), start=1):
    script_content = match.group(1).strip()
    if not script_content:
        continue
    with tempfile.NamedTemporaryFile('w', suffix='.js', delete=False, encoding='utf-8') as tmp:
        tmp.write(script_content)
        tmp_path = tmp.name
    proc = subprocess.run(['node', '--check', tmp_path], capture_output=True, text=True)
    if proc.returncode != 0:
        errors.append((idx, tmp_path, proc.stderr.strip()))
        break
if errors:
    for idx, tmp_path, msg in errors:
        print(f'Script #{idx} failed syntax check:')
        print(msg)
else:
    print('All script blocks passed syntax check (or none failed).')
