#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Build-Monolithic.ps1 — Assemble & link the RawrXD monolithic x64 kernel.
    Auto-detects cl.exe, ml64.exe, link.exe via vswhere + directory probing.

.DESCRIPTION
    Assembles every .asm in src\asm\monolithic\ with ml64.exe, then links into
    a single RawrXD_Monolithic.exe (SUBSYSTEM:WINDOWS, ENTRY:WinMainCRTStartup).

        CI smoke hook behavior:
            When CI is detected (CI=true, GITHUB_ACTIONS=true, or TF_BUILD=True),
            headless smoke runs automatically enable the forced-timeout smoke case
            unless explicitly disabled with -SkipCiTimeoutCaseHook.

    Toolchain resolution order:
      1. vswhere.exe (finds any VS 2022/2026 install)
      2. Known custom paths (C:\VS2022Enterprise, D:\Microsoft Visual Studio 2022)
      3. Standard paths (Program Files, Program Files (x86))

    Windows SDK resolution:
      Scans C:\Program Files (x86)\Windows Kits\10\Include\*\um\windows.h,
      picks the newest version.

.PARAMETER Config
    debug   — /Zi /DEBUG  (default)
    release — /O2 /LTCG

.PARAMETER Clean
    Remove build artifacts before building.

.PARAMETER ValidateContracts
    Force enable tool-lane contract validation before assembly.

.PARAMETER SkipValidateContracts
    Skip tool-lane contract validation (validation runs by default).

.PARAMETER RunRtpToolSmoke
    Run lightweight RTP runtime smoke validation after successful link.

.PARAMETER RunAllSmoke
    Run both RTP smoke and headless smoke in one invocation.

.PARAMETER SmokeTimeoutSec
    Timeout in seconds for each smoke invocation (RTP/headless).

.PARAMETER SmokeMaxAttempts
    Retry count per smoke invocation when transient launcher failures are detected.

.PARAMETER SmokeReportRetention
    Number of timestamped smoke reports to keep in build\monolithic\reports.

.PARAMETER SmokeAlertNonPassThreshold
    Consecutive non-pass streak threshold that flips manifest streak alert state.

.PARAMETER SmokeTimingDeltaAlertMs
    Positive wall-time delta threshold (ms) that flips timing regression alert state.

.PARAMETER SmokeSizeDeltaAlertKb
    Positive exe size delta threshold (KB) that flips size regression alert state.

.PARAMETER LinkMaxAttempts
    Retry count for linker stage when transient output file lock failures are detected.

.PARAMETER RunForcedTimeoutSmokeCase
    Run a deterministic timeout probe to validate timeout classification and reporting.

.PARAMETER SkipCiTimeoutCaseHook
    Disable CI auto-hook that enables forced-timeout smoke when running headless smoke in CI.

.EXAMPLE
    .\Build-Monolithic.ps1
    .\Build-Monolithic.ps1 -Config release
    .\Build-Monolithic.ps1 -Clean
    .\Build-Monolithic.ps1 -SkipValidateContracts
    .\Build-Monolithic.ps1 -RunRtpToolSmoke
    .\Build-Monolithic.ps1 -RunAllSmoke
    .\Build-Monolithic.ps1 -RunAllSmoke -SmokeReportRetention 25
    .\Build-Monolithic.ps1 -RunAllSmoke -SmokeAlertNonPassThreshold 2
    .\Build-Monolithic.ps1 -RunAllSmoke -SmokeTimingDeltaAlertMs 75
    .\Build-Monolithic.ps1 -RunAllSmoke -SmokeSizeDeltaAlertKb 1
    .\Build-Monolithic.ps1 -LinkMaxAttempts 3
    .\Build-Monolithic.ps1 -RunHeadlessSmoke -RunForcedTimeoutSmokeCase -SmokeTimeoutSec 5
    $env:CI='true'; .\Build-Monolithic.ps1 -RunHeadlessSmoke
    $env:CI='true'; .\Build-Monolithic.ps1 -RunHeadlessSmoke -SkipCiTimeoutCaseHook
#>
[CmdletBinding()]
param(
    [ValidateSet("debug","release")]
    [string]$Config = "debug",
    [switch]$Clean,
    [switch]$RunAllSmoke,
    [switch]$RunHeadlessSmoke,
    [switch]$RunRtpToolSmoke,
    [switch]$RunForcedTimeoutSmokeCase,
    [switch]$SkipCiTimeoutCaseHook,
    [ValidateRange(5, 600)]
    [int]$SmokeTimeoutSec = 45,
    [ValidateRange(1, 5)]
    [int]$SmokeMaxAttempts = 2,
    [ValidateRange(1, 1000)]
    [int]$SmokeReportRetention = 50,
    [ValidateRange(1, 100)]
    [int]$SmokeAlertNonPassThreshold = 3,
    [ValidateRange(1, 10000)]
    [int]$SmokeTimingDeltaAlertMs = 50,
    [ValidateRange(1, 100000)]
    [double]$SmokeSizeDeltaAlertKb = 5,
    [ValidateRange(1, 10)]
    [int]$LinkMaxAttempts = 3,
    [switch]$ValidateContracts,
    [switch]$SkipValidateContracts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($RunAllSmoke) {
    $RunRtpToolSmoke = $true
    $RunHeadlessSmoke = $true
}
$combinedSmokeMode = [bool]$RunAllSmoke

$isCiEnvironment = (($env:CI -eq "true") -or ($env:GITHUB_ACTIONS -eq "true") -or ($env:TF_BUILD -eq "True"))
$ciTimeoutCaseHookApplied = $false
if (-not $SkipCiTimeoutCaseHook -and $isCiEnvironment -and $RunHeadlessSmoke -and -not $RunForcedTimeoutSmokeCase) {
    $RunForcedTimeoutSmokeCase = $true
    $ciTimeoutCaseHookApplied = $true
    Write-Host "[ci hook] enabling forced-timeout smoke case for CI headless lane." -ForegroundColor Yellow
}
Write-Host ("[ci hook] summary ciDetected={0} runHeadless={1} hookApplied={2} skip={3} runForcedTimeout={4}" -f
    $isCiEnvironment, $RunHeadlessSmoke, $ciTimeoutCaseHookApplied, $SkipCiTimeoutCaseHook, $RunForcedTimeoutSmokeCase) -ForegroundColor DarkGray

if ($ValidateContracts -and $SkipValidateContracts) {
    throw "Conflicting options: use either -ValidateContracts or -SkipValidateContracts, not both."
}

# ============================================================================
# Paths
# ============================================================================
$ScriptDir = $PSScriptRoot
$RootDir   = Resolve-Path (Join-Path $ScriptDir "..\..\..") # D:\rawrxd
$ObjDir    = Join-Path $RootDir "build\monolithic\obj"
$BinDir    = Join-Path $RootDir "build\monolithic\bin"
$OutExe    = Join-Path $BinDir  "RawrXD_Monolithic.exe"
$script:LastLinkLockSnapshotPath = ""
$script:LastLinkLockSnapshotProcessCount = 0
$script:LastLinkLockSnapshotAttempt = 0

# ============================================================================
# Clean
# ============================================================================
if ($Clean) {
    Write-Host "Cleaning monolithic build..." -ForegroundColor Magenta
    if (Test-Path $ObjDir) { Remove-Item -Recurse -Force $ObjDir }
    if (Test-Path $BinDir) { Remove-Item -Recurse -Force $BinDir }
    Write-Host "Clean done." -ForegroundColor Green
    exit 0
}

# ============================================================================
# Locate MSVC Toolchain (vswhere → directory probing)
# ============================================================================
function Find-MSVC {
    # --- Try vswhere first ---
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -property installationPath 2>$null
        if ($vsPath -and (Test-Path "$vsPath\VC\Tools\MSVC")) {
            $ver = Get-ChildItem -Directory "$vsPath\VC\Tools\MSVC" |
                   Sort-Object Name -Descending | Select-Object -First 1
            if ($ver) { return $ver.FullName }
        }
    }

    # --- Fallback: probe known directories ---
    $candidates = @(
        "C:\VS2022Enterprise\VC\Tools\MSVC",
        "D:\Microsoft Visual Studio 2022\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC"
    )
    foreach ($base in $candidates) {
        if (Test-Path $base) {
            $ver = Get-ChildItem -Directory $base |
                   Sort-Object Name -Descending | Select-Object -First 1
            if ($ver) { return $ver.FullName }
        }
    }
    throw "MSVC toolchain not found. Install Visual Studio 2022+ with C++ Desktop workload."
}

function Find-WindowsSDK {
    $sdkBase = "C:\Program Files (x86)\Windows Kits\10"
    if (-not (Test-Path "$sdkBase\Include")) {
        throw "Windows SDK not found at $sdkBase"
    }
    $ver = Get-ChildItem -Directory "$sdkBase\Include" |
           Where-Object { Test-Path "$($_.FullName)\um\windows.h" } |
           Sort-Object Name -Descending | Select-Object -First 1
    if (-not $ver) { throw "No valid Windows SDK version found." }
    return @{ Root = $sdkBase; Ver = $ver.Name }
}

# Resolve
$MSVCRoot = Find-MSVC
$SDK      = Find-WindowsSDK
$SDKRoot  = $SDK.Root
$SDKVer   = $SDK.Ver

$ML64 = Join-Path $MSVCRoot "bin\Hostx64\x64\ml64.exe"
$CL   = Join-Path $MSVCRoot "bin\Hostx64\x64\cl.exe"
$LINK = Join-Path $MSVCRoot "bin\Hostx64\x64\link.exe"

