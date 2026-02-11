# ============================================================================
# File: D:\lazy init ide\scripts\Generate-PatternSources.ps1
# Purpose: Generate MASM stub sources for the Pattern Bridge toolchain
# ============================================================================

#Requires -Version 7.0

param(
    [Parameter(Mandatory=$false)]
    [string]$OutputDir = "D:\lazy init ide\src",

    [Parameter(Mandatory=$false)]
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

function Write-SourceFile {
    param(
        [string]$FilePath,
        [string]$Content,
        [switch]$ForceWrite
    )

    if ((Test-Path $FilePath) -and -not $ForceWrite) {
        Write-Host "[Generate] Skipping existing file: $FilePath" -ForegroundColor Yellow
        return
    }

    New-Item -ItemType Directory -Path (Split-Path $FilePath) -Force | Out-Null
    $Content | Set-Content -Path $FilePath -Encoding ASCII
    Write-Host "[Generate] Wrote: $FilePath" -ForegroundColor Green
}

$timestamp = (Get-Date).ToString('yyyy-MM-dd HH:mm:ss')

$patternBridge = @"
; ============================================================================
; RawrXD Pattern Recognition Bridge - MASM Stub
; Generated: $timestamp
; Toolchain: PowerShell masm64.ps1/link64.ps1 compatible
; Exports (via DEF): DllMain, ClassifyPattern, InitializePatternEngine,
;                    ShutdownPatternEngine, GetPatternStats
; ============================================================================

option casemap:none

.code

DllMain PROC
    mov eax, 1
    ret
DllMain ENDP

ClassifyPattern PROC
    xor eax, eax
    ret
ClassifyPattern ENDP

InitializePatternEngine PROC
    xor eax, eax
    ret
InitializePatternEngine ENDP

ShutdownPatternEngine PROC
    xor eax, eax
    ret
ShutdownPatternEngine ENDP

GetPatternStats PROC
    xor rax, rax
    ret
GetPatternStats ENDP

END
"@

$pipeServer = @"
; ============================================================================
; RawrXD Pipe Server - MASM Stub
; Generated: $timestamp
; Toolchain: PowerShell masm64.ps1/link64.ps1 compatible
; Exports (via DEF): StartPipeServer, StopPipeServer
; ============================================================================

option casemap:none

.code

StartPipeServer PROC
    xor eax, eax
    ret
StartPipeServer ENDP

StopPipeServer PROC
    xor eax, eax
    ret
StopPipeServer ENDP

END
"@

$simdClassifier = @"
; ============================================================================
; RawrXD SIMD Classifier - MASM Stub
; Generated: $timestamp
; Toolchain: PowerShell masm64.ps1/link64.ps1 compatible
; Exports (via DEF): SIMD_Classify
; ============================================================================

option casemap:none

.code

SIMD_Classify PROC
    mov eax, 1
    ret
SIMD_Classify ENDP

END
"@

Write-SourceFile -FilePath (Join-Path $OutputDir 'RawrXD_PatternBridge.asm') -Content $patternBridge -ForceWrite:$Force
Write-SourceFile -FilePath (Join-Path $OutputDir 'RawrXD_PipeServer.asm') -Content $pipeServer -ForceWrite:$Force
Write-SourceFile -FilePath (Join-Path $OutputDir 'RawrXD_SIMDClassifier.asm') -Content $simdClassifier -ForceWrite:$Force
