#!/usr/bin/env python3
"""
RawrXD MASM/ASM Codebase Digest Tool
=====================================
Production-grade static analysis for x86-64 MASM assembly codebases.

Scans all .asm/.inc/.masm/.s files and produces a comprehensive report:
  - PROC definitions with stub/real classification
  - STRUCT definitions
  - DATA labels, EQU constants
  - EXTERN/EXTERNDEF/PROTO/PUBLIC declarations
  - Undefined CALL targets (with rigorous false-positive filtering)
  - Unused PROCs (excluding PUBLIC exports and entry points)
  - Missing ENDP / missing RET detection
  - TODO/FIXME/STUB/PLACEHOLDER/BLOCKED/HIDDEN comment tracking

Handles three MASM import resolution patterns:
  A) EXTERN Symbol : PROC        -> direct call Symbol
  B) EXTERNDEF __imp_X : QWORD   -> call __imp_X (IAT-direct)
  C) include kernel32.inc        -> implicit Win32 API resolution

Usage:
  python masm_digest_dir.py D:\\RawrXD\\src --out report.txt
"""

import os
import re
import sys
import argparse
from collections import defaultdict

# ============================================================================
#  REGEX PATTERNS
# ============================================================================

# PROC: "MyFunc PROC" or "MyFunc PROC FRAME" or "MyFunc PROC FRAME USES ..."
# Anchored to start-of-line. Requires PROC as exact keyword boundary (\b)
# to prevent matching "g_pi PROCESS_INFORMATION <>".
PROC_PATTERN = re.compile(
    r'^(\w+)\s+PROC\b(?:\s+(?:FRAME|NEAR|FAR|PRIVATE|PUBLIC))?'
    r'(?:\s+USES\b[^\n]*)?'
    r'\s*(?:;.*)?$',
    re.MULTILINE | re.IGNORECASE
)

# Note: NOT anchored to ^  —  handles single-line compacted MASM
# where "R proc; ...; R endp" is all on one line.
ENDP_PATTERN = re.compile(
    r'\b(\w+)\s+ENDP\b',
    re.IGNORECASE
)

STRUCT_PATTERN = re.compile(r'^(\w+)\s+STRUCT\b', re.MULTILINE | re.IGNORECASE)

DATA_PATTERN = re.compile(
    r'^(\w+)\s+(?:DB|DW|DD|DQ|DT|DF|REAL4|REAL8|LABEL|'
    r'BYTE|WORD|DWORD|QWORD|TBYTE|OWORD|XMMWORD|YMMWORD)\b',
    re.MULTILINE | re.IGNORECASE
)

CONST_PATTERN = re.compile(r'^(\w+)\s+EQU\b', re.MULTILINE | re.IGNORECASE)

# EXTERN/EXTERNDEF/EXTRN: "EXTERN sym:TYPE" — captures the symbol name
EXTERN_PATTERN = re.compile(
    r'(?:EXTERN|EXTERNDEF|EXTRN)\s+(\w+)\s*:',
    re.MULTILINE | re.IGNORECASE
)

PROTO_PATTERN = re.compile(r'^(\w+)\s+PROTO\b', re.MULTILINE | re.IGNORECASE)

PUBLIC_PATTERN = re.compile(r'\bPUBLIC\s+(\w+)', re.MULTILINE | re.IGNORECASE)

INCLUDE_PATTERN = re.compile(
    r'^\s*include\s+(\S+)',
    re.MULTILINE | re.IGNORECASE
)

INCLUDELIB_PATTERN = re.compile(
    r'^\s*includelib\s+(\S+)',
    re.MULTILINE | re.IGNORECASE
)

CALL_PATTERN = re.compile(r'\bcall\s+(\w+)', re.IGNORECASE)
INVOKE_PATTERN = re.compile(r'\binvoke\s+(\w+)', re.IGNORECASE)

TODO_PATTERN = re.compile(
    r';.*\b(?:TODO|FIXME|STUB|PLACEHOLDER|BLOCKED|HIDDEN|HACK|XXX)\b',
    re.IGNORECASE
)

# ============================================================================
#  FALSE POSITIVE FILTERING
# ============================================================================

# Complete x86/x86-64 register set
_GP = {'rax','rbx','rcx','rdx','rsi','rdi','rbp','rsp',
       'eax','ebx','ecx','edx','esi','edi','ebp','esp',
       'ax','bx','cx','dx','si','di','bp','sp',
       'al','bl','cl','dl','ah','bh','ch','dh','sil','dil','bpl','spl'}
