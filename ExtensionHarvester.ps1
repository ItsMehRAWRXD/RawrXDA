# EXTENSION HARVESTER - Rip out the useful guts, leave the Electron bloat
# Harvests JS + package.json from key extensions for reverse-engineering to MASM64

$extensions = @(
    "bigdaddyg.bigdaddyg-copilot",
    "bigdaddyg.bigdaddyg-asm-ide",
    "ItsMehRAWRXD.rawrxd-lsp-client",
    "undefined_publisher.cursor-ollama-proxy"
)

$harvestDir = "D:\rawrxd\harvested"
New-Item -ItemType Directory -Force -Path $harvestDir | Out-Null

# Check both Cursor and VS Code extension dirs
$searchBases = @(
    "$env:USERPROFILE\.cursor\extensions",
    "$env:USERPROFILE\.vscode\extensions"
)

foreach ($ext in $extensions) {
    foreach ($base in $searchBases) {
        if (-not (Test-Path $base)) { continue }
        $extPath = "$base\$ext-*"
        $folders = Get-ChildItem -Path $extPath -Directory -ErrorAction SilentlyContinue | Select-Object -First 1

        if ($folders) {
            Write-Host "Harvesting $ext from $base..." -ForegroundColor Cyan

            # Extract extension.js / main.js (the actual logic)
            $jsFiles = Get-ChildItem -Path $folders.FullName -Recurse -Filter "*.js" -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match "^(extension|main|index)\.js$" }

            foreach ($js in $jsFiles) {
                $safeName = $ext.Replace('.', '_')
                $dest = "$harvestDir\${safeName}_$($js.Name)"
                Copy-Item $js.FullName $dest -Force
                $kb = [math]::Round((Get-Item $dest).Length / 1KB, 2)
                if ((Get-Item $dest).Length -lt 50000) {
                    Write-Host "  Copied $($js.Name) ($kb KB)" -ForegroundColor DarkGray
                } else {
                    Write-Host "  Copied minified blob $($js.Name) ($kb KB)" -ForegroundColor Yellow
                }
            }

            # Extract package.json for command definitions
            $pkg = Join-Path $folders.FullName "package.json"
            if (Test-Path $pkg) {
                Copy-Item $pkg "$harvestDir\$($ext.Replace('.','_'))_package.json" -Force
                $json = Get-Content $pkg -Raw -ErrorAction SilentlyContinue | ConvertFrom-Json
                if ($json.contributes.commands) {
                    $typos = $json.contributes.commands | Where-Object { $_.command -match "opencaht" }
                    if ($typos) {
                        Write-Host "  TYPO FOUND: opencaht (should be openChat)" -ForegroundColor Red
                        $typos | ForEach-Object { Write-Host "    $($_.command)" }
                    }
                    $cmds = $json.contributes.commands | Select-Object -First 5
                    Write-Host "  Commands (sample): $($cmds.command -join ', ')" -ForegroundColor Green
                }
            }
            break  # found in this base
        }
    }
}

Write-Host "`nHarvest complete. Output: $harvestDir" -ForegroundColor Cyan
Write-Host "Next: Run FixBigDaddyGTypo.ps1 then UninstallExtensionBloat.ps1" -ForegroundColor Gray
