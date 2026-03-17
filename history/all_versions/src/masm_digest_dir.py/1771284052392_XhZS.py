import os
import re
import argparse
from collections import defaultdict

# Patterns for MASM/ASM features
PROC_PATTERN = re.compile(r'^(\w+)\s+PROC', re.MULTILINE)
ENDP_PATTERN = re.compile(r'^(\w+)\s+ENDP', re.MULTILINE)
STRUCT_PATTERN = re.compile(r'^(\w+)\s+STRUCT', re.MULTILINE)
DATA_PATTERN = re.compile(r'^(\w+)\s+(?:DB|DW|DD|DQ|DT|DF|RESB|RESD|RESQ|RESW|BYTE|WORD|DWORD|QWORD|TBYTE)', re.MULTILINE)
CONST_PATTERN = re.compile(r'^(\w+)\s+EQU', re.MULTILINE)
EXTERN_PATTERN = re.compile(r'EXTERN\s+(\w+)', re.MULTILINE)
TODO_PATTERN = re.compile(r';.*TODO|;.*FIXME|;.*STUB|;.*PLACEHOLDER', re.IGNORECASE)
CALL_PATTERN = re.compile(r'call\s+(\w+)', re.IGNORECASE)
RET_PATTERN = re.compile(r'\bret\b', re.IGNORECASE)
LOCAL_PATTERN = re.compile(r'LOCAL\s+(\w+)', re.IGNORECASE)

# File extensions to scan
ASM_EXTS = {'.asm', '.masm', '.inc', '.s'}


def analyze_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    procs = [(m.group(1), m.start()) for m in PROC_PATTERN.finditer(content)]
    endps = set(m.group(1) for m in ENDP_PATTERN.finditer(content))
    structs = [m.group(1) for m in STRUCT_PATTERN.finditer(content)]
    data = [m.group(1) for m in DATA_PATTERN.finditer(content)]
    consts = [m.group(1) for m in CONST_PATTERN.finditer(content)]
    externs = [m.group(1) for m in EXTERN_PATTERN.finditer(content)]
    todos = [m.group(0) for m in TODO_PATTERN.finditer(content)]
    calls = [m.group(1) for m in CALL_PATTERN.finditer(content)]
    rets = [m.start() for m in RET_PATTERN.finditer(content)]
    locals_ = [m.group(1) for m in LOCAL_PATTERN.finditer(content)]

    # Find stub/empty procs (no logic, only ret or empty)
    stub_procs = []
    for proc, start in procs:
        # Find the end of this proc
        end_match = re.search(rf'{proc}\s+ENDP', content[start:])
        if end_match:
            proc_body = content[start:start+end_match.start()]
            # Heuristic: stub if only ret, or only comments/whitespace
            if re.fullmatch(r'(?s)\s*;.*|\s*ret\s*', proc_body.strip()):
                stub_procs.append(proc)
            elif 'TODO' in proc_body or 'STUB' in proc_body or 'PLACEHOLDER' in proc_body:
                stub_procs.append(proc)

    # Find procs missing ENDP
    missing_endp = [proc for proc, _ in procs if proc not in endps]

    # Find procs missing ret
    missing_ret = []
    for proc, start in procs:
        end_match = re.search(rf'{proc}\s+ENDP', content[start:])
        if end_match:
            proc_body = content[start:start+end_match.start()]
            if not re.search(r'\bret\b', proc_body):
                missing_ret.append(proc)

    return {
        'file': filepath,
        'procs': [proc for proc, _ in procs],
        'stub_procs': stub_procs,
        'missing_endp': missing_endp,
        'missing_ret': missing_ret,
        'structs': structs,
        'data': data,
        'consts': consts,
        'externs': externs,
        'todos': todos,
        'calls': calls,
        'locals': locals_,
    }


def scan_directory(root):
    results = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            ext = os.path.splitext(fname)[1].lower()
            if ext in ASM_EXTS:
                fpath = os.path.join(dirpath, fname)
                results.append(analyze_file(fpath))
    return results


def aggregate_results(results):
    all_procs = set()
    all_structs = set()
    all_data = set()
    all_consts = set()
    all_externs = set()
    all_calls = set()
    all_todos = []
    stub_procs = []
    missing_endp = []
    missing_ret = []

    for r in results:
        all_procs.update(r['procs'])
        all_structs.update(r['structs'])
        all_data.update(r['data'])
        all_consts.update(r['consts'])
        all_externs.update(r['externs'])
        all_calls.update(r['calls'])
        all_todos.extend((r['file'], t) for t in r['todos'])
        stub_procs.extend((r['file'], p) for p in r['stub_procs'])
        missing_endp.extend((r['file'], p) for p in r['missing_endp'])
        missing_ret.extend((r['file'], p) for p in r['missing_ret'])

    undefined_calls = sorted(set(all_calls) - set(all_procs) - set(all_externs))
    unused_procs = sorted(set(all_procs) - set(all_calls))

    return {
        'all_procs': sorted(all_procs),
        'all_structs': sorted(all_structs),
        'all_data': sorted(all_data),
        'all_consts': sorted(all_consts),
        'all_externs': sorted(all_externs),
        'all_calls': sorted(all_calls),
        'all_todos': all_todos,
        'stub_procs': stub_procs,
        'missing_endp': missing_endp,
        'missing_ret': missing_ret,
        'undefined_calls': undefined_calls,
        'unused_procs': unused_procs,
    }


def print_report(agg, outpath=None):
    lines = []
    lines.append('==== MASM/ASM Codebase Digest Report ====')
    lines.append(f'Total PROCs: {len(agg["all_procs"])}')
    lines.append(f'Total STRUCTs: {len(agg["all_structs"])}')
    lines.append(f'Total DATA: {len(agg["all_data"])}')
    lines.append(f'Total CONSTs: {len(agg["all_consts"])}')
    lines.append(f'Total EXTERNs: {len(agg["all_externs"])}')
    lines.append(f'Total CALLs: {len(agg["all_calls"])}')
    lines.append('')
    lines.append('--- TODOs / FIXMEs / STUBs ---')
    for f, t in agg['all_todos']:
        lines.append(f'{f}: {t.strip()}')
    lines.append('')
    lines.append('--- Stub/Empty PROCs ---')
    for f, p in agg['stub_procs']:
        lines.append(f'{f}: {p}')
    lines.append('')
    lines.append('--- PROCs missing ENDP ---')
    for f, p in agg['missing_endp']:
        lines.append(f'{f}: {p}')
    lines.append('')
    lines.append('--- PROCs missing RET ---')
    for f, p in agg['missing_ret']:
        lines.append(f'{f}: {p}')
    lines.append('')
    lines.append('--- Undefined CALL targets ---')
    for c in agg['undefined_calls']:
        lines.append(c)
    lines.append('')
    lines.append('--- Unused PROCs ---')
    for p in agg['unused_procs']:
        lines.append(p)
    lines.append('')
    if outpath:
        with open(outpath, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
    else:
        print('\n'.join(lines))


def main():
    parser = argparse.ArgumentParser(description='MASM/ASM Codebase Digest Tool')
    parser.add_argument('root', help='Root directory to scan')
    parser.add_argument('--out', help='Output file for report (optional)')
    args = parser.parse_args()

    results = scan_directory(args.root)
    agg = aggregate_results(results)
    print_report(agg, args.out)

if __name__ == '__main__':
    main()
