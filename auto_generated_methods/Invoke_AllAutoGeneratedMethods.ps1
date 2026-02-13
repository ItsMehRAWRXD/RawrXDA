$scriptRoot = Split-Path -Parent $PSScriptRoot
# Import shared helpers so auto-methods can use `Write-StructuredLog` and config helpers
$loggingModule = Join-Path $scriptRoot 'RawrXD.Logging.psm1'
$configModule = Join-Path $scriptRoot 'RawrXD.Config.psm1'
if (Test-Path $loggingModule) { try { Import-Module $loggingModule -Force -ErrorAction Stop } catch { Write-Warning "Failed to import logging module: $_" } }
if (Test-Path $configModule) { try { Import-Module $configModule -Force -ErrorAction Stop } catch { Write-Warning "Failed to import config module: $_" } }

# Invokes all auto-generated methods in the output directory
$methodFiles = Get-ChildItem -Path "D:/lazy init ide/auto_generated_methods" -Filter "*_AutoMethod.ps1"
foreach ($file in $methodFiles) {
    . $file.FullName
}

$results = @()
foreach ($file in $methodFiles) {
    $baseName = $file.BaseName -replace '_AutoMethod$',''
    $funcName = "Invoke-${baseName}Auto"
    if (Get-Command $funcName -ErrorAction SilentlyContinue) {
        $result = & $funcName
        $results += [PSCustomObject]@{ Method = $funcName; Result = $result }
    }
}

$results | Format-Table -AutoSize
# Also persist structured JSON results for automated inspection
$outPath = Join-Path (Split-Path -Parent $PSScriptRoot) 'InvokeAllResults.json'
$results | ConvertTo-Json -Depth 6 | Set-Content -Path $outPath -Encoding UTF8
Write-Host "Saved invocation results to: $outPath"
