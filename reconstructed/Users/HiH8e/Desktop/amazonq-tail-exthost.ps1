# Tail the most recent extension host log and filter for Amazon Q related lines
$logsRoot = Join-Path $env:APPDATA 'Code\logs'
$files = Get-ChildItem -Path $logsRoot -Recurse -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -eq 'exthost.log' } | Sort-Object LastWriteTime -Descending
if ($files.Count -eq 0) { Write-Output 'NO_EXTHOST_LOG_FOUND'; exit 0 }
$latest = $files[0].FullName
Write-Output "TAILING:$latest"
Get-Content -Path $latest -Wait -Tail 200 | Select-String -Pattern 'amazonq|amazon Q|Amazon Q|amazon-q' -SimpleMatch | ForEach-Object { $_.Line }
