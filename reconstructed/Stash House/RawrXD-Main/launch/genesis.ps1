<#
.SYNOPSIS
    In-place Genesis launcher — runs the full bootstrap pipeline
    directly from the repo without copying files to AppData.
.DESCRIPTION
    Points all build phases at the actual workspace paths:
      Source:  D:\Stash House\RawrXD-Main\src\asm  (assembly files)
      Objects: D:\Stash House\RawrXD-Main\build\obj
      Binaries: D:\Stash House\RawrXD-Main\build\bin
      Tools:   D:\Stash House\RawrXD-Main\tools\

    Still stamps the registry and applies the thermal profile.
    No files are moved or duplicated to $env:LOCALAPPDATA.
.PARAMETER ThermalProfile
    Iguana (default) | Cheetah | Tortoise
.PARAMETER Target
    Build target name (default: RawrXD-AgenticIDE)
.PARAMETER SkipRegistry
    Skip registry stamp (useful for non-admin runs)
.PARAMETER SkipBuild
    Only configure registry + thermal, don't build
.PARAMETER Launch
    Start the IDE after build
.PARAMETER EnableAnalysis
    Write build telemetry JSON
.EXAMPLE
    .\launch_genesis.ps1
    .\launch_genesis.ps1 -ThermalProfile Cheetah -Launch
    .\launch_genesis.ps1 -SkipBuild   # registry + thermal only
#>
[CmdletBinding()]
param(
    [ValidateSet("Iguana","Cheetah","Tortoise")]
    [string]$ThermalProfile = "Iguana",

    [ValidateSet("RawrXD-AgenticIDE","RawrXD-Win32IDE","RawrXD-Agent","RawrXD-CLI")]
    [string]$Target = "RawrXD-AgenticIDE",

    [switch]$SkipRegistry,
    [switch]$SkipBuild,
    [switch]$Launch,
    [switch]$EnableAnalysis,
    [switch]$SelfHost
)

Set-ExecutionPolicy Bypass -Scope Process -Force
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════
# Path Resolution — everything stays in the repo
# ═══════════════════════════════════════════════════════════════
$RepoRoot   = $PSScriptRoot                                    # D:\Stash House\RawrXD-Main
$ToolsDir   = Join-Path $RepoRoot "tools"
$SrcDir     = Join-Path $RepoRoot "src"
$AsmDir     = Join-Path $SrcDir   "asm"
$BuildRoot  = Join-Path $RepoRoot "build"
$ObjDir     = Join-Path $BuildRoot "obj"
$BinDir     = Join-Path $BuildRoot "bin"
$TelDir     = Join-Path $BuildRoot "telemetry"

# Ensure output dirs exist
@($ObjDir, $BinDir, $TelDir) | ForEach-Object {
    New-Item -ItemType Directory -Path $_ -Force | Out-Null
}

# ═══════════════════════════════════════════════════════════════
# Thermal Config
# ═══════════════════════════════════════════════════════════════
$Thermal = @{
    Iguana   = @{ VoltageFloor=400;  Threads=[Environment]::ProcessorCount;                       Priority="High";        Affinity=0xFFFF }
    Cheetah  = @{ VoltageFloor=0;    Threads=([int]([Environment]::ProcessorCount * 1.5));        Priority="RealTime";    Affinity=-1     }
    Tortoise = @{ VoltageFloor=800;  Threads=1;                                                   Priority="BelowNormal"; Affinity=0x1    }
}
$cfg = $Thermal[$ThermalProfile]

function Write-Log {
    param([string]$Phase, [string]$Msg, [string]$Level = "Info")
    $ts = Get-Date -Format "HH:mm:ss.fff"
    $c = @{ Info="White"; Success="Green"; Warning="Yellow"; Error="Red" }
    Write-Host "[$ts] [$Phase] $Msg" -ForegroundColor $c[$Level]
}

Write-Host ""
Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host "  RawrXD Genesis — IN-PLACE BUILD (no file copying)" -ForegroundColor Cyan
Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host "  Repo:     $RepoRoot" -ForegroundColor White
Write-Host "  Source:   $AsmDir" -ForegroundColor White
Write-Host "  Objects:  $ObjDir" -ForegroundColor White
Write-Host "  Binaries: $BinDir" -ForegroundColor White
Write-Host "  Thermal:  $ThermalProfile ($($cfg.VoltageFloor)mV)" -ForegroundColor Yellow
Write-Host "=====================================================" -ForegroundColor Cyan
Write-Host ""

