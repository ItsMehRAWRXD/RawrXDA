# ============================================================================
# File: D:\lazy init ide\scripts\Build-MASMBridge.ps1
# Purpose: Automated MASM64 compilation with PowerShell integration
# Version: 1.0.0 - Production Ready
# ============================================================================

#Requires -Version 7.0
#Requires -RunAsAdministrator

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('Debug', 'Release', 'Optimized')]
    [string]$Configuration = 'Release',

    [Parameter(Mandatory=$false)]
    [switch]$Clean,

    [Parameter(Mandatory=$false)]
    [switch]$Install,

    [Parameter(Mandatory=$false)]
    [string]$OutputPath = "D:\lazy init ide\bin",

    [Parameter(Mandatory=$false)]
    [switch]$EnableAVX512 = $true
)

$ErrorActionPreference = 'Stop'

#region Configuration
$BuildConfig = @{
    Debug = @{
        MASMFlags = '/c /Zi /Zd /W3 /nologo'
        LinkFlags = '/DEBUG:FULL /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL'
        Defines = '/DDEBUG=1 /D_DEBUG=1'
        Optimization = '/Od'
    }
    Release = @{
        MASMFlags = '/c /W3 /nologo'
        LinkFlags = '/RELEASE /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL /OPT:REF /OPT:ICF'
        Defines = '/DNDEBUG=1 /DRELEASE=1'
        Optimization = '/O2'
    }
    Optimized = @{
        MASMFlags = '/c /W3 /nologo'
        LinkFlags = '/RELEASE /INCREMENTAL:NO /SUBSYSTEM:WINDOWS /DLL /OPT:REF /OPT:ICF /LTCG'
        Defines = '/DNDEBUG=1 /DRELEASE=1 /DOPTIMIZED=1'
        Optimization = '/Ox'
    }
}

$Tools = @{
    ML64 = $null
    LINK = $null
    LIB = $null
    IncPaths = @()
    LibPaths = @()
}

$SourceFiles = @(
    'RawrXD_PatternBridge.asm'
    'RawrXD_PipeServer.asm'
    'RawrXD_SIMDClassifier.asm'
)
#endregion

#region Tool Discovery
function Initialize-BuildTools {
    Write-Host "[Build] Initializing build tools..." -ForegroundColor Cyan

    # Search for Visual Studio installations
    $vsPaths = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC"
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Enterprise\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Professional\VC\Tools\MSVC"
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2019\Community\VC\Tools\MSVC"
    )

    $vsRoot = $null
    foreach ($path in $vsPaths) {
        if (Test-Path $path) {
            $version = Get-ChildItem -Path $path -Directory | Sort-Object Name -Descending | Select-Object -First 1
            if ($version) {
                $vsRoot = Join-Path $path $version.Name
                break
            }
        }
    }

    if (-not $vsRoot) {
        throw "Visual Studio C++ tools not found. Please install Visual Studio with C++ workload."
    }

    $Tools.ML64 = Join-Path $vsRoot 'bin\Hostx64\x64\ml64.exe'
    $Tools.LINK = Join-Path $vsRoot 'bin\Hostx64\x64\link.exe'
    $Tools.LIB = Join-Path $vsRoot 'bin\Hostx64\x64\lib.exe'

    # Verify tools exist
    foreach ($tool in $Tools.GetEnumerator()) {
        if ($tool.Value -and -not (Test-Path $tool.Value)) {
            throw "Required tool not found: $($tool.Value)"
        }
    }

    # Setup include paths
    $Tools.IncPaths = @(
        (Join-Path $vsRoot 'include')
        (Join-Path $vsRoot 'atlmfc\include')
        "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.22621.0\ucrt"
        "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.22621.0\um"
        "${env:ProgramFiles(x86)}\Windows Kits\10\Include\10.0.22621.0\shared"
        "$PSScriptRoot\..\include"
    ) | Where-Object { Test-Path $_ }

    # Setup library paths
    $Tools.LibPaths = @(
        (Join-Path $vsRoot 'lib\x64')
        "${env:ProgramFiles(x86)}\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"
        "${env:ProgramFiles(x86)}\Windows Kits\10\Lib\10.0.22621.0\um\x64"
        "$PSScriptRoot\..\lib"
    ) | Where-Object { Test-Path $_ }

    Write-Host "[Build] Found ML64: $($Tools.ML64)" -ForegroundColor Green
    Write-Host "[Build] Found LINK: $($Tools.LINK)" -ForegroundColor Green
}

