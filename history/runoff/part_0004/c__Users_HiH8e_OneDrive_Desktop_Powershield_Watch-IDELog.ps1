# Real-time IDE Log Monitor
# Monitors C:\RawrXD_IDE.log and displays colored output

param(
    [string]$LogPath = "C:\RawrXD_IDE.log",
    [switch]$Clear
)

if ($Clear -and (Test-Path $LogPath)) {
    Remove-Item $LogPath -Force
    Write-Host "✓ Log file cleared" -ForegroundColor Green
}

Write-Host "=== RawrXD IDE Log Monitor ===" -ForegroundColor Cyan
Write-Host "Monitoring: $LogPath" -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop`n" -ForegroundColor Yellow

$lastSize = 0

while ($true) {
    if (Test-Path $LogPath) {
        $currentSize = (Get-Item $LogPath).Length
        
        if ($currentSize -gt $lastSize) {
            $fs = [System.IO.File]::Open($LogPath, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
            $fs.Seek($lastSize, [System.IO.SeekOrigin]::Begin) | Out-Null
            $reader = [System.IO.StreamReader]::new($fs)
            
            while ($null -ne ($line = $reader.ReadLine())) {
                # Color code based on log level
                if ($line -match "\[CRIT \]|\[ERROR\]") {
                    Write-Host $line -ForegroundColor Red
                } elseif ($line -match "\[WARN \]") {
                    Write-Host $line -ForegroundColor Yellow
                } elseif ($line -match "\[INFO \]") {
                    Write-Host $line -ForegroundColor Green
                } elseif ($line -match "\[DEBUG\]") {
                    Write-Host $line -ForegroundColor Cyan
                } elseif ($line -match "\[TRACE\]") {
                    Write-Host $line -ForegroundColor DarkGray
                } else {
                    Write-Host $line
                }
            }
            
            $reader.Close()
            $fs.Close()
            $lastSize = $currentSize
        }
    } else {
        Write-Host "[$(Get-Date -Format 'HH:mm:ss')] Waiting for log file to be created..." -ForegroundColor DarkYellow
    }
    
    Start-Sleep -Milliseconds 100
}
