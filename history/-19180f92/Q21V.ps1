#!/usr/bin/env pwsh
<#
.SYNOPSIS
Agentic autonomous Win32 build-fix runner for RawrXD.

.DESCRIPTION
Reads a build log file, extracts errors, and processes them one-by-one.
Supports automatic fixes for common build blockers (missing CMake cache, Qt deploy, etc.).

.EXAMPLE
pwsh .\scripts\agentic_build_fix.ps1 -LogPath "C:\Users\HiH8e\OneDrive\Desktop\PS Dlazy init ide cmake --build bui.txt" -RepoRoot "D:\lazy init ide" -AutoFix
#>

param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [string]$RepoRoot = (Get-Location).Path,
    [string]$BuildDir = "build",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Config = "Release",
    [string]$Target = "RawrXD-Win32IDE",
    [int]$MaxErrors = 50,
    [switch]$AutoFix,
    [switch]$AutoWarnings,
    [switch]$DryRun,
    [switch]$FailOnWarnings = $true,
    [switch]$RebuildOnDetect = $true,
    [switch]$TreatWarningsAsErrors = $true
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Section($text) {
    Write-Host ""; Write-Host $text -ForegroundColor Cyan; Write-Host "";
}

function Try-ImportModule($name, $path) {
    if (Test-Path $path) {
        try { Import-Module $path -Force -ErrorAction Stop; return $true } catch { return $false }
    }
    return $false
}

function Normalize-ErrorLine([string]$Line) {
    $clean = $Line -replace '\s+\[.*$',''
    return $clean.Trim()
}

function Get-BuildErrors([string]$Content) {
    $lines = $Content -split "`r?`n"
    $errors = @()
    $seen = @{}

    foreach ($line in $lines) {
        $candidate = $null
        if ($line -match "CMake Error:") {
            $candidate = $line
        } elseif ($line -match "error C\d+" -or $line -match "fatal error" -or $line -match "LNK\d+") {
            $candidate = $line
        } elseif ($line -match "missing CMakeCache.txt" -or $line -match "not a CMake build directory") {
            $candidate = $line
        } elseif ($line -match "0xc0000007b" -or $line -match "Qt6.*dll.*missing" -or $line -match "Qt6.*dll was not found") {
            $candidate = $line
        }

        if ($candidate) {
            $normalized = Normalize-ErrorLine $candidate
            $key = $normalized
            if ($normalized -match '^(?<file>.*)\((?<line>\d+)(,\d+)?\):\s+error\s+(?<code>[A-Z]+\d+):\s+(?<msg>.*)$') {
                $key = "$($matches.file)|$($matches.code)|$($matches.msg)"
            }

            if (-not $seen.ContainsKey($key)) {
                $seen[$key] = $true
                $errors += $normalized
            }
        }
    }

    return $errors | Select-Object -First $MaxErrors
}

function Get-BuildWarnings([string]$Content) {
    $lines = $Content -split "`r?`n"
    $warnings = @()
    $seen = @{}

    foreach ($line in $lines) {
        if ($line -match "warning C\d+") {
            $normalized = Normalize-ErrorLine $line
            $key = $normalized
            if ($normalized -match '^(?<file>.*)\((?<line>\d+)(,\d+)?\):\s+warning\s+(?<code>[A-Z]+\d+):\s+(?<msg>.*)$') {
                $key = "$($matches.file)|$($matches.code)|$($matches.msg)"
            }
            if (-not $seen.ContainsKey($key)) {
                $seen[$key] = $true
                $warnings += $normalized
            }
        }
    }

    return $warnings | Select-Object -First $MaxErrors
}

function Update-FileText {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Pattern,
        [Parameter(Mandatory = $true)][string]$Replacement
    )

    if (-not (Test-Path $Path)) { return $false }
    $content = Get-Content -Path $Path -Raw
    if ($content -notmatch $Pattern) { return $false }

    $updated = $content -replace $Pattern, $Replacement
    if ($DryRun -or -not $AutoWarnings) {
        Write-Host "[DRYRUN] Would update $Path" -ForegroundColor DarkGray
        return $true
    }

    Set-Content -Path $Path -Value $updated -Encoding UTF8
    return $true
}

function Insert-AfterLine {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$MatchLine,
        [Parameter(Mandatory = $true)][string]$InsertText
    )

    if (-not (Test-Path $Path)) { return $false }
    $content = Get-Content -Path $Path -Raw
    if ($content -notmatch [regex]::Escape($MatchLine)) { return $false }
    if ($content -match [regex]::Escape($InsertText)) { return $true }

    $updated = $content -replace [regex]::Escape($MatchLine), ($MatchLine + "`r`n" + $InsertText)
    if ($DryRun -or -not $AutoWarnings) {
        Write-Host "[DRYRUN] Would insert text into $Path" -ForegroundColor DarkGray
        return $true
    }

    Set-Content -Path $Path -Value $updated -Encoding UTF8
    return $true
}