_EXT = {f'r{n}{s}' for n in range(8, 16) for s in ('', 'd', 'w', 'b')}
_VEC = {f'{p}{n}' for p in ('xmm', 'ymm', 'zmm') for n in range(32)}
_SEG = {'cs', 'ds', 'es', 'fs', 'gs', 'ss'}
_CTRL = {f'cr{n}' for n in range(16)} | {f'dr{n}' for n in range(8)}
X86_REGISTERS = _GP | _EXT | _VEC | _SEG | _CTRL

# Comprehensive MASM keywords/directives/instructions — anything that isn't
# a function name. This prevents "call" operands from being called undefined.
MASM_KEYWORDS = {
    # Assembler directives
    'proc', 'endp', 'public', 'extern', 'externdef', 'extrn', 'proto',
    'byte', 'word', 'dword', 'qword', 'tbyte', 'real4', 'real8',
    'oword', 'xmmword', 'ymmword',
    'db', 'dw', 'dd', 'dq', 'dt', 'df',
    'struct', 'ends', 'union', 'record', 'typedef', 'textequ',
    'catstr', 'substr', 'sizestr',
    'macro', 'endm', 'exitm', 'purge', 'local',
    'option', 'include', 'includelib',
    '.if', '.else', '.elseif', '.endif',
    '.while', '.endw', '.repeat', '.until', '.break', '.continue',
    'if', 'else', 'endif', 'elseif', 'ifdef', 'ifndef',
    'elseifdef', 'elseifndef',
    'while', 'endw', 'repeat', 'until', 'for', 'forc', 'irp', 'irpc',
    'equ', 'label', 'segment', 'assume', 'org', 'align', 'even', 'end',
    '.model', '.stack', '.data', '.data?', '.const', '.code',
    'invoke', 'addr', 'offset', 'ptr', 'flat', 'near', 'far', 'short',
    # --- x86/x86-64 instructions (comprehensive) ---
    'mov', 'lea', 'push', 'pop', 'ret', 'retn', 'retf', 'call',
    'enter', 'leave',
    'add', 'sub', 'mul', 'div', 'imul', 'idiv', 'inc', 'dec',
    'neg', 'adc', 'sbb',
    'and', 'or', 'xor', 'not', 'shl', 'shr', 'sar', 'sal',
    'rol', 'ror', 'rcl', 'rcr', 'shld', 'shrd',
    'cmp', 'test', 'bt', 'bts', 'btr', 'btc', 'bsf', 'bsr', 'bswap',
    'jmp', 'je', 'jne', 'jz', 'jnz', 'jg', 'jge', 'jl', 'jle',
    'ja', 'jae', 'jb', 'jbe', 'jc', 'jnc', 'jo', 'jno', 'js', 'jns',
    'jp', 'jnp', 'jecxz', 'jrcxz',
    'loop', 'loope', 'loopne', 'loopz', 'loopnz',
    'nop', 'int', 'int3', 'into', 'iret', 'iretd', 'iretq',
    'syscall', 'sysret', 'sysenter', 'sysexit', 'hlt', 'ud2',
    'cpuid', 'rdtsc', 'rdtscp', 'rdpmc', 'rdrand', 'rdseed',
    'rdmsr', 'wrmsr',
    'movzx', 'movsx', 'movsxd', 'xchg',
    'cmpxchg', 'cmpxchg8b', 'cmpxchg16b', 'xadd', 'lock',
    'cmove', 'cmovne', 'cmovz', 'cmovnz', 'cmovg', 'cmovge',
    'cmovl', 'cmovle', 'cmova', 'cmovae', 'cmovb', 'cmovbe',
    'cmovs', 'cmovns', 'cmovo', 'cmovno', 'cmovp', 'cmovnp',
    'cmovc', 'cmovnc',
    'sete', 'setne', 'setg', 'setge', 'setl', 'setle',
    'seta', 'setae', 'setb', 'setbe',
    'sets', 'setns', 'seto', 'setno', 'setp', 'setnp',
    'setc', 'setnc', 'setz', 'setnz',
    'rep', 'repe', 'repne', 'repz', 'repnz',
    'movsb', 'movsw', 'movsd', 'movsq',
    'stosb', 'stosw', 'stosd', 'stosq',
    'lodsb', 'lodsw', 'lodsd', 'lodsq',
    'scasb', 'scasw', 'scasd', 'scasq',
    'cmpsb', 'cmpsw', 'cmpsd', 'cmpsq',
    'cbw', 'cwde', 'cdqe', 'cwd', 'cdq', 'cqo',
    'clc', 'stc', 'cmc', 'cld', 'std', 'cli', 'sti',
    'clflush', 'clflushopt',
    'in', 'out', 'insb', 'insw', 'insd', 'outsb', 'outsw', 'outsd',
    'prefetch', 'prefetchnta', 'prefetcht0', 'prefetcht1',
    'prefetcht2', 'prefetchw',
    'lfence', 'sfence', 'mfence', 'pause', 'serialize',
    'vzeroall', 'vzeroupper',
    'xlat', 'xlatb', 'lahf', 'sahf',
    'pushf', 'pushfd', 'pushfq', 'popf', 'popfd', 'popfq',
    'stmxcsr', 'ldmxcsr', 'fxsave', 'fxrstor',
    'xsave', 'xrstor', 'xgetbv', 'xsetbv',
    'wait', 'fwait', 'fnop',
    'popcnt', 'lzcnt', 'tzcnt', 'andn', 'bzhi', 'pdep', 'pext',
    'blsi', 'blsr', 'blsmsk',
    'endbr64', 'endbr32',
    # FPU
    'fld', 'fst', 'fstp', 'fild', 'fist', 'fistp',
    'fadd', 'fsub', 'fmul', 'fdiv',
    'fcom', 'fcomp', 'fcompp', 'fucom', 'fucomp', 'fucompp',
    'fcomi', 'fcomip',
    'fabs', 'fchs', 'fsqrt', 'fprem', 'fprem1', 'frndint',
    'fscale', 'fxtract',
    'fsin', 'fcos', 'fsincos', 'fptan', 'fpatan',
    'f2xm1', 'fyl2x', 'fyl2xp1',
    'fldz', 'fld1', 'fldpi', 'fldl2e', 'fldl2t', 'fldlg2', 'fldln2',
    'finit', 'fninit', 'fclex', 'fnclex', 'fstcw', 'fnstcw', 'fldcw',
    'fstsw', 'fnstsw', 'fstenv', 'fnstenv', 'fldenv',
    'fsave', 'fnsave', 'frstor',
    'ffree', 'fdecstp', 'fincstp', 'fxch',
    # SSE/AVX scalar and packed ops
    'movss', 'movsd', 'addss', 'addsd', 'subss', 'subsd',
    'mulss', 'mulsd', 'divss', 'divsd',
    'comiss', 'comisd', 'ucomiss', 'ucomisd',
    'sqrtss', 'sqrtsd', 'maxss', 'maxsd', 'minss', 'minsd',
    'movaps', 'movups', 'movapd', 'movupd', 'movdqa', 'movdqu',
    'addps', 'addpd', 'subps', 'subpd', 'mulps', 'mulpd',
    'divps', 'divpd',
    'andps', 'andpd', 'orps', 'orpd', 'xorps', 'xorpd',
    'shufps', 'shufpd',
    'pxor', 'por', 'pand', 'pandn',
    'paddb', 'paddw', 'paddd', 'paddq',
    'psubb', 'psubw', 'psubd', 'psubq',
    'pmullw', 'pmulld', 'pmulhw',
    'pcmpeqb', 'pcmpeqw', 'pcmpeqd',
    'pcmpgtb', 'pcmpgtw', 'pcmpgtd',
    'pshufd', 'pshufb', 'pshuflw', 'pshufhw',
    'pmovmskb', 'movmskps', 'movmskpd',
    'pinsrb', 'pinsrw', 'pinsrd', 'pinsrq',
    'pextrb', 'pextrw', 'pextrd', 'pextrq',
    'psllw', 'pslld', 'psllq', 'psrlw', 'psrld', 'psrlq',
    'psraw', 'psrad',
    'ptest', 'pcmpistri', 'pcmpistrm', 'pcmpestri', 'pcmpestrm',
    'cvtsi2ss', 'cvtsi2sd', 'cvtss2si', 'cvtsd2si',
    'cvtss2sd', 'cvtsd2ss', 'cvttss2si', 'cvttsd2si',
    # AVX prefixed variants
    'vmovaps', 'vmovups', 'vmovapd', 'vmovupd', 'vmovdqa', 'vmovdqu',
    'vaddps', 'vaddpd', 'vsubps', 'vsubpd', 'vmulps', 'vmulpd',
    'vdivps', 'vdivpd',
    'vbroadcastss', 'vbroadcastsd',
    'vpbroadcastb', 'vpbroadcastw', 'vpbroadcastd', 'vpbroadcastq',
    'vextractf128', 'vextracti128', 'vinsertf128', 'vinserti128',
    'vperm2f128', 'vperm2i128', 'vpermps', 'vpermpd', 'vpermd', 'vpermq',
    'vblendps', 'vblendpd', 'vpblendd',
    'vmaskmovps', 'vmaskmovpd',
    'vfmadd132ps', 'vfmadd213ps', 'vfmadd231ps',
    'vfmadd132pd', 'vfmadd213pd', 'vfmadd231pd',
    # AVX-512 mask ops
    'kmovb', 'kmovw', 'kmovd', 'kmovq',
    'knotw', 'kandw', 'korw', 'kxorw', 'kxnorw',
}

