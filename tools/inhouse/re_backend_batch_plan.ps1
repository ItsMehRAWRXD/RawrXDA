param(
  [string]$BuildDir = "D:\RawrXD\build_gold",
  [string]$OutDir = "D:\RawrXD\build_inhouse\re_backend",
  [int]$BatchSize = 50,
  [string]$ObjRoot = "",
  [string[]]$ObjGlob = @("*.obj"),
  [string[]]$ExcludeDirFragments = @("\\CMakeFiles\\", "\\_deps\\"),
  [switch]$IncludeAsmObjsOnly
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

if ($BatchSize -lt 1) { throw "BatchSize must be >= 1" }

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$ObjRoot = if ($ObjRoot) { $ObjRoot } else { $BuildDir }
if (!(Test-Path $ObjRoot)) { throw "ObjRoot not found: $ObjRoot" }

$objs = @()
foreach ($g in $ObjGlob) {
  $objs += Get-ChildItem -LiteralPath $ObjRoot -Recurse -File -Filter $g -ErrorAction SilentlyContinue
}

$objs = $objs | Where-Object {
  $p = $_.FullName
  foreach ($frag in $ExcludeDirFragments) {
    if ($p -like "*$frag*") { return $false }
  }
  return $true
}

if ($IncludeAsmObjsOnly) {
  # Heuristic: MASM/NASM objs in this tree tend to be small-to-medium and not named *.cpp.obj.
  $objs = $objs | Where-Object { $_.Name -notmatch '\.(c|cc|cpp)\.obj$' }
}

$objs = $objs | Sort-Object FullName

$batches = New-Object System.Collections.Generic.List[object]
for ($i = 0; $i -lt $objs.Count; $i += $BatchSize) {
  $count = [Math]::Min($BatchSize, $objs.Count - $i)
  $slice = @($objs[$i..($i + $count - 1)] | ForEach-Object { $_.FullName })
  $batches.Add([ordered]@{
    batchIndex = [int]($batches.Count)
    count = $slice.Count
    files = $slice
  }) | Out-Null
}

$plan = [ordered]@{
  generatedAtUtc = [DateTime]::UtcNow.ToString('o')
  buildDir = (Resolve-Path $BuildDir).Path
  objRoot = (Resolve-Path $ObjRoot).Path
  batchSize = $BatchSize
  includeAsmObjsOnly = [bool]$IncludeAsmObjsOnly
  totalObjects = $objs.Count
  totalBatches = $batches.Count
  batches = $batches
}

$outPath = Join-Path $OutDir "obj_batches.json"
$plan | ConvertTo-Json -Depth 6 | Set-Content -Path $outPath -Encoding UTF8

Write-Host "OK: Wrote $outPath" -Fore Green
Write-Host "  Objects: $($plan.totalObjects)" -Fore Gray
Write-Host "  Batches: $($plan.totalBatches) (size=$BatchSize)" -Fore Gray

