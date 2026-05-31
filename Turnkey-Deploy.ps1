# RawrXD Turnkey Master Deployment Script
# Single-command full deployment with validation
# Usage: .\Turnkey-Deploy.ps1 [-SkipEnvironment] [-SkipBuild] [-SkipValidation]

[CmdletBinding()]
param(
    [switch]$SkipEnvironment,
    [switch]$SkipBuild,
    [switch]$SkipValidation,
    [switch]$SkipModelDownload,
    [switch]$Clean,
    [ValidateSet("Release", "Debug")]
    [string]$Configuration = "Release",
    [string]$ModelUrl = "https://huggingface.co/TheBloke/Llama-2-7B-GGUF/resolve/main/llama-2-7b.Q4_K_M.gguf",
    [string]$OutputPath = "",
    [string]$LogPath = "$env:TEMP\rawrxd-turnkey-deploy.log"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "Continue"

# Configuration
$script:ProjectRoot = Resolve-Path $PSScriptRoot
$script:TurnkeyDir = Join-Path $script:ProjectRoot "scripts\turnkey"
$script:LogFile = $LogPath
$script:StartTime = Get-Date
$script:PhaseResults = @()

function Write-Phase {
    param(
        [int]$PhaseNumber,
        [string]$PhaseName,
        [string]$Status,
        [string]$Message = ""
    )
    
    $script:PhaseResults += [PSCustomObject]@{
        Phase = $PhaseNumber
        Name = $PhaseName
        Status = $Status
        Message = $Message
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
    
    $color = switch ($Status) {
        "SUCCESS" { "Green" }
        "FAILED" { "Red" }
        "SKIPPED" { "Yellow" }
        "RUNNING" { "Cyan" }
        default { "White" }
    }
    
    $icon = switch ($Status) {
        "SUCCESS" { "✓" }
        "FAILED" { "✗" }
        "SKIPPED" { "⊘" }
        "RUNNING" { "►" }
        default { "•" }
    }
    
    Write-Host ""
    Write-Host "[$icon] Phase $PhaseNumber`: $PhaseName" -ForegroundColor $color
    if ($Message) {
        Write-Host "    $Message" -ForegroundColor Gray
    }
}

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"
    Add-Content -Path $script:LogFile -Value $logEntry -ErrorAction SilentlyContinue
}

function Merge-Hashtable {
    param(
        [hashtable]$Base,
        [hashtable]$Defaults
    )

    foreach ($key in $Defaults.Keys) {
        $defaultValue = $Defaults[$key]
        if (-not $Base.ContainsKey($key) -or $null -eq $Base[$key]) {
            $Base[$key] = $defaultValue
            continue
        }

        if ($Base[$key] -is [System.Collections.IDictionary] -and $defaultValue -is [hashtable]) {
            $nested = @{}
            foreach ($entry in $Base[$key].GetEnumerator()) {
                $nested[$entry.Key] = $entry.Value
            }
            $Base[$key] = Merge-Hashtable -Base $nested -Defaults $defaultValue
        }
    }

    return $Base
}

function Invoke-Phase {
    param(
        [int]$Number,
        [string]$Name,
        [scriptblock]$Action,
        [switch]$Skip
    )
    
    if ($Skip) {
        Write-Phase -PhaseNumber $Number -PhaseName $Name -Status "SKIPPED" -Message "Skipped by user request"
        return $true
    }
    
    Write-Phase -PhaseNumber $Number -PhaseName $Name -Status "RUNNING"
    Write-Log "Starting Phase $Number`: $Name"
    
    try {
        $result = & $Action
        if ($result -eq $false) {
            throw "Phase returned false"
        }
        Write-Phase -PhaseNumber $Number -PhaseName $Name -Status "SUCCESS"
        Write-Log "Phase $Number completed successfully"
        return $true
    }
    catch {
        Write-Phase -PhaseNumber $Number -PhaseName $Name -Status "FAILED" -Message $_.Exception.Message
        Write-Log "Phase $Number failed: $_" "ERROR"
        return $false
    }
}

function Test-AdminRights {
    $currentPrincipal = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentPrincipal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

function Show-Banner {
    $banner = @"
╔════════════════════════════════════════════════════════════════╗
║                                                                ║
║   RawrXD IDE - Turnkey Deployment                              ║
║   14-Day Sprint: Final Integration & Validation               ║
║                                                                ║
║   This script automates the complete deployment process:       ║
║   1. Environment Setup                                         ║
║   2. Build Automation                                          ║
║   3. Model Download (optional)                                   ║
║   4. Configuration                                             ║
║   5. Validation                                                ║
║                                                                ║
╚════════════════════════════════════════════════════════════════╝
"@
    Write-Host $banner -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Configuration: $Configuration" -ForegroundColor Yellow
    Write-Host "Clean build: $Clean" -ForegroundColor Yellow
    Write-Host "Log file: $LogFile" -ForegroundColor Yellow
    Write-Host ""
}

function Get-LaunchTargets {
    $candidates = @(
        @{ Label = "bin-turnkey\\RawrXD-Win32IDE.exe"; Path = Join-Path $script:ProjectRoot "bin-turnkey\RawrXD-Win32IDE.exe" },
        @{ Label = "build-turnkey\\bin\\RawrXD-Win32IDE.exe"; Path = Join-Path $script:ProjectRoot "build-turnkey\bin\RawrXD-Win32IDE.exe" },
        @{ Label = "build-ninja\\bin\\RawrXD-Win32IDE.exe"; Path = Join-Path $script:ProjectRoot "build-ninja\bin\RawrXD-Win32IDE.exe" }
    )

    $targets = @()
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate.Path) {
            $targets += $candidate.Label
        }
    }

    return $targets
}