# Well-known Win32, CRT, and system APIs resolved by the linker via
# includelib or import libraries. Listed as lowercased base names;
# we auto-generate A/W suffix variants.
_WIN32_API_BASES = [
    # Kernel32
    'CreateFile', 'ReadFile', 'WriteFile', 'CloseHandle', 'GetLastError',
    'VirtualAlloc', 'VirtualFree', 'VirtualProtect', 'VirtualQuery',
    'HeapAlloc', 'HeapFree', 'HeapReAlloc', 'HeapCreate', 'HeapDestroy',
    'GetProcAddress', 'GetModuleHandle', 'LoadLibrary', 'FreeLibrary',
    'CreateThread', 'ExitThread', 'SuspendThread', 'ResumeThread',
    'CreateProcess', 'TerminateProcess', 'ExitProcess', 'GetExitCodeProcess',
    'WaitForSingleObject', 'WaitForMultipleObjects',
    'CreateEvent', 'SetEvent', 'ResetEvent',
    'CreateMutex', 'ReleaseMutex', 'CreateSemaphore', 'ReleaseSemaphore',
    'InitializeCriticalSection', 'EnterCriticalSection',
    'LeaveCriticalSection', 'DeleteCriticalSection',
    'Sleep', 'SleepEx', 'SwitchToThread',
    'QueryPerformanceCounter', 'QueryPerformanceFrequency',
    'GetTickCount', 'GetTickCount64',
    'GetSystemInfo', 'GetNativeSystemInfo',
    'GetCurrentProcess', 'GetCurrentThread',
    'GetCurrentProcessId', 'GetCurrentThreadId',
    'SetThreadAffinityMask', 'SetThreadIdealProcessor',
    'FlushFileBuffers', 'SetFilePointer', 'SetFilePointerEx',
    'GetFileSize', 'GetFileSizeEx', 'GetFileType',
    'CreateFileMapping', 'MapViewOfFile', 'UnmapViewOfFile',
    'SetConsoleMode', 'GetConsoleMode',
    'GetStdHandle', 'SetConsoleCursorPosition',
    'GetConsoleScreenBufferInfo',
    'WriteConsole', 'ReadConsole', 'ReadConsoleInput',
    'GetEnvironmentVariable', 'SetEnvironmentVariable',
    'GetCommandLine', 'CommandLineToArgv',
    'GetModuleFileName', 'GetTempPath', 'SearchPath',
    'FindFirstFile', 'FindNextFile', 'FindClose',
    'CreateDirectory', 'RemoveDirectory',
    'PathCombine', 'PathFileExists', 'PathRemoveFileSpec',
    'CopyFile', 'MoveFile', 'DeleteFile',
    'GetOverlappedResult',
    'lstrcpy', 'lstrcmp', 'lstrcmpi', 'lstrcat', 'lstrlen', 'lstrcpyn',
    'wsprintf', 'wvsprintf',
    'MultiByteToWideChar', 'WideCharToMultiByte',
    'GlobalAlloc', 'GlobalFree',
    'RegOpenKeyEx', 'RegCloseKey', 'RegQueryValueEx', 'RegSetValueEx',
    'RegCreateKeyEx', 'RegDeleteKey',
    'IsDebuggerPresent', 'CheckRemoteDebuggerPresent',
    'OutputDebugString', 'IsWow64Process',
    'SetUnhandledExceptionFilter', 'AddVectoredExceptionHandler',
    'RemoveVectoredExceptionHandler', 'SetErrorMode',
    'TlsAlloc', 'TlsFree', 'TlsGetValue', 'TlsSetValue',
    'RtlSecureZeroMemory', 'RtlZeroMemory', 'RtlCopyMemory',
    # User32
    'RegisterClass', 'RegisterClassEx', 'CreateWindow', 'CreateWindowEx',
    'DefWindowProc', 'DestroyWindow',
    'ShowWindow', 'UpdateWindow', 'MoveWindow', 'SetWindowText',
    'GetMessage', 'PeekMessage', 'TranslateMessage', 'DispatchMessage',
    'PostQuitMessage', 'PostMessage', 'SendMessage',
    'BeginPaint', 'EndPaint', 'InvalidateRect',
    'PatBlt', 'GetClientRect', 'GetWindowRect',
    'SetBkMode', 'GetSysColor', 'GetSystemMetrics',
    'SelectObject', 'DeleteObject', 'GetTextMetrics',
    'LoadIcon', 'LoadCursor',
    'AppendMenu', 'CreateMenu', 'CreatePopupMenu', 'SetMenu',
    'MessageBox', 'GetOpenFileName',
    'SetTimer', 'SetLayeredWindowAttributes',
    'InitCommonControlsEx',
    # COM
    'CoInitialize', 'CoInitializeEx', 'CoUninitialize',
    # WinINET
    'InternetOpen', 'InternetConnect', 'HttpOpenRequest',
    'HttpSendRequest', 'HttpQueryInfo', 'InternetReadFile',
    'InternetCloseHandle', 'InternetOpenUrl',
    'InternetQueryDataAvailable', 'InternetSetOption',
    # WinHTTP
    'WinHttpOpen', 'WinHttpConnect', 'WinHttpOpenRequest',
    'WinHttpCloseHandle',
    # HTTP API
    'HttpInitialize', 'HttpTerminate', 'HttpCreateHttpHandle',
    'HttpAddUrl', 'HttpRemoveUrl',
    'HttpReceiveHttpRequest', 'HttpReceiveRequestEntityBody',
    'HttpSendHttpResponse', 'HttpAddRequestHeaders',
    # D3D/DXGI
    'D3D11CreateDevice', 'CreateDXGIFactory1',
    'D2D1CreateFactory', 'DWriteCreateFactory',
    'CreateCoreWebView2EnvironmentWithOptions',
    # Dbghelp
    'MiniDumpWriteDump',
    # Crypt / Certificate
    'CertOpenSystemStore', 'CertCloseStore', 'CertFreeCertificateChain',
    'CertGetCertificateChain', 'CertNameToStr',
    'CryptDecodeObjectEx',
    # Toolhelp / Process enumeration
    'CreateToolhelp32Snapshot', 'Process32First', 'Process32Next',
    'Thread32First', 'Thread32Next', 'EnumProcessModules',
    'GetModuleInformation', 'OpenProcess', 'OpenThread',
    'DisableThreadLibraryCalls', 'IsProcessX64',
    # Job / Timer / Pipe
    'CreateJobObjectA', 'CreateJobObject', 'CreateNamedPipe',
    'CreateTimerQueue', 'DeleteTimerQueue', 'SetNamedPipeHandleState',
    'CreateRemoteThread', 'QueueUserAPC',
    # File / Time
    'FileTimeToSystemTime', 'GetLocalTime', 'GetSystemTime',
    'GetFileAttributes', 'GetFileInformationByHandle',
    'FindFirstChangeNotification', 'FindNextChangeNotification',
    'GetWindowText', 'GetWindowTextLength',
    # Memory
    'VirtualAllocEx', 'VirtualFreeEx', 'QueryWorkingSetEx',
    # Sync
    'InterlockedExchangeAdd', 'SleepConditionVariableSRW',
    'InitializeConditionVariable', 'WakeConditionVariable',
    'WakeAllConditionVariable',
    # WinHTTP additional
    'WinHttpSendRequest', 'WinHttpReceiveResponse',
    'WinHttpAddRequestHeaders', 'WinHttpQueryDataAvailable',
    'WinHttpReadData', 'WinHttpWriteData',
    # Shell
    'SHCreateDirectoryEx', 'ShellExecute',
    # CRT
    'printf', 'fprintf', 'sprintf', 'snprintf', 'sscanf',
    'malloc', 'calloc', 'realloc', 'free',
    'memcpy', 'memmove', 'memset', 'memcmp',
    'strlen', 'strcpy', 'strncpy', 'strcat', 'strcmp', 'strncmp',
    'strchr', 'strrchr', 'strstr', 'strtok', 'strtol', 'strtoul',
    'stricmp', 'strncpy_s', 'sprintf_s',
    'atoi', 'atol', 'itoa', 'ltoa',
    'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell', 'fflush',
    'exit', 'abort', 'atexit', 'system',
    '_set_invalid_parameter_handler', '_set_purecall_handler',
    'wcstombs', 'wcslen', 'mbstowcs',
    'lstrcpynA', 'lstrcpynW', 'lstrlenA', 'lstrlenW',
    'wsprintfA', 'wsprintfW',
]


