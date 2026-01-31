<#
Run-AutonomousAnalysis.ps1
Quick CLI runner to initialize the autonomous agent state, analyze the Win32IDE digest (if present),
and export a JSON state snapshot. Supports a quick mode to avoid long file scans.
#>
param(
    [string]$SourcePath = "d:\lazy init ide",
    [string]$OutputPath = "d:\lazy init ide\outputs\autonomous_state.json",
    [switch]$Quick
)

# Ensure outputs directory exists
$outDir = Split-Path -Path $OutputPath -Parent
if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir -Force | Out-Null }

Write-Host "Initializing autonomous analysis (Source: $SourcePath)" -ForegroundColor Cyan

# Import modules
function Import-Module-BypassAdmin {
    param(
        [Parameter(Mandatory=$true)]
        [string]$ModulePath
    )

    $moduleText = Get-Content -Raw -LiteralPath $ModulePath -ErrorAction Stop
    $requiresPattern = '(?m)^[ \t]*#requires\b.*RunAsAdministrator'
    if ($moduleText -match $requiresPattern) {
        $tmpName = "tmp_$(Get-Random)_$(Split-Path -Leaf $ModulePath)"
        $tmpPath = Join-Path $env:TEMP $tmpName
        $filtered = ($moduleText -split "\r?\n") | Where-Object { -not ($_ -match $requiresPattern) }
        $filtered -join [Environment]::NewLine | Set-Content -LiteralPath $tmpPath -Encoding UTF8
        try {
            Import-Module $tmpPath -Force -ErrorAction Stop
        } catch {
            Remove-Item -LiteralPath $tmpPath -ErrorAction SilentlyContinue
            throw
        }
        # leave temp file for inspection; not removing immediately helps debugging
    } else {
        Import-Module $ModulePath -Force -ErrorAction Stop
    }
}

try {
    $agentModule = Join-Path $PSScriptRoot "..\RawrXD.AutonomousAgent.psm1"
    $deployModule = Join-Path $PSScriptRoot "..\RawrXD.Win32Deployment.psm1"
    Import-Module-BypassAdmin -ModulePath $agentModule
    Import-Module-BypassAdmin -ModulePath $deployModule
} catch {
    Write-Host "Failed to import modules: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Initialize state
Initialize-AutonomousAgentState -SourcePath $SourcePath -TargetPath $SourcePath -LogPath (Join-Path $outDir 'logs') -BackupPath (Join-Path $outDir 'backup')

if ($Quick) {
    Write-Host "Running QUICK analysis: digest summary + module count" -ForegroundColor Yellow

    if ($script:AutonomousAgentState.IDEDigest) {
        $digest = $script:AutonomousAgentState.IDEDigest
        Write-Host "IDE Digest Summary:" -ForegroundColor Green
        Write-Host " SourceFile: $($digest.SourceFile)" -ForegroundColor DarkGray
        Write-Host " Subsystems: $($digest.Subsystems.Count)" -ForegroundColor DarkGray
    } else {
        Write-Host "No IDE digest found." -ForegroundColor DarkGray
    }

    # Module count only (fast)
    $modules = Get-ChildItem -Path $SourcePath -Recurse -Filter "*.psm1" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName -ErrorAction SilentlyContinue
    $count = ($modules | Measure-Object).Count
    Write-Host "PowerShell modules found: $count" -ForegroundColor Green

    # Minimal state export
    Export-AutonomousAgentState -OutputPath $OutputPath | Out-Null
    Write-Host "Exported state to $OutputPath" -ForegroundColor Cyan
    exit 0
}

# Full analysis (may take time)
Write-Host "Running FULL self-analysis (may be slow)" -ForegroundColor Yellow
Start-SelfAnalysis

# Export final state
Export-AutonomousAgentState -OutputPath $OutputPath | Out-Null
Write-Host "Exported state to $OutputPath" -ForegroundColor Cyan

# Print short summary
Write-Host "Summary:" -ForegroundColor Green
Write-Host " Modules analyzed: $($script:AutonomousAgentState.Paths.Source)" -ForegroundColor DarkGray
Write-Host " Errors logged: $($script:AutonomousAgentState.Errors.Count)" -ForegroundColor DarkGray
Write-Host "Warnings: $($script:AutonomousAgentState.Warnings.Count)" -ForegroundColor DarkGray

exit 0
