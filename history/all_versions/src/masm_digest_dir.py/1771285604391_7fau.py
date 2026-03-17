#!/usr/bin/env python3
"""
RawrXD MASM/ASM Codebase Digest Tool
Production-grade static analysis for x64 MASM assembly codebases.
Scans all .asm/.inc files recursively and produces a comprehensive report:
  - PROC definitions with stub detection
  - Missing ENDP / missing RET analysis
  - STRUCT, DATA, EQU, EXTERN inventories
  - Undefined CALL target detection (with false-positive filtering)
  - Unused PROC detection
  - FIXME / STUB / PLACEHOLDER comment tracking
"""
import os
import re
import argparse
from collections import defaultdict

# ============================================================================
# Compiled regex patterns
# ============================================================================
PROC_PATTERN   = re.compile(r'^(\w+)\s+PROC\b', re.MULTILINE | re.IGNORECASE)
ENDP_PATTERN   = re.compile(r'^(\w+)\s+ENDP\b', re.MULTILINE | re.IGNORECASE)
STRUCT_PATTERN = re.compile(r'^(\w+)\s+STRUCT\b', re.MULTILINE | re.IGNORECASE)
DATA_PATTERN   = re.compile(
    r'^(\w+)\s+(?:DB|DW|DD|DQ|DT|DF|RESB|RESD|RESQ|RESW|BYTE|WORD|DWORD|QWORD|TBYTE)\b',
    re.MULTILINE | re.IGNORECASE
)
CONST_PATTERN  = re.compile(r'^(\w+)\s+EQU\b', re.MULTILINE | re.IGNORECASE)
EXTERN_PATTERN = re.compile(r'EXTERN\w*\s+(\w+)', re.MULTILINE | re.IGNORECASE)
ISSUE_PATTERN  = re.compile(r';.*(?:FIXME|STUB|PLACEHOLDER)\b', re.IGNORECASE)
LOCAL_PATTERN  = re.compile(r'LOCAL\s+(\w+)', re.IGNORECASE)

# File extensions to scan
ASM_EXTS = {'.asm', '.masm', '.inc', '.s'}

# ============================================================================
# Known symbols that should NEVER appear as "undefined call targets"
# ============================================================================

# MASM directives / keywords / pseudo-ops
_MASM_KEYWORDS = {
    'proc', 'endp', 'proto', 'invoke', 'extern', 'extrn', 'externdef',
    'public', 'include', 'includelib', 'option', 'end', 'segment', 'ends',
    'struct', 'union', 'typedef', 'textequ', 'macro', 'endm', 'local',
    'if', 'else', 'elseif', 'endif', 'ifdef', 'ifndef', 'while', 'endw',
    'repeat', 'for', 'forc', 'irp', 'irpc', 'exitm', 'purge',
    'db', 'dw', 'dd', 'dq', 'dt', 'df', 'byte', 'word', 'dword', 'qword',
    'tbyte', 'real4', 'real8', 'real10', 'xmmword', 'ymmword', 'zmmword',
    'ptr', 'offset', 'addr', 'flat', 'near', 'far', 'label', 'align',
    '.code', '.data', '.data?', '.const', '.stack', '.model', '.386', '.686',
}

# x64 register names (including sub-registers)
_REGISTERS = {
    'rax', 'rbx', 'rcx', 'rdx', 'rsi', 'rdi', 'rsp', 'rbp',
    'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15',
    'eax', 'ebx', 'ecx', 'edx', 'esi', 'edi', 'esp', 'ebp',
    'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d',
    'ax', 'bx', 'cx', 'dx', 'si', 'di', 'sp', 'bp',
    'al', 'bl', 'cl', 'dl', 'ah', 'bh', 'ch', 'dh',
    'sil', 'dil', 'spl', 'bpl',
    'r8b', 'r9b', 'r10b', 'r11b', 'r12b', 'r13b', 'r14b', 'r15b',
    'r8w', 'r9w', 'r10w', 'r11w', 'r12w', 'r13w', 'r14w', 'r15w',
    'xmm0', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5', 'xmm6', 'xmm7',
    'xmm8', 'xmm9', 'xmm10', 'xmm11', 'xmm12', 'xmm13', 'xmm14', 'xmm15',
    'ymm0', 'ymm1', 'ymm2', 'ymm3', 'ymm4', 'ymm5', 'ymm6', 'ymm7',
    'zmm0', 'zmm1', 'zmm2', 'zmm3', 'zmm4', 'zmm5', 'zmm6', 'zmm7',
    'k0', 'k1', 'k2', 'k3', 'k4', 'k5', 'k6', 'k7',
    'cs', 'ds', 'es', 'fs', 'gs', 'ss',
}