def _build_win32_set():
    """Build case-insensitive lookup set from Win32 API base names + A/W variants."""
    s = set()
    for name in _WIN32_API_BASES:
        low = name.lower()
        s.add(low)
        s.add(low + 'a')
        s.add(low + 'w')
    return s


WIN32_APIS = _build_win32_set()

# Known entry point names that should never be reported as "unused"
ENTRY_POINTS = {
    'main', 'maincrtstartup', 'winmaincrtstart', 'winmaincrtstartup',
    'dllmain', '_dllmaincrtstartup', 'dllentrypoint',
    'wndproc', 'realwndproc', 'windowproc',
    'wndproc_main', 'wndproc_editor', 'wndproc_command',
    'serviceentry', 'servicemain', 'servicectrl', 'servicehandler',
    'testentry', 'sidecarentry', 'sidecarminimalentry',
    'agentbridgeentry', 'pebmain', 'kernelentry_seh',
    'exceptionhandler', 'vectoredexceptionhandler',
    'exceptionfiltercallback', 'crashhandler_exceptionfilter',
    'rawrxd_exceptionfilter', 'rawrxd_vectorizedexceptionhandler',
    'ytfn_exceptionhandler',
    'workerthread', 'workerthreadproc', 'worker_thread_proc',
    'threadpool_workerproc',
    'dlmain', 'demomain', 'winmain', 'sovereignstressmain',
}

