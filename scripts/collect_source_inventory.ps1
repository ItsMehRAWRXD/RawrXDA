# RawrXD Source Inventory - Line counts for every source file
$exts = @('.cpp','.h','.hpp','.asm','.cc','.py','.js','.ts','.cmake')
$excludeDirs = @('node_modules','build','build_qt_free','_deps','.git','test_output_','vendor','third_party','dist','.cursor')
$root = "D:\rawrxd"
$outFile = Join-Path $root "SOURCE_INVENTORY_COMPLETE.txt"
$totalsFile = Join-Path $root "SOURCE_INVENTORY_TOTALS.txt"

$files = Get-ChildItem -Path $root -Recurse -File -ErrorAction SilentlyContinue | 
  Where-Object { 
    $ext = [System.IO.Path]::GetExtension($_.Name)
    $name = $_.Name
    $full = $_.FullName
    ($exts -contains $ext -or $name -eq 'CMakeLists.txt') -and 
    -not ($excludeDirs | Where-Object { $full -like "*\$_\*" })
  }

$results = @()
$totalLines = 0
foreach ($f in $files) {
  try {
    $c = (Get-Content $f.FullName -Raw -ErrorAction SilentlyContinue | Measure-Object -Line).Lines
    if ($null -eq $c) { $c = 0 }
    $totalLines += [int]$c
    $rel = $f.FullName.Replace($root + '\', '').Replace('\', '/')
    $results += [PSCustomObject]@{Path=$rel;Lines=[int]$c}
  } catch {
    # skip
  }
}

$sorted = $results | Sort-Object -Property Lines -Descending
$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine("RawrXD SOURCE INVENTORY - Full Report")
[void]$sb.AppendLine("Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')")
[void]$sb.AppendLine("=" * 80)
[void]$sb.AppendLine("TOTALS: $($results.Count) files | $totalLines lines")
[void]$sb.AppendLine("")

foreach ($r in $sorted) {
  $pct = if ($totalLines -gt 0) { [math]::Round(100.0 * $r.Lines / $totalLines, 2) } else { 0 }
  [void]$sb.AppendLine("$($r.Path)|$($r.Lines)|$pct%")
}

$sb.ToString() | Out-File -FilePath $outFile -Encoding utf8
"TOTAL_FILES:$($results.Count)" | Out-File -FilePath $totalsFile -Encoding utf8
"TOTAL_LINES:$totalLines" | Add-Content -Path $totalsFile -Encoding utf8
Write-Host "Done: $($results.Count) files, $totalLines lines -> $outFile"
