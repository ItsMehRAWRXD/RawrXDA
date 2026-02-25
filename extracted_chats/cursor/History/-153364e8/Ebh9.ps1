# BigDaddyG Assembly Proxy Auto-Start Script
Write-Host "🔧 BigDaddyG Assembly Proxy Manager" -ForegroundColor Cyan

$port = 11441
$proxyScript = "d:\ollama-openai-proxy.js"
$logFile = "d:\assembly-proxy.log"

# Function to check if port is in use
function Test-Port {
    param([int]$Port)
    try {
        $connection = Get-NetTCPConnection -LocalPort $Port -ErrorAction SilentlyContinue
        return $connection -ne $null
    } catch {
        return $false
    }
}

# Function to start proxy server
function Start-AssemblyProxy {
    Write-Host "🚀 Starting Assembly proxy server on port $port..." -ForegroundColor Yellow
    
    if (Test-Port $port) {
        Write-Host "✅ Assembly proxy already running on port $port" -ForegroundColor Green
        return $true
    }
    
    if (-not (Test-Path $proxyScript)) {
        Write-Host "❌ Proxy script not found: $proxyScript" -ForegroundColor Red
        return $false
    }
    
    # Start the server in background
    Start-Process -FilePath "node" -ArgumentList $proxyScript -WindowStyle Hidden -RedirectStandardOutput $logFile -RedirectStandardError "$logFile.err"
    
    # Wait for server to start
    $attempts = 0
    $maxAttempts = 10
    
    while ($attempts -lt $maxAttempts) {
        Start-Sleep -Seconds 1
        if (Test-Port $port) {
            Write-Host "✅ Assembly proxy started successfully on port $port" -ForegroundColor Green
            return $true
        }
        $attempts++
    }
    
    Write-Host "❌ Failed to start Assembly proxy server" -ForegroundColor Red
    return $false
}

# Start the proxy
Start-AssemblyProxy

Write-Host "`n🎯 BigDaddyG Assembly agent should now work!" -ForegroundColor Cyan
Write-Host "📝 Log file: $logFile" -ForegroundColor Gray