function Invoke-FixStep([string]$Command, [string]$Reason) {
    Write-Host "[ACTION] $Reason" -ForegroundColor Yellow
    Write-Host "  -> $Command" -ForegroundColor DarkGray

    if ($DryRun -or -not $AutoFix) { return }

    Push-Location $RepoRoot
    try {
        Invoke-Expression $Command
    } finally {
        Pop-Location
    }
}

function Invoke-BuildAndCapture {
    param(
        [string]$Reason = "Rebuild with enforcement"
    )

    Write-Host "[BUILD] $Reason" -ForegroundColor Cyan
    $buildLog = Join-Path $RepoRoot (Join-Path $BuildDir "agentic_build_fix_autobuild.log")

    $msbuildArgs = "/m"
    if ($TreatWarningsAsErrors) {
        $msbuildArgs += " /p:TreatWarningsAsErrors=true /p:WarnAsError=true"
    }

    $cmd = "cmake --build `"$BuildDir`" --config `"$Config`" --target $Target -- $msbuildArgs"

    if ($DryRun -or -not $AutoFix) {
        Write-Host "[DRYRUN] Would run: $cmd" -ForegroundColor DarkGray
        return $null
    }

    Push-Location $RepoRoot
    try {
        Invoke-Expression $cmd 2>&1 | Tee-Object -FilePath $buildLog | Out-Host
    } finally {
        Pop-Location
    }

    if (Test-Path $buildLog) {
        Write-Host "[BUILD] Captured log: $buildLog" -ForegroundColor Green
        return $buildLog
    }

    Write-Host "[WARN] Build log not produced" -ForegroundColor DarkYellow
    return $null
}

if (-not (Test-Path $LogPath)) {
    throw "Log file not found: $LogPath"
}

$agentModule = Join-Path $RepoRoot "RawrXD.AutonomousAgent.psm1"
$autonomyModule = Join-Path $RepoRoot "RawrXD.Agentic.Autonomy.psm1"

Try-ImportModule -name "RawrXD.AutonomousAgent" -path $agentModule | Out-Null
Try-ImportModule -name "RawrXD.Agentic.Autonomy" -path $autonomyModule | Out-Null

Write-Section "RawrXD Agentic Build Fix Runner"
Write-Host "Log: $LogPath"
Write-Host "Repo: $RepoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "Config: $Config"
Write-Host "AutoFix: $AutoFix  AutoWarnings: $AutoWarnings  DryRun: $DryRun"

$content = Get-Content -Path $LogPath -Raw
$errors = Get-BuildErrors -Content $content
$warnings = Get-BuildWarnings -Content $content

if (-not $errors -or $errors.Count -eq 0) {
    Write-Host "No build errors detected in log." -ForegroundColor Green
}

if ($warnings -and $warnings.Count -gt 0) {
    Write-Section "Detected Warnings"
    $windex = 0
    foreach ($warn in $warnings) {
        $windex++
        Write-Host "[$windex] $warn" -ForegroundColor DarkYellow
    }
}

Write-Section "Detected Errors"
$index = 0
foreach ($err in $errors) {
    $index++
    Write-Host "[$index] $err" -ForegroundColor Red
}

Write-Section "Processing Fixes"
foreach ($err in $errors) {
    if ($err -match "missing CMakeCache.txt" -or $err -match "not a CMake build directory") {
        Invoke-FixStep -Reason "Configure CMake build directory" -Command "cmake -S `"$RepoRoot`" -B `"$BuildDir`" -G `"$Generator`" -A x64"
        continue
    }

    if ($err -match "Qt6.*dll" -or $err -match "0xc0000007b") {
        Invoke-FixStep -Reason "Build + deploy Qt runtime" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target RawrXD-Deploy"
        continue
    }

    if ($err -match "AutonomousAgent.h" -and $err -match "chrono|steady_clock|map") {
        Invoke-FixStep -Reason "Add missing C++ headers to AutonomousAgent.h" -Command "# Fix: add <chrono>/<map>/<utility> includes in src\\win32app\\AutonomousAgent.h"
        continue
    }

    if ($err -match "Win32IDE.cpp" -and $err -match "syntax error") {
        Invoke-FixStep -Reason "Inspect Win32IDE.cpp syntax error near reported line" -Command "# Fix: open src\\win32app\\Win32IDE.cpp around the error line and remove stray tokens"
        continue
    }

    if ($err -match "LNK\d+") {
        Invoke-FixStep -Reason "Rebuild with verbose linker output" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target $Target -- /v:diag"
        continue
    }

    if ($err -match "error C\d+") {
        Invoke-FixStep -Reason "Rebuild to reproduce compiler error" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target $Target"
        continue
    }

    Write-Host "[INFO] No automatic fix rule for: $err" -ForegroundColor DarkYellow
}

