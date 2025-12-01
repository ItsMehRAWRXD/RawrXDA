param(
  [string]$Config = "Release",
  [string]$A       = "x64"
)

Write-Host ">>> Configuring CMake ..."
cmake -B build -A $A -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host ">>> Building ..."
cmake --build build --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

Write-Host ">>> Done: binaries in build\$Config"
