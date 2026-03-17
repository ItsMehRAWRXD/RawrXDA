import os
import re
import argparse
from collections import defaultdict

# Patterns for MASM/ASM features
# PROC pattern: require PROC to NOT be followed by more uppercase forming a longer word
# e.g. "g_pi PROCESS_INFORMATION" should NOT match; "MyFunc PROC FRAME" SHOULD match
PROC_PATTERN = re.compile(r'^(\w+)\s+PROC(?:\s+FRAME)?(?:\s+USES\b[^\n]*)?\s*$', re.MULTILINE | re.IGNORECASE)
ENDP_PATTERN = re.compile(r'^(\w+)\s+ENDP\s*$', re.MULTILINE | re.IGNORECASE)
STRUCT_PATTERN = re.compile(r'^(\w+)\s+STRUCT', re.MULTILINE)
DATA_PATTERN = re.compile(r'^(\w+)\s+(?:DB|DW|DD|DQ|DT|DF|RESB|RESD|RESQ|RESW|BYTE|WORD|DWORD|QWORD|TBYTE)', re.MULTILINE)
CONST_PATTERN = re.compile(r'^(\w+)\s+EQU', re.MULTILINE)
# Match EXTERN, EXTERNDEF, EXTRN, and PROTO declarations
EXTERN_PATTERN = re.compile(r'(?:EXTERN|EXTERNDEF|EXTRN)\s+(\w+)', re.MULTILINE | re.IGNORECASE)
PROTO_PATTERN = re.compile(r'^(\w+)\s+PROTO\b', re.MULTILINE | re.IGNORECASE)
# PUBLIC declarations (exported symbols, not "unused")
PUBLIC_PATTERN = re.compile(r'PUBLIC\s+(\w+)', re.MULTILINE | re.IGNORECASE)
TODO_PATTERN = re.compile(r';.*FIXME|;.*STUB|;.*PLACEHOLDER', re.IGNORECASE)
# Only match call instructions on non-comment lines
CALL_PATTERN = re.compile(r'call\s+(\w+)', re.IGNORECASE)
RET_PATTERN = re.compile(r'\bret\b', re.IGNORECASE)
LOCAL_PATTERN = re.compile(r'LOCAL\s+(\w+)', re.IGNORECASE)

# x86-64 registers - these are NOT call targets
X86_REGISTERS = {
    'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi', 'rbp', 'rsp',
    'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15',
    'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'ebp', 'esp',
    'ax', 'bx', 'cx', 'dx', 'si', 'di', 'bp', 'sp',
    'al', 'bl', 'cl', 'dl', 'ah', 'bh', 'ch', 'dh',
    'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d',
    'r8w', 'r9w', 'r10w', 'r11w', 'r12w', 'r13w', 'r14w', 'r15w',
    'r8b', 'r9b', 'r10b', 'r11b', 'r12b', 'r13b', 'r14b', 'r15b',
    'xmm0', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5', 'xmm6', 'xmm7',
    'xmm8', 'xmm9', 'xmm10', 'xmm11', 'xmm12', 'xmm13', 'xmm14', 'xmm15',
    'ymm0', 'ymm1', 'ymm2', 'ymm3', 'ymm4', 'ymm5', 'ymm6', 'ymm7',
    'ymm8', 'ymm9', 'ymm10', 'ymm11', 'ymm12', 'ymm13', 'ymm14', 'ymm15',
    'zmm0', 'zmm1', 'zmm2', 'zmm3', 'zmm4', 'zmm5', 'zmm6', 'zmm7',
    'zmm8', 'zmm9', 'zmm10', 'zmm11', 'zmm12', 'zmm13', 'zmm14', 'zmm15',
    'cs', 'ds', 'es', 'fs', 'gs', 'ss',
}