Write-Section "Next Steps"
Write-Host "- Re-run this script with -AutoFix to apply commands automatically." -ForegroundColor Gray
Write-Host "- Add -AutoWarnings to apply warning auto-fixes." -ForegroundColor Gray
Write-Host "- Or run with -DryRun to preview commands." -ForegroundColor Gray

if ($AutoWarnings -and $warnings -and $warnings.Count -gt 0) {
    Write-Section "Applying Warning Fixes"
    $win32IdePath = Join-Path $RepoRoot "src\win32app\Win32IDE.cpp"
    $win32IdeHeader = Join-Path $RepoRoot "src\win32app\Win32IDE.h"
    $sidebarPath = Join-Path $RepoRoot "src\win32app\Win32IDE_Sidebar.cpp"
    $vscodePath = Join-Path $RepoRoot "src\win32app\Win32IDE_VSCodeUI.cpp"

    $fix1 = Update-FileText -Path $win32IdePath `
        -Pattern '\[IDE-Digest\] WM_KEYDOWN: wParam=0x%X \(char=%c\)\\n", wParam, \(wParam >= 32 && wParam < 127\) \? wParam : \?\);' `
        -Replacement '[IDE-Digest] WM_KEYDOWN: wParam=0x%IX (char=%c)\\n", static_cast<UINT_PTR>(wParam), (wParam >= 32 && wParam < 127) ? static_cast<int>(wParam) : "?";'

    $fix2 = Update-FileText -Path $win32IdePath `
        -Pattern '\[IDE-Digest\] SendMessageW returned: %ld\\n", result\);' `
        -Replacement '[IDE-Digest] SendMessageW returned: %Id\\n", static_cast<INT_PTR>(result));'

    $fix3 = Insert-AfterLine -Path $win32IdeHeader `
        -MatchLine '#include <windows.h>' `
        -InsertText '#ifndef HMENU_FROM_ID`r`n#define HMENU_FROM_ID(x) reinterpret_cast<HMENU>(static_cast<INT_PTR>(x))`r`n#endif'

    $fix4 = Update-FileText -Path $win32IdePath `
        -Pattern '#define IDC_BTN_CLOSE 5033' `
        -Replacement '#undef IDC_BTN_CLOSE`r`n#define IDC_BTN_CLOSE 5033'

    $fix5 = Update-FileText -Path $win32IdePath `
        -Pattern '\(\s*HMENU\s*\)\s*([A-Z0-9_]+)' `
        -Replacement 'HMENU_FROM_ID($1)'

    $fix6 = Update-FileText -Path $sidebarPath `
        -Pattern '\(\s*HMENU\s*\)\s*([A-Z0-9_]+)' `
        -Replacement 'HMENU_FROM_ID($1)'

    $fix7 = Update-FileText -Path $vscodePath `
        -Pattern '\(\s*HMENU\s*\)\s*([A-Z0-9_]+)' `
        -Replacement 'HMENU_FROM_ID($1)'

    if (-not ($fix1 -or $fix2 -or $fix3 -or $fix4 -or $fix5 -or $fix6 -or $fix7)) {
        Write-Host "[INFO] No known warning auto-fixes matched." -ForegroundColor DarkYellow
    } else {
        Write-Host "[OK] Applied warning auto-fixes (if not DryRun)." -ForegroundColor Green
    }
}

# Optionally rebuild with warnings-as-errors and re-parse
if ($RebuildOnDetect -and ($AutoFix -or $AutoWarnings) -and ($warnings -and $warnings.Count -gt 0)) {
    $rebuiltLog = Invoke-BuildAndCapture -Reason "Warnings detected; enforcing clean build"
    if ($rebuiltLog) {
        $LogPath = $rebuiltLog
        $content = Get-Content -Path $LogPath -Raw
        $errors = Get-BuildErrors -Content $content
        $warnings = Get-BuildWarnings -Content $content
        if (-not $errors -and -not $warnings) {
            Write-Host "[OK] Clean rebuild succeeded with warnings-as-errors." -ForegroundColor Green
        }
    }
}

if ($errors -and $errors.Count -gt 0) {
    Write-Host "[FAIL] Build errors remain after automation." -ForegroundColor Red
    exit 1
}

if ($FailOnWarnings -and $warnings -and $warnings.Count -gt 0) {
    Write-Host "[FAIL] Warnings remain and FailOnWarnings is set." -ForegroundColor Red
    exit 1
}

Write-Host "[OK] No errors or warnings detected." -ForegroundColor Green
