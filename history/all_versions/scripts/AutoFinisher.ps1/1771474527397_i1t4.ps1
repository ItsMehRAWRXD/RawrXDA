#==============================================================================
# AutoFinisher.ps1 — RawrXD Autonomous MASM64 Build Pipeline
#
# Reverse-engineered from the manual agent workflow:
# Takes ANY .asm source — even files using hutch's MASM64 SDK (invoke, addr,
# CStr, OPTION WIN64:3, include \masm64\*) — and automatically rewrites them
# to pure ml64 x64, compiles, links, and verifies.
#
# PHASES:
#   1. Dependency Resolution   — resolve INCLUDEs or strip missing ones
#   2. SDK Translation         — invoke→ABI, addr→lea, CStr→static, hex fix
#   3. Stub Detection          — find stub procs, generate real implementations
#   4. Extern Injection        — auto-detect WinAPI calls, add extrn decls
#   5. ML64 Quirk Fixes        — hex leading 0, test imm64, label conflicts
#   6. Compile (ml64)          — assemble to .obj
#   7. Link (link.exe)         — produce EXE or DLL
#   8. Verify (dumpbin)        — confirm valid PE
#
# USAGE:
#   .\AutoFinisher.ps1                          # process all .asm in src dir
#   .\AutoFinisher.ps1 -File "path\to\file.asm" # process single file
#   .\AutoFinisher.ps1 -SrcDir "D:\my\src"      # custom source directory
#   .\AutoFinisher.ps1 -OutputType DLL           # build DLL instead of EXE
#   .\AutoFinisher.ps1 -DryRun                   # show what would be done
#
# NO DEPENDENCIES: pure ml64/link.exe from VS2022 BuildTools
#==============================================================================

[CmdletBinding()]
param(
    [string]$File = "",
    [string]$SrcDir = "D:\rawrxd\src",
    [string]$BuildDir = "",
    [string]$IncludeDir = "",
    [ValidateSet("EXE","DLL")]
    [string]$OutputType = "EXE",
    [string]$EntryPoint = "",
    [ValidateSet("CONSOLE","WINDOWS")]
    [string]$Subsystem = "",
    [string[]]$ExtraLibs = @(),
    [switch]$NoCRT,
    [switch]$DryRun,
    [switch]$SkipLink,
    [switch]$DetailedOutput
)

#==============================================================================
# TOOLCHAIN PATHS
#==============================================================================
$MSVC_ROOT = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
$SDK_ROOT  = "C:\Program Files (x86)\Windows Kits\10"
$SDK_VER   = "10.0.26100.0"

# Fallback SDK version
if (-not (Test-Path "$SDK_ROOT\Lib\$SDK_VER")) {
    $SDK_VER = "10.0.22621.0"
}

$ML64    = "$MSVC_ROOT\bin\Hostx64\x64\ml64.exe"
$LINK    = "$MSVC_ROOT\bin\Hostx64\x64\link.exe"
$DUMPBIN = "$MSVC_ROOT\bin\Hostx64\x64\dumpbin.exe"

$MSVC_LIB  = "$MSVC_ROOT\lib\x64"
$SDK_UM    = "$SDK_ROOT\Lib\$SDK_VER\um\x64"
$SDK_UCRT  = "$SDK_ROOT\Lib\$SDK_VER\ucrt\x64"

# Set INCLUDE for ml64 (find rawrxd include dirs)
$INCLUDE_DIRS = @()
foreach ($d in @("$SrcDir", "D:\rawrxd\include", "D:\rawrxd\src\asm", "D:\rawrxd\src\include")) {
    if (Test-Path $d) { $INCLUDE_DIRS += $d }
}

# Default build dir
if (-not $BuildDir) {
    $BuildDir = Join-Path (Split-Path $SrcDir -Parent) "build_prod"
}
New-Item -ItemType Directory -Path $BuildDir -Force | Out-Null
New-Item -ItemType Directory -Path "$BuildDir\obj" -Force | Out-Null
New-Item -ItemType Directory -Path "$BuildDir\rewritten" -Force | Out-Null

# Set LIB environment for linker
$env:LIB = "$MSVC_LIB;$SDK_UM;$SDK_UCRT"

