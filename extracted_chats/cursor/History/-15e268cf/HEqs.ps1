 # Fix Config.Msi Permission Error - PowerShell Version
Write-Host "🔧 Fixing Config.Msi permission error..." -ForegroundColor Cyan

# Function to safely kill Node.js processes
function Stop-NodeProcesses {
    Write-Host "🛑 Stopping Node.js processes..." -ForegroundColor Yellow
    Get-Process -Name "node" -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
}

# Function to fix Config.Msi permissions
function Fix-ConfigMsiPermissions {
    Write-Host "🔐 Fixing Config.Msi permissions..." -ForegroundColor Yellow
    
    $configMsiPath = "d:\Config.Msi"
    
    try {
        # Check if directory exists
        if (Test-Path $configMsiPath) {
            Write-Host "Config.Msi directory exists, setting permissions..." -ForegroundColor Green
            # Set permissions for the directory
            icacls $configMsiPath /grant "Everyone:F" /T /Q 2>$null
        } else {
            Write-Host "Config.Msi directory doesn't exist, creating with proper permissions..." -ForegroundColor Yellow
            # Create directory with proper permissions
            New-Item -ItemType Directory -Path $configMsiPath -Force | Out-Null
            icacls $configMsiPath /grant "Everyone:F" /T /Q 2>$null
        }
        
        Write-Host "✅ Config.Msi permissions fixed!" -ForegroundColor Green
    } catch {
        Write-Host "⚠️ Could not fix Config.Msi permissions: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Function to restart Assembly proxy
function Restart-AssemblyProxy {
    Write-Host "🚀 Restarting Assembly proxy server..." -ForegroundColor Yellow
    
    # Start the proxy server
    Start-Process -FilePath "node" -ArgumentList "d:\ollama-openai-proxy.js" -WindowStyle Hidden -RedirectStandardOutput "d:\assembly-proxy.log" -RedirectStandardError "d:\assembly-proxy-error.log"
    
    # Wait for server to start
    $attempts = 0
    $maxAttempts = 10
    
    while ($attempts -lt $maxAttempts) {
        Start-Sleep -Seconds 1
        if (Get-NetTCPConnection -LocalPort 11441 -ErrorAction SilentlyContinue) {
            Write-Host "✅ Assembly proxy restarted successfully on port 11441!" -ForegroundColor Green
            return $true
        }
        $attempts++
    }
    
    Write-Host "❌ Failed to restart Assembly proxy server" -ForegroundColor Red
    return $false
}

# Main execution
try {
    # Stop Node.js processes
    Stop-NodeProcesses
    
    # Fix Config.Msi permissions
    Fix-ConfigMsiPermissions
    
    # Restart Assembly proxy
    if (Restart-AssemblyProxy) {
        Write-Host "🎯 Config.Msi error should now be resolved!" -ForegroundColor Green
        Write-Host "📝 Check logs: d:\assembly-proxy.log and d:\assembly-proxy-error.log" -ForegroundColor Gray
    } else {
        Write-Host "❌ Failed to resolve the issue. Check error logs." -ForegroundColor Red
    }
    
} catch {
    Write-Host "❌ Error occurred: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host "`nPress any key to continue..." -ForegroundColor Gray
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")