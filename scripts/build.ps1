param(
  [string]$Config = "Release",
  [string]$A       = "x64"
)

# Clean any stale, committed build artifacts that contain a mismatched CMakeCache
if (Test-Path -LiteralPath "build") {
  Write-Host ">>> Cleaning stale build folder..."
  Remove-Item -LiteralPath "build" -Recurse -Force -ErrorAction SilentlyContinue
}

Write-Host ">>> Configuring CMake ..."
cmake -S . -B build -A $A -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host ">>> Building ..."
cmake --build build --config $Config
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

Write-Host ">>> Done: binaries in build\$Config"