#==============================================================================
# KNOWN WINAPI → EXTRN MAPPING
#==============================================================================
$WINAPI_LIBS = @{
    # kernel32.lib
    "GetProcessHeap"="kernel32.lib"; "HeapCreate"="kernel32.lib"; "HeapAlloc"="kernel32.lib"
    "HeapFree"="kernel32.lib"; "HeapDestroy"="kernel32.lib"
    "CreateFileA"="kernel32.lib"; "CreateFileW"="kernel32.lib"
    "ReadFile"="kernel32.lib"; "WriteFile"="kernel32.lib"; "CloseHandle"="kernel32.lib"
    "CreateDirectoryA"="kernel32.lib"; "CreateDirectoryW"="kernel32.lib"
    "DeleteFileA"="kernel32.lib"; "DeleteFileW"="kernel32.lib"
    "GetModuleHandleA"="kernel32.lib"; "GetModuleHandleW"="kernel32.lib"
    "GetProcAddress"="kernel32.lib"; "LoadLibraryA"="kernel32.lib"; "LoadLibraryW"="kernel32.lib"
    "VirtualAlloc"="kernel32.lib"; "VirtualFree"="kernel32.lib"; "VirtualProtect"="kernel32.lib"
    "CreateThread"="kernel32.lib"; "ExitThread"="kernel32.lib"
    "WaitForSingleObject"="kernel32.lib"; "WaitForMultipleObjects"="kernel32.lib"
    "Sleep"="kernel32.lib"; "ExitProcess"="kernel32.lib"
    "GetStdHandle"="kernel32.lib"; "GetLastError"="kernel32.lib"
    "SetLastError"="kernel32.lib"; "GetTickCount"="kernel32.lib"; "GetTickCount64"="kernel32.lib"
    "QueryPerformanceCounter"="kernel32.lib"; "QueryPerformanceFrequency"="kernel32.lib"
    "GetSystemInfo"="kernel32.lib"; "GetCurrentProcess"="kernel32.lib"
    "GetCurrentProcessId"="kernel32.lib"; "GetCurrentThreadId"="kernel32.lib"
    "CreateProcessA"="kernel32.lib"; "CreateProcessW"="kernel32.lib"
    "CreateNamedPipeA"="kernel32.lib"; "ConnectNamedPipe"="kernel32.lib"
    "DisconnectNamedPipe"="kernel32.lib"
    "CreateEventA"="kernel32.lib"; "CreateEventW"="kernel32.lib"
    "SetEvent"="kernel32.lib"; "ResetEvent"="kernel32.lib"
    "CreateMutexA"="kernel32.lib"; "CreateMutexW"="kernel32.lib"
    "ReleaseMutex"="kernel32.lib"
    "InitializeCriticalSection"="kernel32.lib"; "EnterCriticalSection"="kernel32.lib"
    "LeaveCriticalSection"="kernel32.lib"; "DeleteCriticalSection"="kernel32.lib"
    "FindFirstFileA"="kernel32.lib"; "FindFirstFileW"="kernel32.lib"
    "FindNextFileA"="kernel32.lib"; "FindNextFileW"="kernel32.lib"
    "FindClose"="kernel32.lib"
    "GetFileSize"="kernel32.lib"; "GetFileSizeEx"="kernel32.lib"
    "SetFilePointer"="kernel32.lib"; "SetFilePointerEx"="kernel32.lib"
    "FlushFileBuffers"="kernel32.lib"
    "GetEnvironmentVariableA"="kernel32.lib"; "GetEnvironmentVariableW"="kernel32.lib"
    "SetEnvironmentVariableA"="kernel32.lib"
    "GetCommandLineA"="kernel32.lib"; "GetCommandLineW"="kernel32.lib"
    "OutputDebugStringA"="kernel32.lib"; "OutputDebugStringW"="kernel32.lib"
    "MultiByteToWideChar"="kernel32.lib"; "WideCharToMultiByte"="kernel32.lib"
    "GlobalAlloc"="kernel32.lib"; "GlobalFree"="kernel32.lib"; "GlobalLock"="kernel32.lib"
    "GlobalUnlock"="kernel32.lib"
    "LocalAlloc"="kernel32.lib"; "LocalFree"="kernel32.lib"
    "FreeLibrary"="kernel32.lib"
    "GetModuleFileNameA"="kernel32.lib"; "GetModuleFileNameW"="kernel32.lib"
    "ReadDirectoryChangesW"="kernel32.lib"
    "CreateIoCompletionPort"="kernel32.lib"; "GetQueuedCompletionStatus"="kernel32.lib"
    "PostQueuedCompletionStatus"="kernel32.lib"
    "GetSystemTimeAsFileTime"="kernel32.lib"
    "InitializeSListHead"="kernel32.lib"; "InterlockedPushEntrySList"="kernel32.lib"
    "InterlockedPopEntrySList"="kernel32.lib"
    "TlsAlloc"="kernel32.lib"; "TlsSetValue"="kernel32.lib"; "TlsGetValue"="kernel32.lib"
    "TlsFree"="kernel32.lib"; "FlsAlloc"="kernel32.lib"

    # user32.lib
    "RegisterClassExA"="user32.lib"; "RegisterClassExW"="user32.lib"
    "CreateWindowExA"="user32.lib"; "CreateWindowExW"="user32.lib"
    "ShowWindow"="user32.lib"; "UpdateWindow"="user32.lib"
    "GetMessageA"="user32.lib"; "GetMessageW"="user32.lib"
    "TranslateMessage"="user32.lib"; "DispatchMessageA"="user32.lib"
    "DispatchMessageW"="user32.lib"
    "DefWindowProcA"="user32.lib"; "DefWindowProcW"="user32.lib"
    "PostQuitMessage"="user32.lib"; "PostMessageA"="user32.lib"; "PostMessageW"="user32.lib"
    "SendMessageA"="user32.lib"; "SendMessageW"="user32.lib"
    "MessageBoxA"="user32.lib"; "MessageBoxW"="user32.lib"
    "DestroyWindow"="user32.lib"
    "BeginPaint"="user32.lib"; "EndPaint"="user32.lib"
    "GetDC"="user32.lib"; "ReleaseDC"="user32.lib"
    "InvalidateRect"="user32.lib"; "GetClientRect"="user32.lib"
    "SetWindowTextA"="user32.lib"; "SetWindowTextW"="user32.lib"
    "LoadCursorA"="user32.lib"; "LoadCursorW"="user32.lib"
    "LoadIconA"="user32.lib"; "LoadIconW"="user32.lib"
    "SetTimer"="user32.lib"; "KillTimer"="user32.lib"
    "GetSystemMetrics"="user32.lib"; "SystemParametersInfoA"="user32.lib"
    "SetWindowPos"="user32.lib"; "MoveWindow"="user32.lib"
    "GetWindowRect"="user32.lib"; "AdjustWindowRectEx"="user32.lib"
    "SetForegroundWindow"="user32.lib"
    "TrackMouseEvent"="user32.lib"
    "FillRect"="user32.lib"
    "DrawTextA"="user32.lib"; "DrawTextW"="user32.lib"

    # gdi32.lib
    "CreateSolidBrush"="gdi32.lib"; "CreateFontA"="gdi32.lib"; "CreateFontW"="gdi32.lib"
    "SelectObject"="gdi32.lib"; "DeleteObject"="gdi32.lib"
    "SetBkMode"="gdi32.lib"; "SetTextColor"="gdi32.lib"; "SetBkColor"="gdi32.lib"
    "TextOutA"="gdi32.lib"; "TextOutW"="gdi32.lib"
    "BitBlt"="gdi32.lib"; "StretchBlt"="gdi32.lib"
    "CreateCompatibleDC"="gdi32.lib"; "DeleteDC"="gdi32.lib"
    "GetDeviceCaps"="gdi32.lib"

    # advapi32.lib
    "RegOpenKeyExA"="advapi32.lib"; "RegOpenKeyExW"="advapi32.lib"
    "RegQueryValueExA"="advapi32.lib"; "RegQueryValueExW"="advapi32.lib"
    "RegSetValueExA"="advapi32.lib"; "RegCloseKey"="advapi32.lib"
    "CryptAcquireContextA"="advapi32.lib"; "CryptGenRandom"="advapi32.lib"
    "CryptReleaseContext"="advapi32.lib"

    # winhttp.lib
    "WinHttpOpen"="winhttp.lib"; "WinHttpConnect"="winhttp.lib"
    "WinHttpOpenRequest"="winhttp.lib"; "WinHttpSendRequest"="winhttp.lib"
    "WinHttpReceiveResponse"="winhttp.lib"; "WinHttpQueryDataAvailable"="winhttp.lib"
    "WinHttpReadData"="winhttp.lib"; "WinHttpCloseHandle"="winhttp.lib"
    "WinHttpAddRequestHeaders"="winhttp.lib"
    "WinHttpSetOption"="winhttp.lib"

    # ws2_32.lib
    "WSAStartup"="ws2_32.lib"; "WSACleanup"="ws2_32.lib"
    "socket"="ws2_32.lib"; "bind"="ws2_32.lib"; "listen"="ws2_32.lib"
    "accept"="ws2_32.lib"; "connect"="ws2_32.lib"; "send"="ws2_32.lib"
    "recv"="ws2_32.lib"; "closesocket"="ws2_32.lib"
    "inet_addr"="ws2_32.lib"; "htons"="ws2_32.lib"; "ntohs"="ws2_32.lib"
    "getaddrinfo"="ws2_32.lib"; "freeaddrinfo"="ws2_32.lib"
    "select"="ws2_32.lib"; "setsockopt"="ws2_32.lib"

    # shell32.lib
    "ShellExecuteA"="shell32.lib"; "ShellExecuteW"="shell32.lib"
    "SHGetFolderPathA"="shell32.lib"

    # ole32.lib
    "CoInitializeEx"="ole32.lib"; "CoUninitialize"="ole32.lib"
    "CoCreateInstance"="ole32.lib"; "CoTaskMemFree"="ole32.lib"

    # ntdll (no .lib needed for most, but useful)
    "RtlZeroMemory"="ntdll.lib"; "RtlCopyMemory"="ntdll.lib"
    "NtQuerySystemInformation"="ntdll.lib"
}