foreach ($tool in @($ML64, $CL, $LINK)) {
    if (-not (Test-Path $tool)) { throw "Missing: $tool" }
}

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " RawrXD Monolithic Kernel Builder"           -ForegroundColor White
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  MSVC:  $MSVCRoot"      -ForegroundColor Gray
Write-Host "  SDK:   $SDKRoot ($SDKVer)" -ForegroundColor Gray
Write-Host "  ml64:  $ML64"          -ForegroundColor Gray
Write-Host "  cl:    $CL"            -ForegroundColor Gray
Write-Host "  link:  $LINK"          -ForegroundColor Gray
Write-Host "  Config: $Config"       -ForegroundColor Gray
Write-Host "============================================" -ForegroundColor Cyan

# ============================================================================
# Environment (INCLUDE / LIB)
# ============================================================================
$env:INCLUDE = [string]::Join(";", @(
    "$MSVCRoot\include",
    "$SDKRoot\Include\$SDKVer\ucrt",
    "$SDKRoot\Include\$SDKVer\shared",
    "$SDKRoot\Include\$SDKVer\um"
))
$env:LIB = [string]::Join(";", @(
    "$MSVCRoot\lib\x64",
    "$MSVCRoot\lib\onecore\x64",
    "$SDKRoot\Lib\$SDKVer\ucrt\x64",
    "$SDKRoot\Lib\$SDKVer\um\x64"
))

# ============================================================================
# Build Flags
# ============================================================================
$MasmFlags = @("/nologo", "/W3", "/c", "/Cx")
if ($Config -eq "debug") {
    $MasmFlags += "/Zi"
    $LinkFlags  = @("/nologo", "/MACHINE:X64", "/SUBSYSTEM:WINDOWS",
                    "/ENTRY:WinMainCRTStartup", "/DEBUG",
                    "/DYNAMICBASE", "/NXCOMPAT", "/LARGEADDRESSAWARE:NO", "/INCREMENTAL:NO")
} else {
    $MasmFlags += "/Zd"
    $LinkFlags  = @("/nologo", "/MACHINE:X64", "/SUBSYSTEM:WINDOWS",
                    "/ENTRY:WinMainCRTStartup", "/LTCG", "/OPT:REF", "/OPT:ICF",
                    "/DYNAMICBASE", "/NXCOMPAT", "/LARGEADDRESSAWARE:NO", "/INCREMENTAL:NO")
}

$LinkLibs = @(
    "kernel32.lib", "user32.lib", "gdi32.lib", "advapi32.lib",
    "shell32.lib", "shlwapi.lib", "comctl32.lib", "comdlg32.lib",
    "ws2_32.lib", "wininet.lib", "ole32.lib"
)

# ============================================================================
# Helpers
# ============================================================================
function Invoke-Tool {
    param([string]$Exe, [string[]]$ToolArgs, [string]$Label)
    Write-Host "  [$Label]" -ForegroundColor Yellow
    $output = & $Exe @ToolArgs 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host ($output -join "`n") -ForegroundColor Red
        throw "$Label failed (exit $LASTEXITCODE)"
    }
    $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
}

function Invoke-MasmWithRetry {
    param(
        [string]$Exe,
        [string[]]$ToolArgs,
        [string]$Label,
        [string]$ObjPath,
        [int]$MaxAttempts = 3
    )

    $attempt = 1
    do {
        Write-Host "  [$Label]" -ForegroundColor Yellow
        $output = & $Exe @ToolArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
            return
        }

        $joined = ($output -join "`n")
        $isA1000Open = ($LASTEXITCODE -eq 1 -and $joined -match "fatal error A1000:cannot open file")
        if ($isA1000Open -and $attempt -lt $MaxAttempts) {
            Write-Host "    [warn] transient MASM output-file lock detected, retrying ($attempt/$MaxAttempts)..." -ForegroundColor Yellow
            if (Test-Path -LiteralPath $ObjPath) {
                try {
                    $quoted = '"' + $ObjPath + '"'
                    cmd /c "del /F /Q $quoted" | Out-Null
                } catch {
                    Write-Host "    [warn] failed to clear locked obj before retry: $($_.Exception.Message)" -ForegroundColor Yellow
                }
            }
            Start-Sleep -Milliseconds 350
            $attempt++
            continue
        }

        Write-Host $joined -ForegroundColor Red
        throw "$Label failed (exit $LASTEXITCODE)"
    } while ($attempt -le $MaxAttempts)
}

function Invoke-LinkWithRetry {
    param(
        [string]$Exe,
        [string[]]$ToolArgs,
        [string]$Label,
        [string]$OutPath,
        [int]$MaxAttempts = 3
    )

    $attempt = 1
    do {
        Write-Host "  [$Label]" -ForegroundColor Yellow
        $output = & $Exe @ToolArgs 2>&1
        if ($LASTEXITCODE -eq 0) {
            $output | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
            return
        }

        $joined = ($output -join "`n")
        $isLnk1104Open = ($LASTEXITCODE -eq 1104 -or $joined -match "fatal error LNK1104: cannot open file")
        if ($isLnk1104Open -and $attempt -lt $MaxAttempts) {
            Write-Host "    [warn] transient linker output-file lock detected, retrying ($attempt/$MaxAttempts)..." -ForegroundColor Yellow

            $exeBase = [System.IO.Path]::GetFileNameWithoutExtension($OutPath)
            $lockingProcs = @(Get-Process -Name $exeBase -ErrorAction SilentlyContinue)

            $lockDiagDir = Join-Path $RootDir "build\monolithic\reports\locks"
            New-Item -ItemType Directory -Force -Path $lockDiagDir | Out-Null
            $lockDiagStamp = Get-Date -Format "yyyyMMdd-HHmmss-fff"
            $lockDiagPath = Join-Path $lockDiagDir ("link_lock_{0}.json" -f $lockDiagStamp)
            $lockProcRows = @()
            foreach ($p in $lockingProcs) {
                $startTime = ""
                $procPath = ""
                try { $startTime = $p.StartTime.ToString("o") } catch { }
                try { $procPath = [string]$p.Path } catch { }
                $lockProcRows += [ordered]@{
                    id = [int]$p.Id
                    name = [string]$p.ProcessName
                    startTime = $startTime
                    path = $procPath
                }
            }

            $lockDiag = [ordered]@{
                timestamp = (Get-Date).ToString("o")
                label = $Label
                attempt = $attempt
                maxAttempts = $MaxAttempts
                outPath = [string]$OutPath
                exitCode = [int]$LASTEXITCODE
                processCount = @($lockProcRows).Count
                processes = @($lockProcRows)
                linkerOutputHead = [string]((@($output) | Select-Object -First 10) -join "`n")
            }
            $lockDiag | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $lockDiagPath -Encoding UTF8
            $script:LastLinkLockSnapshotPath = [string]$lockDiagPath
            $script:LastLinkLockSnapshotProcessCount = [int]@($lockProcRows).Count
            $script:LastLinkLockSnapshotAttempt = [int]$attempt
            Write-Host "    lock_diag=$lockDiagPath" -ForegroundColor DarkGray

            foreach ($proc in $lockingProcs) {
                try {
                    Write-Host "    [warn] stopping locking process $($proc.ProcessName) pid=$($proc.Id) before relink..." -ForegroundColor Yellow
                    Stop-Process -Id $proc.Id -Force -ErrorAction Stop
                } catch {
                    Write-Host "    [warn] failed to stop locking process pid=$($proc.Id): $($_.Exception.Message)" -ForegroundColor Yellow
                }
            }

            if (Test-Path -LiteralPath $OutPath) {
                try {
                    Remove-Item -LiteralPath $OutPath -Force -ErrorAction SilentlyContinue
                } catch {
                    Write-Host "    [warn] failed to clear locked output before retry: $($_.Exception.Message)" -ForegroundColor Yellow
                }
            }
            Start-Sleep -Milliseconds 600
            $attempt++
            continue
        }

        Write-Host $joined -ForegroundColor Red
        throw "$Label failed (exit $LASTEXITCODE)"
    } while ($attempt -le $MaxAttempts)
}

function Invoke-WaitedExe {
    param(
        [string]$ExePath,
        [string[]]$Arguments,
        [string]$Label,
        [int]$TimeoutSec,
        [int]$MaxAttempts = 2
    )
    $timeoutResult = [pscustomobject]@{
        ExitCode = 124
        TimedOut = $true
        Attempts = 1
        CommandLine = ""
        WallMs = 0
    }

    $attempt = 1
    $totalWallMs = 0
    do {
        Write-Host "  [smoke] $Label (timeout=${TimeoutSec}s, attempt=$attempt/$MaxAttempts)" -ForegroundColor Yellow
        $effectiveCmd = "{0} {1}" -f $ExePath, ($Arguments -join " ")
        $attemptStart = Get-Date

        $workingDir = Split-Path -Parent $ExePath
        if ([string]::IsNullOrWhiteSpace($workingDir)) {
            $workingDir = (Get-Location).Path
        }

        $p = Start-Process -FilePath $ExePath -ArgumentList $Arguments -WorkingDirectory $workingDir -PassThru
        Wait-Process -Id $p.Id -Timeout $TimeoutSec -ErrorAction SilentlyContinue
        if (-not $p.HasExited) {
            try {
                $p.Kill()
                Wait-Process -Id $p.Id -Timeout 5 -ErrorAction SilentlyContinue
            } catch {
                Write-Host "    [warn] failed to terminate timed-out process id=$($p.Id): $($_.Exception.Message)" -ForegroundColor Yellow
            }
            $attemptWallMs = [int][math]::Round(((Get-Date) - $attemptStart).TotalMilliseconds)
            $totalWallMs += $attemptWallMs
            Write-Host "    exit=124 (timeout)" -ForegroundColor DarkGray
            $timeoutResult.Attempts = $attempt
            $timeoutResult.CommandLine = $effectiveCmd
            $timeoutResult.WallMs = $totalWallMs
            return $timeoutResult
        }

        $exitCode = [int]$p.ExitCode
        $attemptWallMs = [int][math]::Round(((Get-Date) - $attemptStart).TotalMilliseconds)
        $totalWallMs += $attemptWallMs

        Write-Host "    exit=$exitCode" -ForegroundColor DarkGray
        if ($exitCode -eq -1073741819 -and $attempt -lt $MaxAttempts) {
            Write-Host "    [warn] transient AV (0xC0000005) detected, retrying after backoff..." -ForegroundColor Yellow
            Start-Sleep -Milliseconds 750
            $attempt++
            continue
        }
        return [pscustomobject]@{
            ExitCode = $exitCode
            TimedOut = $false
            Attempts = $attempt
            CommandLine = $effectiveCmd
            WallMs = $totalWallMs
        }
    } while ($attempt -le $MaxAttempts)

    return [pscustomobject]@{
        ExitCode = -1073741819
        TimedOut = $false
        Attempts = $MaxAttempts
        CommandLine = ("{0} {1}" -f $ExePath, ($Arguments -join " "))
        WallMs = $totalWallMs
    }
}