# File extensions to scan
ASM_EXTS = {'.asm', '.masm', '.inc', '.s'}


# ============================================================================
#  HELPER FUNCTIONS
# ============================================================================

def _strip_comment(line):
    """Remove inline comment from a MASM line (everything after first ';')."""
    idx = line.find(';')
    return line[:idx] if idx >= 0 else line


def _is_valid_call_target(name):
    """Return True if this looks like a real callable symbol (not a register,
    keyword, or garbage from comment parsing)."""
    lower = name.lower()
    if lower in X86_REGISTERS:
        return False
    if lower in MASM_KEYWORDS:
        return False
    if len(name) <= 1:
        return False
    # Two-char lowercase — likely a typo or abbreviation, not a symbol
    if len(name) == 2 and name.islower() and '_' not in name:
        return False
    return True


def _is_known_external(name):
    """Check if a symbol is a known Win32/CRT API resolved at link time,
    or a function pointer / Vulkan / noise token."""
    lower = name.lower()
    if lower in WIN32_APIS:
        return True
    if lower.startswith('__imp_'):
        return True
    # Function pointer variables (pNtXxx, pIsXxx, p_Xxx)
    if name.startswith('pNt') or name.startswith('pIs') or name.startswith('p_'):
        return True
    # Vulkan function pointers / symbols
    if lower.startswith('vk') or lower.startswith('g_vk'):
        return True
    # GGML/external library functions
    if lower.startswith('ggml_') or lower.startswith('gguf_'):
        return True
    # Ntdll/Rtl system calls (resolved at runtime)
    if name.startswith('Nt') and len(name) > 3 and name[2].isupper():
        return True
    if name.startswith('Rtl') and len(name) > 4:
        return True
    if name.startswith('Zw') and len(name) > 3:
        return True
    # Common noise words that slip through comment stripping
    if lower in {'log', 'relative', 'classify_identifieruffer'}:
        return True
    return False