# MASM keywords/directives that are NOT call targets
MASM_KEYWORDS = {
    'proc', 'endp', 'public', 'extern', 'externdef', 'extrn', 'proto',
    'byte', 'word', 'dword', 'qword', 'tbyte', 'real4', 'real8',
    'db', 'dw', 'dd', 'dq', 'dt', 'df',
    'struct', 'ends', 'union', 'record',
    'macro', 'endm', 'local', 'invoke',
    'option', 'include', 'includelib', 'if', 'else', 'endif', 'elseif',
    'while', 'endw', 'repeat', 'until',
    'equ', 'textequ', 'typedef', 'label',
    'segment', 'assume', 'org', 'align', 'even',
    'mov', 'lea', 'push', 'pop', 'ret', 'retn', 'call',
    'add', 'sub', 'mul', 'div', 'imul', 'idiv',
    'and', 'or', 'xor', 'not', 'neg', 'shl', 'shr', 'sar', 'sal', 'rol', 'ror',
    'cmp', 'test', 'bt', 'bts', 'btr', 'btc', 'bsf', 'bsr',
    'jmp', 'je', 'jne', 'jz', 'jnz', 'jg', 'jge', 'jl', 'jle',
    'ja', 'jae', 'jb', 'jbe', 'jc', 'jnc', 'jo', 'jno', 'js', 'jns',
    'nop', 'int', 'syscall', 'sysenter', 'hlt', 'ud2',
    'movzx', 'movsx', 'movsxd', 'xchg', 'bswap',
    'cmove', 'cmovne', 'cmovz', 'cmovnz', 'cmovg', 'cmovge', 'cmovl', 'cmovle',
    'cmova', 'cmovae', 'cmovb', 'cmovbe',
    'sete', 'setne', 'setg', 'setge', 'setl', 'setle', 'seta', 'setae', 'setb', 'setbe',
    'rep', 'repe', 'repne', 'repz', 'repnz',
    'movsb', 'movsw', 'movsd', 'movsq', 'stosb', 'stosw', 'stosd', 'stosq',
    'lodsb', 'lodsw', 'lodsd', 'lodsq', 'scasb', 'scasw', 'scasd', 'scasq',
    'cpuid', 'rdtsc', 'rdtscp', 'rdrand', 'rdseed',
    'inc', 'dec', 'adc', 'sbb',
    'lock', 'xadd', 'cmpxchg', 'cmpxchg8b', 'cmpxchg16b',
    'enter', 'leave',
    'cbw', 'cwde', 'cdqe', 'cwd', 'cdq', 'cqo',
    'clc', 'stc', 'cmc', 'cld', 'std', 'cli', 'sti',
    'in', 'out', 'ins', 'outs',
    'prefetch', 'prefetchnta', 'prefetcht0', 'prefetcht1', 'prefetcht2',
    'lfence', 'sfence', 'mfence', 'pause',
    'vzeroall', 'vzeroupper',
    # SSE/AVX mnemonics that might appear after call in comments
    'near', 'far', 'short', 'ptr', 'offset', 'addr', 'flat',
}

# Known well-formed call target patterns (minimum 2 chars, must contain
# at least one uppercase letter or underscore to look like a symbol)
def is_plausible_call_target(name):
    """Filter out false positive call targets."""
    lower = name.lower()
    # Filter registers
    if lower in X86_REGISTERS:
        return False
    # Filter MASM keywords
    if lower in MASM_KEYWORDS:
        return False
    # Filter single-character identifiers
    if len(name) <= 1:
        return False
    # Filter purely lowercase short words likely from comments (< 4 chars)
    if len(name) <= 3 and name.islower():
        return False
    # Filter common English words that appear in comments
    english_noise = {
        'the', 'this', 'that', 'with', 'from', 'into', 'through',
        'for', 'was', 'are', 'not', 'but', 'has', 'had', 'have',
        'will', 'would', 'could', 'should', 'may', 'might',
        'also', 'just', 'only', 'then', 'than', 'when', 'where',
        'what', 'which', 'who', 'how', 'why', 'all', 'each',
        'every', 'some', 'any', 'most', 'more', 'less', 'few',
        'error', 'failed', 'failure', 'success', 'log', 'logger',
        'handler', 'callback', 'listener', 'factory', 'pattern',
        'function', 'option', 'single', 'local', 'stack', 'block',
        'structure', 'relative', 'original', 'external', 'exported',
        'appropriate', 'initialization', 'instruction', 'invoke',
        'hook', 'site', 'task', 'test', 'logic', 'scoring',
        'tricks', 'pushes', 'thunks', 'duration', 'deleter',
        'inference', 'kernel', 'polymorphic', 'back', 'exp',
    }
    if lower in english_noise:
        return False
    return True

# File extensions to scan
ASM_EXTS = {'.asm', '.masm', '.inc', '.s'}


def extract_calls_no_comments(content):
    """Extract call targets only from non-comment portions of lines."""
    calls = []
    for line in content.splitlines():
        # Strip inline comments (everything after first ;)
        code_part = line.split(';')[0]
        for m in CALL_PATTERN.finditer(code_part):
            target = m.group(1)
            if is_plausible_call_target(target):
                calls.append(target)
    return calls


def analyze_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    procs = [(m.group(1), m.start()) for m in PROC_PATTERN.finditer(content)]
    endps = set(m.group(1) for m in ENDP_PATTERN.finditer(content))
    structs = [m.group(1) for m in STRUCT_PATTERN.finditer(content)]
    data = [m.group(1) for m in DATA_PATTERN.finditer(content)]
    consts = [m.group(1) for m in CONST_PATTERN.finditer(content)]
    externs = [m.group(1) for m in EXTERN_PATTERN.finditer(content)]
    protos = [m.group(1) for m in PROTO_PATTERN.finditer(content)]
    publics = [m.group(1) for m in PUBLIC_PATTERN.finditer(content)]
    todos = [m.group(0) for m in TODO_PATTERN.finditer(content)]
    calls = extract_calls_no_comments(content)
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
            elif 'STUB' in proc_body or 'PLACEHOLDER' in proc_body:
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
        'protos': protos,
        'publics': publics,
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
    lines.append('--- FIXMEs / STUBs ---')
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
