# Extension Build Pipeline - compile all extensions
param([switch]$Install, [switch]$Publish)

$base = "D:\rawrxd\extensions"
if ($env:RAWRXD_EXT_BASE) { $base = $env:RAWRXD_EXT_BASE }

$exts = @(
    "ItsMehRAWRXD.cursor-simple-ai",
    "rawrz-underground.rawrz-agentic",
    "ItsMehRAWRXD.rawrxd-lsp-client",
    "ItsMehRAWRXD.cursor-ollama-proxy",
    "ItsMehRAWRXD.cursor-multi-ai",
    "bigdaddyg.bigdaddyg-copilot",
    "bigdaddyg.bigdaddyg-cursor-chat",
    "bigdaddyg.bigdaddyg-asm-ide",
    "bigdaddyg.bigdaddyg-asm-extension"
)

foreach ($id in $exts) {
    $dir = Join-Path $base $id
    if (-not (Test-Path $dir)) {
        Write-Host "Skip (missing): $id" -ForegroundColor Yellow
        continue
    }
    Write-Host "Building $id..." -ForegroundColor Cyan
    Push-Location $dir
    try {
        npm install 2>$null
        npx tsc 2>$null
        npx vsce package --no-dependencies 2>$null
        if ($Install) {
            $vsix = Get-ChildItem -Filter "*.vsix" | Select-Object -First 1
            if ($vsix) { code --install-extension $vsix.FullName --force }
        }
    } finally {
        Pop-Location
    }
}

if ($Publish) {
    foreach ($id in $exts) {
        $dir = Join-Path $base $id
        if (Test-Path $dir) {
            Push-Location $dir
            npx vsce publish --pat $env:VSCE_PAT
            Pop-Location
        }
    }
}

Write-Host "Done." -ForegroundColor Green