# x86/x64 instruction mnemonics (commonly confused with symbols)
_INSTRUCTIONS = {
    'mov', 'add', 'sub', 'mul', 'div', 'inc', 'dec', 'neg', 'not',
    'and', 'or', 'xor', 'shl', 'shr', 'sar', 'sal', 'rol', 'ror',
    'push', 'pop', 'pushes', 'ret', 'retn', 'retf', 'iret',
    'cmp', 'test', 'bt', 'bts', 'btr', 'btc', 'bsf', 'bsr',
    'jmp', 'je', 'jne', 'jz', 'jnz', 'ja', 'jb', 'jae', 'jbe',
    'jg', 'jge', 'jl', 'jle', 'jo', 'jno', 'js', 'jns',
    'loop', 'loope', 'loopne', 'jecxz', 'jrcxz',
    'lea', 'nop', 'int', 'syscall', 'sysenter', 'cpuid', 'rdtsc',
    'movzx', 'movsx', 'movsxd', 'cmov', 'cmovz', 'cmovnz',
    'rep', 'repe', 'repne', 'stosb', 'stosw', 'stosd', 'stosq',
    'movsb', 'movsw', 'movsd', 'movsq', 'scasb', 'cmpsb',
    'lock', 'xchg', 'cmpxchg', 'xadd', 'bswap',
    'imul', 'idiv', 'cbw', 'cwde', 'cdqe', 'cwd', 'cdq', 'cqo',
    'sete', 'setne', 'seta', 'setb', 'setg', 'setl',
    'tzcnt', 'lzcnt', 'popcnt', 'pdep', 'pext',
    'rdrand', 'rdseed', 'pause', 'mfence', 'sfence', 'lfence',
    'clflush', 'clflushopt', 'prefetcht0', 'prefetcht1', 'prefetchnta',
    'vmovdqu64', 'vpbroadcastb', 'vpcmpeqb', 'korq', 'kortestq', 'kmovq',
    'vzeroupper', 'vzeroall',
    'exp', 'log', 'single', 'real',
}

