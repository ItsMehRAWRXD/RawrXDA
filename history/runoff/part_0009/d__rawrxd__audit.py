import os, re

root = r'D:\rawrxd'
skip = {'node_modules', '.git', 'build', 'third_party', 'quickjs'}

sys_prefixes = [
    'windows.', 'winuser.', 'wingdi.', 'commctrl.', 'shlwapi.', 'richedit.',
    'commdlg.', 'shellapi.', 'winsock', 'd3d', 'dxgi', 'vulkan', 'cuda.',
    'hip/', 'nv', 'ws2', 'objbase', 'shlobj', 'dwmapi', 'uxtheme', 'pathcch',
    'wininet', 'atlbase', 'strsafe', 'taskschd', 'mmsystem', 'wrl/', 'dml/',
    'directml', 'unknwn', 'ole2', 'initguid', 'resource.', 'mfapi', 'mfidl',
    'evr.', 'propvarutil', 'ocidl', 'olectl', 'knownfolders', 'propkey',
    'sapi', 'endpointvolume', 'mmdevice', 'audioclient', 'functiondiscovery',
    'setupapi', 'cfgmgr', 'hidsdi', 'hidpi', 'bluetoothapis', 'bthsdpdef',
    'ws2bth', 'dbghelp', 'psapi', 'wintrust', 'softpub', 'mscat', 'wincrypt',
    'bcrypt', 'ncrypt', 'iphlpapi', 'icmpapi', 'pdh', 'powrprof', 'winhttp',
    'urlmon', 'mshtml', 'exdisp', 'mshtmhst', 'dispex', 'activscp', 'xmllite',
    'shobjidl', 'propsys', 'propidl', 'wmcodecdsp', 'mftransform', 'mferror',
    'ks.', 'ksmedia', 'intrin', 'immintrin', 'xmmintrin', 'emmintrin',
    'smmintrin', 'ammintrin', 'pmmintrin', 'tmmintrin', 'avxintrin', 'avx2intrin',
    'avx512', 'nmmintrin', 'bmiintrin', 'bmi2intrin', 'lzcntintrin', 'popcntintrin',
    'fmaintrin', 'f16cintrin', 'wmmintrin', 'x86intrin', 'ia32intrin',
    'arm_neon', 'arm_sve', 'arm64_neon',
]

# Index headers on disk by basename
on_disk = set()
for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in skip]
    for f in filenames:
        if f.endswith(('.h', '.hpp')):
            on_disk.add(f.lower())

# Parse includes from .cpp/.c/.h files
inc_re = re.compile(r'#include\s+"([^"]+)"')
includes = {}
for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in skip]
    for f in filenames:
        if not f.endswith(('.cpp', '.c', '.h', '.hpp')):
            continue
        fp = os.path.join(dirpath, f)
        try:
            for line in open(fp, 'r', errors='ignore'):
                m = inc_re.search(line)
                if m:
                    inc = m.group(1)
                    includes.setdefault(inc, set()).add(f)
        except:
            pass

# Check which are missing
missing = []
for inc, sources in sorted(includes.items()):
    leaf = os.path.basename(inc).lower()
    if not leaf.endswith(('.h', '.hpp')):
        continue
    if any(leaf.startswith(p) for p in sys_prefixes):
        continue
    if leaf not in on_disk:
        missing.append((inc, sources))

print(f'Headers on disk:       {len(on_disk)}')
print(f'Unique include refs:   {len(includes)}')
print(f'MISSING headers:       {len(missing)}')
print()
if missing:
    # Group by directory
    by_dir = {}
    for inc, srcs in missing:
        d = os.path.dirname(inc) or '(root)'
        by_dir.setdefault(d, []).append((inc, srcs))
    
    for d in sorted(by_dir.keys()):
        items = by_dir[d]
        print(f'  [{d}] ({len(items)} missing)')
        for inc, srcs in items:
            src_list = ', '.join(sorted(srcs)[:3])
            if len(srcs) > 3:
                src_list += f' +{len(srcs)-3} more'
            print(f'    X {inc}  <- {src_list}')
        print()
else:
    print('  ALL HEADERS PRESENT!')

# Also check: CMakeLists.txt source files
print('=' * 50)
print('CMakeLists.txt SOURCE FILE CHECK')
print('=' * 50)
cmake_path = os.path.join(root, 'CMakeLists.txt')
src_re = re.compile(r'(?:^|\s)((?:src|include)/\S+\.(?:cpp|c|asm|h|hpp))\b')
cmake_var_re = re.compile(r'\$\{CMAKE_CURRENT_SOURCE_DIR\}/((?:src|include)/\S+\.(?:cpp|c|asm|h|hpp))')
cmake_files = set()
for line in open(cmake_path, 'r', errors='ignore'):
    stripped = line.strip()
    if stripped.startswith('#'):
        continue
    for m in src_re.finditer(stripped):
        cmake_files.add(m.group(1))
    for m in cmake_var_re.finditer(stripped):
        cmake_files.add(m.group(1))

cm_missing = []
cm_found = []
for f in sorted(cmake_files):
    fp = os.path.join(root, f)
    if os.path.exists(fp):
        cm_found.append(f)
    else:
        cm_missing.append(f)

print(f'CMake references:  {len(cmake_files)}')
print(f'Found:             {len(cm_found)}')
print(f'MISSING:           {len(cm_missing)}')
if cm_missing:
    print()
    for f in cm_missing:
        print(f'  X {f}')
print()

