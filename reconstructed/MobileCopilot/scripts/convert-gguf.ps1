Param(
    [Parameter(Mandatory=$false)][string]$SourceDir = "$PSScriptRoot/../android/app/src/main/assets/models",
    [Parameter(Mandatory=$false)][string]$ConverterPath = "D:/Franken/converter/gguf-converter.exe"
)

Write-Host "[convert-gguf] Starting GGUF -> TFLite conversion pipeline" -ForegroundColor Cyan
if (!(Test-Path $SourceDir)) { Write-Warning "SourceDir not found: $SourceDir"; exit 1 }
if (!(Test-Path $ConverterPath)) { Write-Warning "Converter missing: $ConverterPath"; exit 0 }

Get-ChildItem -Path $SourceDir -Filter *.gguf -Recurse | ForEach-Object {
    $inFile = $_.FullName
    $outFile = [IO.Path]::Combine($_.Directory.FullName, ($_.BaseName + '.tflite'))
    if (Test-Path $outFile) {
        Write-Host "[convert-gguf] Skip existing $outFile" -ForegroundColor Yellow
    } else {
        Write-Host "[convert-gguf] Converting $inFile -> $outFile" -ForegroundColor Green
        & $ConverterPath $inFile $outFile 2>&1 | Write-Host
        if (Test-Path $outFile) {
            Write-Host "[convert-gguf] Success: $outFile" -ForegroundColor Green
        } else {
            Write-Warning "[convert-gguf] Failed: $inFile"
        }
    }
}

Write-Host "[convert-gguf] Done" -ForegroundColor Cyan