# Well-known Win32/CRT APIs (resolved at link time, not "undefined")
_KNOWN_WIN32_APIS = {
    'GetStdHandle', 'WriteFile', 'ReadFile', 'CreateFileA', 'CreateFileW',
    'CloseHandle', 'ExitProcess', 'GetLastError', 'SetLastError',
    'VirtualAlloc', 'VirtualFree', 'VirtualProtect', 'VirtualQuery',
    'HeapAlloc', 'HeapFree', 'HeapReAlloc', 'HeapCreate', 'HeapDestroy',
    'GetProcessHeap', 'LocalAlloc', 'LocalFree', 'GlobalAlloc', 'GlobalFree',
    'LoadLibraryA', 'LoadLibraryW', 'GetProcAddress', 'FreeLibrary',
    'GetModuleHandleA', 'GetModuleHandleW', 'GetModuleFileNameA',
    'CreateThread', 'ExitThread', 'GetCurrentThread', 'GetCurrentThreadId',
    'SetThreadPriority', 'SuspendThread', 'ResumeThread', 'TerminateThread',
    'WaitForSingleObject', 'WaitForMultipleObjects', 'Sleep', 'SleepEx',
    'CreateEventA', 'CreateEventW', 'SetEvent', 'ResetEvent',
    'CreateMutexA', 'CreateMutexW', 'ReleaseMutex',
    'CreateSemaphoreA', 'CreateSemaphoreW', 'CreateSemaphore', 'ReleaseSemaphore',
    'EnterCriticalSection', 'LeaveCriticalSection',
    'InitializeCriticalSection', 'DeleteCriticalSection',
    'InitializeSRWLock', 'AcquireSRWLockExclusive', 'ReleaseSRWLockExclusive',
    'AcquireSRWLockShared', 'ReleaseSRWLockShared',
    'TlsAlloc', 'TlsFree', 'TlsGetValue', 'TlsSetValue',
    'QueryPerformanceCounter', 'QueryPerformanceFrequency',
    'GetTickCount', 'GetTickCount64', 'GetSystemTimeAsFileTime',
    'GetSystemTimePreciseAsFileTime',
    'WriteConsoleA', 'WriteConsoleW', 'WriteConsole',
    'ReadConsoleA', 'ReadConsoleW', 'ReadConsoleInputA',
    'GetConsoleScreenBufferInfo', 'SetConsoleCursorPosition', 'SetConsoleMode',
    'GetCommandLineA', 'GetCommandLineW', 'CommandLineToArgvW',
    'CreateFileMappingA', 'CreateFileMappingW', 'MapViewOfFile', 'UnmapViewOfFile',
    'FlushViewOfFile', 'GetFileSizeEx', 'SetFilePointerEx',
    'CreateDirectory', 'CreateDirectoryW', 'RemoveDirectory',
    'FindFirstFileA', 'FindFirstFileW', 'FindNextFileA', 'FindNextFileW', 'FindClose',
    'GetFileAttributesA', 'GetFileAttributesW', 'SetFileAttributesA',
    'DeleteFileA', 'DeleteFileW', 'MoveFileA', 'MoveFileW', 'CopyFileA',
    'GetOverlappedResult', 'CreateIoCompletionPort', 'GetQueuedCompletionStatus',
    'PostQueuedCompletionStatus',
    'CreateProcess', 'CreateProcessA', 'CreateProcessW',
    'GetExitCodeProcess', 'TerminateProcess', 'GetCurrentProcess',
    'GetCurrentProcessId', 'IsWow64Process',
    'MessageBoxA', 'MessageBoxW',
    'RegisterClassA', 'RegisterClassW', 'RegisterClassExA', 'RegisterClassExW',
    'CreateWindowExA', 'CreateWindowExW', 'ShowWindow', 'UpdateWindow',
    'DefWindowProcA', 'DefWindowProcW', 'DestroyWindow', 'MoveWindow',
    'GetMessageA', 'GetMessageW', 'PeekMessageA', 'PeekMessageW',
    'TranslateMessage', 'DispatchMessageA', 'DispatchMessageW',
    'PostQuitMessage', 'SendMessageA', 'SendMessageW',
    'InvalidateRect', 'GetClientRect', 'BeginPaint', 'EndPaint',
    'SetBkMode', 'SetTimer', 'PatBlt', 'DeleteObject',
    'GetTextMetricsA', 'SetWindowTextA', 'SetLayeredWindowAttributes',
    'LoadCursorA', 'LoadIconA', 'GetSysColor', 'GetSystemMetrics',
    'CreateMenu', 'CreatePopupMenu', 'AppendMenuA', 'SetMenu',
    'GetOpenFileNameA', 'GetTempPath', 'SearchPathW',
    'InitCommonControlsEx', 'RegisterClass',
    'SetErrorMode', 'SetUnhandledExceptionFilter',
    'AddVectoredExceptionHandler', 'RemoveVectoredExceptionHandler',
    'IsDebuggerPresent', 'CheckRemoteDebuggerPresent', 'OutputDebugStringA',
    'MiniDumpWriteDump',
    'InternetOpenW', 'InternetConnectW', 'InternetOpenUrlA',
    'InternetReadFile', 'InternetCloseHandle', 'InternetSetOptionW',
    'InternetQueryDataAvailable',
    'HttpOpenRequestW', 'HttpSendRequestW', 'HttpQueryInfoW',
    'HttpAddRequestHeadersW',
    'WinHttpOpen', 'WinHttpConnect', 'WinHttpOpenRequest', 'WinHttpCloseHandle',
    'HttpInitialize', 'HttpTerminate', 'HttpCreateHttpHandle',
    'HttpAddUrl', 'HttpRemoveUrl', 'HttpReceiveHttpRequest',
    'HttpReceiveRequestEntityBody', 'HttpSendHttpResponse',
    'CoInitializeEx', 'CoUninitialize',
    'CreateCoreWebView2EnvironmentWithOptions',
    'D2D1CreateFactory', 'D3D11CreateDevice', 'DWriteCreateFactory',
    'CreateDXGIFactory1',
    'PathCombineW', 'PathFileExistsW', 'PathRemoveFileSpecW',
    'GetEnvironmentVariable', 'GetEnvironmentVariableA', 'GetEnvironmentVariableW',
    'MultiByteToWideChar', 'WideCharToMultiByte',
    'RegOpenKeyExA', 'RegOpenKeyExW', 'RegQueryValueExA', 'RegQueryValueExW',
    'RegSetValueExA', 'RegSetValueExW', 'RegCloseKey',
    'RegCreateKeyExW', 'RegDeleteKeyA',
    'GetModuleFileNameW',
    'SYS_NtAllocateVirtualMemory',
    # CRT functions
    'printf', 'sprintf', 'snprintf', 'fprintf', 'sscanf',
    'malloc', 'calloc', 'realloc', 'free',
    'memcpy', 'memset', 'memmove', 'memcmp',
    'strlen', 'strcpy', 'strncpy', 'strcat', 'strcmp', 'strncmp', 'stricmp',
    'strchr', 'strstr', 'strtol', 'strtoul', 'strtok',
    'atoi', 'atol', 'itoa', 'ltoa',
    'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell', 'fflush',
    'wsprintf', 'wsprintfA', 'wsprintfW',
    'lstrcmpi', 'lstrcpyn', 'lstrcpynA', 'lstrcpynW', 'lstrlenA',
    'wcstombs', 'mbstowcs',
    '_set_invalid_parameter_handler', '_set_purecall_handler',
    'atexit', 'exit', 'abort',
    'strncpy_s', 'sprintf_s',
    'RtlSecureZeroMemory',
    # __imp_ prefixed imports
    '__imp_CloseHandle', '__imp_CreateFileA', '__imp_CreateFileMappingA',
    '__imp_GetFileSizeEx', '__imp_GetStdHandle', '__imp_MapViewOfFile',
    '__imp_UnmapViewOfFile', '__imp_WriteFile',
    # Vulkan loader symbols
    'vkGetInstanceProcAddr', 'vkGetDeviceProcAddr',
    'vkCreateInstance', 'vkDestroyInstance', 'vkEnumeratePhysicalDevices',
    'vkGetPhysicalDeviceProperties', 'vkGetPhysicalDeviceMemoryProperties',
    'vkCreateDevice', 'vkDestroyDevice', 'vkGetDeviceQueue',
    'vkQueueSubmit', 'vkQueueBindSparse',
    'vkCreateCommandPool', 'vkAllocateCommandBuffers',
    'vkCmdBindPipeline', 'vkCmdBindDescriptorSets', 'vkCmdDispatch',
    'vkCmdPipelineBarrier', 'vkCmdPushConstants',
    'vkSetPhysicalDeviceLimits', 'vkSetPhysicalDeviceSparseProperties',
}

