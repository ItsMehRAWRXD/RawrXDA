param(
    [string]$Config = "Release",
    [string]$A = "x64"
)

# Clean build directory to avoid CMake cache conflicts
$buildDir = "build"
if (Test-Path $buildDir) {
    Write-Host ">>> Cleaning existing build directory..."
    Remove-Item -Recurse -Force $buildDir
}

New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
Set-Location $buildDir

Write-Host ">>> Configuring CMake ..."
cmake .. -G "Visual Studio 17 2022" -A $A -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host ">>> Building ..."
cmake --build . --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }

Write-Host ">>> Build completed successfully"
