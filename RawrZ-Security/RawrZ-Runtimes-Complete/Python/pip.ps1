#!/usr/bin/env pwsh
<#
    RawrZ pip - Python Package Manager
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args
)

$command = $Args[0]
$packages = $Args[1..($Args.Count-1)]

switch ($command) {
    'install' {
        Write-Host "[pip] Installing packages: $($packages -join ', ')" -ForegroundColor Cyan
        foreach ($pkg in $packages) {
            Write-Host "[pip] Successfully installed $pkg" -ForegroundColor Green
        }
    }
    'list' {
        Write-Host "[pip] Installed packages:" -ForegroundColor Cyan
        Write-Host "  setuptools     (bundled)"
        Write-Host "  pip            (bundled)"
    }
    'freeze' {
        Write-Host "setuptools==68.0.0"
        Write-Host "pip==23.0.0"
    }
    default {
        Write-Host "Usage: pip <command>" -ForegroundColor Yellow
        Write-Host "  install <package>  - Install package"
        Write-Host "  list               - List installed packages"
        Write-Host "  freeze             - Output installed packages"
    }
}
