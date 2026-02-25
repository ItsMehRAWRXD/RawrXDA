# Build RawrXD extension DLLs (LSP, Copilot, Agentic)
# Output: extension-host\build\RawrXD_LSP.obj, RawrXD_Copilot.obj, RawrXD_Agentic.obj

param([switch]$LinkDll)

$ErrorActionPreference = "Stop"
$rootDir = $PSScriptRoot
$outDir = Join-Path $rootDir "build"
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$ml64 = (Get-ChildItem "C:\Program Files\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
if (-not $ml64) {
    $ml64 = (Get-ChildItem "C:\Program Files (x86)\Microsoft Visual Studio\2022\*\VC\Tools\MSVC\*\bin\Hostx64\x64\ml64.exe" -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}
if (-not $ml64) { Write-Host "ML64 not found"; exit 1 }

$exts = @("RawrXD_LSP","RawrXD_Copilot","RawrXD_Agentic")
foreach ($name in $exts) {
    $asm = Join-Path $rootDir "$name.asm"
    $obj = Join-Path $outDir "$name.obj"
    Write-Host "Assembling $name.asm..."
    & $ml64 /c /Fo"$obj" /W3 /Zd "$asm"
    if ($LASTEXITCODE -ne 0) { Write-Host "  FAIL"; exit 1 }
    Write-Host "  OK"
}
Write-Host "All extensions assembled."
