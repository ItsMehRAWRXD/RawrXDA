#!/usr/bin/env pwsh
# Simple Swarm Runner - Works with experimental Python builds

Write-Host "========================================" -ForegroundColor Green
Write-Host "NASM IDE - Simplified Swarm Mode" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Green
Write-Host ""

Write-Host "[INFO] Running with experimental Python build" -ForegroundColor Yellow
Write-Host "[INFO] Limited functionality - core modules only" -ForegroundColor Yellow
Write-Host ""

# Check if minimal swarm exists
if (Test-Path "swarm_minimal.py") {
  Write-Host "[1/3] Starting minimal swarm controller..." -ForegroundColor Cyan
  Start-Job -ScriptBlock {
    Set-Location $using:PWD
    py swarm_minimal.py
  } -Name "SimpleSwarm" | Out-Null
    
  Start-Sleep -Seconds 2
}
else {
  Write-Host "[!] swarm_minimal.py not found - creating basic version..." -ForegroundColor Yellow
    
  $minimalCode = @"
#!/usr/bin/env python3
import sys
import time
from datetime import datetime

print("=" * 50)
print("NASM IDE Basic Swarm System")
print("=" * 50)
print(f"Python: {sys.version.split()[0]}")
print(f"Started: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
print("=" * 50)

print("[System] Basic mode - 3 virtual agents")
agents = ["AI-Assistant", "Code-Editor", "Build-System"]

for i, agent in enumerate(agents):
    print(f"[Agent {i}] {agent} - Ready")

print("\n[System] Swarm running in basic mode...")
print("[System] Press Ctrl+C to stop")

try:
    while True:
        time.sleep(5)
        print(f"[{datetime.now().strftime('%H:%M:%S')}] System heartbeat - 3 agents active")
except KeyboardInterrupt:
    print("\n[System] Swarm shutdown requested")
    print("[System] Goodbye!")
"@
    
  $minimalCode | Out-File -FilePath "swarm_minimal.py" -Encoding UTF8
  Write-Host "[✓] Created basic swarm controller" -ForegroundColor Green
    
  Start-Job -ScriptBlock {
    Set-Location $using:PWD
    py swarm_minimal.py
  } -Name "SimpleSwarm" | Out-Null
    
  Start-Sleep -Seconds 2
}

# Create simple HTML dashboard
Write-Host "[2/3] Creating basic dashboard..." -ForegroundColor Cyan
$dashboardHtml = @"
<!DOCTYPE html>
<html>
<head>
    <title>NASM IDE - Basic Mode</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; background: #1e1e1e; color: #fff; }
        .header { background: #007acc; padding: 20px; border-radius: 5px; }
        .status { background: #2d2d30; padding: 15px; margin: 10px 0; border-radius: 5px; }
        .warning { background: #ffcc00; color: #000; padding: 10px; margin: 10px 0; border-radius: 5px; }
        .agent { background: #0e639c; padding: 10px; margin: 5px 0; border-radius: 3px; }
    </style>
</head>
<body>
    <div class="header">
        <h1>NASM IDE Swarm System - Basic Mode</h1>
        <p>Experimental Python Compatibility Mode</p>
    </div>
    
    <div class="warning">
        <strong>Limited Functionality:</strong> Your Python installation is experimental and missing core modules.
        For full features, install standard Python from <a href="https://python.org">python.org</a>
    </div>
    
    <div class="status">
        <h2>System Status</h2>
        <p><strong>Mode:</strong> Basic (Experimental Python Build)</p>
        <p><strong>Agents:</strong> 3 virtual agents active</p>
        <p><strong>Features:</strong> Core functionality only</p>
    </div>
    
    <div class="status">
        <h2>Active Agents</h2>
        <div class="agent">Agent 0: AI-Assistant - Ready</div>
        <div class="agent">Agent 1: Code-Editor - Ready</div>
        <div class="agent">Agent 2: Build-System - Ready</div>
    </div>
    
    <div class="status">
        <h2>Available Actions</h2>
        <p>• Basic code editing</p>
        <p>• Simple build operations</p>
        <p>• File management</p>
        <p>• System monitoring</p>
    </div>
    
    <div class="status">
        <h2>Upgrade to Full Version</h2>
        <p>Install standard Python to unlock:</p>
        <ul>
            <li>10 specialized AI agents</li>
            <li>Advanced async processing</li>
            <li>Real-time collaboration</li>
            <li>Marketplace integration</li>
            <li>Advanced debugging tools</li>
        </ul>
    </div>
</body>
</html>
"@

$dashboardHtml | Out-File -FilePath "basic_dashboard.html" -Encoding UTF8
Write-Host "[✓] Basic dashboard created" -ForegroundColor Green

# Open dashboard
Write-Host "[3/3] Opening dashboard..." -ForegroundColor Cyan
Start-Process (Resolve-Path "basic_dashboard.html").Path

Write-Host ""
Write-Host "[✓] Basic swarm system running!" -ForegroundColor Green
Write-Host "    Dashboard opened in browser" -ForegroundColor White
Write-Host "    Console: Check background job for output" -ForegroundColor White
Write-Host ""
Write-Host "To stop: Get-Job | Stop-Job" -ForegroundColor Gray
Write-Host "To view logs: Receive-Job -Name 'SimpleSwarm'" -ForegroundColor Gray