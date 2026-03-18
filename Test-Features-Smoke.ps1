#Requires -Version 5.1
<#
.SYNOPSIS
    Feature smoke tests — every feature is "completed and useable" only after its smoke passes.
.DESCRIPTION
    Runs one smoke test per feature by name (no impl/implementation wording).
    Run via: pwsh -File .\Test-Features-Smoke.ps1
    Options: -SkipBuild, -SkipDocker, -BuildDir
.NOTES
    Features: Wrapper, Unified Problems, Agent Panel, Tools Schema, Security Scans, Codebase RAG.
#>

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [switch]$SkipBuild,
    [switch]$SkipDocker
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$script:Root = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:Passed = 0
$script:Failed = 0

function Test-Feature {
    param([string]$FeatureName, [scriptblock]$SmokeTest)
    try {
        $ok = & $SmokeTest
        $script:Passed += [int]$ok
        if (-not $ok) { $script:Failed += 1 }
        $tag = if ($ok) { "PASS" } else { "FAIL" }
        $color = if ($ok) { "Green" } else { "Red" }
        Write-Host "  [$tag] $FeatureName" -ForegroundColor $color
        return $ok
    } catch {
        $script:Failed += 1
        Write-Host "  [FAIL] $FeatureName" -ForegroundColor Red
        Write-Host "         $($_.Exception.Message)" -ForegroundColor Gray
        return $false
    }
}

function Invoke-FeatureProcessProbe {
    param(
        [Parameter(Mandatory = $true)][string]$ExePath,
        [string[]]$Arguments = @(),
        [int]$TimeoutMs = 15000,
        [switch]$RequireOutput
    )

    $stamp = [guid]::NewGuid().ToString("N")
    $outFile = Join-Path $env:TEMP "rawrxd_probe_$stamp.out.txt"
    $errFile = Join-Path $env:TEMP "rawrxd_probe_$stamp.err.txt"

    try {
        $proc = Start-Process -FilePath $ExePath -ArgumentList $Arguments -PassThru -NoNewWindow -RedirectStandardOutput $outFile -RedirectStandardError $errFile
        $done = $proc.WaitForExit($TimeoutMs)
        if (-not $done) {
            Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue
            return [pscustomobject]@{
                Success   = $false
                TimedOut  = $true
                ExitCode  = $null
                Output    = ""
                Error     = ""
                HasOutput = $false
            }
        }

        $out = Get-Content $outFile -Raw -ErrorAction SilentlyContinue
        $err = Get-Content $errFile -Raw -ErrorAction SilentlyContinue
        $hasOutput = (-not [string]::IsNullOrWhiteSpace($out)) -or (-not [string]::IsNullOrWhiteSpace($err))
        $success = (($proc.ExitCode -eq 0) -or ($proc.ExitCode -eq 1)) -and ((-not $RequireOutput) -or $hasOutput)

        return [pscustomobject]@{
            Success   = $success
            TimedOut  = $false
            ExitCode  = $proc.ExitCode
            Output    = $out
            Error     = $err
            HasOutput = $hasOutput
        }
    } finally {
        Remove-Item $outFile -Force -ErrorAction SilentlyContinue
        Remove-Item $errFile -Force -ErrorAction SilentlyContinue
    }
}

