# UNINSTALL THE BLOAT - Keep only harvested logic / native RawrXD modules
# Removes undefined_publisher and placeholder "your-name" extensions

$killList = @(
    "undefined_publisher.cursor-ollama-proxy",
    "undefined_publisher.bigdaddyg-cursor-chat",
    "undefined_publisher.bigdaddyg-asm-extension",
    "your-name.cursor-simple-ai",
    "your-name.cursor-multi-ai"
)

Write-Host "Uninstalling bloat extensions (code + cursor)..." -ForegroundColor Magenta
foreach ($ext in $killList) {
    $code = $null
    $cursor = $null
    try { code --uninstall-extension $ext 2>&1 | Out-Null; $code = $true } catch {}
    try { cursor --uninstall-extension $ext 2>&1 | Out-Null; $cursor = $true } catch {}
    if ($code -or $cursor) {
        Write-Host "  Uninstalled: $ext" -ForegroundColor Red
    } else {
        Write-Host "  Skip (not installed): $ext" -ForegroundColor DarkGray
    }
}
Write-Host "`nBloat purge complete. Keep: bigdaddyg-copilot, bigdaddyg-asm-ide, rawrxd-lsp-client." -ForegroundColor Cyan