def _extract_calls(content):
    """Extract call/invoke targets from code lines only (not comments)."""
    calls = []
    for line in content.splitlines():
        code = _strip_comment(line)
        for m in CALL_PATTERN.finditer(code):
            target = m.group(1)
            if _is_valid_call_target(target):
                calls.append(target)
        for m in INVOKE_PATTERN.finditer(code):
            target = m.group(1)
            if _is_valid_call_target(target):
                calls.append(target)
    return calls


# ============================================================================
#  FILE ANALYSIS
# ============================================================================

def analyze_file(filepath):
    """Analyze a single assembly file and extract all symbols/metadata."""
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
    includes = [m.group(1) for m in INCLUDE_PATTERN.finditer(content)]
    includelibs = [m.group(1) for m in INCLUDELIB_PATTERN.finditer(content)]
    todos = [m.group(0) for m in TODO_PATTERN.finditer(content)]
    calls = _extract_calls(content)

    # ---- Stub/Empty PROC detection ----
    stub_procs = []
    for proc, start in procs:
        end_match = re.search(
            rf'{re.escape(proc)}\s+ENDP',
            content[start:], re.IGNORECASE
        )
        if end_match:
            proc_body = content[start:start + end_match.start()]
            # Strip the PROC declaration line itself
            first_nl = proc_body.find('\n')
            if first_nl >= 0:
                proc_body = proc_body[first_nl:]
            # Remove comments, LOCAL declarations, prolog directives
            code_lines = []
            for ln in proc_body.splitlines():
                stripped = _strip_comment(ln).strip()
                if not stripped:
                    continue
                up = stripped.upper()
                # Skip prolog/epilog directives and LOCAL declarations
                if up.startswith((
                    'LOCAL ', '.PUSHREG', '.ALLOCSTACK', '.ENDPROLOG',
                    '.SAVEXMM128', '.SETFRAME',
                )):
                    continue
                code_lines.append(stripped)
            # Filter out pure epilogue instructions
            real_code = [
                l for l in code_lines
                if l.lower() not in (
                    'ret', 'retn',
                    'push rbx', 'push rsi', 'push rdi', 'push rbp',
                    'push r12', 'push r13', 'push r14', 'push r15',
                    'pop rbx', 'pop rsi', 'pop rdi', 'pop rbp',
                    'pop r12', 'pop r13', 'pop r14', 'pop r15',
                    'sub rsp, 28h', 'sub rsp, 20h', 'sub rsp, 40h',
                    'add rsp, 28h', 'add rsp, 20h', 'add rsp, 40h',
                    'xor eax, eax', 'xor rax, rax',
                    'mov rbp, rsp', 'leave',
                )
            ]
            if not real_code:
                stub_procs.append(proc)

    # ---- Missing ENDP ----
    missing_endp = [proc for proc, _ in procs if proc not in endps]

    # ---- Missing RET (accounts for ExitProcess / jmp tail calls) ----
    missing_ret = []
    for proc, start in procs:
        end_match = re.search(
            rf'{re.escape(proc)}\s+ENDP',
            content[start:], re.IGNORECASE
        )
        if end_match:
            proc_body = content[start:start + end_match.start()]
            has_ret = bool(re.search(r'\bret\b', proc_body, re.IGNORECASE))
            # Match ExitProcess even inside p_ExitProcess or [pExitProcess]
            has_exit = bool(re.search(
                r'(?:ExitProcess|TerminateProcess|ExitThread)',
                proc_body, re.IGNORECASE
            ))
            # \S+ instead of \w+ to handle jmp @@label
            has_jmp_tail = bool(re.search(
                r'\bjmp\s+\S+', proc_body, re.IGNORECASE
            ))
            if not has_ret and not has_exit and not has_jmp_tail:
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
        'includes': includes,
        'includelibs': includelibs,
        'todos': todos,
        'calls': calls,
    }


