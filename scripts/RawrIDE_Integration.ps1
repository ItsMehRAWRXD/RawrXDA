# RawrIDE Integration Script
# Connects HexMag, ScanBot, and Harvester

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Split-Path -Parent $ScriptDir

# Import Tools
. "$ScriptDir\Invoke-CSharpCode.ps1"

function Show-Menu {
    Clear-Host
    Write-Host "=== QTShell Rawr IDE Integration ===" -ForegroundColor Cyan
    Write-Host "1. Start HexMag Swarm (Python)"
    Write-Host "2. Run LLM Harvester (ASM)"
    Write-Host "3. Execute Dynamic C# Stub"
    Write-Host "4. Build Harvester"
    Write-Host "Q. Quit"
    Write-Host "===================================="
}

while ($true) {
    Show-Menu
    $choice = Read-Host "Select an option"

    switch ($choice) {
        "1" {
            Write-Host "Starting HexMag Swarm..."
            # Assuming HexMag is in services/hexmag
            $HexMagPath = Join-Path $RootDir "services\hexmag\run_loop.py"
            if (Test-Path $HexMagPath) {
                Start-Process python -ArgumentList "$HexMagPath" -NoNewWindow
            } else {
                Write-Error "HexMag not found at $HexMagPath"
            }
            Pause
        }
        "2" {
            Write-Host "Running LLM Harvester..."
            $HarvesterExe = Join-Path $RootDir "model-llm-harvester.exe"
            if (Test-Path $HarvesterExe) {
                & $HarvesterExe
            } else {
                Write-Error "Harvester executable not found. Try building it first (Option 4)."
            }
            Pause
        }
        "3" {
            Write-Host "Enter C# Code (Type 'END' on a new line to finish):"
            $code = @()
            while ($true) {
                $line = Read-Host
                if ($line -eq "END") { break }
                $code += $line
            }
            $source = $code -join "`n"
            Invoke-CSharpCode -Source $source
            Pause
        }
        "4" {
            Write-Host "Building Harvester..."
            Set-Location $RootDir
            .\build_harvester.bat
            Pause
        }
        "Q" { exit }
        default { Write-Host "Invalid selection." }
    }
}