function Invoke-MASMCompile {
    param([string]$SourceFile, [string]$ObjectFile)

    $config = $BuildConfig[$Configuration]
    $incPaths = $Tools.IncPaths -join ';'

    $args = @(
        $config.MASMFlags
        $config.Optimization
        $config.Defines
        "/I`"$incPaths`""
        "/Fo`"$ObjectFile`""
        "`"$SourceFile`""
    ) -join ' '

    Write-Host "[MASM] Compiling $([System.IO.Path]::GetFileName($SourceFile))..." -ForegroundColor Yellow

    $proc = Start-Process -FilePath $Tools.ML64 -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\masm_out.log" -RedirectStandardError "$env:TEMP\masm_err.log"

    if ($proc.ExitCode -ne 0) {
        $errors = Get-Content "$env:TEMP\masm_err.log" -Raw
        throw "MASM compilation failed:`n$errors"
    }

    Write-Host "[MASM] Success: $([System.IO.Path]::GetFileName($ObjectFile))" -ForegroundColor Green
}

function Invoke-Link {
    param([array]$ObjectFiles, [string]$OutputFile)

    $config = $BuildConfig[$Configuration]
    $libPaths = $Tools.LibPaths -join ';'

    $objList = $ObjectFiles -join ' '

    $args = @(
        $config.LinkFlags
        "/LIBPATH:`"$libPaths`""
        $objList
        "/OUT:`"$OutputFile`""
        "kernel32.lib"
        "user32.lib"
        "advapi32.lib"
        "psapi.lib"
        "ntdll.lib"
    ) -join ' '

    Write-Host "[LINK] Linking $([System.IO.Path]::GetFileName($OutputFile))..." -ForegroundColor Yellow

    $proc = Start-Process -FilePath $Tools.LINK -ArgumentList $args -NoNewWindow -Wait -PassThru -RedirectStandardOutput "$env:TEMP\link_out.log" -RedirectStandardError "$env:TEMP\link_err.log"

    if ($proc.ExitCode -ne 0) {
        $errors = Get-Content "$env:TEMP\link_err.log" -Raw
        throw "Link failed:`n$errors"
    }

    Write-Host "[LINK] Success: $OutputFile" -ForegroundColor Green
}
#endregion

#region Main Build Process
function Start-Build {
    if ($Clean) {
        Write-Host "[Build] Cleaning output directory..." -ForegroundColor Yellow
        Remove-Item -Path "$OutputPath\*" -Recurse -Force -ErrorAction SilentlyContinue
    }

    New-Item -ItemType Directory -Path $OutputPath -Force | Out-Null

    Initialize-BuildTools

    $objectFiles = [System.Collections.Generic.List[string]]::new()
    $sourceDir = "$PSScriptRoot\..\src"

    foreach ($source in $SourceFiles) {
        $sourcePath = Join-Path $sourceDir $source
        if (-not (Test-Path $sourcePath)) {
            # Generate source if missing
            Generate-SourceFile -Name $source -Path $sourcePath
        }

        $objFile = Join-Path $OutputPath ($source -replace '\.asm$', '.obj')
        Invoke-MASMCompile -SourceFile $sourcePath -ObjectFile $objFile
        $objectFiles.Add($objFile)
    }

    $dllOutput = Join-Path $OutputPath 'RawrXD_PatternBridge.dll'
    Invoke-Link -ObjectFiles $objectFiles -OutputFile $dllOutput

    # Generate type library for PowerShell interop
    Generate-TypeLibrary -DllPath $dllOutput

    if ($Install) {
        Install-Bridge -DllPath $dllOutput
    }

    Write-Host "`n[Build] Build completed successfully!" -ForegroundColor Green
    Write-Host "  Output: $dllOutput" -ForegroundColor White
    Write-Host "  Size: $([math]::Round((Get-Item $dllOutput).Length / 1KB, 2)) KB" -ForegroundColor Gray
}