# English words / noise that regex picks up from comments
_NOISE_WORDS = {
    'a', 'an', 'the', 'is', 'it', 'in', 'on', 'to', 'of', 'at', 'by',
    'if', 'or', 'be', 'do', 'no', 'so', 'up', 'as', 'we', 'he',
    'was', 'for', 'are', 'but', 'not', 'you', 'all', 'can', 'had',
    'her', 'one', 'our', 'out', 'has', 'its', 'let', 'may', 'new',
    'now', 'old', 'see', 'way', 'who', 'did', 'get', 'got', 'him',
    'his', 'how', 'its', 'may', 'own', 'say', 'she', 'too', 'use',
    'from', 'this', 'that', 'with', 'have', 'will', 'each', 'make',
    'like', 'long', 'look', 'many', 'them', 'then', 'some', 'what',
    'when', 'come', 'into', 'back', 'also', 'been', 'more', 'over',
    'such', 'take', 'than', 'very', 'most', 'about', 'after', 'being',
    'through', 'appropriate', 'single', 'structure', 'relative',
    'failed', 'failure', 'error', 'exported', 'external',
    'function', 'handler', 'hook', 'callback', 'listener', 'factory',
    'deleter', 'initializer', 'instruction', 'kernel', 'logic',
    'logger', 'site', 'task', 'tricks', 'pattern', 'scoring',
    'duration', 'block', 'stack', 'option', 'original', 'local',
    'activate', 'deactivate', 'cleanup', 'inference', 'thunks',
    'polymorphic',
    # Other noise tokens that appear in the old report
    'AELF', 'AMACH', 'APE', 'GGML', 'Win32',
    'kernel32', 'Read', 'Imports', 'Inference',
    'Camellia', 'Main', 'Fmt',
}