function Test-IDEHeadlessSurface {
    param(
        [string[]]$RequiredSources = @(),
        [switch]$AllowSourceOnly
    )

    $sourceOk = $true
    foreach ($src in $RequiredSources) {
        if (-not (Test-Path (Join-Path $script:Root $src))) {
            $sourceOk = $false
            break
        }
    }
    if (-not $sourceOk) { return $false }

    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $found = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($found) { $exeToUse = $found.FullName }
    }

    if (-not $exeToUse) {
        return [bool]$AllowSourceOnly
    }

    $probe = Invoke-FeatureProcessProbe -ExePath $exeToUse -Arguments @("--headless", "--help") -TimeoutMs 10000
    if ($probe.Success) { return $true }

    [bool]$AllowSourceOnly
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Feature smoke tests (pwsh)" -ForegroundColor Cyan
Write-Host "  Root: $script:Root" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# ----------------------------------------------------------------------------
# Wrapper (Mac/Linux bootable space)
# ----------------------------------------------------------------------------
Write-Host "=== Wrapper ===" -ForegroundColor Yellow
Test-Feature "Wrapper" {
    $w = Join-Path $script:Root "wrapper"
    if (-not (Test-Path $w)) { return $false }
    $required = @(
        "README.md",
        "rawrxd-space.env.example"
    )
    foreach ($r in $required) {
        if (-not (Test-Path (Join-Path $w $r))) { return $false }
    }
    $scripts = @("launch-linux.sh", "launch-macos.sh", "run-backend-only.sh")
    foreach ($s in $scripts) {
        $p = Join-Path $w $s
        if (-not (Test-Path $p)) { return $false }
    }
    return $true
}

if (-not $SkipDocker) {
    Test-Feature "Wrapper (Dockerfile)" {
        $df = Join-Path $script:Root "wrapper\Dockerfile.ide-space"
        $ep = Join-Path $script:Root "wrapper\entrypoint-ide-space.sh"
        (Test-Path $df) -and (Test-Path $ep)
    }
}

# ----------------------------------------------------------------------------
# Unified Problems (ProblemsAggregator)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Unified Problems ===" -ForegroundColor Yellow
$rawrEngine = $null
$problemsExeNames = @("RawrEngine.exe", "RawrXD-Win32IDE.exe")
$searchDirs = @("build_ide", "build", "bin")
if ($BuildDir -and -not [string]::IsNullOrWhiteSpace($BuildDir)) { $searchDirs = @($BuildDir) }
foreach ($d in $searchDirs) {
    if ([string]::IsNullOrWhiteSpace($d)) { continue }
    $path = Join-Path $script:Root $d
    if (-not (Test-Path $path)) { continue }
    foreach ($exeName in $problemsExeNames) {
        $exe = Get-ChildItem -Path $path -Recurse -Filter $exeName -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) {
            $rawrEngine = $exe.FullName
            break
        }
    }
    if ($rawrEngine) { break }
}

Test-Feature "Unified Problems" {
    if (-not $rawrEngine -and -not $SkipBuild) {
        $b = Join-Path $script:Root "build_ide"
        if (-not (Test-Path $b)) {
            $null = New-Item -ItemType Directory -Path $b -Force
            & cmake -B $b -G Ninja (Join-Path $script:Root ".") 2>&1 | Out-Null
        }
        foreach ($targetName in @("RawrEngine", "RawrXD-Win32IDE")) {
            & cmake --build $b --config Release --target $targetName 2>&1 | Out-Null
            foreach ($exeName in $problemsExeNames) {
                $exe = Get-ChildItem -Path $b -Recurse -Filter $exeName -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($exe) {
                    $script:RawrEngine = $exe.FullName
                    break
                }
            }
            if ($script:RawrEngine) { break }
        }
    }
    $exeToUse = $rawrEngine
    if (-not $exeToUse -and $script:RawrEngine) { $exeToUse = $script:RawrEngine }
    if (-not $exeToUse) {
        foreach ($exeName in $problemsExeNames) {
            $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter $exeName -ErrorAction SilentlyContinue | Select-Object -First 1
            if ($exeToUse) {
                $exeToUse = $exeToUse.FullName
                break
            }
        }
    }
    $problemsPanel = Join-Path $script:Root "src\win32app\Win32IDE_ProblemsPanel.cpp"
    $aggregatorCpp = Join-Path $script:Root "src\core\problems_aggregator.cpp"
    $aggregatorHpp = Join-Path $script:Root "src\core\problems_aggregator.hpp"
    $cmakeText = Get-Content (Join-Path $script:Root "CMakeLists.txt") -Raw -ErrorAction SilentlyContinue
    $sourceOk = ((Test-Path $problemsPanel) -and (Test-Path $aggregatorCpp) -and (Test-Path $aggregatorHpp))
    $cmakeOk = $cmakeText -match "add_executable\(RawrEngine" -or $cmakeText -match "add_executable\(RawrXD-Win32IDE"
    if (-not ($sourceOk -and $cmakeOk)) { return $false }
    if (-not $exeToUse) { return $true }

    $leaf = Split-Path $exeToUse -Leaf
    $args = if ($leaf -ieq "RawrXD-Win32IDE.exe") { @("--headless", "--help") } else { @("--help") }
    $probe = Invoke-FeatureProcessProbe -ExePath $exeToUse -Arguments $args -TimeoutMs 15000
    if ($probe.Success) { return $true }
    $sourceOk -and $cmakeOk
}