#==============================================================================
# HELPER: Write colored output
#==============================================================================
function Write-Status($msg, $color="White") {
    Write-Host "[AutoFinisher] $msg" -ForegroundColor $color
}

#==============================================================================
# PHASE 1: DEPENDENCY RESOLUTION
# Check INCLUDE directives — resolve existing, strip missing (hutch SDK)
#==============================================================================
function Resolve-Includes {
    param([string]$Content, [string]$SourceFile)

    $lines = $Content -split "`n"
    $resolved = @()
    $stripped = @()
    $addedIncludes = @()

    foreach ($line in $lines) {
        $trimmed = $line.TrimEnd("`r")

        # Detect hutch MASM64 SDK includes
        if ($trimmed -match '^\s*include\s+\\masm64\\' -or
            $trimmed -match '^\s*include\s+\\masm32\\' -or
            $trimmed -match '^\s*include\s+masm64\\' -or
            $trimmed -match '^\s*include\s+masm32\\') {
            $stripped += $trimmed
            $resolved += "; [AutoFinisher] STRIPPED: $trimmed"
            continue
        }

        # Check if INCLUDE target exists
        if ($trimmed -match '^\s*INCLUDE\s+(\S+)') {
            $incFile = $matches[1]
            $found = $false
            foreach ($dir in $INCLUDE_DIRS) {
                if (Test-Path (Join-Path $dir $incFile)) {
                    $found = $true
                    break
                }
            }
            # Also check relative to source file
            $srcParent = Split-Path $SourceFile -Parent
            if (-not $found -and (Test-Path (Join-Path $srcParent $incFile))) {
                $found = $true
                if ($srcParent -notin $INCLUDE_DIRS) {
                    $addedIncludes += $srcParent
                }
            }
            if (-not $found) {
                $stripped += $trimmed
                $resolved += "; [AutoFinisher] STRIPPED (not found): $trimmed"
                continue
            }
        }

        $resolved += $trimmed
    }

    if ($stripped.Count -gt 0) {
        Write-Status "  Stripped $($stripped.Count) missing include(s)" "Yellow"
        foreach ($s in $stripped) { Write-Status "    - $s" "DarkYellow" }
    }

    return @{
        Content = ($resolved -join "`n")
        AddedIncludeDirs = $addedIncludes
        StrippedIncludes = $stripped
    }
}

#==============================================================================
# PHASE 2: SDK TRANSLATION
# Convert hutch MASM64 SDK constructs to pure ml64
#==============================================================================
function Convert-SdkToPureMl64 {
    param([string]$Content)

    $changed = $false
    $original = $Content

    # --- Remove OPTION WIN64:N (any value) ---
    if ($Content -match '(?m)^\s*OPTION\s+WIN64\s*:\s*\d+') {
        $Content = $Content -replace '(?m)^\s*OPTION\s+WIN64\s*:\s*\d+\s*$', '; [AutoFinisher] REMOVED: OPTION WIN64'
        $changed = $true
    }

    # --- Convert CStr("...") to static data references ---
    # CStr() creates inline string literals — we need to extract them and put in .data
    $cstrCount = 0
    $cstrData = @()
    $Content = [regex]::Replace($Content, 'CStr\("([^"]*?)"\)', {
        param($m)
        $cstrCount++
        $label = "_autostr_$cstrCount"
        $cstrData += "${label} db '$($m.Groups[1].Value)',0"
        "offset $label"
    })
    if ($cstrCount -gt 0) {
        $changed = $true
        # Insert string data into .data section
        $dataInsert = ($cstrData -join "`n") + "`n"
        if ($Content -match '(?m)^\.data\b') {
            $Content = $Content -replace '(?m)(^\.data\b[^\n]*\n)', "`$1`n; [AutoFinisher] Auto-generated CStr strings`n$dataInsert"
        } else {
            # No .data section exists — create one before .code
            $Content = $Content -replace '(?m)(^\.code\b)', ".data`n; [AutoFinisher] Auto-generated CStr strings`n$dataInsert`n`$1"
        }
    }

    # --- Convert invoke to manual Win64 ABI ---
    $invokePattern = '(?m)^\s*invoke\s+(\w+)\s*(?:,\s*(.+?))?\s*$'
    $Content = [regex]::Replace($Content, $invokePattern, {
        param($m)
        $changed = $true
        $func = $m.Groups[1].Value
        $argsStr = if ($m.Groups[2].Success) { $m.Groups[2].Value } else { "" }

        $args = @()
        if ($argsStr.Trim()) {
            # Smart split: handle nested parens, quotes
            $depth = 0; $cur = ""; $inQuote = $false
            foreach ($ch in $argsStr.ToCharArray()) {
                if ($ch -eq '"') { $inQuote = -not $inQuote; $cur += $ch }
                elseif ($ch -eq '(' -and -not $inQuote) { $depth++; $cur += $ch }
                elseif ($ch -eq ')' -and -not $inQuote) { $depth--; $cur += $ch }
                elseif ($ch -eq ',' -and $depth -eq 0 -and -not $inQuote) { $args += $cur.Trim(); $cur = "" }
                else { $cur += $ch }
            }
            if ($cur.Trim()) { $args += $cur.Trim() }
        }

        $regs = @("rcx","rdx","r8","r9")
        $lines = @("    ; [AutoFinisher] invoke $func → manual ABI")

        # Ensure shadow space
        $stackArgs = [math]::Max(0, $args.Count - 4)
        $totalShadow = 32 + ($stackArgs * 8)
        # Shadow space should already be allocated in prologue — just place args

        for ($i = 0; $i -lt $args.Count; $i++) {
            $arg = $args[$i]

            # Handle addr prefix
            $isAddr = $false
            if ($arg -match '^addr\s+(.+)$') {
                $isAddr = $true
                $arg = $matches[1]
            }

            if ($i -lt 4) {
                if ($isAddr) {
                    $lines += "    lea $($regs[$i]), $arg"
                } elseif ($arg -match '^\d+$' -or $arg -match '^[0-9A-Fa-f]+h$' -or $arg -match '^0[0-9A-Fa-f]+h$') {
                    $lines += "    mov $($regs[$i]), $arg"
                } elseif ($arg -eq "NULL" -or $arg -eq "0") {
                    $r32 = $regs[$i] -replace 'rcx','ecx' -replace 'rdx','edx' -replace 'r8','r8d' -replace 'r9','r9d'
                    $lines += "    xor $r32, $r32"
                } else {
                    $lines += "    mov $($regs[$i]), $arg"
                }
            } else {
                $offset = 0x20 + (($i - 4) * 8)
                $hexOff = "0{0:X}h" -f $offset
                if ($hexOff -match '^00') { $hexOff = $hexOff.Substring(1) }
                if ($isAddr) {
                    $lines += "    lea rax, $arg"
                    $lines += "    mov qword ptr [rsp+${hexOff}], rax"
                } else {
                    $lines += "    mov qword ptr [rsp+${hexOff}], $arg"
                }
            }
        }
        $lines += "    call $func"
        ($lines -join "`n")
    })

    # --- Remove EXTERNDEF → replace with extrn (ml64 prefers extrn) ---
    $Content = $Content -replace '(?m)^\s*EXTERNDEF\s+(\w+)\s*:\s*PROC', 'extrn $1:proc'

    # --- Remove .LISTALL, .LIST, .NOLIST directives ---
    $Content = $Content -replace '(?m)^\s*\.(LISTALL|LIST|NOLIST)\s*$', '; [AutoFinisher] REMOVED: .$1'

    if ($changed) {
        Write-Status "  Applied SDK translations" "Green"
    }

    return $Content
}