# Build a single set for fast O(1) lookup (all lowercased for case-insensitive)
_FALSE_POSITIVE_SET = set()
for s in _MASM_KEYWORDS:
    _FALSE_POSITIVE_SET.add(s.lower())
for s in _REGISTERS:
    _FALSE_POSITIVE_SET.add(s.lower())
for s in _INSTRUCTIONS:
    _FALSE_POSITIVE_SET.add(s.lower())
for s in _KNOWN_WIN32_APIS:
    _FALSE_POSITIVE_SET.add(s.lower())
for s in _NOISE_WORDS:
    _FALSE_POSITIVE_SET.add(s.lower())


def _is_false_positive(symbol):
    """Return True if symbol should be excluded from undefined call results."""
    low = symbol.lower()
    if low in _FALSE_POSITIVE_SET:
        return True
    # Filter single-char and 2-char symbols (almost always noise)
    if len(symbol) <= 2:
        return True
    # Filter __imp_ prefixed (linker import thunks)
    if symbol.startswith('__imp_'):
        return True
    # Filter g_vk prefixed (Vulkan function pointers)
    if symbol.startswith('g_vk'):
        return True
    # Filter pNt / pIs prefixed (function pointers loaded at runtime)
    if symbol.startswith('pNt') or symbol.startswith('pIs'):
        return True
    return False


def _strip_comment(line):
    """Remove comment portion from a MASM line (handles quoted strings)."""
    in_quote = False
    quote_char = None
    for i, ch in enumerate(line):
        if ch in ('"', "'") and not in_quote:
            in_quote = True
            quote_char = ch
        elif ch == quote_char and in_quote:
            in_quote = False
        elif ch == ';' and not in_quote:
            return line[:i]
    return line


def _extract_calls(content):
    """Extract CALL targets only from non-comment code lines."""
    calls = set()
    call_re = re.compile(r'\bcall\s+(\w+)', re.IGNORECASE)
    for line in content.splitlines():
        code = _strip_comment(line).strip()
        if not code:
            continue
        for m in call_re.finditer(code):
            sym = m.group(1)
            if not _is_false_positive(sym):
                calls.add(sym)
    return calls


