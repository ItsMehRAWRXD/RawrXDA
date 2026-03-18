$ErrorActionPreference = "Stop"

param(
    [string]$LogPath = "$env:LOCALAPPDATA\RawrXD\logs\ide.log",
    [string]$AltLogPath = ".\ide.log"
)

function Resolve-LogPath {
    param([string]$Primary, [string]$Secondary)
    if (Test-Path $Primary) { return (Resolve-Path $Primary).Path }
    if (Test-Path $Secondary) { return (Resolve-Path $Secondary).Path }
    throw "Log file not found. Checked: $Primary, $Secondary"
}

$path = Resolve-LogPath -Primary $LogPath -Secondary $AltLogPath
Write-Host "Watching log: $path" -ForegroundColor Cyan
Write-Host "Filters: ghost_text|multicursor|peek_overlay|caret|cosmetics|ollama" -ForegroundColor Cyan

Get-Content -Path $path -Tail 50 -Wait |
    Where-Object {
        $_ -match 'ghost_text' -or
        $_ -match 'multicursor' -or
        $_ -match 'peek_overlay' -or
        $_ -match 'caret' -or
        $_ -match 'cosmetics' -or
        $_ -match 'ollama'
    } |
    ForEach-Object {
        Write-Host $_
    }
