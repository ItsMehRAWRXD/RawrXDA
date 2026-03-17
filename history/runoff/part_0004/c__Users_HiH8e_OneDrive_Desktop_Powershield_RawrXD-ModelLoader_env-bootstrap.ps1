# Environment Bootstrap for RawrXD Build
# Usage:  .\env-bootstrap.ps1 [-Force] [-Verbose]
param(
    [switch]$Force,
    [switch]$Verbose
)

Write-Host "[env] Bootstrapping MSVC/SDK/CMake environment" -ForegroundColor Cyan

function Log($msg, $color='Gray') { Write-Host $msg -ForegroundColor $color }
function Ok($msg) { Log "✓ $msg" 'Green' }
function Warn($msg) { Log "⚠ $msg" 'Yellow' }
function Err($msg) { Log "✗ $msg" 'Red' }

# 1. Locate vcvars
$vcvarsCandidates = @(
  'C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat',
  'C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat',
  'C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat',
  'C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat'
)
$vcvarsPath = $vcvarsCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $vcvarsPath) { Err "Visual Studio vcvars64.bat not found"; return }
Ok "Found vcvars: $vcvarsPath"

$needInit = $Force -or -not (Get-Command cl.exe -ErrorAction SilentlyContinue)
if ($needInit) {
    Log "Invoking vcvars64.bat..." 'Yellow'
    $quoted = '"' + $vcvarsPath + '"'
    $envDump = cmd /c "$quoted && set"
    if (-not $envDump) { Err "vcvars invocation failed"; return }
    $count=0
    foreach ($line in $envDump) {
        if ($line -match '^([^=]+)=(.*)$') {
            Set-Item -Path Env:$($matches[1]) -Value $matches[2]
            $count++
        }
    }
    Ok "Imported $count environment entries"
} else {
    Ok "MSVC already initialized (cl present)"
}

if (Get-Command cl.exe -ErrorAction SilentlyContinue) { Ok "cl.exe available: $(Get-Command cl.exe).Source" } else { Err "cl.exe still missing" }

# 2. CMake detection
$cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmakeCmd) {
    $cmakeFallbacks = @(
      'C:\Program Files\CMake\bin\cmake.exe',
      'C:\Program Files\Kitware\CMake\bin\cmake.exe',
      'C:\VS2022Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    )
    $cmakeFound = $cmakeFallbacks | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($cmakeFound) {
        $env:PATH = (Split-Path $cmakeFound) + ';' + $env:PATH
        $cmakeCmd = Get-Command cmake -ErrorAction SilentlyContinue
        Ok "Added CMake path: $(Split-Path $cmakeFound)"
    }
}
if ($cmakeCmd) { Ok "CMake: $(cmake --version | Select-Object -First 1)" } else { Warn "CMake not found in PATH" }

# 3. Ninja optional
$ninjaCmd = Get-Command ninja -ErrorAction SilentlyContinue
if ($ninjaCmd) { Ok "Ninja available" } else { Warn "Ninja not found (optional)" }

# 4. Windows SDK sanity
$windowsSdkDir = $env:WindowsSdkDir
$windowsSdkVer = $env:WindowsSdkVersion
if ($windowsSdkDir -and (Test-Path $windowsSdkDir)) { Ok "Windows SDK dir: $windowsSdkDir ($windowsSdkVer)" } else { Warn "WindowsSdkDir not set" }

# 5. MSVC tools dir
if ($env:VCToolsInstallDir -and (Test-Path $env:VCToolsInstallDir)) { Ok "VCTools: $env:VCToolsInstallDir" } else { Warn "VCToolsInstallDir missing" }

# 6. LLVM/Clang fallback
$clang = Get-Command clang-cl.exe -ErrorAction SilentlyContinue
if ($clang) { Ok "clang-cl: $($clang.Source)" } else { Warn "clang-cl not found (build.ps1 will try MSVC)" }

# 7. Qt detection (optional)
$qtHint = 'C:\Qt'
if (Test-Path $qtHint) {
    $qtDirs = Get-ChildItem $qtHint -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '6\.' }
    if ($qtDirs) { Ok "Qt root(s): $($qtDirs.Name -join ', ')" } else { Warn "Qt directory exists but no versioned subfolders" }
} else { Warn "Qt root C:\Qt not present" }

# 8. Summary
Log "---- Summary ----" 'Cyan'
Log "cl.exe:       " + (Get-Command cl.exe -ErrorAction SilentlyContinue ? 'OK' : 'MISSING')
Log "CMake:        " + ($cmakeCmd ? 'OK' : 'MISSING')
Log "Ninja:        " + ($ninjaCmd ? 'OK' : 'MISSING')
Log "Windows SDK:  " + (($windowsSdkDir -and (Test-Path $windowsSdkDir)) ? 'OK' : 'MISSING')
Log "VCTools:      " + (($env:VCToolsInstallDir -and (Test-Path $env:VCToolsInstallDir)) ? 'OK' : 'MISSING')
Log "Qt:           " + (Test-Path $qtHint ? 'FOUND ROOT' : 'NOT FOUND')

Ok "Environment bootstrap complete"