def analyze_file(filepath):
    """Analyze a single .asm file for PROCs, structs, calls, etc."""
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()

    procs = [(m.group(1), m.start()) for m in PROC_PATTERN.finditer(content)]
    endps = set(m.group(1) for m in ENDP_PATTERN.finditer(content))
    structs = [m.group(1) for m in STRUCT_PATTERN.finditer(content)]
    data = [m.group(1) for m in DATA_PATTERN.finditer(content)]
    consts = [m.group(1) for m in CONST_PATTERN.finditer(content)]
    externs = [m.group(1) for m in EXTERN_PATTERN.finditer(content)]
    todos = [m.group(0) for m in ISSUE_PATTERN.finditer(content)]
    calls = _extract_calls(content)
    locals_ = [m.group(1) for m in LOCAL_PATTERN.finditer(content)]

    # ---- Stub/Empty PROC detection ----
    stub_procs = []
    for proc, start in procs:
        end_match = re.search(rf'{re.escape(proc)}\s+ENDP', content[start:], re.IGNORECASE)
        if end_match:
            proc_body = content[start:start + end_match.start()]
            # Strip the PROC declaration line itself
            first_nl = proc_body.find('\n')
            if first_nl >= 0:
                proc_body = proc_body[first_nl:]
            # Remove comments and empty lines
            code_lines = []
            for ln in proc_body.splitlines():
                stripped = _strip_comment(ln).strip()
                if stripped and not stripped.upper().startswith((
                    'LOCAL', '.PUSHREG', '.ALLOCSTACK', '.ENDPROLOG',
                    '.SAVEXMM128', 'PUSH ', 'SUB RSP',
                )):
                    code_lines.append(stripped)
            # If body is ONLY ret/push/pop/sub/add rsp or empty -> stub
            real_code = [l for l in code_lines if l.lower() not in (
                'ret', 'retn', 'pop rbx', 'pop rsi', 'pop rdi',
                'pop r12', 'pop r13', 'pop r14', 'pop r15',
                'add rsp, 40h', 'add rsp, 48', 'add rsp, 56',
            )]
            if not real_code:
                stub_procs.append(proc)

    # ---- Missing ENDP ----
    missing_endp = [proc for proc, _ in procs if proc not in endps]

    # ---- Missing RET (accounts for ExitProcess / jmp epilogues) ----
    missing_ret = []
    for proc, start in procs:
        end_match = re.search(rf'{re.escape(proc)}\s+ENDP', content[start:], re.IGNORECASE)
        if end_match:
            proc_body = content[start:start + end_match.start()]
            has_ret = bool(re.search(r'\bret\b', proc_body, re.IGNORECASE))
            has_exit = bool(re.search(
                r'\b(ExitProcess|TerminateProcess|ExitThread)\b',
                proc_body, re.IGNORECASE
            ))
            has_jmp = bool(re.search(r'\bjmp\s+\w+', proc_body, re.IGNORECASE))
            if not has_ret and not has_exit and not has_jmp:
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
    """Walk directory tree and analyze all assembly files."""
    results = []
    for dirpath, _, filenames in os.walk(root):
        for fname in filenames:
            ext = os.path.splitext(fname)[1].lower()
            if ext in ASM_EXTS:
                fpath = os.path.join(dirpath, fname)
                results.append(analyze_file(fpath))
    return results