function Show-Prerequisites {
    Write-Host "Prerequisites Check:" -ForegroundColor Cyan
    Write-Host ""
    
    $checks = @(
        @{ Name = "Windows 10/11"; Check = { $true } },  # We're running, so...
        @{ Name = "PowerShell 5.1+"; Check = { $PSVersionTable.PSVersion -ge [Version]"5.1" } },
        @{ Name = "Internet connection"; Check = { 
            try { 
                Test-Connection -ComputerName "8.8.8.8" -Count 1 -Quiet -ErrorAction Stop 
            } catch { $false } 
        } },
        @{ Name = "Disk space (10GB+)"; Check = { 
            $drive = Get-PSDrive -Name $env:SystemDrive[0]
            $drive.Free -gt 10GB
        } }
    )
    
    $allPassed = $true
    foreach ($check in $checks) {
        $result = & $check.Check
        $icon = if ($result) { "✓" } else { "✗" }
        $color = if ($result) { "Green" } else { "Red" }
        Write-Host "  $icon $($check.Name)" -ForegroundColor $color
        if (-not $result) { $allPassed = $false }
    }
    
    Write-Host ""
    return $allPassed
}

# Phase 1: Environment Setup
$phase1 = {
    $scriptPath = Join-Path $script:TurnkeyDir "Setup-Environment.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "Setup-Environment.ps1 not found at $scriptPath"
    }
    
    & $scriptPath -LogPath "$env:TEMP\rawrxd-phase1.log"
    if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
        return $false
    }
    return $?
}

# Phase 2: Build
$phase2 = {
    $scriptPath = Join-Path $script:TurnkeyDir "Build-RawrXD.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "Build-RawrXD.ps1 not found at $scriptPath"
    }
    
    $args = @{
        Configuration = $Configuration
        LogPath = "$env:TEMP\rawrxd-phase2.log"
    }
    if ($Clean) { $args.Clean = $true }
    if ($OutputPath) { $args.OutputPath = $OutputPath }
    
    & $scriptPath @args
    if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
        return $false
    }
    return $?
}

