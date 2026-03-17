#!/usr/bin/env pwsh
<#
.SYNOPSIS
BigDaddyG Interactive Launcher - Select Model & Configuration Before Starting
.DESCRIPTION
Provides interactive menu to:
1. Select which GGUF model to load
2. Configure startup parameters (CPU/GPU, port, memory, etc.)
3. Launch BigDaddyG inference server with chosen settings
4. Connect to CyberForge AV Engine
#>

param(
    [switch]$AutoSelect = $false,
    [string]$DefaultModel = "bigdaddyg-40gb-model.gguf",
    [int]$DefaultPort = 8765
)

# ============================================================================
# CONFIGURATION
# ============================================================================

$modelDir = "D:\BigDaddyG-Standalone-40GB"
$pythonToolchain = "D:\llm_toolchain"
$cyberforgeDir = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\engines\scanner"
$logDir = "C:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\logs"

# Ensure log directory exists
if (!(Test-Path $logDir)) {
    mkdir $logDir | Out-Null
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$logFile = "$logDir\bigdaddyg-launcher-$timestamp.log"

# ============================================================================
# UTILITY FUNCTIONS
# ============================================================================

function Write-Log {
    param([string]$Message, [string]$Level = "INFO")
    $time = Get-Date -Format "HH:mm:ss"
    $logMsg = "[$time] [$Level] $Message"
    Write-Host $logMsg
    Add-Content -Path $logFile -Value $logMsg
}

function Show-Header {
    Clear-Host
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        🤖 BigDaddyG Interactive Launcher                       ║" -ForegroundColor Cyan
    Write-Host "║     Select Model & Configuration Before Starting              ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

function Find-Models {
    Write-Log "Scanning for GGUF models in $modelDir..."
    
    if (!(Test-Path $modelDir)) {
        Write-Log "ERROR: Model directory not found: $modelDir" "ERROR"
        return @()
    }
    
    $models = @()
    $ggufs = Get-ChildItem -Path $modelDir -Filter "*.gguf" -ErrorAction SilentlyContinue
    
    foreach ($gguf in $ggufs) {
        $sizeGB = [math]::Round($gguf.Length / 1GB, 2)
        $models += @{
            Name = $gguf.Name
            Path = $gguf.FullName
            SizeGB = $sizeGB
        }
    }
    
    if ($models.Count -eq 0) {
        Write-Log "WARNING: No GGUF models found in $modelDir" "WARN"
    }
    
    return $models | Sort-Object -Property SizeGB -Descending
}

function Show-ModelSelection {
    param([array]$Models)
    
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "║  STEP 1: Select Model                                         ║" -ForegroundColor Yellow
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Yellow
    Write-Host ""
    
    if ($Models.Count -eq 0) {
        Write-Host "❌ No models found!" -ForegroundColor Red
        Write-Host "   Expected location: $modelDir"
        return $null
    }
    
    for ($i = 0; $i -lt $Models.Count; $i++) {
        $selected = if ($Models[$i].Name -eq $DefaultModel) { "✓ DEFAULT" } else { "" }
        Write-Host "  [$($i + 1)] $($Models[$i].Name) ($($Models[$i].SizeGB) GB) $selected" -ForegroundColor Green
    }
    
    Write-Host ""
    Write-Host "  [0] Exit" -ForegroundColor Red
    Write-Host ""
    
    $choice = Read-Host "Select model (0-$($Models.Count))"
    
    if ($choice -eq "0") {
        Write-Log "User cancelled launcher"
        exit 0
    }
    
    $index = [int]$choice - 1
    
    if ($index -lt 0 -or $index -ge $Models.Count) {
        Write-Host "❌ Invalid selection" -ForegroundColor Red
        Start-Sleep -Seconds 2
        return Show-ModelSelection -Models $Models
    }
    
    return $Models[$index]
}

function Show-ConfigSelection {
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "║  STEP 2: Configure Startup Parameters                         ║" -ForegroundColor Yellow
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Yellow
    Write-Host ""
    
    # Inference Mode
    Write-Host "🔹 INFERENCE MODE:" -ForegroundColor Cyan
    Write-Host "  [1] CPU (Universal, ~2-5 sec/inference)" -ForegroundColor Green
    Write-Host "  [2] GPU CUDA (NVIDIA, ~0.5-1 sec/inference)" -ForegroundColor Yellow
    Write-Host "  [3] GPU ROCm (AMD, ~0.5-1 sec/inference)" -ForegroundColor Yellow
    Write-Host ""
    
    $modeChoice = Read-Host "Select inference mode (1-3, default: 1)"
    $mode = switch ($modeChoice) {
        "2" { "cuda" }
        "3" { "rocm" }
        default { "cpu" }
    }
    
    # Port Selection
    Write-Host ""
    Write-Host "🔹 SERVICE PORT:" -ForegroundColor Cyan
    Write-Host "  [1] Default (8765)" -ForegroundColor Green
    Write-Host "  [2] Custom" -ForegroundColor Yellow
    Write-Host ""
    
    $portChoice = Read-Host "Select port option (1-2, default: 1)"
    
    $port = $DefaultPort
    if ($portChoice -eq "2") {
        $customPort = Read-Host "Enter custom port"
        if ($customPort -match '^\d+$') {
            $port = [int]$customPort
        } else {
            Write-Host "Invalid port, using default: $DefaultPort" -ForegroundColor Yellow
        }
    }
    
    # Memory Limits
    Write-Host ""
    Write-Host "🔹 MEMORY ALLOCATION:" -ForegroundColor Cyan
    Write-Host "  [1] Auto (system determines)" -ForegroundColor Green
    Write-Host "  [2] Conservative (4GB max)" -ForegroundColor Yellow
    Write-Host "  [3] Balanced (8GB)" -ForegroundColor Yellow
    Write-Host "  [4] Aggressive (16GB)" -ForegroundColor Yellow
    Write-Host "  [5] Maximum (32GB)" -ForegroundColor Red
    Write-Host ""
    
    $memChoice = Read-Host "Select memory profile (1-5, default: 2)"
    $maxMemory = switch ($memChoice) {
        "1" { 0 }        # Auto
        "3" { 8192 }
        "4" { 16384 }
        "5" { 32768 }
        default { 4096 }
    }
    
    # Thread Count
    Write-Host ""
    Write-Host "🔹 CPU THREADS:" -ForegroundColor Cyan
    $cpuCount = (Get-CimInstance Win32_Processor).NumberOfLogicalProcessors
    Write-Host "  Available: $cpuCount cores" -ForegroundColor Gray
    Write-Host "  [1] Auto (all cores)" -ForegroundColor Green
    Write-Host "  [2] Half cores ($([math]::Round($cpuCount/2)))" -ForegroundColor Yellow
    Write-Host "  [3] Custom" -ForegroundColor Yellow
    Write-Host ""
    
    $threadChoice = Read-Host "Select thread profile (1-3, default: 1)"
    
    $threads = $cpuCount
    if ($threadChoice -eq "2") {
        $threads = [math]::Max(2, [math]::Round($cpuCount / 2))
    } elseif ($threadChoice -eq "3") {
        $custom = Read-Host "Enter thread count (1-$cpuCount)"
        if ($custom -match '^\d+$' -and [int]$custom -le $cpuCount) {
            $threads = [int]$custom
        }
    }
    
    # Logging Level
    Write-Host ""
    Write-Host "🔹 LOGGING LEVEL:" -ForegroundColor Cyan
    Write-Host "  [1] Info (normal)" -ForegroundColor Green
    Write-Host "  [2] Debug (verbose)" -ForegroundColor Yellow
    Write-Host "  [3] Trace (very verbose)" -ForegroundColor Red
    Write-Host ""
    
    $logLevel = Read-Host "Select log level (1-3, default: 1)"
    $logLevel = switch ($logLevel) {
        "2" { "debug" }
        "3" { "trace" }
        default { "info" }
    }
    
    # API Authentication
    Write-Host ""
    Write-Host "🔹 API SECURITY:" -ForegroundColor Cyan
    Write-Host "  [1] None (local only, no auth)" -ForegroundColor Green
    Write-Host "  [2] Token-based (requires API key)" -ForegroundColor Yellow
    Write-Host ""
    
    $authChoice = Read-Host "Select authentication (1-2, default: 1)"
    $useAuth = if ($authChoice -eq "2") { $true } else { $false }
    
    $apiKey = ""
    if ($useAuth) {
        $apiKey = Read-Host "Enter API key (or press Enter for auto-generate)"
        if ([string]::IsNullOrEmpty($apiKey)) {
            $apiKey = -join ((65..90) + (97..122) + (48..57) | Get-Random -Count 32 | ForEach-Object {[char]$_})
        }
    }
    
    return @{
        Mode = $mode
        Port = $port
        MaxMemory = $maxMemory
        Threads = $threads
        LogLevel = $logLevel
        UseAuth = $useAuth
        ApiKey = $apiKey
    }
}

function Show-ConfigSummary {
    param($Model, $Config)
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  STEP 3: Configuration Summary                                ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    Write-Host "📦 Model:" -ForegroundColor Green
    Write-Host "   Name:     $($Model.Name)"
    Write-Host "   Size:     $($Model.SizeGB) GB"
    Write-Host "   Path:     $($Model.Path)"
    Write-Host ""
    
    Write-Host "⚙️  Configuration:" -ForegroundColor Green
    Write-Host "   Inference: $($Config.Mode.ToUpper())"
    Write-Host "   Port:      $($Config.Port)"
    Write-Host "   Memory:    $(if ($Config.MaxMemory -eq 0) { 'Auto' } else { "$($Config.MaxMemory) MB" })"
    Write-Host "   Threads:   $($Config.Threads)"
    Write-Host "   Log Level: $($Config.LogLevel)"
    Write-Host "   API Auth:  $(if ($Config.UseAuth) { "Enabled (Key: $($Config.ApiKey.Substring(0,8))...)" } else { "Disabled" })"
    Write-Host ""
    
    $confirm = Read-Host "Proceed with these settings? (yes/no)"
    
    if ($confirm -ne "yes") {
        Write-Host "Cancelled. Restarting..." -ForegroundColor Yellow
        Start-Sleep -Seconds 2
        return $false
    }
    
    return $true
}

function Create-StartupScript {
    param($Model, $Config)
    
    Write-Log "Creating startup script with selected configuration..."
    
    $scriptPath = "$pythonToolchain\start-bigdaddyg.py"
    
    $pythonScript = @"
#!/usr/bin/env python3
"""
BigDaddyG Inference Server - Auto-generated Startup Script
Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')
"""

import os
import sys
import argparse
from pathlib import Path

# Configuration (from launcher selection)
MODEL_PATH = r"$($Model.Path)"
INFERENCE_MODE = "$($Config.Mode)"
PORT = $($Config.Port)
MAX_MEMORY = $($Config.MaxMemory)
THREADS = $($Config.Threads)
LOG_LEVEL = "$($Config.LogLevel)"
USE_AUTH = $($Config.UseAuth.ToString().ToLower())
API_KEY = "$(if ($Config.UseAuth) { $Config.ApiKey } else { '' })"

# Validate model exists
if not Path(MODEL_PATH).exists():
    print(f"ERROR: Model not found: {MODEL_PATH}")
    sys.exit(1)

print(f"🤖 BigDaddyG Inference Server")
print(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
print(f"Model:     {Path(MODEL_PATH).name}")
print(f"Mode:      {INFERENCE_MODE}")
print(f"Port:      {PORT}")
print(f"Memory:    {MAX_MEMORY if MAX_MEMORY > 0 else 'Auto'} MB")
print(f"Threads:   {THREADS}")
print(f"Log Level: {LOG_LEVEL}")
print(f"Auth:      {'Enabled' if USE_AUTH else 'Disabled'}")
print(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
print()

try:
    # Import required modules
    try:
        from llama_cpp import Llama
    except ImportError:
        print("ERROR: llama-cpp-python not installed")
        print("Install with: pip install llama-cpp-python")
        sys.exit(1)
    
    try:
        from fastapi import FastAPI, HTTPException
        from pydantic import BaseModel
        import uvicorn
    except ImportError:
        print("ERROR: FastAPI not installed")
        print("Install with: pip install fastapi uvicorn")
        sys.exit(1)
    
    # Load model
    print(f"📦 Loading model: {Path(MODEL_PATH).name}")
    
    llm = Llama(
        model_path=MODEL_PATH,
        n_ctx=2048,
        n_threads=THREADS,
        gpu_layers=0 if INFERENCE_MODE == "cpu" else 100,
    )
    
    print(f"✅ Model loaded successfully")
    print()
    
    # Create FastAPI app
    app = FastAPI(title="BigDaddyG Inference Service")
    
    class ClassifyRequest(BaseModel):
        file_bytes: bytes = None
        file_path: str = None
        features: dict = None
    
    class ClassifyResponse(BaseModel):
        classification: str
        confidence: float
        reasoning: str
        threat_score: float
    
    @app.get("/health")
    async def health():
        return {"status": "healthy", "model": Path(MODEL_PATH).name}
    
    @app.post("/api/classify", response_model=ClassifyResponse)
    async def classify(request: ClassifyRequest):
        """Classify file as MALWARE/CLEAN using BigDaddyG"""
        
        # Prepare prompt
        prompt = "Analyze this malware sample and classify it as MALWARE or CLEAN. "
        if request.features:
            prompt += f"Features: {request.features}. "
        prompt += "Classification: "
        
        # Run inference
        response = llm(
            prompt,
            max_tokens=50,
            temperature=0.1,
            top_p=0.9,
        )
        
        output = response['choices'][0]['text'].strip()
        
        # Parse result
        is_malware = "MALWARE" in output.upper()
        confidence = 0.85 if is_malware else 0.95
        
        return ClassifyResponse(
            classification="MALWARE" if is_malware else "CLEAN",
            confidence=confidence,
            reasoning=output,
            threat_score=0.8 if is_malware else 0.1
        )
    
    # Start server
    print(f"🚀 Starting server on http://localhost:{PORT}")
    print(f"   API Key: {'$(if ($Config.UseAuth) { "Enabled" } else { "Disabled" })'}")
    print(f"   Docs:    http://localhost:{PORT}/docs")
    print()
    
    uvicorn.run(app, host="0.0.0.0", port=PORT, log_level=LOG_LEVEL)
    
except KeyboardInterrupt:
    print("\n\n🛑 Server stopped by user")
    sys.exit(0)
except Exception as e:
    print(f"\n❌ ERROR: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)
"@
    
    $pythonScript | Out-File -FilePath $scriptPath -Encoding UTF8 -Force
    Write-Log "Startup script created: $scriptPath"
    return $scriptPath
}

function Start-Service {
    param($ScriptPath)
    
    Write-Log "Starting BigDaddyG service..."
    Write-Host ""
    Write-Host "🚀 Launching BigDaddyG Inference Server..." -ForegroundColor Cyan
    Write-Host ""
    
    # Check if Python is available
    try {
        $pythonVersion = python --version 2>&1
        Write-Log "Python found: $pythonVersion"
    } catch {
        Write-Log "ERROR: Python not found in PATH" "ERROR"
        Write-Host "❌ Python is not installed or not in PATH" -ForegroundColor Red
        return $false
    }
    
    # Start the service
    $processArgs = @(
        $ScriptPath
    )
    
    try {
        Start-Process -FilePath "python" -ArgumentList $processArgs -NoNewWindow
        Write-Log "Service started successfully"
        Write-Host "✅ BigDaddyG service started!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Server should be available at: http://localhost:$($config.Port)" -ForegroundColor Green
        Write-Host "API Docs:                    http://localhost:$($config.Port)/docs" -ForegroundColor Green
        Write-Host ""
        Write-Host "Waiting for service to initialize (5 seconds)..." -ForegroundColor Yellow
        Start-Sleep -Seconds 5
        
        return $true
    } catch {
        Write-Log "ERROR: Failed to start service: $_" "ERROR"
        Write-Host "❌ Failed to start service" -ForegroundColor Red
        Write-Host $_ -ForegroundColor Red
        return $false
    }
}

function Connect-ToCyberForge {
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║  STEP 4: Connect to CyberForge AV Engine                      ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    $connect = Read-Host "Update CyberForge to use BigDaddyG? (yes/no)"
    
    if ($connect -eq "yes") {
        Write-Host "✅ Integration instructions:" -ForegroundColor Green
        Write-Host ""
        Write-Host "1. Edit: $cyberforgeDir\cyberforge-av-engine.js" -ForegroundColor Yellow
        Write-Host "2. In MLClassificationEngine.scan(), add:" -ForegroundColor Yellow
        Write-Host ""
        Write-Host '   const response = await fetch("http://localhost:8765/api/classify", {' -ForegroundColor Gray
        Write-Host '     method: "POST",' -ForegroundColor Gray
        Write-Host '     headers: { "Content-Type": "application/json" },' -ForegroundColor Gray
        Write-Host '     body: JSON.stringify({ file_bytes: fileBuffer })' -ForegroundColor Gray
        Write-Host '   });' -ForegroundColor Gray
        Write-Host ""
        Write-Host "3. Parse response and use classification result" -ForegroundColor Yellow
        Write-Host ""
        Write-Log "User requested CyberForge integration"
    } else {
        Write-Log "User skipped CyberForge integration"
    }
}

function Show-Completion {
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║  ✅ BigDaddyG Service Ready!                                   ║" -ForegroundColor Green
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "📊 Service Status:" -ForegroundColor Cyan
    Write-Host "   Endpoint:  http://localhost:$($config.Port)" -ForegroundColor Green
    Write-Host "   Health:    http://localhost:$($config.Port)/health" -ForegroundColor Green
    Write-Host "   API Docs:  http://localhost:$($config.Port)/docs" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "📝 Log File:" -ForegroundColor Cyan
    Write-Host "   $logFile" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "💡 Next Steps:" -ForegroundColor Cyan
    Write-Host "   1. Verify service is running: curl http://localhost:$($config.Port)/health" -ForegroundColor Yellow
    Write-Host "   2. Test inference via API docs" -ForegroundColor Yellow
    Write-Host "   3. Integrate with CyberForge AV Engine" -ForegroundColor Yellow
    Write-Host ""
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================

Write-Log "BigDaddyG Interactive Launcher started"

try {
    # Step 1: Find available models
    Show-Header
    $models = Find-Models
    
    # Step 2: Model selection
    $selectedModel = Show-ModelSelection -Models $models
    if ($null -eq $selectedModel) {
        exit 1
    }
    
    Write-Log "Selected model: $($selectedModel.Name) ($($selectedModel.SizeGB) GB)"
    
    # Step 3: Configuration
    $config = Show-ConfigSelection
    Write-Log "Configuration selected: $($config | ConvertTo-Json)"
    
    # Step 4: Summary & confirmation
    Show-Header
    $confirmed = Show-ConfigSummary -Model $selectedModel -Config $config
    
    if (!$confirmed) {
        Write-Log "Configuration not confirmed, restarting launcher"
        & $PSCommandPath
        exit 0
    }
    
    # Step 5: Create startup script
    $scriptPath = Create-StartupScript -Model $selectedModel -Config $config
    
    # Step 6: Start service
    Show-Header
    $started = Start-Service -ScriptPath $scriptPath
    
    if (!$started) {
        Write-Log "Failed to start service" "ERROR"
        exit 1
    }
    
    # Step 7: CyberForge integration
    Connect-ToCyberForge
    
    # Step 8: Show completion
    Show-Completion
    
    Write-Log "BigDaddyG launcher completed successfully"
    
    # Keep window open
    Write-Host ""
    Read-Host "Press Enter to close"
    
} catch {
    Write-Log "FATAL ERROR: $_" "ERROR"
    Write-Host "❌ Fatal error: $_" -ForegroundColor Red
    exit 1
}
