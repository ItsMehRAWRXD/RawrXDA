import re, json, subprocess, pathlib
text = pathlib.Path('src/core/command_registry.hpp').read_text(encoding='utf-8', errors='ignore')
existing_ids = set(int(m.group(1)) for m in re.finditer(r'\bX\((\d+),', text))
p = subprocess.run(['py','-3','scripts/audit_command_table.py','--json-only'], capture_output=True, text=True)
if p.returncode not in (0,2):
    raise SystemExit(p.stderr)
j=json.loads(p.stdout)
missing=j['missing_from_table']
conf=[m for m in missing if m['value'] in existing_ids]
uniq=[m for m in missing if m['value'] not in existing_ids]
print('missing',len(missing),'conflict_ids',len(conf),'unique_ids',len(uniq))
for m in conf[:30]:
    print(f"conflict {m['name']}={m['value']}")