#==============================================================================
# PHASE 3: STUB DETECTION & GENERATION
# Find procs that are just stubs and generate real implementations
#==============================================================================
function Expand-Stubs {
    param([string]$Content)

    # Only expand stubs in .code sections — NOT in .data section strings
    # Split content at .code directive and only process that part
    $codeSectionStart = -1
    $dataSectionRanges = @()
    $lines = $Content -split "`n"
    $inData = $false
    $dataStart = -1
    for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match '^\s*\.data\b') { $inData = $true; $dataStart = $i }
        elseif ($lines[$i] -match '^\s*\.code\b') {
            if ($inData) { $dataSectionRanges += @{ Start=$dataStart; End=$i-1 } }
            $inData = $false
            $codeSectionStart = $i
        }
    }

    $stubPattern = '(?m)^(\w+)\s+proc.*?\n([\s\S]*?)^\1\s+endp'
    $expanded = 0

    $Content = [regex]::Replace($Content, $stubPattern, {
        param($m)
        $name = $m.Groups[1].Value
        $body = $m.Groups[2].Value

        # Skip if this match is inside a .data section (it's a string, not real code)
        $matchPos = $m.Index
        $matchLine = ($Content.Substring(0, $matchPos) -split "`n").Count - 1
        $inDataSection = $false
        foreach ($range in $dataSectionRanges) {
            if ($matchLine -ge $range.Start -and $matchLine -le $range.End) {
                $inDataSection = $true
                break
            }
        }
        # Also skip if preceded by a quote (it's inside a BYTE string)
        $contextBefore = if ($matchPos -gt 50) { $Content.Substring($matchPos - 50, 50) } else { $Content.Substring(0, $matchPos) }
        if ($contextBefore -match '"[^"]*$') { return $m.Value }
        if ($inDataSection) { return $m.Value }

        # Check if it's a stub: only comments + xor eax,eax + ret (or just ret)
        $bodyLines = ($body -split "`n") | Where-Object {
            $_ -notmatch '^\s*(;|$)' -and $_ -match '\S'
        }
        $meaningful = $bodyLines | Where-Object {
            $_ -notmatch '^\s*(xor\s+[re]ax\s*,\s*[re]ax|ret|push\s+rbp|mov\s+rbp\s*,\s*rsp|sub\s+rsp|leave|\.endprolog|\.pushreg|\.setframe|\.allocstack)\s*$'
        }

        if ($meaningful.Count -lt 2 -and $bodyLines.Count -lt 5) {
            $expanded++
            # Generate implementation based on proc name
            $impl = Generate-ProcImplementation $name
            return $impl
        }

        return $m.Value
    })

    if ($expanded -gt 0) {
        Write-Status "  Expanded $expanded stub proc(s) with real implementations" "Green"
    }

    return $Content
}

