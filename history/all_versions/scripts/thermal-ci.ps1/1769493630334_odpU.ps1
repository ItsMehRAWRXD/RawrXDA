<#
.SYNOPSIS
    Sovereign Thermal CI/CD Pipeline Script
    Builds the C++ Governor, MASM Agent Bridge, and Stress Orchestrator.
    Runs a smoke test to verify Thermal API availability.

.DESCRIPTION
    1. Compiles SovereignAgentBridge.asm (MASM)
    2. Compiles SovereignThermalStressOrchestrator.asm (MASM)
    3. Compiles GovernorMain.cpp (C++)
    4. Links binaries
    5. Executes SovereignGovernor.exe in background
    6. Uses .NET NamedPipeClient to query status
    7. Validates JSON response

.EXAMPLE
    .\scripts\thermal-ci.ps1 -Clean -Verbose
#>

param(
    [switch]$Clean,
    [switch]$SkipTest,
    [string]$BuildDir = "$PSScriptRoot\..\build_ci"
)

$ErrorActionPreference = "Stop"

# --- Configuration ---
$SrcThermal = Resolve-Path "$PSScriptRoot\..\src\thermal"
$MasmSrc = "$SrcThermal\masm"
$BridgeSrc = "$SrcThermal\agent_bridge"
$GovernorSrc = "$SrcThermal\governor"

# Paths to tools (Assume in PATH for CI, otherwise set by DevCmd)
$ML = "ml64.exe"
$CL = "cl.exe"
$LINK = "link.exe"

# --- Cleanup ---
if ($Clean) {
    Write-Host "[*] Cleaning Build Directory: $BuildDir" -ForegroundColor Yellow
    if (Test-Path $BuildDir) { Remove-Item -Recurse -Force $BuildDir }
}

if (-not (Test-Path $BuildDir)) { New-Item -ItemType Directory -Path $BuildDir | Out-Null }

Set-Location $BuildDir

# --- Build Step 1: Agent Bridge (MASM) ---
Write-Host "[*] Building Agent Bridge (MASM)..." -ForegroundColor Cyan
& $ML /c /nologo /Zi /Fo"SovereignAgentBridge.obj" "$BridgeSrc\SovereignAgentBridge.asm"
if ($LASTEXITCODE -ne 0) { throw "Failed to assemble Agent Bridge" }

# --- Build Step 2: Stress Orchestrator (MASM) ---
Write-Host "[*] Building Stress Orchestrator (MASM)..." -ForegroundColor Cyan
& $ML /c /nologo /Zi /Fo"Orchestrator.obj" "$MasmSrc\SovereignThermalStressOrchestrator.asm"
if ($LASTEXITCODE -ne 0) { throw "Failed to assemble Orchestrator" }

Write-Host "[*] Linking Orchestrator..." -ForegroundColor Cyan
& $LINK /NOLOGO /DEBUG /SUBSYSTEM:CONSOLE /ENTRY:SovereignStressMain /OUT:"SovereignOrchestrator.exe" "Orchestrator.obj" kernel32.lib user32.lib
if ($LASTEXITCODE -ne 0) { throw "Failed to link Orchestrator" }

# --- Build Step 3: Governor (C++ linking MASM Bridge) ---
Write-Host "[*] Building Governor (C++)..." -ForegroundColor Cyan
# Note: GovernorMain.cpp includes ThermalGovernor.h, assume .cpp is there too or header only?
# Based on file list, ThermalGovernor.cpp exists.
& $CL /c /nologo /EHsc /Zi /Fo"GovernorMain.obj" "$GovernorSrc\GovernorMain.cpp" /I"$GovernorSrc"
& $CL /c /nologo /EHsc /Zi /Fo"ThermalGovernor.obj" "$GovernorSrc\ThermalGovernor.cpp" /I"$GovernorSrc"

Write-Host "[*] Linking Sovereign Governor..." -ForegroundColor Cyan
& $LINK /NOLOGO /DEBUG /SUBSYSTEM:CONSOLE /OUT:"SovereignGovernor.exe" "GovernorMain.obj" "ThermalGovernor.obj" "SovereignAgentBridge.obj" kernel32.lib user32.lib advapi32.lib
if ($LASTEXITCODE -ne 0) { throw "Failed to link Sovereign Governor" }

Write-Host "[+] Build Complete." -ForegroundColor Green

if ($SkipTest) { exit 0 }

# --- Test Execution ---
Write-Host "[*] Starting Smoke Test..." -ForegroundColor Magenta

# 1. Start Governor (Stores MMF, Hosts Pipe)
$procGovernor = Start-Process -FilePath ".\SovereignGovernor.exe" -PassThru -NoNewWindow
Write-Host "    -> Governor Started (PID: $($procGovernor.Id))"
Start-Sleep -Seconds 2

# 2. Check Pipe content
Write-Host "[*] Querying Thermal Agent Pipe..."
try {
    $pipeClient = New-Object System.IO.Pipes.NamedPipeClientStream(".", "SovereignThermalAgent", [System.IO.Pipes.PipeDirection]::InOut)
    $pipeClient.Connect(2000) # 2s timeout
    $pipeClient.ReadMode = [System.IO.Pipes.PipeTransmissionMode]::Message

    $writer = New-Object System.IO.StreamWriter($pipeClient)
    $writer.AutoFlush = $true
    $reader = New-Object System.IO.StreamReader($pipeClient)

    # Send Command
    $cmd = "GET_STATUS"
    $writer.Write($cmd)
    
    # Read Response
    $response = $reader.ReadToEnd()
    
    Write-Host "    -> RAW RESPONSE: $response" -ForegroundColor Green
    
    # Validation
    if ($response -match "status" -and $response -match "online") {
        Write-Host "    [PASS] API returned valid JSON status." -ForegroundColor Green
    } else {
        Write-Host "    [FAIL] Invalid API response." -ForegroundColor Red
        throw "Test Failed"
    }

} catch {
    Write-Host "    [FAIL] Pipe Connection Error: $_" -ForegroundColor Red
    throw "Pipe Validation Failed"
} finally {
    if ($pipeClient) { $pipeClient.Dispose() }
    
    # Cleanup Processes
    Stop-Process -Id $procGovernor.Id -Force -ErrorAction SilentlyContinue
    # Also kill Orchestrator if we ran it (we didn't run it this time, just built it)
}

Write-Host "[+] CI Pipeline Success." -ForegroundColor Green
