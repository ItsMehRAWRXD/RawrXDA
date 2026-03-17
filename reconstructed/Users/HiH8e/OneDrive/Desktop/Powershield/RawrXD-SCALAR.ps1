<#
.SYNOPSIS
    RawrXD - SCALAR-ONLY AI-Powered Text Editor with Ollama Integration
.DESCRIPTION
    Core UI shell for the RawrXD IDE with pop-out editing and AI helpers.
    ALL OPERATIONS ARE SCALAR - NO PIPELINE OPERATORS, EXPLICIT LOOPS ONLY
#>

# ============================================
# EMERGENCY LOGGING SETUP - SCALAR MODE

$script:EmergencyLogPath = Join-Path $env:APPDATA "RawrXD"
if (-not (Test-Path $script:EmergencyLogPath)) {
    try { 
        $null = New-Item -ItemType Directory -Path $script:EmergencyLogPath -Force
    } catch { }
}
$script:StartupLogFile = Join-Path $script:EmergencyLogPath "startup.log"

function Write-EmergencyLog {
    param(
        [Parameter(Mandatory = $true)][string]$Message,
        [ValidateSet("DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR", "CRITICAL")]
        [string]$Level = "INFO"
    )

    try {
        $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss.fff"
        $logEntry = "[$timestamp] [$Level] $Message"
        if ($script:StartupLogFile) {
            Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }

        # SCALAR: explicit comparison, no -in operator
        $isError = $false
        if ($Level -eq "ERROR") { $isError = $true }
        if ($Level -eq "CRITICAL") { $isError = $true }
        
        if ($isError) {
            Write-Host $logEntry -ForegroundColor Red
        }
    }
    catch {
        $displayColor = "Yellow"
        if ($Level -eq "ERROR") { $displayColor = "Red" }
        Write-Host "[$Level] $Message" -ForegroundColor $displayColor
    }
}

Write-EmergencyLog "Emergency logging initialized - SCALAR MODE" "INFO"

# ============================================
# STARTUP LOGGING - SCALAR MODE

function Write-StartupLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        [ValidateSet("DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR")]
        [string]$Level = "INFO"
    )

    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $logEntry = "[$timestamp] [$Level] $Message"

    # SCALAR: explicit switch, no collection operations
    $color = "White"
    if ($Level -eq "DEBUG") { $color = "DarkGray" }
    if ($Level -eq "INFO") { $color = "Cyan" }
    if ($Level -eq "SUCCESS") { $color = "Green" }
    if ($Level -eq "WARNING") { $color = "Yellow" }
    if ($Level -eq "ERROR") { $color = "Red" }

    Write-Host $logEntry -ForegroundColor $color

    if ($script:StartupLogFile) {
        try {
            Add-Content -Path $script:StartupLogFile -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }
        catch { }
    }
}

function Write-DevConsole {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Message,
        [ValidateSet("DEBUG", "INFO", "SUCCESS", "WARNING", "ERROR")]
        [string]$Level = "INFO"
    )
    Write-StartupLog -Message $Message -Level $Level
}

# SCALAR: Get-OllamaModels with explicit loops, no pipeline
function Get-OllamaModels {
    $models = @()
    $modelCount = 0

    # 1) Try to fetch from local Ollama instance - SCALAR
    try {
        $resp = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method Get -ErrorAction Stop -TimeoutSec 2
        if ($resp -and $resp.models) {
            # SCALAR: explicit loop instead of ForEach-Object | Where-Object
            $respModels = $resp.models
            $respModelCount = $respModels.Count
            for ($i = 0; $i -lt $respModelCount; $i++) {
                $modelName = $respModels[$i].name
                if ($modelName) {
                    $models += $modelName
                    $modelCount++
                }
            }
        }
    }
    catch {
        # ignore - endpoint may not be available yet
    }

    # 2) Scan common model directories for .gguf or model files - SCALAR
    $candidatePathsRaw = @(
        "$PSScriptRoot\OllamaModels",
        "D:\OllamaModels",
        "C:\OllamaModels",
        "$env:USERPROFILE\OllamaModels",
        "$PSScriptRoot\models",
        "$PSScriptRoot"
    )
    
    # SCALAR: Remove duplicates with explicit loop instead of Select-Object -Unique
    $candidatePaths = @()
    $candidateCount = 0
    for ($i = 0; $i -lt $candidatePathsRaw.Count; $i++) {
        $path = $candidatePathsRaw[$i]
        $isDuplicate = $false
        for ($j = 0; $j -lt $candidateCount; $j++) {
            if ($candidatePaths[$j] -eq $path) {
                $isDuplicate = $true
                break
            }
        }
        if (-not $isDuplicate) {
            $candidatePaths += $path
            $candidateCount++
        }
    }

    # SCALAR: explicit foreach loop
    foreach ($p in $candidatePaths) {
        if (-not $p) { continue }
        try {
            if (Test-Path $p) {
                # SCALAR: Get .gguf files with explicit loop instead of pipeline
                $ggufFiles = Get-ChildItem -Path $p -Filter "*.gguf" -Recurse -ErrorAction SilentlyContinue
                if ($ggufFiles) {
                    for ($i = 0; $i -lt $ggufFiles.Count; $i++) {
                        $models += $ggufFiles[$i].BaseName
                        $modelCount++
                    }
                }

                # SCALAR: Get plain model files with explicit loop instead of Where-Object | ForEach-Object
                $allFiles = Get-ChildItem -Path $p -File -ErrorAction SilentlyContinue
                if ($allFiles) {
                    for ($i = 0; $i -lt $allFiles.Count; $i++) {
                        $fileName = $allFiles[$i].Name
                        if ($fileName -match 'model|bigdaddy|ggml|q4|q5') {
                            $models += $fileName
                            $modelCount++
                        }
                    }
                }
            }
        }
        catch { }
    }

    # 3) Include configured fallback if present
    try {
        if ($script:settings -and $script:settings.OllamaModel) { 
            $models += $script:settings.OllamaModel 
            $modelCount++
        }
    } catch { }

    # 4) Final sensible defaults - try to get models dynamically
    if ($modelCount -eq 0) {
        try {
            $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 2 -ErrorAction Stop
            # SCALAR: explicit loop instead of ForEach-Object
            if ($response.models) {
                for ($i = 0; $i -lt $response.models.Count; $i++) {
                    $models += $response.models[$i].name
                    $modelCount++
                }
            }
        } catch {
            # Absolute fallback if Ollama is offline
            $models = @("llama3.2", "phi")
            $modelCount = 2
        }
    }

    # SCALAR: Remove duplicates with explicit loop instead of Select-Object -Unique
    $uniqueModels = @()
    $uniqueCount = 0
    for ($i = 0; $i -lt $modelCount; $i++) {
        $model = $models[$i]
        $isDuplicate = $false
        for ($j = 0; $j -lt $uniqueCount; $j++) {
            if ($uniqueModels[$j] -eq $model) {
                $isDuplicate = $true
                break
            }
        }
        if (-not $isDuplicate) {
            $uniqueModels += $model
            $uniqueCount++
        }
    }

    return $uniqueModels
}