function Format-ExitCode {
    param([int]$Code)
    $u32 = [BitConverter]::ToUInt32([BitConverter]::GetBytes([int]$Code), 0)
    return "signed=$Code hex=0x{0:X8}" -f $u32
}

function Get-ExitCodeKind {
    param([int]$Code)
    switch ($Code) {
        0 { return "success" }
        124 { return "timeout" }
        -1073741819 { return "access-violation" }  # 0xC0000005
        -1073741510 { return "terminated" }        # 0xC000013A
        default {
            if ($Code -lt 0) { return "ntstatus-failure" }
            return "nonzero-exit"
        }
    }
}

function Get-ProcessExitKind {
    param(
        [int]$Code,
        [bool]$TimedOut = $false
    )

    if ($TimedOut) {
        return "timeout"
    }
    return (Get-ExitCodeKind -Code $Code)
}

# ============================================================================
# Create output dirs
# ============================================================================
New-Item -ItemType Directory -Force -Path $ObjDir | Out-Null
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

# ============================================================================
# Optional lane-contract validation gate
# ============================================================================
$runContractValidation = $ValidateContracts -or (-not $SkipValidateContracts)
if ($runContractValidation) {
    $contractScript = Join-Path $ScriptDir "Validate-ToolLaneContracts.ps1"
    if (-not (Test-Path -LiteralPath $contractScript)) {
        throw "Contract validator not found: $contractScript"
    }
    Write-Host "`n=== Validating tool lane contracts ===" -ForegroundColor Cyan
    Invoke-Tool $contractScript @() "contracts gate"
} else {
    Write-Host "`n=== Validating tool lane contracts ===" -ForegroundColor Cyan
    Write-Host "  [contracts gate] skipped by -SkipValidateContracts" -ForegroundColor Yellow
}

# ============================================================================
# Assemble all .asm files
# ============================================================================
Write-Host "`n=== Skipping C++ compile (Cached) ===
     Compiling: quantum_agent_orchestrator.cpp
     # & $CL /c /O2 /EHsc /std:c++17 /Fo$ObjDir\stubs.obj D:\rawrxd\src\agent\quantum_agent_orchestrator.cpp /I D:\rawrxd\src\agent

     Compiling: quantum_agent_orchestrator_thunks.cpp
     # & $CL /c /O2 /EHsc /std:c++17 /Fo$ObjDir\thunks.obj D:\rawrxd\src\agent\quantum_agent_orchestrator_thunks.cpp /I D:\rawrxd\src\agent

=== Assembling monolithic kernel ===" -ForegroundColor Cyan

$excludedAsm = @(
    "kv_pruning_test.asm",   # standalone test
    "pe_test_harness.asm",   # standalone PE test (duplicates WinMainCRTStartup)
    "stress_harness.asm",    # standalone stress test (duplicates g_hInstance/WinMainCRTStartup)
    "stream_bench.asm",      # standalone benchmark (needs all WebView2/ExtHost)
    "ollama_sovereign_proxy.asm",  # standalone proxy (has own main PROC)
    "swarmlink_v2_consensus.asm"   # duplicates SwarmLink_FastCopySIMD symbol in swarmlink_v2.asm
)

$asmAllFiles = @(Get-ChildItem -Path $ScriptDir -Filter "*.asm" -File)
$asmFiles = @($asmAllFiles | Where-Object { $_.Name -notin $excludedAsm })

$objFiles = @()
foreach ($asm in $asmFiles) {
    $objName = [System.IO.Path]::ChangeExtension($asm.Name, ".obj")
    $objPath = Join-Path $ObjDir $objName
    $asmArgs = $MasmFlags + @("/Fo$objPath", $asm.FullName)
    Invoke-MasmWithRetry $ML64 $asmArgs "ml64 $($asm.Name)" $objPath
    $objFiles += $objPath
}

# --- INJECTED LINKER ASSETS ---
$thunksObj = Join-Path $ObjDir "thunks.obj"
if (Test-Path $thunksObj) {
    $objFiles += $thunksObj
} else {
    Write-Host "  [warn] thunks.obj missing; linking without thunk object" -ForegroundColor Yellow
}
# removed redundant stubs.obj

Write-Host "  asm_total=$($asmAllFiles.Count) excluded=$($excludedAsm.Count) assembled=$($objFiles.Count)" -ForegroundColor Green

# ============================================================================
# Link
# ============================================================================
Write-Host "`n=== Linking ===" -ForegroundColor Cyan
$linkArgs = $LinkFlags + @("/OUT:$OutExe") + $objFiles + $LinkLibs
Invoke-LinkWithRetry -Exe $LINK -ToolArgs $linkArgs -Label "link RawrXD_Monolithic.exe" -OutPath $OutExe -MaxAttempts $LinkMaxAttempts