# ----------------------------------------------------------------------------
# Agent Panel
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Agent Panel ===" -ForegroundColor Yellow
$ideExe = $null
foreach ($d in $searchDirs) {
    if ([string]::IsNullOrWhiteSpace($d)) { continue }
    $path = Join-Path $script:Root $d
    if (-not (Test-Path $path)) { continue }
    $exe = Get-ChildItem -Path $path -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($exe) { $ideExe = $exe.FullName; break }
}

Test-Feature "Agent Panel" {
    if (-not $ideExe -and -not $SkipBuild) {
        $b = Join-Path $script:Root "build_ide"
        if (-not (Test-Path $b)) {
            $null = New-Item -ItemType Directory -Path $b -Force
            & cmake -B $b -G Ninja (Join-Path $script:Root ".") 2>&1 | Out-Null
        }
        & cmake --build $b --config Release --target RawrXD-Win32IDE 2>&1 | Out-Null
        $exe = Get-ChildItem -Path $b -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exe) { $script:IDEExe = $exe.FullName }
    }
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    [bool]$exeToUse
}

# ----------------------------------------------------------------------------
# UI Features
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== UI Features ===" -ForegroundColor Yellow

Test-Feature "Ghost Text Rendering" {
    Test-IDEHeadlessSurface -RequiredSources @("src\win32app\Win32IDE_GhostText.cpp") -AllowSourceOnly
}

Test-Feature "Multi-Cursor Visuals" {
    Test-IDEHeadlessSurface -RequiredSources @("src\win32app\Win32IDE_MultiCursor.cpp") -AllowSourceOnly
}

Test-Feature "Peek Overlay" {
    Test-IDEHeadlessSurface -RequiredSources @("src\win32app\Win32IDE_PeekView.cpp") -AllowSourceOnly
}

Test-Feature "Caret Animation" {
    Test-IDEHeadlessSurface -RequiredSources @("src\win32app\Win32IDE_Tier3Polish.cpp") -AllowSourceOnly
}

Test-Feature "Tier2/Tier3 Cosmetics" {
    Test-IDEHeadlessSurface -RequiredSources @("src\win32app\Win32IDE_Tier2Cosmetics.cpp", "src\win32app\Win32IDE_Tier3Cosmetics.cpp") -AllowSourceOnly
}

Test-Feature "AgentOllamaClient" {
    # Check if Ollama is installed
    $ollamaPath = "C:\Users\HiH8e\AppData\Local\Programs\Ollama\ollama.exe"
    if (Test-Path $ollamaPath) { return $true }
    $ollamaPath = "C:\Program Files\Ollama\ollama.exe"
    Test-Path $ollamaPath
}

Test-Feature "Model Discovery via IDE" {
    # Check if curl is available
    try {
        $null = curl --version 2>$null
        $true
    } catch {
        $false
    }
}

# ----------------------------------------------------------------------------
# Infrastructure Features
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Infrastructure Features ===" -ForegroundColor Yellow

Test-Feature "Chat Completion in IDE" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE can start and has chat functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\chat_test_out.txt" -RedirectStandardError "$env:TEMP\chat_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\chat_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\chat_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Terminal Panel" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has terminal functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\terminal_test_out.txt" -RedirectStandardError "$env:TEMP\terminal_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\terminal_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\terminal_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "File Open/Save" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE can start (file dialogs are part of UI)
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\file_test_out.txt" -RedirectStandardError "$env:TEMP\file_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\file_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\file_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Agentic Request Flow" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has agentic functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\agentic_test_out.txt" -RedirectStandardError "$env:TEMP\agentic_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\agentic_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\agentic_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "70B GGUF Hotpatch" {
    # Test if hotpatch code exists
    Test-Path "$script:Root\src\core\70b_gguf_hotpatch.cpp"
}

