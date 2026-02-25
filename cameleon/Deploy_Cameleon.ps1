# Cameleon Deploy - Dual mode launcher
param([ValidateSet("Auto","Parasite","Native","Bridge")]$Mode="Auto")

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$dll = Join-Path $scriptDir "RawrXD_Cameleon.dll"

switch($Mode) {
    "Auto" {
        $vscode = Get-Process "Code" -ErrorAction SilentlyContinue
        $cursor = Get-Process "Cursor" -ErrorAction SilentlyContinue
        if ($vscode -or $cursor) {
            Write-Host " VS Code/Cursor detected - Parasite mode (load in host)" -ForegroundColor Cyan
            if (Test-Path $dll) {
                Add-Type -Path $dll -ErrorAction SilentlyContinue
                Write-Host " Load RawrXD_Cameleon.dll into extHost for injection." -ForegroundColor Gray
            }
        } else {
            Write-Host " RawrXD Native mode" -ForegroundColor Green
            if (Test-Path $dll) {
                [System.Reflection.Assembly]::LoadFrom($dll) | Out-Null
                Write-Host " Cameleon DLL loaded (native)." -ForegroundColor Gray
            }
        }
    }
    "Parasite" {
        Write-Host " Force Parasite: inject into VS Code/Cursor when available." -ForegroundColor Yellow
    }
    "Native" {
        Write-Host " Force Native: load as RawrXD extension host only." -ForegroundColor Yellow
        if (Test-Path $dll) { [System.Reflection.Assembly]::LoadFrom($dll) | Out-Null }
    }
    "Bridge" {
        Write-Host " Bridge: both hosts active, commands synchronized." -ForegroundColor Magenta
    }
}
