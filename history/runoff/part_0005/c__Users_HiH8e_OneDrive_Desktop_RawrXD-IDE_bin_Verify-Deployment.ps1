param(
    [string]$Root = "$env:USERPROFILE\OneDrive\Desktop\RawrXD-IDE"
)

$exeDir = Join-Path $Root 'bin\Executables'
$dllDir = Join-Path $Root 'bin\Libraries'

Write-Host "Verifying deployment at: $Root" -ForegroundColor Cyan

if (!(Test-Path $exeDir)) { Write-Host "Executables folder missing: $exeDir" -ForegroundColor Red; exit 1 }
if (!(Test-Path $dllDir)) { Write-Host "Libraries folder missing: $dllDir" -ForegroundColor Yellow }

$exePath = Join-Path $exeDir 'RawrXD-AgenticIDE.exe'
if (!(Test-Path $exePath)) {
    Write-Host "RawrXD-AgenticIDE.exe not found yet. Deployment pending." -ForegroundColor Yellow
    exit 2
}

$exes = Get-ChildItem $exeDir -Filter *.exe -ErrorAction SilentlyContinue
if ($exes) {
    Write-Host "\nExecutables:" -ForegroundColor Green
    $exes | Select-Object Name,@{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}} | Format-Table -AutoSize

    Write-Host "\nHashes (SHA256):" -ForegroundColor Green
    foreach ($e in $exes) {
        $h = Get-FileHash $e.FullName -Algorithm SHA256
        '{0}  {1}' -f $h.Hash, $e.Name | Write-Output
    }
}

if (Test-Path $dllDir) {
    $dlls = Get-ChildItem $dllDir -Filter *.dll -ErrorAction SilentlyContinue
    Write-Host "\nLibraries (.dll): $($dlls.Count) files" -ForegroundColor Green
    $dlls | Sort-Object Length -Descending | Select-Object -First 15 Name,@{N='SizeMB';E={[math]::Round($_.Length/1MB,2)}} | Format-Table -AutoSize
}

Write-Host "\nVerification complete." -ForegroundColor Cyan
