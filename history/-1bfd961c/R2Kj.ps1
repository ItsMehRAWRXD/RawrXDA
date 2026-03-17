Param(
  [string]$Src = (Join-Path $PSScriptRoot '..\src'),
  [switch]$SummaryOnly
)

if (!(Test-Path $Src)) { Write-Host "Source folder not found: $Src" -ForegroundColor Red; exit 2 }

$asmFiles = Get-ChildItem -Path $Src -Filter *.asm -Recurse
$usage = @{}

foreach ($f in $asmFiles) {
  $lines = Get-Content $f.FullName
  $matches = $lines | Select-String -Pattern '(^|\s)(?i:includelib|INCLUDELIB)\s+([^\s;]+)' -AllMatches
  if ($matches) {
    foreach ($m in $matches) {
      $lib = $m.Matches[0].Groups[2].Value.Trim()
      if (-not $usage.ContainsKey($lib)) { $usage[$lib] = @() }
      $usage[$lib] += $f.FullName
    }
  }
}

if ($usage.Count -eq 0) { Write-Host "No includelib statements found." -ForegroundColor Green; exit 0 }

Write-Host "\n🔎 includelib usage by library:\n" -ForegroundColor Cyan
foreach ($k in $usage.Keys | Sort-Object) {
  $count = ($usage[$k] | Select-Object -Unique).Count
  Write-Host (" - {0}  (in {1} file{2})" -f $k, $count, ($(if($count -ne 1){'s'}else{''})))
}

if (-not $SummaryOnly) {
  Write-Host "\n📄 Detailed file list:" -ForegroundColor Cyan
  foreach ($k in $usage.Keys | Sort-Object) {
    Write-Host "\n[$k]" -ForegroundColor Yellow
    $usage[$k] | Select-Object -Unique | ForEach-Object { Write-Host "  - $_" }
  }
}

Write-Host "\n💡 Recommendation: Replace static imports with dynamic binding via LoadLibraryA/GetProcAddress, or PEB-based resolver for zero import tables. See PURE_MASM_README.md" -ForegroundColor Green