# ============================================================================
#  DIRECTORY SCANNING
# ============================================================================

def scan_directory(root):
    """Walk directory tree and analyze all assembly files."""
    results = []
    for dirpath, _, filenames in os.walk(root):
        for fname in sorted(filenames):
            ext = os.path.splitext(fname)[1].lower()
            if ext in ASM_EXTS:
                fpath = os.path.join(dirpath, fname)
                results.append(analyze_file(fpath))
    return results


# ============================================================================
#  AGGREGATION
# ============================================================================

def aggregate_results(results):
    """Merge per-file results into a codebase-wide report with deduplication."""
    all_procs = set()
    all_structs = set()
    all_data = set()
    all_consts = set()
    all_externs = set()
    all_protos = set()
    all_publics = set()
    all_calls = set()
    all_includes = set()
    all_includelibs = set()
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
        all_protos.update(r['protos'])
        all_publics.update(r['publics'])
        all_calls.update(r['calls'])
        all_includes.update(r['includes'])
        all_includelibs.update(r['includelibs'])
        all_todos.extend((r['file'], t) for t in r['todos'])
        stub_procs.extend((r['file'], p) for p in r['stub_procs'])
        missing_endp.extend((r['file'], p) for p in r['missing_endp'])
        missing_ret.extend((r['file'], p) for p in r['missing_ret'])

    # Build the set of all known/resolvable symbols:
    # 1. PROCs defined in the codebase
    # 2. EXTERNs/EXTERNDEFs declared
    # 3. PROTOs declared
    # 4. DATA labels (struct instances, variables)
    # 5. EQU constants
    # 6. STRUCTs (type names)
    # 7. PUBLIC exports (also known)
    known_symbols = (
        all_procs | all_externs | all_protos |
        all_data | all_consts | all_structs | all_publics
    )
    known_lower = {s.lower() for s in known_symbols}

    # Undefined = called but not in known_symbols AND not a Win32/CRT API
    undefined_calls = sorted(
        c for c in all_calls
        if c.lower() not in known_lower and not _is_known_external(c)
    )

    # Unused = defined as PROC but never called AND not PUBLIC AND not entry point
    called_lower = {c.lower() for c in all_calls}
    unused_procs = sorted(
        p for p in all_procs
        if (p.lower() not in called_lower
            and p.lower() not in ENTRY_POINTS
            and p not in all_publics)
    )

    return {
        'all_procs': sorted(all_procs),
        'all_structs': sorted(all_structs),
        'all_data': sorted(all_data),
        'all_consts': sorted(all_consts),
        'all_externs': sorted(all_externs),
        'all_protos': sorted(all_protos),
        'all_publics': sorted(all_publics),
        'all_calls': sorted(all_calls),
        'all_includes': sorted(all_includes),
        'all_includelibs': sorted(all_includelibs),
        'all_todos': all_todos,
        'stub_procs': stub_procs,
        'missing_endp': missing_endp,
        'missing_ret': missing_ret,
        'undefined_calls': undefined_calls,
        'unused_procs': unused_procs,
    }


