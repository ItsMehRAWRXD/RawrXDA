param(
  [string]$Source = "native_http_server.cpp",
  [string]$Out = "native_http_server.exe"
)

Write-Host "[PS-UCC] Building $Source -> $Out" -ForegroundColor Cyan

function Invoke-Compile([string]$exe, [string]$argList) {
  Write-Host "[PS-UCC] Trying: $exe $argList" -ForegroundColor DarkGray
  Start-Process -FilePath $exe -ArgumentList $argList -NoNewWindow -Wait -ErrorAction SilentlyContinue | Out-Null
  if ($LASTEXITCODE -eq 0) { return $true } else { return $false }
}

# 1) Try MSVC (vcvars)
$vcVarsPaths = @(
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat",
  "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"
)

foreach ($p in $vcVarsPaths) {
  if (Test-Path $p) {
    Write-Host "[PS-UCC] Using MSVC via $p" -ForegroundColor Yellow
    $cmd = "cmd"
    $argList = "/c `"`"$p`" && cl /nologo /std:c++17 /O2 `"$Source`" /Fe:`"$Out`" ws2_32.lib`""
    $ok = Invoke-Compile $cmd $argList
    if ($ok) { exit 0 }
  }
}

# 2) Try clang++
if (Get-Command clang++ -ErrorAction SilentlyContinue) {
  if (Invoke-Compile "clang++" "-std=c++17 -O2 `"$Source`" -o `"$Out`" -lws2_32") { exit 0 }
}

# 3) Try g++
if (Get-Command g++ -ErrorAction SilentlyContinue) {
  if (Invoke-Compile "g++" "-std=c++17 -O2 `"$Source`" -o `"$Out`" -lws2_32") { exit 0 }
}

# 4) Try MinGW default
if (Test-Path "C:\MinGW\bin\g++.exe") {
  if (Invoke-Compile "C:\MinGW\bin\g++.exe" "-std=c++17 -O2 `"$Source`" -o `"$Out`" -lws2_32") { exit 0 }
}

# 5) Try Zig (portable)
if (Get-Command zig -ErrorAction SilentlyContinue) {
  if (Invoke-Compile "zig" "c++ -std=c++17 -O2 `"$Source`" -o `"$Out`" -lws2_32") { exit 0 }
}

Write-Host "[PS-UCC] ERROR: No suitable C/C++ compiler found." -ForegroundColor Red
exit 1