def aggregate_results(results):
    """Merge per-file results into a codebase-wide report."""
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

    # Undefined = called but not defined as PROC and not declared EXTERN
    known_symbols = set(all_procs) | set(all_externs) | set(all_data) | set(all_consts)
    known_lower = {s.lower() for s in known_symbols}
    undefined_calls = sorted(
        c for c in all_calls
        if c.lower() not in known_lower and not _is_false_positive(c)
    )

    # Unused = defined as PROC but never called anywhere
    called_lower = {c.lower() for c in all_calls}
    entry_points = {
        'main', 'maincrtstart', 'winmaincrtstart', 'winmaincrtstartup',
        'maincrtstartup', 'dllmain', '_dllmaincrtstartup', 'wndproc',
        'realwndproc', 'windowproc', 'serviceentry', 'servicemain',
        'dllentrypoint', 'testentry', 'sidecarentry', 'agentbridgeentry',
        'kernelentry_seh', 'pebmain', 'sidecarminimalentry',
        'wndproc_main', 'wndproc_editor', 'wndproc_command',
        'workerthread', 'workerthreadproc', 'worker_thread_proc',
    }
    unused_procs = sorted(
        p for p in all_procs
        if p.lower() not in called_lower and p.lower() not in entry_points
    )

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
    """Format and output the digest report."""
    lines = []
    lines.append('=' * 72)
    lines.append('  RawrXD MASM/ASM Codebase Digest Report')
    lines.append('  Production Static Analysis')
    lines.append('=' * 72)
    lines.append('')
    lines.append(f'  Total PROCs:   {len(agg["all_procs"]):>6}')
    lines.append(f'  Total STRUCTs: {len(agg["all_structs"]):>6}')
    lines.append(f'  Total DATA:    {len(agg["all_data"]):>6}')
    lines.append(f'  Total CONSTs:  {len(agg["all_consts"]):>6}')
    lines.append(f'  Total EXTERNs: {len(agg["all_externs"]):>6}')
    lines.append(f'  Total CALLs:   {len(agg["all_calls"]):>6}')
    lines.append('')

    # Issues section
    n_stubs = len(agg['stub_procs'])
    n_endp = len(agg['missing_endp'])
    n_ret = len(agg['missing_ret'])
    n_undef = len(agg['undefined_calls'])
    n_unused = len(agg['unused_procs'])
    n_todos = len(agg['all_todos'])

    lines.append(f'  Stub/Empty PROCs:    {n_stubs:>4}')
    lines.append(f'  Missing ENDP:        {n_endp:>4}')
    lines.append(f'  Missing RET:         {n_ret:>4}')
    lines.append(f'  Undefined CALLs:     {n_undef:>4}')
    lines.append(f'  Unused PROCs:        {n_unused:>4}')
    lines.append(f'  FIXME/STUB comments: {n_todos:>4}')
    lines.append('')
    lines.append('-' * 72)

    if agg['all_todos']:
        lines.append('')
        lines.append('--- FIXMEs / STUBs ---')
        for f, t in agg['all_todos']:
            lines.append(f'  {f}: {t.strip()}')

    if agg['stub_procs']:
        lines.append('')
        lines.append('--- Stub/Empty PROCs ---')
        for f, p in agg['stub_procs']:
            lines.append(f'  {f}: {p}')

    if agg['missing_endp']:
        lines.append('')
        lines.append('--- PROCs missing ENDP ---')
        for f, p in agg['missing_endp']:
            lines.append(f'  {f}: {p}')

    if agg['missing_ret']:
        lines.append('')
        lines.append('--- PROCs missing RET ---')
        for f, p in agg['missing_ret']:
            lines.append(f'  {f}: {p}')

    if agg['undefined_calls']:
        lines.append('')
        lines.append('--- Undefined CALL targets (internal) ---')
        for c in agg['undefined_calls']:
            lines.append(f'  {c}')

    if agg['unused_procs']:
        lines.append('')
        lines.append('--- Unused PROCs (no callers found) ---')
        for p in agg['unused_procs']:
            lines.append(f'  {p}')

    lines.append('')
    lines.append('=' * 72)
    lines.append('  End of Report')
    lines.append('=' * 72)
    lines.append('')

    report = '\n'.join(lines)
    if outpath:
        with open(outpath, 'w', encoding='utf-8') as f:
            f.write(report)
        print(f'Report written to {outpath}')
        print(f'  {n_stubs} stubs, {n_endp} missing ENDP, {n_ret} missing RET,')
        print(f'  {n_undef} undefined calls, {n_unused} unused PROCs')
    else:
        print(report)


def main():
    parser = argparse.ArgumentParser(
        description='RawrXD MASM/ASM Codebase Digest Tool - Production Static Analysis'
    )
    parser.add_argument('root', help='Root directory to scan')
    parser.add_argument('--out', help='Output file for report (optional)')
    args = parser.parse_args()

    results = scan_directory(args.root)
    agg = aggregate_results(results)
    print_report(agg, args.out)


if __name__ == '__main__':
    main()
