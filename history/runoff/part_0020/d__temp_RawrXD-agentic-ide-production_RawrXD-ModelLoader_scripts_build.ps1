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
# Disable Vulkan in CI if not available
$cmakeArgs = @("-S", ".", "-B", "build", "-A", $A, "-DCMAKE_BUILD_TYPE=$Config")

if ($env:CI -eq "true") {
  Write-Host "CI environment detected - disabling Vulkan (not available in CI)"
  $cmakeArgs += "-DGGML_VULKAN=OFF"
  $cmakeArgs += "-DENABLE_VULKAN=OFF"
}

# Set OpenSSL path if available from environment or common install location
$opensslPath = $null
if ($env:OPENSSL_DIR) {
  $opensslPath = $env:OPENSSL_DIR
} elseif ($env:OPENSSL_ROOT_DIR) {
  $opensslPath = $env:OPENSSL_ROOT_DIR
} elseif (Test-Path "C:\\Program Files\\OpenSSL-Win64") {
  # Fallback to standard install path used by choco 'openssl' (full SDK)
  $opensslPath = "C:\\Program Files\\OpenSSL-Win64"
}

if ($opensslPath) {
  $opensslInclude = Join-Path $opensslPath "include"
  $opensslLib = Join-Path $opensslPath "lib"
  $cryptoLib = Join-Path $opensslLib "libcrypto.lib"
  $sslLib = Join-Path $opensslLib "libssl.lib"

  Write-Host "Using OpenSSL from: $opensslPath"
  if (-not (Test-Path $cryptoLib) -or -not (Test-Path $sslLib) -or -not (Test-Path (Join-Path $opensslInclude "openssl"))) {
    Write-Warning "OpenSSL SDK appears incomplete at '$opensslPath'. Ensure full SDK (not openssl.light) is installed."
  }

  $cmakeArgs += "-DOPENSSL_ROOT_DIR=$opensslPath"
  $cmakeArgs += "-DOPENSSL_DIR=$opensslPath"
  $cmakeArgs += "-DOPENSSL_INCLUDE_DIR=$opensslInclude"
  $cmakeArgs += "-DOPENSSL_CRYPTO_LIBRARY=$cryptoLib"
  $cmakeArgs += "-DOPENSSL_SSL_LIBRARY=$sslLib"
  if ($env:CMAKE_PREFIX_PATH) {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$opensslPath;$($env:CMAKE_PREFIX_PATH)"
  } else {
    $cmakeArgs += "-DCMAKE_PREFIX_PATH=$opensslPath"
  }
}

cmake @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }

Write-Host ">>> Building ..."
cmake --build build --config $Config --parallel
if ($LASTEXITCODE -ne 0) { throw "CMake build failed" }

Write-Host ">>> Done: binaries in build\$Config"