# ═══════════════════════════════════════════════════════════════
# Registry Stamp (optional — works without admin if keys exist)
# ═══════════════════════════════════════════════════════════════
if (-not $SkipRegistry) {
    Write-Log "REGISTRY" "Stamping HKCU:\Software\RawrXD..."
    $regRoot = "HKCU:\Software\RawrXD"
    try {
        foreach ($sub in @($regRoot, "$regRoot\Thermal", "$regRoot\Memory", "$regRoot\StreamOrc")) {
            if (!(Test-Path $sub)) { New-Item -Path $sub -Force | Out-Null }
        }
        Set-ItemProperty -Path $regRoot          -Name "Genesis"       -Value (Get-Date -Format "o")       -Force
        Set-ItemProperty -Path $regRoot          -Name "Version"       -Value "14.2.0-Genesis"              -Force
        Set-ItemProperty -Path $regRoot          -Name "RepoRoot"      -Value $RepoRoot                    -Force
        Set-ItemProperty -Path "$regRoot\Thermal" -Name "Mode"         -Value $ThermalProfile               -Force
        Set-ItemProperty -Path "$regRoot\Thermal" -Name "VoltageFloor" -Value $cfg.VoltageFloor             -Force
        Set-ItemProperty -Path "$regRoot\Memory"  -Name "USBRamBridge" -Value "Enabled"                     -Force
        Set-ItemProperty -Path "$regRoot\StreamOrc"-Name "DMABufferMB" -Value 64                            -Force
        Write-Log "REGISTRY" "Done — $ThermalProfile profile stamped" "Success"
    } catch {
        Write-Log "REGISTRY" "Non-fatal: $_" "Warning"
    }
}

# ═══════════════════════════════════════════════════════════════
# Thermal Application (process priority / affinity)
# ═══════════════════════════════════════════════════════════════
try {
    $proc = Get-Process -Id $PID
    if ($cfg.Affinity -ne -1) { $proc.ProcessorAffinity = $cfg.Affinity }
    $proc.PriorityClass = $cfg.Priority
    Write-Log "THERMAL" "Applied: $ThermalProfile | $($cfg.VoltageFloor)mV | $($cfg.Threads) threads | $($cfg.Priority)" "Success"
} catch {
    Write-Log "THERMAL" "Could not set process constraints: $_" "Warning"
}

if ($SkipBuild) {
    Write-Log "BUILD" "SkipBuild flag set — registry + thermal configured, exiting." "Warning"
    return
}

# ═══════════════════════════════════════════════════════════════
# Phase 1: Assemble (build_scc.ps1 — pointing at repo asm dir)
# ═══════════════════════════════════════════════════════════════
$phase1 = Join-Path $ToolsDir "build_scc.ps1"
if (Test-Path $phase1) {
    Write-Log "PHASE1" "Assembling from $AsmDir ..." "Info"
    & $phase1 -SourceDir $AsmDir -OutputDir $ObjDir
    Write-Log "PHASE1" "Assembly complete" "Success"
} else {
    Write-Log "PHASE1" "build_scc.ps1 not found at $phase1" "Error"
}

# ═══════════════════════════════════════════════════════════════
# Phase 2: Link (build_link.ps1 — pointing at repo obj/bin dirs)
# ═══════════════════════════════════════════════════════════════
$phase2 = Join-Path $ToolsDir "build_link.ps1"
if (Test-Path $phase2) {
    Write-Log "PHASE2" "Linking $Target ..." "Info"
    & $phase2 -ObjDir $ObjDir -BinDir $BinDir -Target $Target -ThermalSignature "${ThermalProfile}_MODE"
    Write-Log "PHASE2" "Link complete" "Success"
} else {
    Write-Log "PHASE2" "build_link.ps1 not found at $phase2" "Error"
}

# ═══════════════════════════════════════════════════════════════
# Validation
# ═══════════════════════════════════════════════════════════════
$exe = Join-Path $BinDir "$Target.exe"
if (Test-Path $exe) {
    $sizeMB = [math]::Round((Get-Item $exe).Length / 1MB, 2)
    Write-Host ""
    Write-Host "=====================================================" -ForegroundColor Green
    Write-Host "  BUILD SUCCESS" -ForegroundColor Green
    Write-Host "  Binary:  $exe" -ForegroundColor White
    Write-Host "  Size:    $sizeMB MB" -ForegroundColor White
    Write-Host "  Thermal: $ThermalProfile ($($cfg.VoltageFloor)mV)" -ForegroundColor Yellow
    Write-Host "=====================================================" -ForegroundColor Green

    if ($EnableAnalysis) {
        @{
            Timestamp    = Get-Date -Format "o"
            Target       = $Target
            SizeMB       = $sizeMB
            Thermal      = $ThermalProfile
            VoltageFloor = $cfg.VoltageFloor
            RepoRoot     = $RepoRoot
            SelfHost     = $SelfHost.IsPresent
        } | ConvertTo-Json -Depth 4 | Out-File (Join-Path $TelDir "build_result.json") -Force
        Write-Log "TELEMETRY" "Written to $TelDir\build_result.json" "Success"
    }

    if ($Launch) {
        Write-Log "LAUNCH" "Starting $Target ..." "Info"
        $launchArgs = @("--stream-orc-phase1", "--thermal-$($ThermalProfile.ToLower())", "--dma-buffer", "64MB")
        Start-Process $exe -ArgumentList $launchArgs
    }
} else {
    Write-Host ""
    Write-Host "=====================================================" -ForegroundColor Red
    Write-Host "  BUILD INCOMPLETE — no .exe produced" -ForegroundColor Red
    Write-Host "  Check Phase 1/2 output above for errors." -ForegroundColor Yellow
    Write-Host "  Binary expected at: $exe" -ForegroundColor White
    Write-Host "=====================================================" -ForegroundColor Red
}
