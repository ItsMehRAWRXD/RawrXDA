param(
  [string]$Root = "D:\RawrXD"
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Root = (Resolve-Path $Root).Path

$zero = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter *.obj -ErrorAction SilentlyContinue |
  Where-Object { $_.Length -eq 0 }

if (-not $zero -or $zero.Count -eq 0) {
  Write-Host "OK: No 0-byte .obj files under $Root"
  exit 0
}

Write-Host "Removing $($zero.Count) zero-byte .obj files under $Root..."
$zero | ForEach-Object {
  try {
    Remove-Item -LiteralPath $_.FullName -Force
    Write-Host "  deleted: $($_.FullName)"
  } catch {
    Write-Warning "  failed : $($_.FullName) ($_)"
  }
}