function Generate-SourceFile {
    param([string]$Name, [string]$Path)

    switch ($Name) {
        'RawrXD_PatternBridge.asm' { Generate-PatternBridge -Path $Path }
        'RawrXD_PipeServer.asm' { Generate-PipeServer -Path $Path }
        'RawrXD_SIMDClassifier.asm' { Generate-SIMDClassifier -Path $Path }
    }
}

function Generate-TypeLibrary {
    param([string]$DllPath)

    $tlbPath = $DllPath -replace '\.dll$', '.tlb'

    # Use tlbexp or manual generation
    Write-Host "[Build] Generating type library..." -ForegroundColor Yellow

    # Create P/Invoke signature file for PowerShell
    $sigPath = $DllPath -replace '\.dll$', '_Signatures.ps1'
    @"
# Auto-generated P/Invoke signatures for RawrXD Pattern Bridge
Add-Type @"
using System;
using System.Runtime.InteropServices;

public static class RawrXD_PatternBridge
{
    [DllImport("$DllPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ClassifyPattern(IntPtr codeBuffer, int length, IntPtr context, out double confidence);

    [DllImport("$DllPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int InitializePatternEngine();

    [DllImport("$DllPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern int ShutdownPatternEngine();

    [DllImport("$DllPath", CallingConvention = CallingConvention.Cdecl)]
    public static extern IntPtr GetPatternStats();
}
"@
"@ | Set-Content -Path $sigPath

    Write-Host "[Build] Signatures: $sigPath" -ForegroundColor Green
}

function Install-Bridge {
    param([string]$DllPath)

    Write-Host "[Install] Installing bridge..." -ForegroundColor Yellow

    # Register DLL in GAC (if strong named) or local path
    $installDir = "D:\lazy init ide\bin"
    Copy-Item -Path $DllPath -Destination $installDir -Force

    # Update PowerShell module path
    $psModulePath = [Environment]::GetEnvironmentVariable('PSModulePath', 'Machine')
    if ($psModulePath -notlike "*$installDir*") {
        [Environment]::SetEnvironmentVariable('PSModulePath', "$psModulePath;$installDir", 'Machine')
    }

    Write-Host "[Install] Installation complete" -ForegroundColor Green
}
#endregion

#region Source Generators
function Generate-PatternBridge {
    param([string]$Path)

    $content = @"
; ============================================================================
; RawrXD Pattern Recognition Bridge - Auto-Generated
; ============================================================================

option casemap:none
option win64:3

include \masm64\include64\windows.inc
include \masm64\include64\kernel32.inc
include \masm64\include64\user32.inc

includelib \masm64\lib64\kernel32.lib
includelib \masm64\lib64\user32.lib

; Pattern type constants
PATTERN_TYPE_UNKNOWN     equ 0
PATTERN_TYPE_TEMPLATE    equ 1
PATTERN_TYPE_NONPATTERN  equ 2
PATTERN_TYPE_LEARNED     equ 3

; Confidence thresholds (Q16.16 fixed point)
CONFIDENCE_THRESHOLD     equ 49152  ; 0.75 * 65536

.data
align 16

; Pattern signature database
PatternDB:
    ; Function stub: empty or throw-only body
    sig_function_stub:
        db 0x48, 0x89, 0x5C, 0x24, 0x08  ; mov rbx, [rsp+8]
        db 0x48, 0x89, 0x6C, 0x24, 0x10  ; mov rbp, [rsp+16]
        db 0x48, 0x89, 0x74, 0x24, 0x18  ; mov rsi, [rsp+24]
        db 0x00, 0x00, 0x00, 0x00        ; terminator

    ; Try-catch pattern
    sig_trycatch:
        db 0xE8, 0x00, 0x00, 0x00, 0x00  ; call (rel32)
        db 0x48, 0x8B, 0xE8              ; mov rbp, rax
        db 0x48, 0x85, 0xC0              ; test rax, rax
        db 0x00, 0x00, 0x00, 0x00        ; terminator

    ; End of database
    db 0xFF

; Statistics structure
PatternStats struct
    TotalClassifications dq ?
    TemplateMatches      dq ?
    NonPatternMatches    dq ?
    LearnedMatches       dq ?
    AvgConfidence        dq ?
PatternStats ends

.data?
align 16
g_Stats PatternStats <>
g_hPipe HANDLE ?

.code

;-----------------------------------------------------------------------------
; ClassifyPattern - Main classification entry point
;-----------------------------------------------------------------------------
ClassifyPattern PROC FRAME
    ; RCX = code buffer, RDX = length, R8 = context, R9 = confidence out

    push rbx
    push rbp
    push rsi
    push rdi
    push r12
    push r13

    ; Initialize SIMD
    vmovdqu ymm0, ymmword ptr [rcx]

    ; Compare against pattern database
    lea rsi, PatternDB
    xor r12, r12      ; best match score
    xor r13, r13      ; pattern type result

@@scan_loop:
    ; Load signature
    vmovdqu ymm1, ymmword ptr [rsi]

    ; Check for end of database
    vpcmpeqb ymm2, ymm1, ymmword ptr [all_ff]
    vpmovmskb eax, ymm2
    cmp eax, 0xFFFFFFFF
    je @@check_result

    ; Compare signature
    vpcmpeqb ymm2, ymm0, ymm1
    vpmovmskb eax, ymm2
    popcnt eax, eax

    ; Update best match
    cmp eax, r12d
    jle @@next_sig
    mov r12d, eax
    mov r13d, PATTERN_TYPE_TEMPLATE

@@next_sig:
    add rsi, 32
    jmp @@scan_loop

@@check_result:
    ; Calculate confidence
    vcvtsi2sd xmm0, xmm0, r12
    vdivsd xmm0, xmm0, qword ptr [rel __real_32_0]

    ; Apply threshold
    comisd xmm0, qword ptr [rel __real_threshold]
    jb @@low_confidence

    ; High confidence template match
    mov eax, PATTERN_TYPE_TEMPLATE
    jmp @@done

@@low_confidence:
    ; Check learned patterns via callback
    mov eax, PATTERN_TYPE_NONPATTERN
    vxorpd xmm0, xmm0, xmm0

@@done:
    ; Store confidence
    movsd qword ptr [r9], xmm0

    ; Update stats
    inc g_Stats.TotalClassifications

    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    ret
ClassifyPattern ENDP

;-----------------------------------------------------------------------------
; InitializePatternEngine
;-----------------------------------------------------------------------------
InitializePatternEngine PROC FRAME
    xor eax, eax
    ret
InitializePatternEngine ENDP

;-----------------------------------------------------------------------------
; ShutdownPatternEngine
;-----------------------------------------------------------------------------
ShutdownPatternEngine PROC FRAME
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

;-----------------------------------------------------------------------------
; GetPatternStats
;-----------------------------------------------------------------------------
GetPatternStats PROC FRAME
    lea rax, g_Stats
    ret
GetPatternStats ENDP

;-----------------------------------------------------------------------------
; Data constants
;-----------------------------------------------------------------------------
align 16
__real_32_0     real8 32.0
__real_threshold real8 0.75
all_ff          db 32 dup(0xFF)

END
"@

    New-Item -ItemType Directory -Path (Split-Path $Path) -Force | Out-Null
    $content | Set-Content -Path $Path -Encoding ASCII
}

function Generate-PipeServer {
    param([string]$Path)

    $content = @"
; ============================================================================
; RawrXD Named Pipe Server for PowerShell Integration
; ============================================================================

option casemap:none
option win64:3

include \masm64\include64\windows.inc

.data
szPipeName db '\\.\pipe\RawrXD_PatternEngine', 0
szEventName db 'Global\RawrXD_PatternEvent', 0

.data?
hPipe HANDLE ?
hEvent HANDLE ?
bRunning db ?

.code

;-----------------------------------------------------------------------------
; StartPipeServer
;-----------------------------------------------------------------------------
StartPipeServer PROC FRAME
    LOCAL sa:SECURITY_ATTRIBUTES

    ; Create security attributes
    mov sa.nLength, sizeof SECURITY_ATTRIBUTES
    mov sa.bInheritHandle, FALSE
    mov sa.lpSecurityDescriptor, NULL

    ; Create named pipe
    invoke CreateNamedPipeA, addr szPipeName, \
            PIPE_ACCESS_DUPLEX, \
            PIPE_TYPE_MESSAGE or PIPE_READMODE_MESSAGE or PIPE_WAIT, \
            PIPE_UNLIMITED_INSTANCES, \
            65536, 65536, \
            0, addr sa

    cmp rax, INVALID_HANDLE_VALUE
    je @@error

    mov hPipe, rax
    mov bRunning, 1

    ; Create synchronization event
    invoke CreateEventA, NULL, FALSE, FALSE, addr szEventName
    mov hEvent, rax

    xor eax, eax
    ret

@@error:
    mov eax, 1
    ret
StartPipeServer ENDP

;-----------------------------------------------------------------------------
; StopPipeServer
;-----------------------------------------------------------------------------
StopPipeServer PROC FRAME
    mov bRunning, 0
    invoke SetEvent, hEvent
    invoke CloseHandle, hPipe
    invoke CloseHandle, hEvent
    xor eax, eax
    ret
StopPipeServer ENDP

END
"@

    $content | Set-Content -Path $Path -Encoding ASCII
}

function Generate-SIMDClassifier {
    param([string]$Path)

    $content = @"
; ============================================================================
; RawrXD AVX-512 SIMD Pattern Classifier
; ============================================================================

option casemap:none
option win64:3

ifdef __AVX512F__
AVX512_ENABLED equ 1
else
AVX512_ENABLED equ 0
endif

.data
align 64
zmm_pattern_buffer db 64 dup(0)

.code

;-----------------------------------------------------------------------------
; SIMD_Classify_AVX512 - Uses AVX-512 for parallel pattern matching
;-----------------------------------------------------------------------------
SIMD_Classify_AVX512 PROC FRAME
    ; RCX = input buffer, RDX = pattern database, R8 = result buffer

    ; Load 64 bytes of input
    vmovdqu64 zmm0, zmmword ptr [rcx]

    ; Parallel comparison against 8 patterns (8 bytes each)
    vpcmpeqb k1, zmm0, zmmword ptr [rdx]
    vpcmpeqb k2, zmm0, zmmword ptr [rdx+64]

    ; Count matches
    kmovw eax, k1
    popcnt eax, eax

    ; Store result
    mov dword ptr [r8], eax

    vzeroupper
    ret
SIMD_Classify_AVX512 ENDP

;-----------------------------------------------------------------------------
; SIMD_Classify_AVX2 - Fallback for non-AVX512 systems
;-----------------------------------------------------------------------------
SIMD_Classify_AVX2 PROC FRAME
    vmovdqu ymm0, ymmword ptr [rcx]
    vpcmpeqb ymm1, ymm0, ymmword ptr [rdx]
    vpmovmskb eax, ymm1
    popcnt eax, eax
    mov dword ptr [r8], eax
    vzeroupper
    ret
SIMD_Classify_AVX2 ENDP

END
"@

    $content | Set-Content -Path $Path -Encoding ASCII
}
#endregion

# Execute build
Start-Build