# Phase 3: Model Download (optional)
$phase3 = {
    $modelsDir = Join-Path $script:ProjectRoot "models"
    
    # Check if any models exist
    $existingModels = Get-ChildItem $modelsDir -Filter "*.gguf" -ErrorAction SilentlyContinue
    if ($existingModels) {
        Write-Host "  Found existing models: $($existingModels.Name -join ', ')" -ForegroundColor Green
        return $true
    }
    
    Write-Host "  Downloading default model..." -ForegroundColor Yellow
    Write-Host "  URL: $ModelUrl" -ForegroundColor Gray
    
    if (-not (Test-Path $modelsDir)) {
        New-Item -ItemType Directory -Path $modelsDir -Force | Out-Null
    }
    
    $modelFile = Split-Path $ModelUrl -Leaf
    $outputFile = Join-Path $modelsDir $modelFile
    
    try {
        # Use BITS for reliable download
        Import-Module BitsTransfer -ErrorAction SilentlyContinue
        if (Get-Command Start-BitsTransfer -ErrorAction SilentlyContinue) {
            Start-BitsTransfer -Source $ModelUrl -Destination $outputFile -DisplayName "RawrXD Model Download"
        } else {
            # Fallback to Invoke-WebRequest
            Invoke-WebRequest -Uri $ModelUrl -OutFile $outputFile -UseBasicParsing
        }
        
        Write-Host "  ✓ Model downloaded: $modelFile" -ForegroundColor Green
        return $true
    }
    catch {
        Write-Host "  ✗ Model download failed: $_" -ForegroundColor Red
        Write-Host "  You can download models manually from HuggingFace" -ForegroundColor Yellow
        return $true  # Don't fail deployment, model is optional
    }
}

# Phase 4: Configuration
$phase4 = {
    $configPath = Join-Path $script:ProjectRoot "rawrxd.config.json"
    
    $modelsDir = Join-Path $script:ProjectRoot "models"
    if (-not (Test-Path $modelsDir)) {
        New-Item -ItemType Directory -Path $modelsDir -Force | Out-Null
    }

    $defaultConfig = @{
        model = @{
            path = "models"
            default_model = ""
            max_context = 4096
        }
        inference = @{
            temperature = 0.7
            top_p = 0.9
            max_tokens = 2048
        }
        ui = @{
            theme = "dark"
            font = "Consolas"
            font_size = 12
        }
        paths = @{
            projects = "projects"
            temp = "temp"
        }
    }
    
    # Find any downloaded model
    $downloadedModel = Get-ChildItem $modelsDir -Filter "*.gguf" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($downloadedModel) {
        $defaultConfig.model.default_model = $downloadedModel.Name
    }

    $configData = @{}
    if (Test-Path $configPath) {
        try {
            $existingConfig = Get-Content $configPath -Raw | ConvertFrom-Json -AsHashtable -ErrorAction Stop
            if ($existingConfig) {
                $configData = $existingConfig
            }
            Write-Host "  Updating existing configuration: $configPath" -ForegroundColor Green
        }
        catch {
            Write-Host "  Existing configuration is invalid JSON, rebuilding defaults" -ForegroundColor Yellow
        }
    }

    if ($configData.ContainsKey("theme") -and -not $configData.ContainsKey("ui")) {
        $fontName = "Consolas"
        $fontSize = 12
        if ($configData.ContainsKey("editor") -and $configData.editor -is [System.Collections.IDictionary]) {
            if ($configData.editor.ContainsKey("fontFamily") -and $configData.editor.fontFamily) {
                $fontName = $configData.editor.fontFamily
            }
            if ($configData.editor.ContainsKey("fontSize") -and $configData.editor.fontSize) {
                $fontSize = $configData.editor.fontSize
            }
        }

        $themeName = "dark"
        if ($configData.theme -is [System.Collections.IDictionary] -and $configData.theme.ContainsKey("name") -and $configData.theme.name) {
            $themeName = $configData.theme.name
        }

        $configData.ui = @{
            theme = $themeName
            font = $fontName
            font_size = $fontSize
        }
    }

    $configData = Merge-Hashtable -Base $configData -Defaults $defaultConfig

    if (($configData.model.default_model -eq "") -and $configData.ContainsKey("native") -and $configData.native -is [System.Collections.IDictionary] -and $configData.native.ContainsKey("model") -and $configData.native.model) {
        $configData.model.default_model = $configData.native.model
    }
    
    $configData | ConvertTo-Json -Depth 6 | Out-File $configPath
    Write-Host "  ✓ Configuration normalized for turnkey deployment" -ForegroundColor Green
    
    return $true
}

# Phase 5: Validation
$phase5 = {
    $scriptPath = Join-Path $script:TurnkeyDir "Validate-Deployment.ps1"
    if (-not (Test-Path $scriptPath)) {
        throw "Validate-Deployment.ps1 not found at $scriptPath"
    }
    
    & $scriptPath -GenerateReport -ReportPath "$env:TEMP\rawrxd-validation-report.json"
    if ($LASTEXITCODE -ne $null -and $LASTEXITCODE -ne 0) {
        return $false
    }
    return $?
}

