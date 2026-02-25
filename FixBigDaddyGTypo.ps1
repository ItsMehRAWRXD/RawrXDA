# Fix bigdaddyg-copilot typo: opencaht -> openChat
# Cursor can't find the command when manifest/JS use wrong spelling

$bases = @(
    "$env:USERPROFILE\.cursor\extensions",
    "$env:USERPROFILE\.vscode\extensions"
)

$patched = 0
foreach ($base in $bases) {
    if (-not (Test-Path $base)) { continue }
    $dirs = Get-ChildItem -Path $base -Directory -Filter "bigdaddyg.bigdaddyg-copilot-*" -ErrorAction SilentlyContinue
    foreach ($d in $dirs) {
        # package.json
        $pkg = Join-Path $d.FullName "package.json"
        if (Test-Path $pkg) {
            $content = Get-Content $pkg -Raw
            if ($content -match "opencaht") {
                $content = $content -replace "opencaht", "openChat"
                Set-Content $pkg -Value $content -NoNewline
                Write-Host "Patched package.json: $($d.FullName)" -ForegroundColor Green
                $patched++
            }
        }
        # extension.js (and any .js in out/)
        Get-ChildItem -Path $d.FullName -Recurse -Filter "*.js" -ErrorAction SilentlyContinue | ForEach-Object {
            $c = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
            if ($c -and $c -match "opencaht") {
                $c = $c -replace "opencaht", "openChat"
                Set-Content $_.FullName -Value $c -NoNewline
                Write-Host "Patched $($_.Name): $($_.FullName)" -ForegroundColor Green
                $patched++
            }
        }
    }
}

if ($patched -eq 0) {
    Write-Host "No 'opencaht' typo found (already correct or extension not installed)." -ForegroundColor Gray
} else {
    Write-Host "`nDone. Patched $patched file(s). Reload Cursor/VS Code window." -ForegroundColor Cyan
}
