# Integrates all new missing feature stubs into the universal integration pipeline
$featureFiles = Get-ChildItem -Path "D:/lazy init ide/auto_generated_methods" -Filter "*_AutoFeature.ps1"
foreach ($file in $featureFiles) { . $file.FullName }

$results = @()
foreach ($file in $featureFiles) {
    $baseName = $file.BaseName -replace '_AutoFeature$',''
    $funcName = "Invoke-${baseName}"
    if (Get-Command $funcName -ErrorAction SilentlyContinue) {
        $result = & $funcName
        $results += [PSCustomObject]@{ Feature = $funcName; Result = $result }
    }
}

$results | Format-Table -AutoSize
Write-Host "[IntegrateMissingFeatures] All new features integrated and executed."