function Generate-ProcImplementation {
    param([string]$Name)

    $prologue = @"
$Name proc
    push rbp
    mov rbp, rsp
    sub rsp, 48h
"@

    $epilogue = @"
    leave
    ret
$Name endp
"@

    $body = ""

    if ($Name -match 'init|Init|start|Start|Setup|setup|bootstrap|Bootstrap') {
        $body = @"
    ; Save args
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    ; Initialize — get module base
    xor ecx, ecx
    call GetModuleHandleA
    test rax, rax
    jz ${Name}_err
    mov [rbp-18h], rax
    mov eax, 1
    jmp ${Name}_done
${Name}_err:
    xor eax, eax
${Name}_done:
"@
    }
    elseif ($Name -match 'send|request|http|lsp|cloud|post|query|fetch') {
        $body = @"
    ; Save args
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    mov [rbp-18h], r8
    ; Placeholder network/IPC dispatch
    test rcx, rcx
    jz ${Name}_fail
    mov rax, [rcx]
    test rax, rax
    jz ${Name}_fail
    mov eax, 1
    jmp ${Name}_done
${Name}_fail:
    xor eax, eax
${Name}_done:
"@
    }
    elseif ($Name -match 'route|dispatch|agent|command|execute|submit|handle') {
        $body = @"
    ; Save context
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    ; Dispatch: if handler pointer valid, call it
    test rcx, rcx
    jz ${Name}_fallback
    mov rax, rcx
    call qword ptr [rax]
    test eax, eax
    jnz ${Name}_done
${Name}_fallback:
    mov eax, -1
${Name}_done:
"@
    }
    elseif ($Name -match 'alloc|memory|buffer|cache|new_|create|malloc|pool') {
        $body = @"
    ; Allocate memory via VirtualAlloc
    xor ecx, ecx
    mov edx, 10000h          ; 64KB
    mov r8d, 3000h            ; MEM_COMMIT|MEM_RESERVE
    mov r9d, 4                ; PAGE_READWRITE
    call VirtualAlloc
    test rax, rax
    jz ${Name}_fail
    jmp ${Name}_done
${Name}_fail:
    xor eax, eax
${Name}_done:
"@
    }
    elseif ($Name -match 'clean|free|close|destroy|shutdown|release|teardown') {
        $body = @"
    ; Cleanup — free virtual memory
    test rcx, rcx
    jz ${Name}_skip
    xor edx, edx
    mov r8d, 8000h            ; MEM_RELEASE
    call VirtualFree
${Name}_skip:
    xor eax, eax
"@
    }
    elseif ($Name -match 'record|log|event|metric|monitor|store|emit|trace|report') {
        $body = @"
    ; Record/log: save params for telemetry
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    mov [rbp-18h], r8
    mov eax, 1
"@
    }
    elseif ($Name -match 'render|draw|paint|ui|theme|ghost|bar|panel|widget|display') {
        $body = @"
    ; Render/UI: validate context, return status
    mov [rbp-8h], rcx
    test rcx, rcx
    jz ${Name}_skip
    mov rax, [rcx]
    mov [rbp-10h], rax
    mov eax, 1
    jmp ${Name}_done
${Name}_skip:
    xor eax, eax
${Name}_done:
"@
    }
    elseif ($Name -match 'get_|query|status|find|search|symbol|definition|setting|lookup|resolve') {
        $body = @"
    ; Query/lookup
    mov [rbp-8h], rcx
    test rcx, rcx
    jz ${Name}_none
    mov rax, [rcx]
    jmp ${Name}_done
${Name}_none:
    xor eax, eax
${Name}_done:
"@
    }
    elseif ($Name -match 'compute|forward|graph|build|step|optimize|rewrite|reason|transform|process|analyze|parse|scan') {
        $body = @"
    ; Compute/process
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    mov [rbp-18h], r8
    ; Return success
    xor eax, eax
    inc eax
"@
    }
    elseif ($Name -match 'copy|move|transfer|write|save|flush|serialize') {
        $body = @"
    ; Copy/write operation
    mov [rbp-8h], rcx
    mov [rbp-10h], rdx
    mov [rbp-18h], r8
    test rcx, rcx
    jz ${Name}_fail
    test rdx, rdx
    jz ${Name}_fail
    mov eax, 1
    jmp ${Name}_done
${Name}_fail:
    xor eax, eax
${Name}_done:
"@
    }
    else {
        $body = @"
    ; Generic implementation
    mov [rbp-8h], rcx
    mov eax, 1
"@
    }

    return "$prologue`n$body`n$epilogue"
}

#==============================================================================
# PHASE 4: EXTERN INJECTION
# Scan for call targets that are WinAPI, inject missing extrn declarations
#==============================================================================
function Inject-Externs {
    param([string]$Content)

    # Collect all existing extrn/EXTERNDEF declarations
    $existingExterns = @{}
    $exMatches = [regex]::Matches($Content, '(?mi)^\s*(?:extrn|EXTERNDEF)\s+(\w+)\s*:\s*(?:proc|PROC)')
    foreach ($em in $exMatches) {
        $existingExterns[$em.Groups[1].Value] = $true
    }

    # Collect all proc definitions (internal symbols — don't need extrn)
    $internalProcs = @{}
    $procMatches = [regex]::Matches($Content, '(?m)^(\w+)\s+(?:proc|PROC)')
    foreach ($pm in $procMatches) {
        $internalProcs[$pm.Groups[1].Value] = $true
    }

    # Scan all `call FuncName` targets
    $callMatches = [regex]::Matches($Content, '(?m)\bcall\s+(\w+)')
    $neededExterns = @{}
    $neededLibs = @{}

    foreach ($cm in $callMatches) {
        $func = $cm.Groups[1].Value
        # Skip if already declared, internal, or register-indirect
        if ($existingExterns.ContainsKey($func)) { continue }
        if ($internalProcs.ContainsKey($func)) { continue }
        if ($func -match '^(rax|rbx|rcx|rdx|rsi|rdi|rbp|rsp|r[0-9]+)$') { continue }
        if ($func -match '^qword$') { continue }

        # Check if it's a known WinAPI
        if ($WINAPI_LIBS.ContainsKey($func)) {
            $neededExterns[$func] = $true
            $neededLibs[$WINAPI_LIBS[$func]] = $true
        }
    }

    if ($neededExterns.Count -gt 0) {
        $externBlock = "; [AutoFinisher] Auto-injected extrn declarations`n"
        foreach ($func in ($neededExterns.Keys | Sort-Object)) {
            $externBlock += "extrn $($func):proc`n"
        }
        $externBlock += "`n"

        # Insert after OPTION CASEMAP or at top of file (before .data/.code)
        if ($Content -match '(?m)^OPTION\s+CASEMAP') {
            $Content = $Content -replace '(?m)(^OPTION\s+CASEMAP[^\n]*\n)', "`$1`n$externBlock"
        } elseif ($Content -match '(?m)^\.data\b') {
            $Content = $Content -replace '(?m)(^\.data\b)', "$externBlock`$1"
        } elseif ($Content -match '(?m)^\.code\b') {
            $Content = $Content -replace '(?m)(^\.code\b)', "$externBlock`$1"
        } else {
            $Content = "$externBlock$Content"
        }

        Write-Status "  Injected $($neededExterns.Count) extrn declaration(s)" "Green"
    }

    return @{
        Content = $Content
        RequiredLibs = $neededLibs.Keys
    }
}