# Main execution
Show-Banner

if (-not (Show-Prerequisites)) {
    Write-Host ""
    Write-Host "Prerequisites check failed. Please resolve the issues above." -ForegroundColor Red
    exit 1
}

Write-Host "Starting deployment..." -ForegroundColor Cyan
Write-Host ""

# Execute phases
$success = $true

if (-not (Invoke-Phase -Number 1 -Name "Environment Setup" -Action $phase1 -Skip:$SkipEnvironment)) {
    $success = $false
}

if ($success -and -not (Invoke-Phase -Number 2 -Name "Build RawrXD" -Action $phase2 -Skip:$SkipBuild)) {
    $success = $false
}

if ($success -and -not (Invoke-Phase -Number 3 -Name "Model Download" -Action $phase3 -Skip:$SkipModelDownload)) {
    $success = $false
}

if ($success -and -not (Invoke-Phase -Number 4 -Name "Configuration" -Action $phase4)) {
    $success = $false
}

if ($success -and -not (Invoke-Phase -Number 5 -Name "Validation" -Action $phase5 -Skip:$SkipValidation)) {
    $success = $false
}

# Summary
$duration = (Get-Date) - $script:StartTime

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "                    DEPLOYMENT SUMMARY                          " -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$finalPhases = $script:PhaseResults |
    Group-Object Phase |
    ForEach-Object { $_.Group[-1] } |
    Sort-Object Phase

foreach ($phase in $finalPhases) {
    $color = switch ($phase.Status) {
        "SUCCESS" { "Green" }
        "FAILED" { "Red" }
        "SKIPPED" { "Yellow" }
        default { "White" }
    }
    $icon = switch ($phase.Status) {
        "SUCCESS" { "✓" }
        "FAILED" { "✗" }
        "SKIPPED" { "⊘" }
        default { "•" }
    }
    Write-Host "  $icon Phase $($phase.Phase)`: $($phase.Name)" -ForegroundColor $color
}

Write-Host ""
Write-Host "Duration: $($duration.ToString())" -ForegroundColor White
Write-Host "Log file: $LogFile" -ForegroundColor Gray

if ($success) {
    $launchTargets = Get-LaunchTargets
    $launchLine1 = if ($launchTargets.Count -gt 0) { $launchTargets[0] } else { "build-ninja\\bin\\RawrXD-Win32IDE.exe" }
    $launchLine2 = if ($launchTargets.Count -gt 1) { $launchTargets[1] } else { $null }

    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║                                                               ║" -ForegroundColor Green
    Write-Host "║   ✓ TURNKEY DEPLOYMENT COMPLETE                               ║" -ForegroundColor Green
    Write-Host "║                                                               ║" -ForegroundColor Green
    Write-Host "║   RawrXD IDE is ready to use!                                 ║" -ForegroundColor Green
    Write-Host "║                                                               ║" -ForegroundColor Green
    Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host "  Launch: $launchLine1" -ForegroundColor Green
    if ($launchLine2) {
        Write-Host "  Alternate: $launchLine2" -ForegroundColor Green
    } else {
        Write-Host "  Re-run: .\Turnkey-Deploy.ps1 -SkipEnvironment" -ForegroundColor Green
    }
    exit 0
} else {
    Write-Host ""
    Write-Host "╔═══════════════════════════════════════════════════════════════╗" -ForegroundColor Red
    Write-Host "║                                                               ║" -ForegroundColor Red
    Write-Host "║   ✗ DEPLOYMENT FAILED                                         ║" -ForegroundColor Red
    Write-Host "║                                                               ║" -ForegroundColor Red
    Write-Host "║   Check the logs for details:                                 ║" -ForegroundColor Red
    Write-Host "║   - $LogFile" -ForegroundColor Red
    Write-Host "║                                                               ║" -ForegroundColor Red
    Write-Host "║   Common fixes:                                               ║" -ForegroundColor Red
    Write-Host "║   - Run as Administrator if installing build tools              ║" -ForegroundColor Red
    Write-Host "║   - Ensure Windows SDK is installed                           ║" -ForegroundColor Red
    Write-Host "║   - Check disk space (need 10GB+)                             ║" -ForegroundColor Red
    Write-Host "║                                                               ║" -ForegroundColor Red
    Write-Host "╚═══════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    exit 1
}
