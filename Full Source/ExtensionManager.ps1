#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Standalone Extension Manager - No external dependencies
    
.EXAMPLE
    .\ExtensionManager.ps1
#>

$ExtensionRoot = "d:\lazy init ide\extensions"
$ModuleStore = "d:\lazy init ide\scripts\modules"

# Ensure directories
@($ExtensionRoot, $ModuleStore) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -Path $_ -ItemType Directory -Force | Out-Null }
}

$RegistryPath = Join-Path $ExtensionRoot "registry.json"
$Registry = @{}

# Load registry
if (Test-Path $RegistryPath) {
    try {
        $content = Get-Content $RegistryPath -Raw | ConvertFrom-Json
        foreach ($prop in $content.PSObject.Properties) {
            $Registry[$prop.Name] = $prop.Value
        }
    } catch {}
}

function Save-Registry {
    $Registry | ConvertTo-Json -Depth 10 | Set-Content $RegistryPath
}

function Show-Menu {
    while ($true) {
        Clear-Host
        Write-Host "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║         IDE EXTENSION & PLUGIN MANAGER                       ║" -ForegroundColor Cyan
        Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        Write-Host ""
        
        # Show extensions
        if ($Registry.Count -gt 0) {
            Write-Host "Extensions:" -ForegroundColor White
            foreach ($key in $Registry.Keys | Sort-Object) {
                $ext = $Registry[$key]
                $statusIcon = if ($ext.Enabled) { "🟢" } elseif ($ext.Installed) { "🟡" } else { "⚪" }
                $statusText = if ($ext.Enabled) { "ON " } elseif ($ext.Installed) { "OFF" } else { "NEW" }
                Write-Host "  $statusIcon [$statusText] " -NoNewline
                Write-Host "$key " -NoNewline -ForegroundColor Cyan
                Write-Host "($($ext.Type))" -ForegroundColor Gray
            }
        } else {
            Write-Host "  No extensions yet" -ForegroundColor Gray
        }
        
        Write-Host ""
        Write-Host "[1] Create extension   [4] Enable extension"
        Write-Host "[2] Install extension  [5] Disable extension"
        Write-Host "[3] List extensions    [Q] Quit"
        Write-Host ""
        
        $choice = Read-Host "Select"
        
        switch ($choice) {
            '1' {
                Write-Host ""
                $name = Read-Host "Name"
                if (-not $name) { continue }
                
                $template = @"
#!/usr/bin/env pwsh
# $name Extension

function Invoke-$name {
    Write-Host "Running $name extension..." -ForegroundColor Cyan
    # Add your logic here
}

Export-ModuleMember -Function 'Invoke-$name'
"@
                
                $path = Join-Path $ExtensionRoot "$name.psm1"
                Set-Content -Path $path -Value $template
                
                $Registry[$name] = @{
                    Type = "Custom"
                    Path = $path
                    Created = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
                    Enabled = $false
                    Installed = $false
                }
                Save-Registry
                
                Write-Host "✓ Created: $path" -ForegroundColor Green
                Read-Host "Press Enter"
            }
            '2' {
                $name = Read-Host "Extension name"
                if (-not $name -or -not $Registry.ContainsKey($name)) {
                    Write-Host "Extension not found" -ForegroundColor Red
                    Read-Host "Press Enter"
                    continue
                }
                
                $ext = $Registry[$name]
                $dest = Join-Path $ModuleStore "$name.psm1"
                Copy-Item -Path $ext.Path -Destination $dest -Force
                $ext.Installed = $true
                $ext.InstalledPath = $dest
                Save-Registry
                
                Write-Host "✓ Installed to: $dest" -ForegroundColor Green
                Read-Host "Press Enter"
            }
            '3' {
                Write-Host ""
                Write-Host "Name                 Type      Installed  Enabled" -ForegroundColor White
                Write-Host "──────────────────── ──────── ────────── ────────" -ForegroundColor Gray
                foreach ($key in $Registry.Keys | Sort-Object) {
                    $ext = $Registry[$key]
                    $installed = if ($ext.Installed) { "Yes" } else { "No" }
                    $enabled = if ($ext.Enabled) { "Yes" } else { "No" }
                    Write-Host ("{0,-20} {1,-8} {2,-10} {3}" -f $key, $ext.Type, $installed, $enabled)
                }
                Write-Host ""
                Read-Host "Press Enter"
            }
            '4' {
                $name = Read-Host "Extension name"
                if (-not $name -or -not $Registry.ContainsKey($name)) {
                    Write-Host "Extension not found" -ForegroundColor Red
                    Read-Host "Press Enter"
                    continue
                }
                
                $ext = $Registry[$name]
                
                if (-not $ext.Installed) {
                    Write-Host "Installing first..." -ForegroundColor Yellow
                    $dest = Join-Path $ModuleStore "$name.psm1"
                    Copy-Item -Path $ext.Path -Destination $dest -Force
                    $ext.Installed = $true
                    $ext.InstalledPath = $dest
                }
                
                if ($ext.InstalledPath -and (Test-Path $ext.InstalledPath)) {
                    Import-Module -Path $ext.InstalledPath -Force -DisableNameChecking
                    $ext.Enabled = $true
                    Save-Registry
                    Write-Host "✓ Enabled: $name" -ForegroundColor Green
                } else {
                    Write-Host "Failed to enable" -ForegroundColor Red
                }
                
                Read-Host "Press Enter"
            }
            '5' {
                $name = Read-Host "Extension name"
                if (-not $name -or -not $Registry.ContainsKey($name)) {
                    Write-Host "Extension not found" -ForegroundColor Red
                    Read-Host "Press Enter"
                    continue
                }
                
                Remove-Module -Name $name -Force -ErrorAction SilentlyContinue
                $Registry[$name].Enabled = $false
                Save-Registry
                Write-Host "✓ Disabled: $name" -ForegroundColor Green
                Read-Host "Press Enter"
            }
            'Q' { return }
            'q' { return }
        }
    }
}

Show-Menu