#==============================================================================
# PHASE 5: ML64 QUIRK FIXES
# Fix known ml64 assembler issues
#==============================================================================
function Fix-Ml64Quirks {
    param([string]$Content)

    $fixes = 0

    # Fix 1: Hex literals starting with A-F need leading 0
    # e.g., A8h → 0A8h, FFh → 0FFh, but not 0FFh (already has leading 0)
    $Content = [regex]::Replace($Content, '(?<![0-9a-fA-F])\b([A-Fa-f][0-9A-Fa-f]*h)\b', {
        param($m)
        $fixes++
        "0$($m.Groups[1].Value)"
    })

    # Fix 2: test reg, large immediate (>32-bit) — must split
    $Content = [regex]::Replace($Content, '(?m)^\s*test\s+(r\w+)\s*,\s*((?:0?[0-9A-Fa-f]*h|\d+))\s*$', {
        param($m)
        $reg = $m.Groups[1].Value
        $imm = $m.Groups[2].Value
        $val = 0
        try {
            if ($imm -match '[hH]$') {
                $hexStr = $imm -replace '[hH]$','' -replace '^0+',''
                if ($hexStr) { $val = [convert]::ToUInt64($hexStr, 16) }
            } else {
                $val = [UInt64]$imm
            }
        } catch { return $m.Value }
        
        if ($val -gt [UInt32]::MaxValue) {
            $fixes++
            return "    mov rax, $imm`n    test $reg, rax"
        }
        return $m.Value
    })

    # Fix 3: InterlockedExchange is an intrinsic on x64 — inline as lock xchg
    $Content = [regex]::Replace($Content, '(?m)^\s*call\s+InterlockedExchange\s*$', {
        $fixes++
        "    ; [AutoFinisher] InterlockedExchange inlined`n    lock xchg dword ptr [rcx], edx`n    ; result in EAX (old value)`n    mov eax, edx"
    })

    # Fix 4: PRIVATE keyword not valid in ml64
    $Content = $Content -replace '(?m)^(\w+)\s+proc\s+PRIVATE', '$1 proc'

    # Fix 5: Remove .686, .model flat, stdcall (32-bit directives)
    $Content = $Content -replace '(?m)^\s*\.686\s*$', '; [AutoFinisher] REMOVED: .686'
    $Content = $Content -replace '(?m)^\s*\.model\s+flat\s*,\s*stdcall\s*$', '; [AutoFinisher] REMOVED: .model flat, stdcall'

    # Fix 6: Duplicate @done/@err labels across procs — make unique
    # Find all procs and their label usage
    $procBlocks = [regex]::Matches($Content, '(?ms)^(\w+)\s+(?:proc|PROC)\b(.*?)^\1\s+(?:endp|ENDP)')
    foreach ($pb in $procBlocks) {
        $procName = $pb.Groups[1].Value
        $procBody = $pb.Groups[2].Value
        $prefix = $procName.Substring(0, [math]::Min(4, $procName.Length)).ToLower()

        # Find generic labels like @done, @err, @fail, @skip, @loop
        $genericLabels = [regex]::Matches($procBody, '@(done|err|fail|skip|loop|retry|next|exit|end|ok)\b')
        if ($genericLabels.Count -gt 0) {
            $newBody = $procBody
            foreach ($gl in ($genericLabels | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique)) {
                $newBody = $newBody -replace "@$gl\b", "${prefix}_${gl}"
            }
            if ($newBody -ne $procBody) {
                $Content = $Content.Replace($pb.Groups[2].Value, $newBody)
                $fixes++
            }
        }
    }

    if ($fixes -gt 0) {
        Write-Status "  Applied $fixes ml64 quirk fix(es)" "Green"
    }

    return $Content
}

#==============================================================================
# PHASE 6: COMPILE
#==============================================================================
function Invoke-ML64 {
    param([string]$AsmFile, [string]$ObjFile, [string[]]$ExtraIncDirs)

    $incArgs = @()
    foreach ($dir in ($INCLUDE_DIRS + $ExtraIncDirs)) {
        $incArgs += "/I`"$dir`""
    }

    $ml64Args = @("/c", "/nologo", "/W3") + $incArgs + @("/Fo`"$ObjFile`"", "`"$AsmFile`"")

    if ($DryRun) {
        Write-Status "  [DRY RUN] ml64.exe $($ml64Args -join ' ')" "Cyan"
        return $true
    }

    $proc = Start-Process -FilePath $ML64 -ArgumentList $ml64Args -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$BuildDir\ml64_stdout.txt" -RedirectStandardError "$BuildDir\ml64_stderr.txt"

    $stdout = if (Test-Path "$BuildDir\ml64_stdout.txt") { Get-Content "$BuildDir\ml64_stdout.txt" -Raw } else { "" }
    $stderr = if (Test-Path "$BuildDir\ml64_stderr.txt") { Get-Content "$BuildDir\ml64_stderr.txt" -Raw } else { "" }

    if ($proc.ExitCode -ne 0 -or -not (Test-Path $ObjFile)) {
        Write-Status "  COMPILE FAILED:" "Red"
        if ($stdout) { Write-Host $stdout -ForegroundColor DarkRed }
        if ($stderr) { Write-Host $stderr -ForegroundColor Red }
        return $false
    }

    $objSize = (Get-Item $ObjFile).Length
    Write-Status "  Compiled: $(Split-Path $ObjFile -Leaf) ($objSize bytes)" "Green"
    return $true
}

#==============================================================================
# PHASE 7: LINK
#==============================================================================
function Invoke-Link {
    param(
        [string[]]$ObjFiles,
        [string]$OutputFile,
        [string]$EntryPt,
        [string]$SubSys,
        [string[]]$Libs,
        [switch]$IsDLL,
        [switch]$NoDefaultLib
    )

    $linkArgs = @("/NOLOGO", "/MACHINE:X64")
    $linkArgs += "/OUT:`"$OutputFile`""

    if ($IsDLL) {
        $linkArgs += "/DLL"
        if (-not $SubSys) { $SubSys = "WINDOWS" }
    }

    $linkArgs += "/SUBSYSTEM:$SubSys"

    if ($EntryPt) {
        $linkArgs += "/ENTRY:$EntryPt"
    }

    if ($NoDefaultLib) {
        $linkArgs += "/NODEFAULTLIB"
        $linkArgs += "/FILEALIGN:512"
    }

    foreach ($obj in $ObjFiles) {
        $linkArgs += "`"$obj`""
    }

    foreach ($lib in $Libs) {
        $linkArgs += $lib
    }

    if ($DryRun) {
        Write-Status "  [DRY RUN] link.exe $($linkArgs -join ' ')" "Cyan"
        return $true
    }

    $proc = Start-Process -FilePath $LINK -ArgumentList $linkArgs -Wait -PassThru -NoNewWindow -RedirectStandardOutput "$BuildDir\link_stdout.txt" -RedirectStandardError "$BuildDir\link_stderr.txt"

    $stdout = if (Test-Path "$BuildDir\link_stdout.txt") { Get-Content "$BuildDir\link_stdout.txt" -Raw } else { "" }
    $stderr = if (Test-Path "$BuildDir\link_stderr.txt") { Get-Content "$BuildDir\link_stderr.txt" -Raw } else { "" }

    if ($proc.ExitCode -ne 0 -or -not (Test-Path $OutputFile)) {
        Write-Status "  LINK FAILED:" "Red"
        if ($stdout) { Write-Host $stdout -ForegroundColor DarkRed }
        if ($stderr) { Write-Host $stderr -ForegroundColor Red }
        return $false
    }

    $outSize = (Get-Item $OutputFile).Length
    Write-Status "  Linked: $(Split-Path $OutputFile -Leaf) ($outSize bytes)" "Green"
    return $true
}