# ============================================================================
# Summary
# ============================================================================
if (Test-Path $OutExe) {
    $sz = [math]::Round((Get-Item $OutExe).Length / 1KB, 1)
    Write-Host "`n============================================" -ForegroundColor Green
    Write-Host " BUILD SUCCESS: $OutExe ($sz KB)" -ForegroundColor White
    Write-Host "============================================" -ForegroundColor Green

    $runAnySmoke = ($RunRtpToolSmoke -or $RunHeadlessSmoke)
    $deferredSmokeFailure = $null
    $smokeReport = $null
    $smokeReportPath = $null
    $smokeReportTimestampedPath = $null
    if ($runAnySmoke) {
        $smokeReportDir = Join-Path $RootDir "build\monolithic\reports"
        New-Item -ItemType Directory -Force -Path $smokeReportDir | Out-Null
        $reportStamp = Get-Date -Format "yyyyMMdd-HHmmss"
        $smokeReportTimestampedPath = Join-Path $smokeReportDir ("smoke_{0}.json" -f $reportStamp)
        $smokeReportPath = Join-Path $smokeReportDir "smoke_latest.json"
        $smokeReport = [ordered]@{
            timestamp = (Get-Date).ToString("o")
            build = [ordered]@{
                config = $Config
                outExe = [string]$OutExe
                sizeKb = $sz
            }
            report = [ordered]@{
                latest = [string]$smokeReportPath
                timestamped = [string]$smokeReportTimestampedPath
            }
            diagnostics = [ordered]@{
                linkLockSnapshotPath = [string]$script:LastLinkLockSnapshotPath
                linkLockSnapshotProcessCount = [int]$script:LastLinkLockSnapshotProcessCount
                linkLockSnapshotAttempt = [int]$script:LastLinkLockSnapshotAttempt
            }
            mode = [ordered]@{
                runAllSmoke = [bool]$RunAllSmoke
                runRtpToolSmoke = [bool]$RunRtpToolSmoke
                runHeadlessSmoke = [bool]$RunHeadlessSmoke
                runForcedTimeoutSmokeCase = [bool]$RunForcedTimeoutSmokeCase
                ciDetected = [bool]$isCiEnvironment
                ciTimeoutCaseHookApplied = [bool]$ciTimeoutCaseHookApplied
                skipCiTimeoutCaseHook = [bool]$SkipCiTimeoutCaseHook
                smokeTimeoutSec = $SmokeTimeoutSec
                smokeMaxAttempts = $SmokeMaxAttempts
                smokeReportRetention = $SmokeReportRetention
                smokeAlertNonPassThreshold = $SmokeAlertNonPassThreshold
                smokeTimingDeltaAlertMs = $SmokeTimingDeltaAlertMs
                smokeSizeDeltaAlertKb = $SmokeSizeDeltaAlertKb
            }
            ciHookSummary = [ordered]@{
                ciDetected = [bool]$isCiEnvironment
                runHeadlessSmoke = [bool]$RunHeadlessSmoke
                hookApplied = [bool]$ciTimeoutCaseHookApplied
                skipHook = [bool]$SkipCiTimeoutCaseHook
                runForcedTimeoutSmokeCase = [bool]$RunForcedTimeoutSmokeCase
            }
            results = [ordered]@{}
        }
    }

    if ($RunRtpToolSmoke) {
        $rtpSmokeScript = Join-Path $ScriptDir "Validate-RtpToolRuntimeSmoke.ps1"
        if (-not (Test-Path -LiteralPath $rtpSmokeScript)) {
            throw "RTP smoke validator not found: $rtpSmokeScript"
        }

        Write-Host "`n=== RTP Runtime Smoke ===" -ForegroundColor Cyan
        Write-Host "  [rtp smoke]" -ForegroundColor Yellow
        $rtpSw = [System.Diagnostics.Stopwatch]::StartNew()
        $rtpOutput = & $rtpSmokeScript -ExePath $OutExe -WorkspaceRoot ([string]$RootDir) -TimeoutSec $SmokeTimeoutSec 2>&1
        $rtpSw.Stop()
        $rtpWallMs = [int]$rtpSw.ElapsedMilliseconds
        $rtpExit = [int]$LASTEXITCODE
        $rtpExitKind = Get-ExitCodeKind -Code $rtpExit
        Write-Host "    rtp_exit_kind=$rtpExitKind" -ForegroundColor DarkGray
        Write-Host "    rtp_wall_ms=$rtpWallMs" -ForegroundColor DarkGray
        if ($smokeReport) {
            $smokeReport.results.rtp = [ordered]@{
                status = $(if ($rtpExit -eq 0) { "pass" } else { "fail" })
                exitCode = $rtpExit
                exitKind = $rtpExitKind
                wallMs = $rtpWallMs
            }
        }
        if ($rtpExit -ne 0) {
            Write-Host ($rtpOutput -join "`n") -ForegroundColor Red
            $deferredSmokeFailure = "rtp smoke failed (exit $rtpExit)"
        } else {
            $rtpOutput | ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        }
    }

    if ($RunHeadlessSmoke -and -not $deferredSmokeFailure) {
        Write-Host "`n=== Headless Smoke ===" -ForegroundColor Cyan
        $smokePath = Join-Path $RootDir "rawrxd_agent_smoke.txt"
        if (-not (Test-Path -LiteralPath $smokePath)) {
            New-Item -ItemType File -Path $smokePath -Force | Out-Null
        }
        # Clear stale content so smoke_head reflects the current run only.
        Set-Content -LiteralPath $smokePath -Value "" -Encoding UTF8
        $smokeFileBefore = (Get-Item -LiteralPath $smokePath).LastWriteTimeUtc

        $agentResult = Invoke-WaitedExe -ExePath $OutExe -Arguments @("--agent-prompt", "--prompt", "smoke") -Label "agent prompt" -TimeoutSec $SmokeTimeoutSec -MaxAttempts $SmokeMaxAttempts
        $benchResult = Invoke-WaitedExe -ExePath $OutExe -Arguments @("--benchmark", "--prompt", "smoke") -Label "benchmark" -TimeoutSec $SmokeTimeoutSec -MaxAttempts $SmokeMaxAttempts
        $agentExit = [int]$agentResult.ExitCode
        $benchExit = [int]$benchResult.ExitCode
        $agentTimedOut = [bool]$agentResult.TimedOut
        $benchTimedOut = [bool]$benchResult.TimedOut
        $agentAttempts = [int]$agentResult.Attempts
        $benchAttempts = [int]$benchResult.Attempts
        $agentCommandLine = [string]$agentResult.CommandLine
        $benchCommandLine = [string]$benchResult.CommandLine
        $agentWallMs = [int]$agentResult.WallMs
        $benchWallMs = [int]$benchResult.WallMs
        $agentExitFmt = Format-ExitCode -Code $agentExit
        $benchExitFmt = Format-ExitCode -Code $benchExit
        $agentExitKind = Get-ProcessExitKind -Code $agentExit -TimedOut $agentTimedOut
        $benchExitKind = Get-ProcessExitKind -Code $benchExit -TimedOut $benchTimedOut
        Write-Host "    agent_exit_kind=$agentExitKind" -ForegroundColor DarkGray
        Write-Host "    bench_exit_kind=$benchExitKind" -ForegroundColor DarkGray
        Write-Host "    agent_timed_out=$agentTimedOut" -ForegroundColor DarkGray
        Write-Host "    bench_timed_out=$benchTimedOut" -ForegroundColor DarkGray
        Write-Host "    agent_attempts=$agentAttempts" -ForegroundColor DarkGray
        Write-Host "    bench_attempts=$benchAttempts" -ForegroundColor DarkGray
        Write-Host "    agent_wall_ms=$agentWallMs" -ForegroundColor DarkGray
        Write-Host "    bench_wall_ms=$benchWallMs" -ForegroundColor DarkGray

        if ($benchExit -ge 0 -and -not $benchTimedOut) {
            Write-Host "    bench_elapsed_ms=$benchExit" -ForegroundColor DarkGray
        }

        $line = ""
        $smokeFileUpdated = $false
        if (Test-Path $smokePath) {
            $smokeFileAfter = (Get-Item -LiteralPath $smokePath).LastWriteTimeUtc
            $smokeFileUpdated = ($smokeFileAfter -gt $smokeFileBefore)
            $line = (Get-Content $smokePath -TotalCount 1 -ErrorAction SilentlyContinue)
            if ($null -eq $line) { $line = "" }
            if (-not $smokeFileUpdated) {
                $line = "[stale] smoke file not updated by agent prompt run"
            }
            Write-Host "    smoke_file=$smokePath" -ForegroundColor DarkGray
            Write-Host "    smoke_file_updated=$smokeFileUpdated" -ForegroundColor DarkGray
            Write-Host "    smoke_head=$line" -ForegroundColor DarkGray
        } else {
            Write-Host "    smoke_file=missing ($smokePath)" -ForegroundColor DarkGray
        }

        $headlessFailed = ($agentExit -ne 0 -or $agentTimedOut -or $benchTimedOut -or $benchExit -lt 0)
        if ($smokeReport) {
            $smokeReport.results.headless = [ordered]@{
                status = $(if ($headlessFailed) { $(if ($combinedSmokeMode) { "warn" } else { "fail" }) } else { "pass" })
                agentExitCode = $agentExit
                agentTimedOut = $agentTimedOut
                agentAttempts = $agentAttempts
                agentExitKind = $agentExitKind
                agentCommandLine = $agentCommandLine
                agentWallMs = $agentWallMs
                benchExitCode = $benchExit
                benchTimedOut = $benchTimedOut
                benchAttempts = $benchAttempts
                benchExitKind = $benchExitKind
                benchCommandLine = $benchCommandLine
                benchWallMs = $benchWallMs
                smokeFile = [string]$smokePath
                smokeFileUpdated = [bool]$smokeFileUpdated
                smokeHead = [string]$line
            }
        }
        if ($headlessFailed) {
            $headlessMsg = "Headless smoke failed (agent: $agentExitFmt, bench: $benchExitFmt)"
            if ($combinedSmokeMode) {
                Write-Host "  [warn] $headlessMsg" -ForegroundColor Yellow
                Write-Host "  [warn] continuing because -RunAllSmoke treats headless as advisory." -ForegroundColor Yellow
            } else {
                throw $headlessMsg
            }
        } else {
            Write-Host "  Headless smoke passed." -ForegroundColor Green
        }
    }

    if ($RunForcedTimeoutSmokeCase -and -not $deferredSmokeFailure) {
        Write-Host "`n=== Forced Timeout Smoke Case ===" -ForegroundColor Cyan

        $shellCmd = Get-Command pwsh -ErrorAction SilentlyContinue
        if ($null -eq $shellCmd) {
            $timeoutProbeExe = "powershell"
        } else {
            $timeoutProbeExe = $shellCmd.Source
        }

        $probeSleepSec = [Math]::Max($SmokeTimeoutSec + 5, 10)
        $probeArgs = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", "Start-Sleep -Seconds $probeSleepSec; exit 0")
        $timeoutProbe = Invoke-WaitedExe -ExePath $timeoutProbeExe -Arguments $probeArgs -Label "forced timeout probe" -TimeoutSec $SmokeTimeoutSec -MaxAttempts 1

        $probeExit = [int]$timeoutProbe.ExitCode
        $probeTimedOut = [bool]$timeoutProbe.TimedOut
        $probeAttempts = [int]$timeoutProbe.Attempts
        $probeExitKind = Get-ProcessExitKind -Code $probeExit -TimedOut $probeTimedOut
        $probeTimeoutEquivalent = ($probeTimedOut -or $probeExitKind -eq "terminated")
        Write-Host "    probe_exit_kind=$probeExitKind" -ForegroundColor DarkGray
        Write-Host "    probe_timed_out=$probeTimedOut" -ForegroundColor DarkGray
        Write-Host "    probe_timeout_equivalent=$probeTimeoutEquivalent" -ForegroundColor DarkGray
        Write-Host "    probe_attempts=$probeAttempts" -ForegroundColor DarkGray

        $forcedTimeoutFailed = (-not $probeTimeoutEquivalent)
        if ($smokeReport) {
            $smokeReport.results.forcedTimeout = [ordered]@{
                status = $(if ($forcedTimeoutFailed) { $(if ($combinedSmokeMode) { "warn" } else { "fail" }) } else { "pass" })
                exitCode = $probeExit
                timedOut = $probeTimedOut
                timeoutEquivalent = $probeTimeoutEquivalent
                attempts = $probeAttempts
                exitKind = $probeExitKind
                commandLine = [string]$timeoutProbe.CommandLine
            }
        }

        if ($forcedTimeoutFailed) {
            $msg = "Forced timeout smoke case failed (exitKind=$probeExitKind, timedOut=$probeTimedOut)"
            if ($combinedSmokeMode) {
                Write-Host "  [warn] $msg" -ForegroundColor Yellow
                Write-Host "  [warn] continuing because -RunAllSmoke treats forced-timeout as advisory." -ForegroundColor Yellow
            } else {
                throw $msg
            }
        } else {
            Write-Host "  Forced-timeout smoke case passed." -ForegroundColor Green
        }
    }

    if ($smokeReport) {
        if ($RunHeadlessSmoke -and $deferredSmokeFailure) {
            $smokeReport.results.headless = [ordered]@{
                status = "skipped"
                reason = "Skipped due to earlier RTP smoke failure."
            }
        }
        if ($RunForcedTimeoutSmokeCase -and $deferredSmokeFailure) {
            $smokeReport.results.forcedTimeout = [ordered]@{
                status = "skipped"
                reason = "Skipped due to earlier RTP smoke failure."
            }
        }
        $smokeJson = $smokeReport | ConvertTo-Json -Depth 6
        $smokeJson | Set-Content -LiteralPath $smokeReportTimestampedPath -Encoding UTF8
        $smokeJson | Set-Content -LiteralPath $smokeReportPath -Encoding UTF8

        $timestampedReports = @(Get-ChildItem -LiteralPath $smokeReportDir -Filter "smoke_*.json" -File |
            Where-Object { $_.Name -match '^smoke_\d{8}-\d{6}\.json$' } |
            Sort-Object LastWriteTime -Descending)
        if (@($timestampedReports).Count -gt $SmokeReportRetention) {
            $toRemove = @($timestampedReports | Select-Object -Skip $SmokeReportRetention)
            foreach ($oldReport in $toRemove) {
                Remove-Item -LiteralPath $oldReport.FullName -Force -ErrorAction SilentlyContinue
            }
            Write-Host "  pruned_reports=$(@($toRemove).Count)" -ForegroundColor DarkGray
        }

        Write-Host "`n=== Smoke Report ===" -ForegroundColor Cyan
        Write-Host "  report_timestamped=$smokeReportTimestampedPath" -ForegroundColor DarkGray
        Write-Host "  report=$smokeReportPath" -ForegroundColor DarkGray
        if (-not [string]::IsNullOrWhiteSpace([string]$script:LastLinkLockSnapshotPath)) {
            Write-Host "  link_lock_snapshot=$($script:LastLinkLockSnapshotPath)" -ForegroundColor DarkGray
        }

        $smokeManifestPath = Join-Path $smokeReportDir "smoke_manifest.json"
        $resultStatuses = @()
        if ($smokeReport.results.Contains('rtp')) {
            $resultStatuses += [string]$smokeReport.results.rtp.status
        }
        if ($smokeReport.results.Contains('headless')) {
            $resultStatuses += [string]$smokeReport.results.headless.status
        }

        $overallStatus = "unknown"
        if ($resultStatuses -contains "fail") {
            $overallStatus = "fail"
        } elseif ($resultStatuses -contains "warn") {
            $overallStatus = "warn"
        } elseif ($resultStatuses -contains "skipped") {
            $overallStatus = "partial"
        } elseif (@($resultStatuses).Count -gt 0) {
            $overallStatus = "pass"
        }

        $manifestEntry = [ordered]@{
            timestamp = [string]$smokeReport.timestamp
            status = $overallStatus
            reportTimestamped = [string]$smokeReportTimestampedPath
            reportLatest = [string]$smokeReportPath
            config = $Config
            sizeKb = [double]$sz
            runAllSmoke = [bool]$RunAllSmoke
            runRtpToolSmoke = [bool]$RunRtpToolSmoke
            runHeadlessSmoke = [bool]$RunHeadlessSmoke
            runForcedTimeoutSmokeCase = [bool]$RunForcedTimeoutSmokeCase
            ciDetected = [bool]$isCiEnvironment
            ciTimeoutCaseHookApplied = [bool]$ciTimeoutCaseHookApplied
            skipCiTimeoutCaseHook = [bool]$SkipCiTimeoutCaseHook
        }
        $entryMetrics = [ordered]@{}
        if ($smokeReport.results.Contains('rtp') -and $smokeReport.results.rtp.Contains('wallMs')) {
            $entryMetrics.rtpWallMs = [int]$smokeReport.results.rtp.wallMs
        }
        if ($smokeReport.results.Contains('headless')) {
            if ($smokeReport.results.headless.Contains('agentWallMs')) {
                $entryMetrics.agentWallMs = [int]$smokeReport.results.headless.agentWallMs
            }
            if ($smokeReport.results.headless.Contains('benchWallMs')) {
                $entryMetrics.benchWallMs = [int]$smokeReport.results.headless.benchWallMs
            }
        }
        if (@($entryMetrics.Keys).Count -gt 0) {
            $manifestEntry.metrics = $entryMetrics
        }

        $history = @()
        if (Test-Path -LiteralPath $smokeManifestPath) {
            try {
                $manifestRaw = Get-Content -LiteralPath $smokeManifestPath -Raw -ErrorAction Stop
                $manifestObj = $manifestRaw | ConvertFrom-Json -ErrorAction Stop
                if ($null -ne $manifestObj.history) {
                    $history = @($manifestObj.history)
                }
            } catch {
                Write-Host "  [warn] smoke manifest parse failed, rewriting fresh manifest." -ForegroundColor Yellow
            }
        }

        $history = @($manifestEntry) + @($history | Where-Object {
            $_.reportTimestamped -ne $manifestEntry.reportTimestamped
        })
        if (@($history).Count -gt $SmokeReportRetention) {
            $history = @($history | Select-Object -First $SmokeReportRetention)
        }

        $sizeDeltaKb = $null
        if (@($history).Count -gt 1) {
            $prevSzEntry = $history[1]
            $prevSz = $null
            if ($prevSzEntry -is [System.Collections.IDictionary]) {
                if ($prevSzEntry.Contains('sizeKb')) { $prevSz = [double]$prevSzEntry['sizeKb'] }
            } elseif ($null -ne $prevSzEntry.PSObject.Properties['sizeKb']) {
                $prevSz = [double]$prevSzEntry.sizeKb
            }
            if ($null -ne $prevSz) {
                $sizeDeltaKb = [math]::Round([double]$sz - $prevSz, 1)
            }
        }

        $sizeRegressionAlert = $false
        if ($null -ne $sizeDeltaKb -and [double]$sizeDeltaKb -ge [double]$SmokeSizeDeltaAlertKb) {
            $sizeRegressionAlert = $true
        }

        $timingDelta = [ordered]@{}
        $timingDeltaBase = [ordered]@{}
        $timingDeltaBaseConfig = [ordered]@{}
        if (@($history).Count -gt 1) {
            $currentEntry = $history[0]

            $getEntryMetricValue = {
                param($entryObj, [string]$name)
                if ($null -eq $entryObj) {
                    return $null
                }

                $hasMetrics = $false
                if ($entryObj -is [System.Collections.IDictionary]) {
                    $hasMetrics = $entryObj.Contains('metrics')
                } else {
                    $hasMetrics = ($null -ne $entryObj.PSObject.Properties['metrics'])
                }
                if (-not $hasMetrics) {
                    return $null
                }

                $metricObj = $entryObj.metrics
                if ($null -eq $metricObj) {
                    return $null
                }
                if ($metricObj -is [System.Collections.IDictionary]) {
                    if ($metricObj.Contains($name)) {
                        return [int]$metricObj[$name]
                    }
                    return $null
                }
                if ($null -ne $metricObj.PSObject.Properties[$name]) {
                    return [int]$metricObj.$name
                }
                return $null
            }

            foreach ($metricName in @('rtpWallMs', 'agentWallMs', 'benchWallMs')) {
                $currentValue = & $getEntryMetricValue $currentEntry $metricName
                if ($null -eq $currentValue) {
                    continue
                }

                $previousValue = $null
                $previousTimestamp = ""
                $previousConfig = ""
                for ($i = 1; $i -lt @($history).Count; $i++) {
                    $candidateValue = & $getEntryMetricValue $history[$i] $metricName
                    if ($null -ne $candidateValue) {
                        $previousValue = $candidateValue
                        $previousTimestamp = [string]$history[$i].timestamp
                        $previousConfig = [string]$history[$i].config
                        if ($history[$i].timestamp -is [datetime]) {
                            $previousTimestamp = $history[$i].timestamp.ToString("o")
                        }
                        break
                    }
                }

                if ($null -ne $previousValue) {
                    $timingDelta[$metricName] = ([int]$currentValue - [int]$previousValue)
                    $timingDeltaBase[$metricName] = $previousTimestamp
                    $timingDeltaBaseConfig[$metricName] = $previousConfig
                }
            }
        }

        $statusCounts = [ordered]@{
            pass = 0
            warn = 0
            fail = 0
            partial = 0
            unknown = 0
        }
        foreach ($entry in $history) {
            $k = [string]$entry.status
            if ([string]::IsNullOrWhiteSpace($k)) {
                $k = "unknown"
            }
            if (-not $statusCounts.Contains($k)) {
                $k = "unknown"
            }
            $statusCounts[$k] = [int]$statusCounts[$k] + 1
        }

        $getEntryBoolField = {
            param($entryObj, [string]$name)
            if ($null -eq $entryObj) {
                return $false
            }
            if ($entryObj -is [System.Collections.IDictionary]) {
                if ($entryObj.Contains($name) -and $null -ne $entryObj[$name]) {
                    return [bool]$entryObj[$name]
                }
                return $false
            }
            if ($null -ne $entryObj.PSObject.Properties[$name]) {
                return [bool]$entryObj.$name
            }
            return $false
        }

        $ciHookCounts = [ordered]@{
            ciRuns = 0
            ciHookAppliedRuns = 0
            ciHookSkippedRuns = 0
            forcedTimeoutRuns = 0
            forcedTimeoutViaCiHookRuns = 0
        }
        foreach ($entry in @($history)) {
            $entryCiDetected = (& $getEntryBoolField $entry 'ciDetected')
            $entryHookApplied = (& $getEntryBoolField $entry 'ciTimeoutCaseHookApplied')
            $entrySkipHook = (& $getEntryBoolField $entry 'skipCiTimeoutCaseHook')
            $entryForcedTimeout = (& $getEntryBoolField $entry 'runForcedTimeoutSmokeCase')

            if ($entryCiDetected) {
                $ciHookCounts.ciRuns = [int]$ciHookCounts.ciRuns + 1
            }
            if ($entryHookApplied) {
                $ciHookCounts.ciHookAppliedRuns = [int]$ciHookCounts.ciHookAppliedRuns + 1
            }
            if ($entryCiDetected -and $entrySkipHook) {
                $ciHookCounts.ciHookSkippedRuns = [int]$ciHookCounts.ciHookSkippedRuns + 1
            }
            if ($entryForcedTimeout) {
                $ciHookCounts.forcedTimeoutRuns = [int]$ciHookCounts.forcedTimeoutRuns + 1
            }
            if ($entryHookApplied -and $entryForcedTimeout) {
                $ciHookCounts.forcedTimeoutViaCiHookRuns = [int]$ciHookCounts.forcedTimeoutViaCiHookRuns + 1
            }
        }

        $ciHookRatios = [ordered]@{
            hookAppliedPctOfCiRuns = $(if ($ciHookCounts.ciRuns -gt 0) { [math]::Round((100.0 * $ciHookCounts.ciHookAppliedRuns) / $ciHookCounts.ciRuns, 2) } else { 0.0 })
            hookSkippedPctOfCiRuns = $(if ($ciHookCounts.ciRuns -gt 0) { [math]::Round((100.0 * $ciHookCounts.ciHookSkippedRuns) / $ciHookCounts.ciRuns, 2) } else { 0.0 })
            forcedTimeoutPctOfCiRuns = $(if ($ciHookCounts.ciRuns -gt 0) { [math]::Round((100.0 * $ciHookCounts.forcedTimeoutRuns) / $ciHookCounts.ciRuns, 2) } else { 0.0 })
            forcedTimeoutViaHookPctOfForcedTimeoutRuns = $(if ($ciHookCounts.forcedTimeoutRuns -gt 0) { [math]::Round((100.0 * $ciHookCounts.forcedTimeoutViaCiHookRuns) / $ciHookCounts.forcedTimeoutRuns, 2) } else { 0.0 })
        }

        $consecutiveNonPass = 0
        foreach ($entry in @($history)) {
            $st = [string]$entry.status
            if ($st -eq "pass") {
                break
            }
            $consecutiveNonPass++
        }

        $lastNonPassTimestamp = ""
        foreach ($entry in @($history)) {
            $st = [string]$entry.status
            if ($st -ne "pass") {
                $lastNonPassTimestamp = [string]$entry.timestamp
                if ($entry.timestamp -is [datetime]) {
                    $lastNonPassTimestamp = $entry.timestamp.ToString("o")
                }
                break
            }
        }

        $consecutivePass = 0
        foreach ($entry in @($history)) {
            $st = [string]$entry.status
            if ($st -ne "pass") {
                break
            }
            $consecutivePass++
        }

        $lastFailTimestamp = ""
        foreach ($entry in @($history)) {
            $st = [string]$entry.status
            if ($st -eq "fail") {
                $lastFailTimestamp = [string]$entry.timestamp
                if ($entry.timestamp -is [datetime]) {
                    $lastFailTimestamp = $entry.timestamp.ToString("o")
                }
                break
            }
        }

        $streakAlert = ($consecutiveNonPass -ge $SmokeAlertNonPassThreshold)

        $timingRegressionAlert = $false
        $timingDeltaPeakMs = 0
        $timingTriggeredMetrics = New-Object System.Collections.Generic.List[string]
        $timingTriggeredDetails = @()
        foreach ($delta in @($timingDelta.Values)) {
            $d = [int]$delta
            if ($d -gt $timingDeltaPeakMs) {
                $timingDeltaPeakMs = $d
            }
        }
        if ($timingDeltaPeakMs -ge $SmokeTimingDeltaAlertMs) {
            $timingRegressionAlert = $true
        }
        foreach ($k in @('rtpWallMs', 'agentWallMs', 'benchWallMs')) {
            if ($timingDelta.Contains($k) -and ([int]$timingDelta[$k] -ge $SmokeTimingDeltaAlertMs)) {
                $timingTriggeredMetrics.Add($k) | Out-Null
                $baseTs = ""
                if ($timingDeltaBase.Contains($k)) {
                    $baseTs = [string]$timingDeltaBase[$k]
                }
                $baseCfg = ""
                if ($timingDeltaBaseConfig.Contains($k)) {
                    $baseCfg = [string]$timingDeltaBaseConfig[$k]
                }
                $timingTriggeredDetails += [pscustomobject]@{
                    metric = $k
                    deltaMs = [int]$timingDelta[$k]
                    baseTimestamp = $baseTs
                    baseConfig = $baseCfg
                }
            }
        }

        $healthLevel = "ok"
        if ($overallStatus -eq "fail") {
            $healthLevel = "fail"
        } elseif ($overallStatus -eq "warn" -or $overallStatus -eq "partial" -or $streakAlert -or $timingRegressionAlert -or $sizeRegressionAlert) {
            $healthLevel = "warn"
        }

        $healthReasons = New-Object System.Collections.Generic.List[string]
        if ($overallStatus -eq "fail") {
            $healthReasons.Add("status_fail") | Out-Null
        }
        if ($overallStatus -eq "warn") {
            $healthReasons.Add("status_warn") | Out-Null
        }
        if ($overallStatus -eq "partial") {
            $healthReasons.Add("status_partial") | Out-Null
        }
        if ($streakAlert) {
            $healthReasons.Add("streak_alert") | Out-Null
        }
        if ($timingRegressionAlert) {
            $healthReasons.Add("timing_alert") | Out-Null
        }
        if ($sizeRegressionAlert) {
            $healthReasons.Add("size_alert") | Out-Null
        }
        if (@($healthReasons).Count -eq 0) {
            $healthReasons.Add("healthy") | Out-Null
        }

        $healthReasonDetails = [ordered]@{
            status = $overallStatus
            reasons = @($healthReasons)
            streak = [ordered]@{
                consecutiveNonPass = $consecutiveNonPass
                threshold = $SmokeAlertNonPassThreshold
                alert = $streakAlert
            }
            timing = [ordered]@{
                peakDeltaMs = $timingDeltaPeakMs
                thresholdMs = $SmokeTimingDeltaAlertMs
                alert = $timingRegressionAlert
                triggeredMetrics = @($timingTriggeredMetrics)
            }
            size = [ordered]@{
                deltaKb = $sizeDeltaKb
                thresholdKb = $SmokeSizeDeltaAlertKb
                alert = $sizeRegressionAlert
            }
        }

        $manifestOut = [ordered]@{
            updatedAt = (Get-Date).ToString("o")
            latest = $manifestEntry
            health = [ordered]@{
                level = $healthLevel
                latestStatus = $overallStatus
                streakAlert = $streakAlert
                timingRegressionAlert = $timingRegressionAlert
                sizeRegressionAlert = $sizeRegressionAlert
                reasons = @($healthReasons)
                reasonDetails = $healthReasonDetails
            }
            counters = $statusCounts
            ciHookCounts = $ciHookCounts
            ciHookRatios = $ciHookRatios
            streak = [ordered]@{
                consecutivePass = $consecutivePass
                consecutiveNonPass = $consecutiveNonPass
                lastNonPassTimestamp = $lastNonPassTimestamp
                lastFailTimestamp = $lastFailTimestamp
                alertThreshold = $SmokeAlertNonPassThreshold
                alert = $streakAlert
            }
            timingDelta = $timingDelta
            timingDeltaBase = $timingDeltaBase
            timingDeltaBaseConfig = $timingDeltaBaseConfig
            timingAlert = [ordered]@{
                thresholdMs = $SmokeTimingDeltaAlertMs
                peakDeltaMs = $timingDeltaPeakMs
                alert = $timingRegressionAlert
                triggeredMetrics = @($timingTriggeredMetrics)
                triggeredDetails = @($timingTriggeredDetails)
            }
            sizeAlertThresholdKb = [double]$SmokeSizeDeltaAlertKb
            sizeAlert = $sizeRegressionAlert
            sizeDeltaKb = $sizeDeltaKb
            history = $history
        }
        $manifestOut | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $smokeManifestPath -Encoding UTF8

        $smokeManifestSummaryPath = Join-Path $smokeReportDir "smoke_manifest_latest.txt"
        $summaryLines = New-Object System.Collections.Generic.List[string]
        $summaryLines.Add("updatedAt=$($manifestOut.updatedAt)") | Out-Null
        $summaryLines.Add("latest.status=$($manifestEntry.status)") | Out-Null
        $summaryLines.Add("health.level=$healthLevel") | Out-Null
        $summaryLines.Add("health.reasons=$(@($healthReasons) -join ',')") | Out-Null
        $summaryLines.Add("health.reasonDetails.status=$($healthReasonDetails.status)") | Out-Null
        $summaryLines.Add("health.reasonDetails.streak.consecutiveNonPass=$($healthReasonDetails.streak.consecutiveNonPass)") | Out-Null
        $summaryLines.Add("health.reasonDetails.streak.threshold=$($healthReasonDetails.streak.threshold)") | Out-Null
        $summaryLines.Add("health.reasonDetails.streak.alert=$($healthReasonDetails.streak.alert)") | Out-Null
        $summaryLines.Add("health.reasonDetails.timing.peakDeltaMs=$($healthReasonDetails.timing.peakDeltaMs)") | Out-Null
        $summaryLines.Add("health.reasonDetails.timing.thresholdMs=$($healthReasonDetails.timing.thresholdMs)") | Out-Null
        $summaryLines.Add("health.reasonDetails.timing.alert=$($healthReasonDetails.timing.alert)") | Out-Null
        $summaryLines.Add("health.reasonDetails.timing.triggeredMetrics=$(@($healthReasonDetails.timing.triggeredMetrics) -join ',')") | Out-Null
        $summaryLines.Add("health.reasonDetails.size.deltaKb=$($healthReasonDetails.size.deltaKb)") | Out-Null
        $summaryLines.Add("health.reasonDetails.size.thresholdKb=$($healthReasonDetails.size.thresholdKb)") | Out-Null
        $summaryLines.Add("health.reasonDetails.size.alert=$($healthReasonDetails.size.alert)") | Out-Null
        $summaryLines.Add("latest.timestamp=$($manifestEntry.timestamp)") | Out-Null
        $summaryLines.Add("latest.config=$($manifestEntry.config)") | Out-Null
        $summaryLines.Add("latest.sizeKb=$sz") | Out-Null
        if ($null -ne $sizeDeltaKb) {
            $summaryLines.Add("latest.sizeDeltaKb=$sizeDeltaKb") | Out-Null
        }
        $summaryLines.Add("sizeAlert.thresholdKb=$SmokeSizeDeltaAlertKb") | Out-Null
        $summaryLines.Add("sizeAlert.alert=$sizeRegressionAlert") | Out-Null
        if ($null -ne $sizeDeltaKb) {
            $summaryLines.Add("sizeAlert.deltaKb=$sizeDeltaKb") | Out-Null
        }
        $summaryLines.Add("latest.ciDetected=$($manifestEntry.ciDetected)") | Out-Null
        $summaryLines.Add("latest.ciTimeoutCaseHookApplied=$($manifestEntry.ciTimeoutCaseHookApplied)") | Out-Null
        $summaryLines.Add("latest.skipCiTimeoutCaseHook=$($manifestEntry.skipCiTimeoutCaseHook)") | Out-Null
        $summaryLines.Add("latest.runForcedTimeoutSmokeCase=$($manifestEntry.runForcedTimeoutSmokeCase)") | Out-Null
        $summaryLines.Add("ciHookCounts.ciRuns=$($ciHookCounts.ciRuns)") | Out-Null
        $summaryLines.Add("ciHookCounts.ciHookAppliedRuns=$($ciHookCounts.ciHookAppliedRuns)") | Out-Null
        $summaryLines.Add("ciHookCounts.ciHookSkippedRuns=$($ciHookCounts.ciHookSkippedRuns)") | Out-Null
        $summaryLines.Add("ciHookCounts.forcedTimeoutRuns=$($ciHookCounts.forcedTimeoutRuns)") | Out-Null
        $summaryLines.Add("ciHookCounts.forcedTimeoutViaCiHookRuns=$($ciHookCounts.forcedTimeoutViaCiHookRuns)") | Out-Null
        $summaryLines.Add("ciHookRatios.hookAppliedPctOfCiRuns=$($ciHookRatios.hookAppliedPctOfCiRuns)") | Out-Null
        $summaryLines.Add("ciHookRatios.hookSkippedPctOfCiRuns=$($ciHookRatios.hookSkippedPctOfCiRuns)") | Out-Null
        $summaryLines.Add("ciHookRatios.forcedTimeoutPctOfCiRuns=$($ciHookRatios.forcedTimeoutPctOfCiRuns)") | Out-Null
        $summaryLines.Add("ciHookRatios.forcedTimeoutViaHookPctOfForcedTimeoutRuns=$($ciHookRatios.forcedTimeoutViaHookPctOfForcedTimeoutRuns)") | Out-Null
        if ($smokeReport.results.Contains('rtp')) {
            $summaryLines.Add("latest.rtp.status=$($smokeReport.results.rtp.status)") | Out-Null
            $summaryLines.Add("latest.rtp.exitCode=$($smokeReport.results.rtp.exitCode)") | Out-Null
            if ($smokeReport.results.rtp.Contains('exitKind')) {
                $summaryLines.Add("latest.rtp.exitKind=$($smokeReport.results.rtp.exitKind)") | Out-Null
            }
            if ($smokeReport.results.rtp.Contains('wallMs')) {
                $summaryLines.Add("latest.rtp.wallMs=$($smokeReport.results.rtp.wallMs)") | Out-Null
            }
        }
        if ($smokeReport.results.Contains('headless')) {
            $summaryLines.Add("latest.headless.status=$($smokeReport.results.headless.status)") | Out-Null
            if ($smokeReport.results.headless.Contains('agentExitCode')) {
                $summaryLines.Add("latest.headless.agentExitCode=$($smokeReport.results.headless.agentExitCode)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('agentExitKind')) {
                $summaryLines.Add("latest.headless.agentExitKind=$($smokeReport.results.headless.agentExitKind)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('agentAttempts')) {
                $summaryLines.Add("latest.headless.agentAttempts=$($smokeReport.results.headless.agentAttempts)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('agentWallMs')) {
                $summaryLines.Add("latest.headless.agentWallMs=$($smokeReport.results.headless.agentWallMs)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('benchExitCode')) {
                $summaryLines.Add("latest.headless.benchExitCode=$($smokeReport.results.headless.benchExitCode)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('benchExitKind')) {
                $summaryLines.Add("latest.headless.benchExitKind=$($smokeReport.results.headless.benchExitKind)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('benchAttempts')) {
                $summaryLines.Add("latest.headless.benchAttempts=$($smokeReport.results.headless.benchAttempts)") | Out-Null
            }
            if ($smokeReport.results.headless.Contains('benchWallMs')) {
                $summaryLines.Add("latest.headless.benchWallMs=$($smokeReport.results.headless.benchWallMs)") | Out-Null
            }
        }
        $summaryLines.Add("counters.pass=$($statusCounts.pass)") | Out-Null
        $summaryLines.Add("counters.warn=$($statusCounts.warn)") | Out-Null
        $summaryLines.Add("counters.fail=$($statusCounts.fail)") | Out-Null
        $summaryLines.Add("counters.partial=$($statusCounts.partial)") | Out-Null
        $summaryLines.Add("counters.unknown=$($statusCounts.unknown)") | Out-Null
        $summaryLines.Add("streak.consecutivePass=$consecutivePass") | Out-Null
        $summaryLines.Add("streak.consecutiveNonPass=$consecutiveNonPass") | Out-Null
        $summaryLines.Add("streak.lastNonPassTimestamp=$lastNonPassTimestamp") | Out-Null
        $summaryLines.Add("streak.lastFailTimestamp=$lastFailTimestamp") | Out-Null
        $summaryLines.Add("streak.alertThreshold=$SmokeAlertNonPassThreshold") | Out-Null
        $summaryLines.Add("streak.alert=$streakAlert") | Out-Null
        $summaryLines.Add("timingAlert.thresholdMs=$SmokeTimingDeltaAlertMs") | Out-Null
        $summaryLines.Add("timingAlert.peakDeltaMs=$timingDeltaPeakMs") | Out-Null
        $summaryLines.Add("timingAlert.alert=$timingRegressionAlert") | Out-Null
        $summaryLines.Add("timingAlert.triggeredMetrics=$(@($timingTriggeredMetrics) -join ',')") | Out-Null
        if (@($timingTriggeredDetails).Count -gt 0) {
            foreach ($d in @($timingTriggeredDetails)) {
                $summaryLines.Add("timingAlert.triggeredDetail=$($d.metric):$($d.deltaMs):$($d.baseTimestamp):$($d.baseConfig)") | Out-Null
            }
        }
        if (@($timingDelta.Keys).Count -gt 0) {
            if ($timingDelta.Contains('rtpWallMs')) {
                $summaryLines.Add("timingDelta.rtpWallMs=$($timingDelta.rtpWallMs)") | Out-Null
                if ($timingDeltaBase.Contains('rtpWallMs')) {
                    $summaryLines.Add("timingDelta.base.rtpWallMs=$($timingDeltaBase.rtpWallMs)") | Out-Null
                }
                if ($timingDeltaBaseConfig.Contains('rtpWallMs')) {
                    $summaryLines.Add("timingDelta.baseConfig.rtpWallMs=$($timingDeltaBaseConfig.rtpWallMs)") | Out-Null
                }
            }
            if ($timingDelta.Contains('agentWallMs')) {
                $summaryLines.Add("timingDelta.agentWallMs=$($timingDelta.agentWallMs)") | Out-Null
                if ($timingDeltaBase.Contains('agentWallMs')) {
                    $summaryLines.Add("timingDelta.base.agentWallMs=$($timingDeltaBase.agentWallMs)") | Out-Null
                }
                if ($timingDeltaBaseConfig.Contains('agentWallMs')) {
                    $summaryLines.Add("timingDelta.baseConfig.agentWallMs=$($timingDeltaBaseConfig.agentWallMs)") | Out-Null
                }
            }
            if ($timingDelta.Contains('benchWallMs')) {
                $summaryLines.Add("timingDelta.benchWallMs=$($timingDelta.benchWallMs)") | Out-Null
                if ($timingDeltaBase.Contains('benchWallMs')) {
                    $summaryLines.Add("timingDelta.base.benchWallMs=$($timingDeltaBase.benchWallMs)") | Out-Null
                }
                if ($timingDeltaBaseConfig.Contains('benchWallMs')) {
                    $summaryLines.Add("timingDelta.baseConfig.benchWallMs=$($timingDeltaBaseConfig.benchWallMs)") | Out-Null
                }
            }
        }
        $summaryLines.Add("recent:") | Out-Null
        $idx = 0
        foreach ($entry in @($history)) {
            if ($idx -ge 5) { break }
            $entryTimestamp = [string]$entry.timestamp
            if ($entry.timestamp -is [datetime]) {
                $entryTimestamp = $entry.timestamp.ToString("o")
            }
            $summaryLines.Add("- $entryTimestamp | $($entry.status) | cfg=$($entry.config) | all=$($entry.runAllSmoke)") | Out-Null
            $idx++
        }
        $summaryLines | Set-Content -LiteralPath $smokeManifestSummaryPath -Encoding UTF8

        $smokeManifestCsvPath = Join-Path $smokeReportDir "smoke_history_latest.csv"
        $csvRows = foreach ($entry in @($history)) {
            $entryTimestamp = [string]$entry.timestamp
            if ($entry.timestamp -is [datetime]) {
                $entryTimestamp = $entry.timestamp.ToString("o")
            }

            [pscustomobject]@{
                timestamp = $entryTimestamp
                status = [string]$entry.status
                config = [string]$entry.config
                sizeKb = if ($entry -is [System.Collections.IDictionary]) { if ($entry.Contains('sizeKb')) { [double]$entry['sizeKb'] } else { "" } } elseif ($null -ne $entry.PSObject.Properties['sizeKb']) { [double]$entry.sizeKb } else { "" }
                rtpWallMs = if ($entry -is [System.Collections.IDictionary]) { $m = if ($entry.Contains('metrics')) { $entry['metrics'] } else { $null }; if ($null -ne $m) { if ($m -is [System.Collections.IDictionary]) { if ($m.Contains('rtpWallMs')) { [int]$m['rtpWallMs'] } else { "" } } elseif ($null -ne $m.PSObject.Properties['rtpWallMs']) { [int]$m.rtpWallMs } else { "" } } else { "" } } else { $m = if ($null -ne $entry.PSObject.Properties['metrics']) { $entry.metrics } else { $null }; if ($null -ne $m -and $null -ne $m.PSObject.Properties['rtpWallMs']) { [int]$m.rtpWallMs } else { "" } }
                agentWallMs = if ($entry -is [System.Collections.IDictionary]) { $m = if ($entry.Contains('metrics')) { $entry['metrics'] } else { $null }; if ($null -ne $m) { if ($m -is [System.Collections.IDictionary]) { if ($m.Contains('agentWallMs')) { [int]$m['agentWallMs'] } else { "" } } elseif ($null -ne $m.PSObject.Properties['agentWallMs']) { [int]$m.agentWallMs } else { "" } } else { "" } } else { $m = if ($null -ne $entry.PSObject.Properties['metrics']) { $entry.metrics } else { $null }; if ($null -ne $m -and $null -ne $m.PSObject.Properties['agentWallMs']) { [int]$m.agentWallMs } else { "" } }
                benchWallMs = if ($entry -is [System.Collections.IDictionary]) { $m = if ($entry.Contains('metrics')) { $entry['metrics'] } else { $null }; if ($null -ne $m) { if ($m -is [System.Collections.IDictionary]) { if ($m.Contains('benchWallMs')) { [int]$m['benchWallMs'] } else { "" } } elseif ($null -ne $m.PSObject.Properties['benchWallMs']) { [int]$m.benchWallMs } else { "" } } else { "" } } else { $m = if ($null -ne $entry.PSObject.Properties['metrics']) { $entry.metrics } else { $null }; if ($null -ne $m -and $null -ne $m.PSObject.Properties['benchWallMs']) { [int]$m.benchWallMs } else { "" } }
                runAllSmoke = [bool]$entry.runAllSmoke
                runRtpToolSmoke = [bool]$entry.runRtpToolSmoke
                runHeadlessSmoke = [bool]$entry.runHeadlessSmoke
                runForcedTimeoutSmokeCase = [bool]$entry.runForcedTimeoutSmokeCase
                ciDetected = [bool]$entry.ciDetected
                ciTimeoutCaseHookApplied = [bool]$entry.ciTimeoutCaseHookApplied
                skipCiTimeoutCaseHook = [bool]$entry.skipCiTimeoutCaseHook
                reportTimestamped = [string]$entry.reportTimestamped
                reportLatest = [string]$entry.reportLatest
            }
        }
        @($csvRows) | Export-Csv -LiteralPath $smokeManifestCsvPath -NoTypeInformation -Encoding UTF8

        Write-Host "  manifest=$smokeManifestPath" -ForegroundColor DarkGray
        Write-Host "  manifest_summary=$smokeManifestSummaryPath" -ForegroundColor DarkGray
        Write-Host "  manifest_csv=$smokeManifestCsvPath" -ForegroundColor DarkGray
        Write-Host "  manifest_health=level:$healthLevel latestStatus:$overallStatus streakAlert:$streakAlert timingAlert:$timingRegressionAlert sizeAlert:$sizeRegressionAlert reasons:$(@($healthReasons) -join ',')" -ForegroundColor DarkGray
        Write-Host "  manifest_health_details=status:$($healthReasonDetails.status) streak:$($healthReasonDetails.streak.consecutiveNonPass)/$($healthReasonDetails.streak.threshold) timing:$($healthReasonDetails.timing.peakDeltaMs)/$($healthReasonDetails.timing.thresholdMs) size:$($healthReasonDetails.size.deltaKb)/$($healthReasonDetails.size.thresholdKb)" -ForegroundColor DarkGray
        $sizeDeltaMsg = if ($null -ne $sizeDeltaKb) { " sizeDeltaKb:$sizeDeltaKb" } else { "" }
        Write-Host "  manifest_size=sizeKb:$sz$sizeDeltaMsg" -ForegroundColor DarkGray
        Write-Host "  manifest_size_alert=thresholdKb:$SmokeSizeDeltaAlertKb alert:$sizeRegressionAlert" -ForegroundColor DarkGray
        if ($sizeRegressionAlert) {
            Write-Host "  [warn] size delta threshold reached." -ForegroundColor Yellow
        }
        Write-Host "  manifest_counters=pass:$($statusCounts.pass) warn:$($statusCounts.warn) fail:$($statusCounts.fail) partial:$($statusCounts.partial) unknown:$($statusCounts.unknown)" -ForegroundColor DarkGray
        Write-Host "  manifest_ci_hook_counts=ciRuns:$($ciHookCounts.ciRuns) hookApplied:$($ciHookCounts.ciHookAppliedRuns) hookSkipped:$($ciHookCounts.ciHookSkippedRuns) forcedTimeout:$($ciHookCounts.forcedTimeoutRuns) viaHook:$($ciHookCounts.forcedTimeoutViaCiHookRuns)" -ForegroundColor DarkGray
        Write-Host "  manifest_ci_hook_ratios=hookAppliedPct:$($ciHookRatios.hookAppliedPctOfCiRuns) hookSkippedPct:$($ciHookRatios.hookSkippedPctOfCiRuns) forcedTimeoutPct:$($ciHookRatios.forcedTimeoutPctOfCiRuns) viaHookPctOfForcedTimeout:$($ciHookRatios.forcedTimeoutViaHookPctOfForcedTimeoutRuns)" -ForegroundColor DarkGray
        Write-Host "  manifest_streak=consecutivePass:$consecutivePass consecutiveNonPass:$consecutiveNonPass lastNonPass:$lastNonPassTimestamp lastFail:$lastFailTimestamp alertThreshold:$SmokeAlertNonPassThreshold alert:$streakAlert" -ForegroundColor DarkGray
        if ($streakAlert) {
            Write-Host "  [warn] non-pass streak threshold reached." -ForegroundColor Yellow
        }
        Write-Host "  manifest_timing_alert=thresholdMs:$SmokeTimingDeltaAlertMs peakDeltaMs:$timingDeltaPeakMs alert:$timingRegressionAlert" -ForegroundColor DarkGray
        if (@($timingTriggeredMetrics).Count -gt 0) {
            Write-Host "  manifest_timing_alert_metrics=$(@($timingTriggeredMetrics) -join ',')" -ForegroundColor DarkGray
            $timingDetailParts = New-Object System.Collections.Generic.List[string]
            foreach ($d in @($timingTriggeredDetails)) {
                $timingDetailParts.Add(("{0}:{1}:{2}:{3}" -f $d.metric, $d.deltaMs, $d.baseTimestamp, $d.baseConfig)) | Out-Null
            }
            if (@($timingDetailParts).Count -gt 0) {
                Write-Host "  manifest_timing_alert_details=$($timingDetailParts -join ' ')" -ForegroundColor DarkGray
            }
        }
        if ($timingRegressionAlert) {
            Write-Host "  [warn] timing delta threshold reached." -ForegroundColor Yellow
        }
        if (@($timingDelta.Keys).Count -gt 0) {
            $deltaParts = New-Object System.Collections.Generic.List[string]
            foreach ($k in @('rtpWallMs', 'agentWallMs', 'benchWallMs')) {
                if ($timingDelta.Contains($k)) {
                    $label = $k -replace 'WallMs$', ''
                    $deltaParts.Add(("{0}:{1}" -f $label, [int]$timingDelta[$k])) | Out-Null
                }
            }
            Write-Host "  manifest_timing_delta=$($deltaParts -join ' ')" -ForegroundColor DarkGray

            $deltaBaseParts = New-Object System.Collections.Generic.List[string]
            foreach ($k in @('rtpWallMs', 'agentWallMs', 'benchWallMs')) {
                if ($timingDeltaBase.Contains($k)) {
                    $label = $k -replace 'WallMs$', ''
                    $deltaBaseParts.Add(("{0}:{1}" -f $label, [string]$timingDeltaBase[$k])) | Out-Null
                }
            }
            if (@($deltaBaseParts).Count -gt 0) {
                Write-Host "  manifest_timing_delta_base=$($deltaBaseParts -join ' ')" -ForegroundColor DarkGray
            }

            $deltaBaseConfigParts = New-Object System.Collections.Generic.List[string]
            foreach ($k in @('rtpWallMs', 'agentWallMs', 'benchWallMs')) {
                if ($timingDeltaBaseConfig.Contains($k)) {
                    $label = $k -replace 'WallMs$', ''
                    $deltaBaseConfigParts.Add(("{0}:{1}" -f $label, [string]$timingDeltaBaseConfig[$k])) | Out-Null
                }
            }
            if (@($deltaBaseConfigParts).Count -gt 0) {
                Write-Host "  manifest_timing_delta_base_config=$($deltaBaseConfigParts -join ' ')" -ForegroundColor DarkGray
            }
        }
    }

    if ($deferredSmokeFailure) {
        throw $deferredSmokeFailure
    }
} else {
    throw "Link completed but output not found: $OutExe"
}
