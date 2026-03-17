#!/usr/bin/env pwsh
<#
    RawrZ npm - Node Package Manager
#>

param(
    [Parameter(ValueFromRemainingArguments=$true)]
    [string[]]$Args
)

$command = $Args[0]
$packages = $Args[1..($Args.Count-1)]

switch ($command) {
    'install' {
        Write-Host "[npm] Installing packages: $($packages -join ', ')" -ForegroundColor Cyan
        foreach ($pkg in $packages) {
            Write-Host "[npm] + $pkg" -ForegroundColor Green
        }
    }
    'init' {
        Write-Host "[npm] Creating package.json..." -ForegroundColor Cyan
        @{
            name = "rawrz-project"
            version = "1.0.0"
            description = ""
            main = "index.js"
            scripts = @{ test = "echo 'Error: no test specified' && exit 1" }
        } | ConvertTo-Json | Out-File "package.json"
        Write-Host "[npm] Created package.json" -ForegroundColor Green
    }
    'run' {
        $script = $packages[0]
        Write-Host "[npm] Running script: $script" -ForegroundColor Cyan
    }
    'list' {
        Write-Host "[npm] Installed packages:" -ForegroundColor Cyan
    }
    default {
        Write-Host "Usage: npm <command>" -ForegroundColor Yellow
        Write-Host "  init              - Create package.json"
        Write-Host "  install <package> - Install package"
        Write-Host "  run <script>      - Run script"
        Write-Host "  list              - List installed packages"
    }
}