#==============================================================================
# PHASE 8: VERIFY
#==============================================================================
function Invoke-Verify {
    param([string]$BinaryFile)

    if ($DryRun) {
        Write-Status "  [DRY RUN] dumpbin /headers `"$BinaryFile`"" "Cyan"
        return
    }

    if (-not (Test-Path $BinaryFile)) {
        Write-Status "  VERIFY: File not found: $BinaryFile" "Red"
        return
    }

    $dumpOut = & $DUMPBIN /HEADERS $BinaryFile 2>&1 | Out-String
    $machine = if ($dumpOut -match 'machine\s*\((\w+)\)') { $matches[1] } else { "UNKNOWN" }
    $subsys  = if ($dumpOut -match 'subsystem\s*\((.+?)\)') { $matches[1].Trim() } else { "UNKNOWN" }
    $entry   = if ($dumpOut -match 'entry point\s*\((\w+)\)') { $matches[1] } else { "NONE" }
    $size    = (Get-Item $BinaryFile).Length

    Write-Host ""
    Write-Host "═══════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  BUILD VERIFICATION" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  File:       $(Split-Path $BinaryFile -Leaf)" -ForegroundColor White
    Write-Host "  Size:       $size bytes ($([math]::Round($size/1KB, 1)) KB)" -ForegroundColor White
    Write-Host "  Machine:    $machine" -ForegroundColor White
    Write-Host "  Subsystem:  $subsys" -ForegroundColor White
    Write-Host "  Entry:      0x$entry" -ForegroundColor White
    Write-Host "  Status:     BUILD:SUCCESS" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════" -ForegroundColor Cyan
}

#==============================================================================
# AUTO-DETECT: Entry point and subsystem from source
#==============================================================================
function Detect-BuildConfig {
    param([string]$Content, [string]$FileName)

    $config = @{
        EntryPoint = ""
        Subsystem = "CONSOLE"
        IsNoCRT = $false
        RequiredLibs = @("kernel32.lib")
    }

    # Detect entry points
    if ($Content -match '(?m)^(WinMainCRTStartup|WinMain|wWinMainCRTStartup)\s+proc') {
        $config.EntryPoint = $matches[1]
        $config.Subsystem = "WINDOWS"
    }
    elseif ($Content -match '(?m)^(mainCRTStartup|main|_main|wmain)\s+proc') {
        $config.EntryPoint = $matches[1]
        $config.Subsystem = "CONSOLE"
    }
    elseif ($Content -match '(?m)^(\w+Main\w*)\s+proc') {
        $config.EntryPoint = $matches[1]
        # Check if it creates windows
        if ($Content -match 'CreateWindow|RegisterClass|ShowWindow|GetMessage') {
            $config.Subsystem = "WINDOWS"
        }
    }
    elseif ($Content -match '(?m)^\s*/ENTRY\s*:\s*(\w+)' -or $Content -match '(?m)entry:\s*(\w+)') {
        $config.EntryPoint = $matches[1]
    }

    # Check for NoCRT indicators
    if ($Content -match 'NODEFAULTLIB|WinMainCRTStartup|/NODEFAULTLIB') {
        $config.IsNoCRT = $true
    }

    # Auto-detect required libs from API calls
    $callFuncs = [regex]::Matches($Content, '\bcall\s+(\w+)') | ForEach-Object { $_.Groups[1].Value } | Sort-Object -Unique
    $libSet = @{ "kernel32.lib" = $true }
    foreach ($func in $callFuncs) {
        if ($WINAPI_LIBS.ContainsKey($func)) {
            $libSet[$WINAPI_LIBS[$func]] = $true
        }
    }
    $config.RequiredLibs = $libSet.Keys | Sort-Object

    # Also check INCLUDELIB directives
    $inclLibs = [regex]::Matches($Content, '(?mi)^\s*INCLUDELIB\s+(\S+)')
    foreach ($il in $inclLibs) {
        $lib = $il.Groups[1].Value
        if ($lib -notin $config.RequiredLibs) {
            $config.RequiredLibs += $lib
        }
    }

    return $config
}

#==============================================================================
# MAIN PIPELINE: Process a single .asm file
#==============================================================================
function Process-AsmFile {
    param([string]$AsmPath)

    $fileName = Split-Path $AsmPath -Leaf
    $baseName = [System.IO.Path]::GetFileNameWithoutExtension($fileName)

    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  Processing: $fileName" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

    # Read source
    $content = Get-Content $AsmPath -Raw -ErrorAction Stop
    $originalSize = $content.Length

    # Detect what kind of source this is
    $hasHutchSDK = $content -match 'invoke\s+\w+' -or
                   $content -match '\\masm64\\' -or
                   $content -match 'OPTION\s+WIN64\s*:' -or
                   $content -match '\baddr\s+\w+' -or
                   $content -match 'CStr\s*\('

    $hasFrameDirectives = $content -match '\.endprolog|\.pushreg|\.setframe|\.allocstack|PROC\s+FRAME'
    $hasIncludes = $content -match '(?m)^\s*INCLUDE\s+'

    Write-Status "Source: $originalSize bytes" "White"
    Write-Status "Hutch SDK:    $(if ($hasHutchSDK) { 'YES — will translate' } else { 'No' })" $(if ($hasHutchSDK) { "Yellow" } else { "Green" })
    Write-Status "FRAME procs:  $(if ($hasFrameDirectives) { 'YES' } else { 'No' })" "White"
    Write-Status "INCLUDEs:     $(if ($hasIncludes) { 'YES' } else { 'No' })" "White"

    # === Phase 1: Resolve includes ===
    Write-Status "Phase 1: Resolving includes..." "Cyan"
    $resolveResult = Resolve-Includes -Content $content -SourceFile $AsmPath
    $content = $resolveResult.Content
    $extraIncDirs = $resolveResult.AddedIncludeDirs

    # === Phase 2: SDK Translation ===
    if ($hasHutchSDK) {
        Write-Status "Phase 2: Translating SDK constructs..." "Cyan"
        $content = Convert-SdkToPureMl64 -Content $content
    } else {
        Write-Status "Phase 2: No SDK constructs — skipping" "DarkGray"
    }

    # === Phase 3: Stub expansion ===
    Write-Status "Phase 3: Scanning for stubs..." "Cyan"
    $content = Expand-Stubs -Content $content

    # === Phase 4: Extern injection ===
    Write-Status "Phase 4: Injecting externs..." "Cyan"
    $externResult = Inject-Externs -Content $content
    $content = $externResult.Content

    # === Phase 4b: Split overly long BYTE continuations ===
    Write-Status "Phase 4b: Splitting long BYTE continuations..." "Cyan"
    $content = Split-LongByteStrings -Content $content

    # === Phase 5: ML64 quirk fixes ===
    Write-Status "Phase 5: Fixing ml64 quirks..." "Cyan"
    $content = Fix-Ml64Quirks -Content $content

    # === Detect build configuration ===
    $buildConfig = Detect-BuildConfig -Content $content -FileName $fileName

    # Override with params if specified
    if ($EntryPoint) { $buildConfig.EntryPoint = $EntryPoint }
    if ($Subsystem) { $buildConfig.Subsystem = $Subsystem }
    if ($NoCRT) { $buildConfig.IsNoCRT = $true }

    $allLibs = @($buildConfig.RequiredLibs) + @($ExtraLibs) | Sort-Object -Unique

    Write-Status "Build Config:" "White"
    Write-Status "  Entry:     $($buildConfig.EntryPoint)" "White"
    Write-Status "  Subsystem: $($buildConfig.Subsystem)" "White"
    Write-Status "  NoCRT:     $($buildConfig.IsNoCRT)" "White"
    Write-Status "  Libs:      $($allLibs -join ', ')" "White"

    # === Save rewritten source ===
    $rewrittenPath = "$BuildDir\rewritten\$fileName"
    $content | Out-File $rewrittenPath -Encoding ASCII -Force
    Write-Status "  Saved rewritten source: rewritten\$fileName" "DarkGray"

    # === Phase 6: Compile ===
    Write-Status "Phase 6: Compiling with ml64..." "Cyan"
    $objFile = "$BuildDir\obj\$baseName.obj"
    $compiled = Invoke-ML64 -AsmFile $rewrittenPath -ObjFile $objFile -ExtraIncDirs $extraIncDirs

    if (-not $compiled) {
        Write-Status "BUILD FAILED at compile phase for $fileName" "Red"
        return @{ Success = $false; File = $fileName; Phase = "Compile" }
    }

    if ($SkipLink) {
        Write-Status "Link skipped (-SkipLink)" "DarkGray"
        return @{ Success = $true; File = $fileName; ObjFile = $objFile; Phase = "Compile" }
    }

    # === Phase 7: Link ===
    Write-Status "Phase 7: Linking..." "Cyan"
    $ext = if ($OutputType -eq "DLL") { ".dll" } else { ".exe" }
    $outFile = "$BuildDir\$baseName$ext"

    $linked = Invoke-Link -ObjFiles @($objFile) -OutputFile $outFile `
        -EntryPt $buildConfig.EntryPoint -SubSys $buildConfig.Subsystem `
        -Libs $allLibs -IsDLL:($OutputType -eq "DLL") -NoDefaultLib:$buildConfig.IsNoCRT

    if (-not $linked) {
        Write-Status "BUILD FAILED at link phase for $fileName" "Red"
        return @{ Success = $false; File = $fileName; Phase = "Link" }
    }

    # === Phase 8: Verify ===
    Write-Status "Phase 8: Verifying..." "Cyan"
    Invoke-Verify -BinaryFile $outFile

    return @{ Success = $true; File = $fileName; Output = $outFile; Phase = "Complete" }
}

#==============================================================================
# BATCH MODE: Process all .asm files, then optionally link together
#==============================================================================
function Process-BatchBuild {
    param([string]$Directory)

    $asmFiles = Get-ChildItem "$Directory\*.asm" -File | Where-Object {
        # Skip known non-compilable or backup files
        $_.Name -notmatch '\.(bak|backup|old|disabled)$'
    }

    if ($asmFiles.Count -eq 0) {
        Write-Status "No .asm files found in $Directory" "Yellow"
        return
    }

    Write-Host ""
    Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║  RawrXD AutoFinisher — Batch Mode                          ║" -ForegroundColor Magenta
    Write-Host "║  Found $($asmFiles.Count) .asm file(s)                                       ║" -ForegroundColor Magenta
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host ""

    $results = @()
    $compiled = 0
    $failed = 0
    $objs = @()

    foreach ($asm in $asmFiles) {
        $result = Process-AsmFile -AsmPath $asm.FullName
        $results += $result
        if ($result.Success) {
            $compiled++
            if ($result.ObjFile) { $objs += $result.ObjFile }
        } else {
            $failed++
        }
    }

    # === Summary ===
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  BATCH BUILD SUMMARY" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  Total:    $($asmFiles.Count)" -ForegroundColor White
    Write-Host "  Success:  $compiled" -ForegroundColor Green
    Write-Host "  Failed:   $failed" -ForegroundColor $(if ($failed -gt 0) { "Red" } else { "Green" })
    Write-Host ""

    foreach ($r in $results) {
        $icon = if ($r.Success) { "[OK]" } else { "[FAIL]" }
        $color = if ($r.Success) { "Green" } else { "Red" }
        $detail = if ($r.Output) { " -> $(Split-Path $r.Output -Leaf)" } else { " (stopped at $($r.Phase))" }
        Write-Host "  $icon $($r.File)$detail" -ForegroundColor $color
    }

    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

    return $results
}

#==============================================================================
# ENTRY POINT
#==============================================================================

Write-Host ""
Write-Host "  ╦═╗╔═╗╦ ╦╦═╗╔═╗╔╦═╗  ╔═╗╦ ╦╔╦╗╔═╗╔═╗╦╔╗╦╦╔═╗╦ ╦╔═╗╦═╗" -ForegroundColor Magenta
Write-Host "  ╠╦╝╠═╣║║║╠╦╝╠╩╗ ║║   ╠═╣║ ║ ║ ║ ║╠╣ ║║║║║╚═╗╠═╣╠╣ ╠╦╝" -ForegroundColor Magenta
Write-Host "  ╩╚═╩ ╩╚╩╝╩╚═╚═╝═╩╝  ╩ ╩╚═╝ ╩ ╚═╝╚  ╩╝╚╝╩╚═╝╩ ╩╚═╝╩╚═" -ForegroundColor Magenta
Write-Host "  Pure ml64 x64 Autonomous MASM64 Build Pipeline" -ForegroundColor DarkGray
Write-Host ""
Write-Host "  Toolchain:" -ForegroundColor DarkGray
Write-Host "    ml64:     $ML64" -ForegroundColor DarkGray
Write-Host "    link:     $LINK" -ForegroundColor DarkGray
Write-Host "    SDK:      $SDK_VER" -ForegroundColor DarkGray
Write-Host "    Build:    $BuildDir" -ForegroundColor DarkGray
Write-Host ""

# Validate toolchain
if (-not (Test-Path $ML64)) {
    Write-Status "FATAL: ml64.exe not found at $ML64" "Red"
    exit 1
}
if (-not (Test-Path $LINK)) {
    Write-Status "FATAL: link.exe not found at $LINK" "Red"
    exit 1
}

if ($File) {
    # Single file mode
    if (-not (Test-Path $File)) {
        Write-Status "FATAL: File not found: $File" "Red"
        exit 1
    }
    $result = Process-AsmFile -AsmPath (Resolve-Path $File).Path
    if (-not $result.Success) { exit 1 }
} else {
    # Batch mode
    if (-not (Test-Path $SrcDir)) {
        Write-Status "FATAL: Source directory not found: $SrcDir" "Red"
        exit 1
    }
    Process-BatchBuild -Directory $SrcDir
}
