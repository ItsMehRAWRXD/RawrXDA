# Patcher: inject UIEnhancements module import & post-init actions into RawrXD.ps1
$raw = 'RawrXD.ps1'
if (-not (Test-Path $raw)) { Write-Host "RAW: File not found"; exit 1 }
$content = Get-Content -LiteralPath $raw -Raw
$marker = '[DEBUG] LogFile:'
if ($content -match 'UIEnhancements module loaded') {
  Write-Host 'Patch already applied.'; exit 0
}
$inject = @'
# --- BEGIN UIEnhancements injection ---
try {
    $uiModPath = Join-Path $PSScriptRoot 'tools/UIEnhancements.psm1'
    if (Test-Path $uiModPath) {
        Import-Module $uiModPath -Force -ErrorAction Stop
        if (Get-Command Write-DevConsole -ErrorAction SilentlyContinue) { Write-DevConsole 'UIEnhancements module loaded' 'SUCCESS' } else { Write-Host '[INFO] UIEnhancements module loaded' }
    } else { Write-Host '[WARN] UIEnhancements module not found' }
} catch { Write-Host "[WARN] Failed to load UIEnhancements module: $($_.Exception.Message)" }
if (-not $script:PostInitActions) { $script:PostInitActions = @() }
$script:PostInitActions += { Apply-Theme $script:CurrentTheme; Warmup-Marketplace; Fix-ChatLayout }
# --- END UIEnhancements injection ---
'@
if ($content -notmatch [regex]::Escape($marker)) {
  Write-Host 'Marker not found; aborting.'; exit 1
}
# insert after first occurrence of marker line
$lines = $content -split "`r?`n"
for ($i=0; $i -lt $lines.Length; $i++) {
  if ($lines[$i] -like "*$marker*") {
    $before = $lines[0..$i]
    $after = $lines[($i+1)..($lines.Length-1)]
    $new = @($before + $inject + $after) -join "`r`n"
    $backup = "$raw.bak.$((Get-Date).ToString('yyyyMMdd_HHmmss'))"
    Copy-Item $raw $backup -Force
    Set-Content -LiteralPath $raw -Value $new -Encoding UTF8
    Write-Host "Patched RawrXD.ps1 (backup: $backup)"
    break
  }
}
