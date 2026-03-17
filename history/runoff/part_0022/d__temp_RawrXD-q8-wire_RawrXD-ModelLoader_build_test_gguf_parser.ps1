$ErrorActionPreference = "Stop"

Write-Host "`n=== GGUF Parser Integration Build & Test ===" -ForegroundColor Cyan
Write-Host ""

$qtPath = "C:\Qt\6.7.3\msvc2022_64"
$qtBin = "$qtPath\bin"
$qtInclude = "$qtPath\include"
$qtLib = "$qtPath\lib"

# Source files
$sources = @(
    "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\src\qtapp\gguf_parser.cpp",
    "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\src\qtapp\test_gguf_parser.cpp"
)

$includes = @(
    "/I$qtInclude",
    "/I$qtInclude\QtCore",
    "/Id:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\src\qtapp"
)

$libs = @(
    "/LIBPATH:$qtLib",
    "Qt6Core.lib"
)

Write-Host "[1/3] Compiling GGUF parser..." -ForegroundColor Yellow

$compileCmd = "cl /EHsc /std:c++17 /Zc:__cplusplus /permissive- /MD /Fe:test_gguf_parser.exe $($includes -join ' ') $($sources -join ' ') /link $($libs -join ' ')"

Write-Host $compileCmd -ForegroundColor Gray
Invoke-Expression $compileCmd 2>&1 | Select-String -Pattern "error|warning" | ForEach-Object {
    if ($_ -match "error") {
        Write-Host $_ -ForegroundColor Red
    } else {
        Write-Host $_ -ForegroundColor Yellow
    }
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Compilation failed!" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Compilation successful" -ForegroundColor Green
Write-Host ""

Write-Host "[2/3] Setting up Qt environment..." -ForegroundColor Yellow
$env:PATH = "$qtBin;$env:PATH"
Write-Host "✅ Qt binaries added to PATH" -ForegroundColor Green
Write-Host ""

Write-Host "[3/3] Running GGUF parser test..." -ForegroundColor Yellow
Write-Host ""

$testModel = "D:\OllamaModels\BigDaddyG-Q2_K-PRUNED-16GB.gguf"

if (Test-Path $testModel) {
    .\test_gguf_parser.exe $testModel
    if ($LASTEXITCODE -eq 0) {
        Write-Host "`n✅ GGUF Parser test PASSED!" -ForegroundColor Green
    } else {
        Write-Host "`n❌ GGUF Parser test FAILED!" -ForegroundColor Red
    }
} else {
    Write-Host "⚠ Test model not found: $testModel" -ForegroundColor Yellow
    Write-Host "   Please specify a GGUF model path:" -ForegroundColor Gray
    Write-Host "   .\test_gguf_parser.exe <path_to_model.gguf>" -ForegroundColor Gray
}

Write-Host ""
Write-Host "=== Build Complete ===" -ForegroundColor Cyan
Write-Host ""
