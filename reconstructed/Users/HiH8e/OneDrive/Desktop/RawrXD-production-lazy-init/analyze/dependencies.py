import os
import re

src_dir = r'C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\src'
include_dir = r'C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\masm_ide\include'
raw_file = r'C:\Users\HiH8e\OneDrive\Desktop\RawrXD-production-lazy-init\dependencies_raw.txt'

# Get list of files in src and include
src_files = [f.lower() for f in os.listdir(src_dir) if f.endswith('.asm') or f.endswith('.inc')]
include_files = [f.lower() for f in os.listdir(include_dir) if f.endswith('.inc')]

# Data structures
file_includes = {} # file -> list of includes
file_externs = {}  # file -> list of externs
public_symbols = {} # symbol -> file where it is defined

# Regex patterns
include_re = re.compile(r'include\s+([^\s;]+)', re.IGNORECASE)
extern_re = re.compile(r'extern\s+([^\s:;]+)', re.IGNORECASE)
public_re = re.compile(r'public\s+([^\s;]+)', re.IGNORECASE)

with open(raw_file, 'r') as f:
    for line in f:
        line = line.strip()
        if not line: continue
        
        # Format: path:line
        # On Windows, path might contain : (e.g. C:\...)
        # Grep output is usually path:line_content
        # But if we used grep -r, it might be path:line_number:line_content
        
        # Let's try to find the last colon that is followed by include/extern/public
        m = re.search(r'^(.*):(?:include|extern|public|EXTERN|PUBLIC)', line, re.IGNORECASE)
        if not m: continue
        
        file_path = m.group(1)
        content = line[len(file_path)+1:]
        
        file_name = os.path.basename(file_path)
        
        # Extract includes
        m = include_re.search(content)
        if m:
            inc = m.group(1).strip().lower()
            # Ignore standard MASM32 includes
            if not inc.startswith('\\masm32\\'):
                if file_name not in file_includes: file_includes[file_name] = []
                file_includes[file_name].append(inc)
        
        # Extract externs
        m = extern_re.search(content)
        if m:
            ext = m.group(1).strip()
            if file_name not in file_externs: file_externs[file_name] = []
            file_externs[file_name].append(ext)
            
        # Extract publics
        m = public_re.search(content)
        if m:
            pub = m.group(1).strip()
            public_symbols[pub] = file_name

# Analysis
missing_files = {}
missing_symbols = {}

for file_name, includes in file_includes.items():
    for inc in includes:
        # Check if inc exists in src or include
        # Some includes might have paths like ..\include\winapi_min.inc
        inc_base = os.path.basename(inc)
        if inc_base not in src_files and inc_base not in include_files:
            if file_name not in missing_files: missing_files[file_name] = []
            missing_files[file_name].append(inc)

# Common Windows/C symbols to ignore
ignore_symbols = {
    'void', 'proc', 'ptr', 'dword', 'word', 'byte', 'qword',
    'malloc', 'free', 'memcpy', 'memset', 'memmove', 'strlen', 'strcpy', 'strcat', 'strcmp', 'stricmp',
    'GetModuleHandleA', 'GetTickCount', 'CreateThread', 'CloseHandle', 'ExitThread', 'RtlZeroMemory',
    'lstrcpyA', 'lstrlenA', 'lstrcatA', 'lstrcmpA', 'lstrcmpiA', 'CreateFileA', 'ReadFile', 'WriteFile',
    'GetFileSize', 'TerminateThread', 'RtlCopyMemory', 'wsprintfA', 'SendMessageA', 'PostMessageA',
    'GetWindowRect', 'SetWindowPos', 'ShowWindow', 'UpdateWindow', 'CreateWindowExA', 'RegisterClassExA',
    'DefWindowProcA', 'LoadCursorA', 'LoadIconA', 'GetMessageA', 'TranslateMessage', 'DispatchMessageA',
    'MessageBoxA', 'GetOpenFileNameA', 'GetSaveFileNameA', 'ChooseColorA', 'GetStockObject', 'SelectObject',
    'DeleteObject', 'CreateFontIndirectA', 'GetDC', 'ReleaseDC', 'BeginPaint', 'EndPaint', 'FillRect',
    'DrawTextA', 'SetTextColor', 'SetBkColor', 'SetBkMode', 'CreateSolidBrush', 'CreatePen', 'LineTo', 'MoveToEx',
    'GetSystemMetrics', 'GetClientRect', 'InvalidateRect', 'GetParent', 'GetDlgItem', 'SetDlgItemTextA',
    'GetDlgItemTextA', 'CheckDlgButton', 'IsDlgButtonChecked', 'EnableWindow', 'SetFocus', 'GetFocus',
    'SetTimer', 'KillTimer', 'GetLocalTime', 'GetSystemTime', 'SystemTimeToFileTime', 'FileTimeToSystemTime',
    'GetCommandLineA', 'ExitProcess', 'GetLastError', 'SetLastError', 'Sleep', 'WaitForSingleObject',
    'CreateEventA', 'SetEvent', 'ResetEvent', 'OpenEventA', 'CreateMutexA', 'ReleaseMutex', 'OpenMutexA',
    'CreateSemaphoreA', 'ReleaseSemaphore', 'OpenSemaphoreA', 'VirtualAlloc', 'VirtualFree', 'HeapAlloc',
    'HeapFree', 'GetProcessHeap', 'GlobalAlloc', 'GlobalFree', 'GlobalLock', 'GlobalUnlock',
    'LoadLibraryA', 'GetProcAddress', 'FreeLibrary', 'GetModuleFileNameA', 'GetCurrentProcess',
    'GetCurrentThread', 'OpenProcess', 'TerminateProcess', 'GetExitCodeProcess', 'GetExitCodeThread',
    'CreateProcessA', 'ShellExecuteA', 'SHGetFolderPathA', 'CoInitialize', 'CoUninitialize', 'CoCreateInstance',
    'SysAllocString', 'SysFreeString', 'VariantInit', 'VariantClear', 'hMainWindow', 'hInstance', 'g_hInstance',
    'g_hMainWindow', 'g_hEditorWindow', 'g_hStatusBar', 'g_hToolbar', 'hStatus', 'hEditorFont', 'hMonoFont',
    'hMainFont', 'g_hMainFont', 'g_hFileTree', 'hFileTree', 'hDriveCombo', 'g_modernTheme', 'g_camoPattern',
    'bPerfLoggingEnabled', 'clrBackground', 'clrForeground', 'clrAccent'
}

for file_name, externs in file_externs.items():
    for ext in externs:
        if ext not in public_symbols and ext not in ignore_symbols:
            if file_name not in missing_symbols: missing_symbols[file_name] = []
            missing_symbols[file_name].append(ext)

# Output report
print("# Dependency Analysis Report")
print("\n## Missing Included Files")
if not missing_files:
    print("None found.")
else:
    for file_name, files in missing_files.items():
        print(f"- **{file_name}**:")
        for f in files:
            print(f"  - `{f}`")

print("\n## Missing EXTERN Symbols")
if not missing_symbols:
    print("None found.")
else:
    for file_name, symbols in missing_symbols.items():
        print(f"- **{file_name}**:")
        for s in symbols:
            print(f"  - `{s}`")
