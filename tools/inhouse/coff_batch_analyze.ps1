param(
  [string]$ObjRoot = "D:\\rawrxd",
  [string]$OutDir = "D:\\rawrxd\\build_inhouse\\re_backend",
  [int]$BatchSize = 50,
  [string]$CoffDumpExe = "",
  [switch]$IncludeAsmObjsOnly
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$ObjRoot = (Resolve-Path $ObjRoot).Path
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

if (-not $CoffDumpExe) {
  $candidates = @(
    "D:\\rawrxd\\tools\\inhouse\\bin\\rawrxd_coffdump.exe",
    "D:\\rawrxd\\tools\\inhouse\\RawrXD.CoffDump\\bin\\Release\\net8.0\\win-x64\\publish\\rawrxd_coffdump.exe",
    "D:\\rawrxd\\tools\\inhouse\\RawrXD.CoffDump\\bin\\Release\\net8.0\\rawrxd_coffdump.exe"
  )
  foreach ($c in $candidates) { if (Test-Path $c) { $CoffDumpExe = $c; break } }
}

if (-not $CoffDumpExe -or -not (Test-Path $CoffDumpExe)) {
  throw "rawrxd_coffdump.exe not found. Build it first: dotnet publish D:\\rawrxd\\tools\\inhouse\\RawrXD.CoffDump -c Release -r win-x64"
}

$batchPlan = Join-Path $OutDir "obj_batches.json"
& "D:\\rawrxd\\tools\\inhouse\\re_backend_batch_plan.ps1" -BuildDir $ObjRoot -OutDir $OutDir -BatchSize $BatchSize -ObjRoot $ObjRoot -ObjGlob "*.obj" @(
  if($IncludeAsmObjsOnly){ "-IncludeAsmObjsOnly" }
)

$plan = Get-Content $batchPlan -Raw | ConvertFrom-Json
$resultsDir = Join-Path $OutDir "coff_batches"
New-Item -ItemType Directory -Force -Path $resultsDir | Out-Null

for ($i = 0; $i -lt $plan.totalBatches; $i++) {
  $batch = $plan.batches[$i]
  $listPath = Join-Path $resultsDir ("batch_{0:D3}.txt" -f $i)
  $outPath = Join-Path $resultsDir ("batch_{0:D3}.json" -f $i)
  $batch.files | Set-Content -Path $listPath -Encoding UTF8
  Write-Host ("[COFF] batch {0}/{1} ({2} objs)" -f ($i+1), $plan.totalBatches, $batch.count) -Fore Cyan
  & $CoffDumpExe --in-list $listPath --out $outPath --max-undef 500 | Out-Host
}

Write-Host "OK: wrote $resultsDir" -Fore Green