# ============================================================================
#  REPORT GENERATION
# ============================================================================

def print_report(agg, outpath=None):
    """Format and output the digest report."""
    lines = []
    lines.append('=' * 72)
    lines.append('  RawrXD MASM/ASM Codebase Digest Report')
    lines.append('  Production Static Analysis')
    lines.append('=' * 72)
    lines.append('')
    lines.append(f'  Total PROCs:       {len(agg["all_procs"]):>6}')
    lines.append(f'  Total STRUCTs:     {len(agg["all_structs"]):>6}')
    lines.append(f'  Total DATA labels: {len(agg["all_data"]):>6}')
    lines.append(f'  Total CONSTs:      {len(agg["all_consts"]):>6}')
    lines.append(f'  Total EXTERNs:     {len(agg["all_externs"]):>6}')
    lines.append(f'  Total PUBLICs:     {len(agg["all_publics"]):>6}')
    lines.append(f'  Total CALLs:       {len(agg["all_calls"]):>6}')
    lines.append('')

    n_stubs = len(agg['stub_procs'])
    n_endp = len(agg['missing_endp'])
    n_ret = len(agg['missing_ret'])
    n_undef = len(agg['undefined_calls'])
    n_unused = len(agg['unused_procs'])
    n_todos = len(agg['all_todos'])

    lines.append('  --- Issue Summary ---')
    lines.append(f'  Stub/Empty PROCs:    {n_stubs:>4}')
    lines.append(f'  Missing ENDP:        {n_endp:>4}')
    lines.append(f'  Missing RET:         {n_ret:>4}')
    lines.append(f'  Undefined CALLs:     {n_undef:>4}')
    lines.append(f'  Unused PROCs:        {n_unused:>4}')
    lines.append(f'  FIXME/STUB markers:  {n_todos:>4}')
    lines.append('')
    lines.append('-' * 72)

    if agg['all_todos']:
        lines.append('')
        lines.append('--- FIXMEs / STUBs / TODOs ---')
        for f, t in agg['all_todos']:
            lines.append(f'  {f}: {t.strip()}')

    if agg['stub_procs']:
        lines.append('')
        lines.append('--- Stub/Empty PROCs (no real logic) ---')
        for f, p in agg['stub_procs']:
            lines.append(f'  {f}: {p}')

    if agg['missing_endp']:
        lines.append('')
        lines.append('--- PROCs missing ENDP ---')
        for f, p in agg['missing_endp']:
            lines.append(f'  {f}: {p}')

    if agg['missing_ret']:
        lines.append('')
        lines.append('--- PROCs missing RET (no ret/ExitProcess/jmp) ---')
        for f, p in agg['missing_ret']:
            lines.append(f'  {f}: {p}')

    if agg['undefined_calls']:
        lines.append('')
        lines.append('--- Undefined CALL targets (not found in codebase) ---')
        lines.append('  (Excludes Win32 APIs, CRT, registers, keywords)')
        for c in agg['undefined_calls']:
            lines.append(f'  {c}')

    if agg['unused_procs']:
        lines.append('')
        lines.append('--- Unused PROCs (defined but no callers found) ---')
        lines.append('  (Excludes PUBLIC exports and known entry points)')
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


# ============================================================================
#  MAIN
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description='RawrXD MASM/ASM Codebase Digest Tool — Production Static Analysis'
    )
    parser.add_argument('root', help='Root directory to scan')
    parser.add_argument('--out', '-o', help='Output file for report')
    args = parser.parse_args()

    print(f'Scanning {args.root} ...')
    results = scan_directory(args.root)
    print(f'  Analyzed {len(results)} files')

    agg = aggregate_results(results)
    print_report(agg, args.out)


if __name__ == '__main__':
    main()