# ============================================
# AGENTIC MODULE INITIALIZATION - SCALAR MODE
# ============================================

Write-StartupLog "Initializing Ollama agentic backend (SCALAR)..." "INFO"

# ============================================
# SECURITY MODULES INITIALIZATION - SCALAR MODE
# ============================================

Write-StartupLog "🔒 Initializing Security & Authenticity Modules (SCALAR)..." "INFO"

# Load Security Module
$securityModulePath = Join-Path $PSScriptRoot "RawrXD-SecureCredentials.psm1"
if (Test-Path $securityModulePath) {
    try {
        Import-Module $securityModulePath -Force
        Write-StartupLog "✅ Security Module loaded" "SUCCESS"
    } catch {
        Write-StartupLog "❌ Failed to load Security Module: $_" "ERROR"
    }
} else {
    Write-StartupLog "❌ Security Module not found: $securityModulePath" "ERROR"
}

# Load Authenticity Module
$authenticityModulePath = Join-Path $PSScriptRoot "RawrXD-CopilotIntegration.psm1"
if (Test-Path $authenticityModulePath) {
    try {
        Import-Module $authenticityModulePath -Force
        Write-StartupLog "✅ Authenticity Module loaded" "SUCCESS"
    } catch {
        Write-StartupLog "❌ Failed to load Authenticity Module: $_" "ERROR"
    }
} else {
    Write-StartupLog "❌ Authenticity Module not found: $authenticityModulePath" "ERROR"
}

# Load Performance Module
$performanceModulePath = Join-Path $PSScriptRoot "RawrXD-PerformanceScalability.psm1"
if (Test-Path $performanceModulePath) {
    try {
        Import-Module $performanceModulePath -Force
        Write-StartupLog "✅ Performance Module loaded" "SUCCESS"
    } catch {
        Write-StartupLog "❌ Failed to load Performance Module: $_" "ERROR"
    }
} else {
    Write-StartupLog "❌ Performance Module not found: $performanceModulePath" "ERROR"
}

# Load Collaboration Module
$collaborationModulePath = Join-Path $PSScriptRoot "RawrXD-CollaborationSync.psm1"
if (Test-Path $collaborationModulePath) {
    try {
        Import-Module $collaborationModulePath -Force
        Write-StartupLog "✅ Collaboration Module loaded" "SUCCESS"
    } catch {
        Write-StartupLog "❌ Failed to load Collaboration Module: $_" "ERROR"
    }
} else {
    Write-StartupLog "❌ Collaboration Module not found: $collaborationModulePath" "ERROR"
}

Write-StartupLog "=== SCALAR MODE INITIALIZED ===" "SUCCESS"
Write-StartupLog "All operations use explicit loops - NO pipeline operators" "INFO"
Write-StartupLog "Every iteration is visible and counted explicitly" "INFO"

# ============================================
# MAIN APPLICATION ENTRY POINT
# ============================================

Write-StartupLog "Starting RawrXD IDE in SCALAR mode..." "INFO"
Write-StartupLog "Loading models..." "INFO"
$availableModels = Get-OllamaModels
Write-StartupLog "Found $($availableModels.Count) models" "INFO"

# SCALAR: Display each model with explicit loop
for ($i = 0; $i -lt $availableModels.Count; $i++) {
    Write-StartupLog "  [$i] $($availableModels[$i])" "DEBUG"
}

Write-StartupLog "RawrXD-SCALAR initialization complete" "SUCCESS"
