param(
  [string]$Config = "Release",
  [string]$A       = "x64"
)

# Clean any stale, committed build artifacts that contain a mismatched CMakeCache
if (Test-Path -LiteralPath "build") {
  Write-Host ">>> Cleaning stale build folder..."
  Remove-Item -LiteralPath "build" -Recurse -Force -ErrorAction SilentlyContinue
}

# Also clean any existing CMakeCache.txt files that might be in the wrong location
if (Test-Path -LiteralPath "CMakeCache.txt") {
  Write-Host ">>> Cleaning stray CMakeCache.txt..."
  Remove-Item -LiteralPath "CMakeCache.txt" -Force -ErrorAction SilentlyContinue
}

# Ensure build directory exists after cleaning
New-Item -ItemType Directory -Path "build" -Force | Out-Null

Write-Host ">>> Configuring CMake ..."
cmake -S . -B build -A $A -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host ">>> Building ..."
cmake --build build --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

Write-Host ">>> Done: binaries in build\$Config"