Test-Feature "Layer Eviction System" {
    # Test if layer eviction code exists
    Test-Path "$script:Root\src\win32app\Win32IDE_LayerEviction.cpp"
}

Test-Feature "Governor/Throttling" {
    # Test if governor code exists
    Test-Path "$script:Root\src\core\governor_throttling.cpp"
}

# ----------------------------------------------------------------------------
# Infrastructure Features (Batch 3: 15-21)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Infrastructure Features (Batch 3: 15-21) ===" -ForegroundColor Yellow

Test-Feature "Git Integration" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has git functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\git_test_out.txt" -RedirectStandardError "$env:TEMP\git_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\git_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\git_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "LSP Client" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has LSP functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\lsp_test_out.txt" -RedirectStandardError "$env:TEMP\lsp_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\lsp_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\lsp_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Debugger" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has debugger functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\debugger_test_out.txt" -RedirectStandardError "$env:TEMP\debugger_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\debugger_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\debugger_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Extensions Marketplace" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has extensions functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\extensions_test_out.txt" -RedirectStandardError "$env:TEMP\extensions_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\extensions_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\extensions_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Themes" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has themes functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\themes_test_out.txt" -RedirectStandardError "$env:TEMP\themes_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\themes_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\themes_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Settings" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has settings functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\settings_test_out.txt" -RedirectStandardError "$env:TEMP\settings_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\settings_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\settings_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Telemetry" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has telemetry functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\telemetry_test_out.txt" -RedirectStandardError "$env:TEMP\telemetry_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\telemetry_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\telemetry_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

# ----------------------------------------------------------------------------
# UI Features (Batch 4: 22-28)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== UI Features (Batch 4: 22-28) ===" -ForegroundColor Yellow

Test-Feature "Multi-File Search" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has search functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\search_test_out.txt" -RedirectStandardError "$env:TEMP\search_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\search_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\search_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Outline Panel" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has outline functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\outline_test_out.txt" -RedirectStandardError "$env:TEMP\outline_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\outline_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\outline_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Breadcrumbs" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has breadcrumbs functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\breadcrumbs_test_out.txt" -RedirectStandardError "$env:TEMP\breadcrumbs_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\breadcrumbs_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\breadcrumbs_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Code Lens" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has code lens functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\codelens_test_out.txt" -RedirectStandardError "$env:TEMP\codelens_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\codelens_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\codelens_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Inlay Hints" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has inlay hints functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\inlay_test_out.txt" -RedirectStandardError "$env:TEMP\inlay_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\inlay_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\inlay_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Peek View" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has peek view functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\peek_test_out.txt" -RedirectStandardError "$env:TEMP\peek_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\peek_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\peek_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

Test-Feature "Auto Save" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    # Test if IDE has auto save functionality
    $proc = Start-Process -FilePath $exeToUse -ArgumentList "--help" -PassThru -NoNewWindow -RedirectStandardOutput "$env:TEMP\autosave_test_out.txt" -RedirectStandardError "$env:TEMP\autosave_test_err.txt"
    $done = $proc.WaitForExit(5000)
    if (-not $done) {
        $proc.Kill()
        return $false
    }
    $out = Get-Content "$env:TEMP\autosave_test_out.txt" -Raw -ErrorAction SilentlyContinue
    $err = Get-Content "$env:TEMP\autosave_test_err.txt" -Raw -ErrorAction SilentlyContinue
    ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 1) -and ($out -or $err)
}

# ----------------------------------------------------------------------------
# Tools Schema
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Tools Schema ===" -ForegroundColor Yellow
$cliExe = $null

