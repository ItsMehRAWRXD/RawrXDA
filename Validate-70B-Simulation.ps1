# TITAN 70B Multi-Node Simulation Validation Script
# Version 1.0 (v15.0-pre-tag)

$TITAN_PATH = "D:\rawrxd\TITAN_800B_PRODUCTION.exe"
$GGUF_PATH = "D:\rawrxd\70b_simulation.gguf"
$LOG_PORT_9001 = "D:\rawrxd\titan_9001.log"
$LOG_PORT_9002 = "D:\rawrxd\titan_9002.log"

function Measure-Latency {
    param([string]$LogPath)
    if (Test-Path $LogPath) {
        $lines = Get-Content $LogPath -ReadCount 0 # Optimized for fast read
        $meshTime = $null
        $startTime = $null
        
        foreach ($line in $lines) {
            if ($line -match "Node Started at: (.*)") { $startTime = [DateTime]::Parse($matches[1]) }
            if ($line -match "Mesh Synchronized") { 
                # If we have a timestamp in the log, calculate delta. Otherwise use the line itself.
                Write-Host "[LATENCY] $line" -ForegroundColor Cyan
                return $true 
            }
        }
    }
    return $false
}

Write-Host "--- TITAN 70B MULTI-NODE SIMULATION STARTING ---" -ForegroundColor Green

# 1. Start Node 1 (Port 9001)
Write-Host "[NODE 1] Starting on port 9001..."
$proc1 = Start-Process $TITAN_PATH -ArgumentList "--port 9001 --model $GGUF_PATH --node-id 1" -PassThru -NoNewWindow -RedirectStandardOutput $LOG_PORT_9001

# 2. Start Node 2 (Port 9002)
Write-Host "[NODE 2] Starting on port 9002..."
$proc2 = Start-Process $TITAN_PATH -ArgumentList "--port 9002 --model $GGUF_PATH --node-id 2 --peer localhost:9001" -PassThru -NoNewWindow -RedirectStandardOutput $LOG_PORT_9002

Write-Host "[WARMUP] Waiting for RawrXD_Swarm_Link handshake..."
Start-Sleep -Seconds 5

# 3. Validation: Latency Check
$lat1 = Measure-Latency $LOG_PORT_9001
$lat2 = Measure-Latency $LOG_PORT_9002

if ($lat1 -and $lat2) {
    Write-Host "[SUCCESS] Mesh Synchronized across both nodes." -ForegroundColor Green
} else {
    Write-Host "[WARNING] Mesh Synchronization not detected yet." -ForegroundColor Yellow
}

# 4. Failover Test: Kill Node 1
Write-Host "[FAILOVER] Killing Node 1 mid-simulation to trigger RawrXD_P2P_ReplicateShard..." -ForegroundColor Red
Stop-Process -Id $proc1.Id -Force

Start-Sleep -Seconds 3

# Check Node 2 for failover recovery
$recovery = Get-Content $LOG_PORT_9002 -Tail 20 | Where-Object { $_ -match "Replicating Shard" -or $_ -match "Failover Active" }
if ($recovery) {
    Write-Host "[SUCCESS] Node 2 successfully detected Node 1 failure and initiated shard replication." -ForegroundColor Green
} else {
    Write-Host "[FAILURE] Node 2 did not trigger failover logic." -ForegroundColor Red
}

Write-Host "[CLEANUP] Stopping Node 2..."
Stop-Process -Id $proc2.Id -Force
Write-Host "--- SIMULATION COMPLETE ---"
