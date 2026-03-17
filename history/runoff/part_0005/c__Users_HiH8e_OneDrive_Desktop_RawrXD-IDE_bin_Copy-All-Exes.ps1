param(
    [string]$ReleaseDir = 'D:\RawrXD-production-lazy-init\build\Release',
    [string]$OutRoot = "$env:USERPROFILE\OneDrive\Desktop\RawrXD-IDE"
)

$exeOut = Join-Path $OutRoot 'bin\Executables'
$dllOut = Join-Path $OutRoot 'bin\Libraries'
New-Item -ItemType Directory -Force -Path $exeOut,$dllOut | Out-Null

if (!(Test-Path $ReleaseDir)) { Write-Host "[Copy-All-Exes] Release directory not found: $ReleaseDir" -ForegroundColor Red; exit 1 }

$exes = Get-ChildItem $ReleaseDir -Filter *.exe -ErrorAction SilentlyContinue
$dlls = Get-ChildItem $ReleaseDir -Filter *.dll -ErrorAction SilentlyContinue

Write-Host "[Copy-All-Exes] Copying $($exes.Count) executables and $($dlls.Count) libraries..." -ForegroundColor Cyan
foreach ($f in $exes) { Copy-Item $f.FullName -Destination $exeOut -Force }
foreach ($f in $dlls) { Copy-Item $f.FullName -Destination $dllOut -Force }

Write-Host "[Copy-All-Exes] Done. Current executables in output:" -ForegroundColor Green
Get-ChildItem $exeOut -Filter *.exe -ErrorAction SilentlyContinue | Select-Object Name,@{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}} | Format-Table -AutoSize