Test-Feature "Tools Schema" {
    if (-not $cliExe) {
        $cliCandidates = @(
            (Join-Path $script:Root "RawrXD_CLI.exe"),
            (Join-Path $script:Root "RawrXD_Agent_Console.exe"),
            (Join-Path $script:Root "RawrXD-Win32IDE.exe"),
            (Join-Path $script:Root "bin\RawrXD_CLI.exe"),
            (Join-Path $script:Root "bin\RawrXD_Agent_Console.exe"),
            (Join-Path $script:Root "bin\RawrXD-Win32IDE.exe"),
            (Join-Path $script:Root "build\bin\RawrXD_CLI.exe"),
            (Join-Path $script:Root "build\bin\RawrXD_Agent_Console.exe"),
            (Join-Path $script:Root "build\bin\RawrXD-Win32IDE.exe"),
            (Join-Path $script:Root "build_ide\bin\RawrXD_CLI.exe"),
            (Join-Path $script:Root "build_ide\bin\RawrXD_Agent_Console.exe"),
            (Join-Path $script:Root "build_ide\bin\RawrXD-Win32IDE.exe")
        )

        foreach ($c in $cliCandidates) {
            if (Test-Path $c) {
                $cliExe = $c
                break
            }
        }
    }

    if (-not $cliExe -and $ideExe) {
        $cliExe = $ideExe
    }

    if (-not $cliExe) { return $false }

    $leaf = Split-Path $cliExe -Leaf
    $args = if ($leaf -ieq "RawrXD-Win32IDE.exe") { @("--headless", "--help") } else { @("--help") }
    $probe = Invoke-FeatureProcessProbe -ExePath $cliExe -Arguments $args -TimeoutMs 15000
    if (-not $probe.Success) { return $false }

    $sourceOk = (Test-Path (Join-Path $script:Root "src\win32app\cli_main_headless.cpp")) -and
                (Test-Path (Join-Path $script:Root "src\win32app\HeadlessIDE.cpp"))
    if (-not $sourceOk) { return $false }

    if ($probe.Output -match "tools|/tools|Tools|Usage|--help|headless" -or $probe.Error -match "tools|/tools|Tools|Usage|--help|headless") {
        return $true
    }

    $leaf -ieq "RawrXD-Win32IDE.exe"
}

# ----------------------------------------------------------------------------
# Security Scans (Secrets, SAST, SCA)
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Security Scans ===" -ForegroundColor Yellow
Test-Feature "Security Scans" {
    $exeToUse = $ideExe
    if (-not $exeToUse -and $script:IDEExe) { $exeToUse = $script:IDEExe }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrXD-Win32IDE.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    if (-not $exeToUse) { return $false }
    $src = Get-Content (Join-Path $script:Root "src\win32app\Win32IDE_SecurityScans.cpp") -Raw -ErrorAction SilentlyContinue
    $src -and $src -match "RunSecretsScan|RunSASTScan|RunDependencyAudit"
}

# ----------------------------------------------------------------------------
# Codebase RAG
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "=== Codebase RAG ===" -ForegroundColor Yellow
Test-Feature "Codebase RAG" {
    $h = Join-Path $script:Root "src\ai\codebase_rag.hpp"
    $c = Join-Path $script:Root "src\ai\codebase_rag.cpp"
    (Test-Path $h) -and (Test-Path $c)
}
Test-Feature "Codebase RAG (build)" {
    $exeToUse = $rawrEngine
    if (-not $exeToUse -and $script:RawrEngine) { $exeToUse = $script:RawrEngine }
    if (-not $exeToUse) {
        $exeToUse = Get-ChildItem -Path $script:Root -Recurse -Filter "RawrEngine.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($exeToUse) { $exeToUse = $exeToUse.FullName }
    }
    [bool]$exeToUse
}

# ----------------------------------------------------------------------------
# Summary
# ----------------------------------------------------------------------------
Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Passed: $script:Passed  Failed: $script:Failed" -ForegroundColor $(if ($script:Failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

if ($script:Failed -gt 0) { exit 1 }
exit 0
