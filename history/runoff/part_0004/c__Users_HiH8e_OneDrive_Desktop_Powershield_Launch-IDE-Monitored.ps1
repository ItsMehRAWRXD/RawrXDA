# IDE Diagnostic & Launch Script
# Clears logs, launches IDE, monitors in real-time

param(
    [switch]$NoLaunch,
    [switch]$KeepOldLogs
)

$ErrorActionPreference = "Stop"
$LogPath = "C:\RawrXD_IDE.log"
$IdePath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD-ModelLoader\build\bin\RawrXD-Win32IDE.exe"

Write-Host "`n=== RawrXD IDE Diagnostics ===" -ForegroundColor Cyan
Write-Host "Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n" -ForegroundColor Gray

# Archive old log if exists
if (-not $KeepOldLogs -and (Test-Path $LogPath)) {
    $timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
    $archivePath = "C:\RawrXD_IDE_$timestamp.log"
    Move-Item $LogPath $archivePath -Force
    Write-Host "📁 Archived old log to: $archivePath" -ForegroundColor Yellow
}

# Check IDE executable
if (Test-Path $IdePath) {
    $ideInfo = Get-Item $IdePath
    Write-Host "✓ IDE Executable Found" -ForegroundColor Green
    Write-Host "  Path: $IdePath" -ForegroundColor Gray
    Write-Host "  Size: $([math]::Round($ideInfo.Length/1MB, 2)) MB" -ForegroundColor Gray
    Write-Host "  Modified: $($ideInfo.LastWriteTime)" -ForegroundColor Gray
} else {
    Write-Host "✗ IDE executable not found at: $IdePath" -ForegroundColor Red
    exit 1
}

# Launch IDE if requested
if (-not $NoLaunch) {
    Write-Host "`n🚀 Launching IDE..." -ForegroundColor Cyan
    Start-Process $IdePath
    Start-Sleep -Seconds 1
    
    # Check if process started
    $ideProcess = Get-Process -Name "RawrXD-Win32IDE" -ErrorAction SilentlyContinue
    if ($ideProcess) {
        Write-Host "✓ IDE Process Started (PID: $($ideProcess.Id))" -ForegroundColor Green
    } else {
        Write-Host "⚠ IDE may have failed to start or exited immediately" -ForegroundColor Yellow
    }
}

# Monitor log
Write-Host "`n📊 Monitoring Log File..." -ForegroundColor Cyan
Write-Host "Press Ctrl+C to stop`n" -ForegroundColor Yellow

$lastSize = 0
$startTime = Get-Date

while ($true) {
    if (Test-Path $LogPath) {
        $currentSize = (Get-Item $LogPath).Length
        
        if ($currentSize -gt $lastSize) {
            $fs = [System.IO.File]::Open($LogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            $fs.Seek($lastSize, [System.IO.SeekOrigin]::Begin) | Out-Null
            $reader = [System.IO.StreamReader]::new($fs)
            
            while ($null -ne ($line = $reader.ReadLine())) {
                # Color code based on log level
                if ($line -match "\[CRIT \]") {
                    Write-Host $line -ForegroundColor Magenta -BackgroundColor DarkRed
                } elseif ($line -match "\[ERROR\]") {
                    Write-Host $line -ForegroundColor Red
                } elseif ($line -match "\[WARN \]") {
                    Write-Host $line -ForegroundColor Yellow
                } elseif ($line -match "\[INFO \]") {
                    Write-Host $line -ForegroundColor White
                } elseif ($line -match "\[DEBUG\]") {
                    Write-Host $line -ForegroundColor Cyan
                } elseif ($line -match "\[TRACE\]") {
                    Write-Host $line -ForegroundColor DarkGray
                } elseif ($line -match ">>>|<<<") {
                    Write-Host $line -ForegroundColor Green
                } else {
                    Write-Host $line
                }
            }
            
            $reader.Close()
            $fs.Close()
            $lastSize = $currentSize
        }
    } else {
        $elapsed = ((Get-Date) - $startTime).TotalSeconds
        if ($elapsed -gt 5) {
            Write-Host "[$(Get-Date -Format 'HH:mm:ss')] ⚠ Log file not created after 5 seconds - IDE may have crashed on startup!" -ForegroundColor Red
            break
        }
    }
    
    # Check if IDE is still running
    $ideProcess = Get-Process -Name "RawrXD-Win32IDE" -ErrorAction SilentlyContinue
    if (-not $ideProcess -and -not $NoLaunch) {
        Write-Host "`n✗ IDE process has exited!" -ForegroundColor Red
        if (Test-Path $LogPath) {
            Write-Host "`n=== Final Log Contents ===" -ForegroundColor Yellow
            Get-Content $LogPath | Select-Object -Last 20
        }
        break
    }
    
    Start-Sleep -Milliseconds 100
}
