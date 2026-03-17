#!/usr/bin/env python3
"""
RawrXD MASM Digest Script
Scans a MASM .asm file and outputs a comprehensive report of:
- All PROC (function) definitions: name, line, stub/real
- All struct definitions: name, line, fields
- All .data labels
- All EQU constants
- All extern references and undefined invokes
- All TODO/FIXME/BLOCKED/HIDDEN comments
- All procs calling undefined procs
- All unused local variables
- All procs missing ret
- All empty .if/.while blocks
"""
import re
import sys
from collections import defaultdict

if len(sys.argv) < 2:
    print("Usage: masm_digest.py <file.asm>")
    sys.exit(1)

filename = sys.argv[1]
with open(filename, 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

procs = []
structs = []
data_labels = []
equ_constants = []
externs = []
invokes = []
todos = []
undefined_calls = set()
proc_names = set()
proc_calls = defaultdict(list)
local_vars = defaultdict(list)
used_locals = defaultdict(set)
procs_missing_ret = set()
empty_blocks = []

struct_fields = []
inside_struct = None
inside_proc = None
inside_data = False
block_stack = []

for i, line in enumerate(lines):
    l = line.strip()
    # PROC detection
    m = re.match(r'([\w@]+)\s+proc(\s+frame)?', l, re.IGNORECASE)
    if m:
        name = m.group(1)
        procs.append({'name': name, 'line': i+1, 'stub': False, 'ret': False, 'locals': [], 'calls': [], 'body': []})
        proc_names.add(name.lower())
        inside_proc = name
        continue
    # ENDP detection
    if re.match(r'([\w@]+)\s+endp', l, re.IGNORECASE):
        inside_proc = None
        continue
    # STRUCT detection
    m = re.match(r'([\w@]+)\s+struct', l, re.IGNORECASE)
    if m:
        inside_struct = {'name': m.group(1), 'line': i+1, 'fields': []}
        continue
    if inside_struct and l.lower() == 'ends':
        structs.append(inside_struct)
        inside_struct = None
        continue
    if inside_struct:
        inside_struct['fields'].append(l)
        continue
    # .data section
    if l.lower() == '.data':
        inside_data = True
        continue
    if l.startswith('.code') or l.startswith('.const') or l.startswith('.stack'):
        inside_data = False
    if inside_data and l and not l.startswith(';'):
        m = re.match(r'([\w@]+)\s+(db|dw|dd|dq|label|real4|real8)', l, re.IGNORECASE)
        if m:
            data_labels.append({'name': m.group(1), 'line': i+1, 'type': m.group(2)})
    # EQU constants
    m = re.match(r'([\w@]+)\s+equ\s+(.+)', l, re.IGNORECASE)
    if m:
        equ_constants.append({'name': m.group(1), 'line': i+1, 'value': m.group(2)})
    # extern
    m = re.match(r'extern\s+([\w@:, ]+)', l, re.IGNORECASE)
    if m:
        for sym in m.group(1).split(','):
            externs.append({'name': sym.strip(), 'line': i+1})
    # invoke
    m = re.findall(r'invoke\s+([\w@]+)', l, re.IGNORECASE)
    for sym in m:
        invokes.append({'name': sym, 'line': i+1})
        if inside_proc:
            proc_calls[inside_proc].append((sym, i+1))
    # TODO/FIXME/BLOCKED/HIDDEN
    if re.search(r'\b(TODO|FIXME|BLOCKED|HIDDEN)\b', l, re.IGNORECASE):
        todos.append({'line': i+1, 'text': l})
    # local variables
    if inside_proc:
        m = re.match(r'local\s+([\w@:]+)', l, re.IGNORECASE)
        if m:
            for var in m.group(1).split(','):
                var = var.split(':')[0].strip()
                if var:
                    local_vars[inside_proc].append({'name': var, 'line': i+1})
        # ret detection
        if re.match(r'ret\b', l, re.IGNORECASE):
            for p in procs:
                if p['name'] == inside_proc:
                    p['ret'] = True
        # body for stub detection
        if l and not l.startswith(';') and not l.lower().startswith('local') and not l.lower().startswith('ret'):
            for p in procs:
                if p['name'] == inside_proc:
                    p['body'].append((i+1, l))
        # used locals
        for var in local_vars[inside_proc]:
            if re.search(r'\b'+re.escape(var['name'])+r'\b', l):
                used_locals[inside_proc].add(var['name'])
        # .if/.while block detection
        if re.match(r'\.(if|while)\b', l, re.IGNORECASE):
            block_stack.append({'type': l.split()[0], 'start': i+1, 'body': []})
        if block_stack:
            if l and not l.startswith(';') and not re.match(r'\.(if|while)\b', l, re.IGNORECASE):
                block_stack[-1]['body'].append((i+1, l))
            if l.lower() == '.endif' or l.lower() == '.endw':
                block = block_stack.pop()
                if not any(b[1] and not b[1].startswith(';') for b in block['body']):
                    empty_blocks.append({'type': block['type'], 'start': block['start']})

# Post-processing
for p in procs:
    if not p['ret']:
        procs_missing_ret.add(p['name'])
    if not p['body'] or all(l[1].startswith(';') or not l[1] for l in p['body']):
        p['stub'] = True
    # Unused locals
    for var in local_vars[p['name']]:
        if var['name'] not in used_locals[p['name']]:
            var['unused'] = True
        else:
            var['unused'] = False
    p['locals'] = local_vars[p['name']]
    # Calls to undefined procs
    for call, call_line in proc_calls[p['name']]:
        if call.lower() not in proc_names:
            undefined_calls.add((p['name'], call, call_line))
    p['calls'] = proc_calls[p['name']]

# Output
print("==== PROC/FUNCTIONS ====")
for p in procs:
    print(f"{p['name']} (line {p['line']}): {'STUB' if p['stub'] else 'REAL'}{' (NO RET)' if not p['ret'] else ''}")
    if p['locals']:
        for var in p['locals']:
            if var['unused']:
                print(f"  UNUSED LOCAL: {var['name']} (line {var['line']})")
    if p['calls']:
        for call, call_line in p['calls']:
            print(f"  CALLS: {call} (line {call_line})")
print()
print("==== STRUCTS ====")
for s in structs:
    print(f"{s['name']} (line {s['line']}): fields: {', '.join(s['fields'])}")
print()
print("==== DATA LABELS ====")
for d in data_labels:
    print(f"{d['name']} (line {d['line']}): {d['type']}")
print()
print("==== EQU CONSTANTS ====")
for e in equ_constants:
    print(f"{e['name']} (line {e['line']}): {e['value']}")
print()
print("==== EXTERNS ====")
for ex in externs:
    print(f"{ex['name']} (line {ex['line']})")
print()
print("==== INVOKES TO UNDEFINED ====")
for p, call, call_line in sorted(undefined_calls):
    print(f"{p} calls undefined {call} (line {call_line})")
print()
print("==== TODO/FIXME/BLOCKED/HIDDEN COMMENTS ====")
for t in todos:
    print(f"Line {t['line']}: {t['text']}")
print()
print("==== PROCS MISSING RET ====")
for p in procs_missing_ret:
    print(p)
print()
print("==== EMPTY .IF/.WHILE BLOCKS ====")
for b in empty_blocks:
    print(f"{b['type']} block starting at line {b['start']} is empty")