# Check .asm EXTERN references
print('=' * 50)
print('ASM EXTERN SYMBOL CHECK (sampling)')
print('=' * 50)
asm_externs = set()
asm_publics = set()
extern_re = re.compile(r'EXTERN\s+(\w+)')
public_re = re.compile(r'PUBLIC\s+(\w+)')
for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in skip]
    for f in filenames:
        if not f.endswith('.asm'):
            continue
        fp = os.path.join(dirpath, f)
        try:
            for line in open(fp, 'r', errors='ignore'):
                for m in extern_re.finditer(line):
                    asm_externs.add(m.group(1))
                for m in public_re.finditer(line):
                    asm_publics.add(m.group(1))
        except:
            pass

# Check externs satisfied by C++ exports
cpp_exports = set()
export_re = re.compile(r'extern\s+"C"\s+.*?\b(\w+)\s*\(')
for dirpath, dirnames, filenames in os.walk(root):
    dirnames[:] = [d for d in dirnames if d not in skip]
    for f in filenames:
        if not f.endswith(('.cpp', '.c', '.h')):
            continue
        fp = os.path.join(dirpath, f)
        try:
            for line in open(fp, 'r', errors='ignore'):
                for m in export_re.finditer(line):
                    cpp_exports.add(m.group(1))
        except:
            pass

unsatisfied = asm_externs - asm_publics - cpp_exports
# Filter out known Win32 API symbols
win32_api = {'GetProcAddress', 'LoadLibraryA', 'LoadLibraryW', 'VirtualAlloc', 'VirtualFree',
             'VirtualProtect', 'CreateFileA', 'CreateFileW', 'ReadFile', 'WriteFile',
             'CloseHandle', 'GetLastError', 'SetLastError', 'HeapAlloc', 'HeapFree',
             'GetProcessHeap', 'ExitProcess', 'GetModuleHandleA', 'GetModuleHandleW',
             'CreateThread', 'WaitForSingleObject', 'Sleep', 'QueryPerformanceCounter',
             'QueryPerformanceFrequency', 'GetTickCount64', 'GetSystemInfo',
             'MultiByteToWideChar', 'WideCharToMultiByte', 'OutputDebugStringA',
             'RtlCaptureContext', 'RtlLookupFunctionEntry', 'RtlVirtualUnwind',
             'AddVectoredExceptionHandler', 'RemoveVectoredExceptionHandler',
             'IsProcessorFeaturePresent', 'FlushInstructionCache',
             'InitializeSRWLock', 'AcquireSRWLockExclusive', 'ReleaseSRWLockExclusive',
             'AcquireSRWLockShared', 'ReleaseSRWLockShared',
             'InitializeConditionVariable', 'SleepConditionVariableSRW', 'WakeConditionVariable',
             'WakeAllConditionVariable', 'GetCurrentThreadId', 'GetCurrentProcessId',
             'InterlockedIncrement', 'InterlockedDecrement', 'InterlockedExchange',
             'InterlockedCompareExchange', 'InterlockedExchange64',
             '_aligned_malloc', '_aligned_free', 'malloc', 'free', 'calloc', 'realloc',
             'memcpy', 'memset', 'memcmp', 'memmove', 'strlen', 'strcmp', 'strncmp',
             'strcpy', 'strncpy', 'strcat', 'sprintf', 'snprintf', '_snprintf',
             'printf', 'fprintf', 'fopen', 'fclose', 'fread', 'fwrite', 'fseek', 'ftell',
             'CreateFileMappingA', 'MapViewOfFile', 'UnmapViewOfFile',
             'WSAStartup', 'WSACleanup', 'socket', 'bind', 'listen', 'accept',
             'connect', 'send', 'recv', 'closesocket', 'select', 'ioctlsocket',
             'htons', 'htonl', 'ntohs', 'ntohl', 'inet_addr', 'inet_ntoa',
             'getaddrinfo', 'freeaddrinfo', 'setsockopt',
             'CreateIoCompletionPort', 'GetQueuedCompletionStatus', 'PostQueuedCompletionStatus',
             'SetFilePointerEx', 'GetFileSizeEx', 'DeviceIoControl',
             '__security_cookie', '__security_check_cookie', '__GSHandlerCheck',
             '__CxxFrameHandler3', '__CxxFrameHandler4',
             '_RTC_CheckStackVars', '_RTC_InitBase', '_RTC_Shutdown',
             '__chkstk', '___chkstk_ms', '_fltused',
             'wcslen', 'wcscpy', 'wcscat', 'wcscmp', 'wcsncmp',
             }
unsatisfied = unsatisfied - win32_api

print(f'ASM EXTERN symbols:    {len(asm_externs)}')
print(f'ASM PUBLIC symbols:    {len(asm_publics)}')
print(f'C/C++ extern "C":      {len(cpp_exports)}')
print(f'Potentially unresolved: {len(unsatisfied)}')
if unsatisfied and len(unsatisfied) < 100:
    for s in sorted(unsatisfied):
        print(f'  ? {s}')
elif unsatisfied:
    print(f'  (showing first 50)')
    for s in sorted(unsatisfied)[:50]:
        print(f'  ? {s}')

# SUMMARY
print()
print('=' * 50)
print('FINAL SUMMARY')
print('=' * 50)
total_missing = len(missing) + len(cm_missing)
print(f'Missing headers:           {len(missing)}')
print(f'Missing CMake sources:     {len(cm_missing)}')
print(f'Unresolved ASM externs:    {len(unsatisfied)}')
print(f'TOTAL GAPS:                {total_missing + len(unsatisfied)}')
