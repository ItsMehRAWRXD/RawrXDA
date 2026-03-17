# RawrXD-NativeBuild.ps1 — Zero External Dependencies
# Build with: .\RawrXD-NativeBuild.ps1 -Target RawrXD-Native.exe
param(
    [string]$Target = "RawrXD-Native.exe",
    [ValidateSet("Release","Debug")][string]$Config = "Release"
)

# Native tool paths (no CMake)
$ML64 = "ml64.exe"
$CL = "cl.exe"
$LINK = "link.exe"

# Initialize toolchain (same as main build)
$vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vsWhere)) {
    throw "Visual Studio not found. Install VS2022 Build Tools."
}

$vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) {
    throw "VC++ Tools not found. Install 'Desktop development with C++' workload."
}

# Import VC vars
$vcvarsPath = Join-Path $vsPath "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvarsPath)) {
    throw "vcvars64.bat not found at $vcvarsPath"
}

# Extract environment variables from vcvars — use $env: for proper child process inheritance
$tempFile = [System.IO.Path]::GetTempFileName()
cmd /c " `"$vcvarsPath`" && set > `"$tempFile`" "
Get-Content $tempFile | ForEach-Object {
    if ($_ -match "^(.*?)=(.*)$") {
        Set-Item -Path "env:$($matches[1])" -Value $matches[2] -Force
    }
}
Remove-Item $tempFile

# Verify tools
$ML64 = Get-Command ml64.exe -ErrorAction Stop
$CL = Get-Command cl.exe -ErrorAction Stop
$LINK = Get-Command link.exe -ErrorAction Stop

Write-Host "Native Toolchain Ready:" -ForegroundColor Green
Write-Host "  ml64: $($ML64.Source)"
Write-Host "  cl:   $($CL.Source)"
Write-Host "  link: $($LINK.Source)"

# Source groups
$ASM = Get-ChildItem "native_core\kernels\*.asm" -Recurse
$CPP = Get-ChildItem "native_core\*.cpp" -Recurse

# Output dirs
$OBJ = "obj-native"
$BIN = "bin-native"
New-Item -ItemType Directory -Force -Path $OBJ,$BIN | Out-Null

# Compile MASM
$ASM | ForEach-Object {
    & $ML64.Source /c /nologo /Fo"$OBJ\$($_.BaseName).obj" $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "MASM failed on $_" }
}

# Compile C++ (native only—no external includes)
# Resolve Windows SDK paths for system headers
$windowsSdkDir = $env:WindowsSdkDir
$windowsSdkVersion = ($env:WindowsSDKVersion -replace '\\$','')

# Fallback: probe typical install locations
if (-not $windowsSdkDir) {
    $sdkPaths = @(
        "C:\Program Files (x86)\Windows Kits\10",
        "C:\Program Files\Windows Kits\10",
        "${env:ProgramFiles(x86)}\Windows Kits\10"
    )
    foreach ($path in $sdkPaths) {
        if (Test-Path $path) {
            $windowsSdkDir = $path
            $includeDir = Join-Path $path "Include"
            if (Test-Path $includeDir) {
                $latestSdk = Get-ChildItem $includeDir -Directory |
                    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
                    Sort-Object Name -Descending |
                    Select-Object -First 1
                if ($latestSdk) {
                    $windowsSdkVersion = $latestSdk.Name
                    break
                }
            }
        }
    }
}

Write-Host "  SDK: $windowsSdkDir ($windowsSdkVersion)"

$CPPFLAGS = @(
    "/std:c++20", "/EHsc", "/W4", "/nologo",
    "/I.", "/Inative_core",
    "/DUNICODE", "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
    "/DRAWXD_NATIVE_BUILD"
)

# Add Windows SDK include paths
if ($windowsSdkDir -and $windowsSdkVersion) {
    $umPath     = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\um"
    $sharedPath = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\shared"
    $ucrtPath   = Join-Path $windowsSdkDir "Include\$windowsSdkVersion\ucrt"

    Write-Host "  UM Include: $umPath (exists: $(Test-Path $umPath))"
    Write-Host "  Shared Include: $sharedPath (exists: $(Test-Path $sharedPath))"
    Write-Host "  UCRT Include: $ucrtPath (exists: $(Test-Path $ucrtPath))"

    if (Test-Path $umPath)     { $CPPFLAGS += "/I`"$umPath`"" }
    if (Test-Path $sharedPath) { $CPPFLAGS += "/I`"$sharedPath`"" }
    if (Test-Path $ucrtPath)   { $CPPFLAGS += "/I`"$ucrtPath`"" }
}

if ($Config -eq "Release") {
    $CPPFLAGS += "/O2", "/Ob2", "/Oi", "/Ot", "/GL"
} else {
    $CPPFLAGS += "/Od", "/Zi", "/RTC1"
}

$CPP | ForEach-Object {
    & $CL.Source @CPPFLAGS /c /Fo"$OBJ\$($_.BaseName).obj" $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "CL failed on $_" }
}

# Link (kernel32, ws2_32 only—OS components)
$OBJS = Get-ChildItem "$OBJ\*.obj" | ForEach-Object { $_.FullName }
$LINKFLAGS = @(
    "/OUT:$BIN\$Target",
    "/SUBSYSTEM:CONSOLE",
    "/MACHINE:X64",
    "/NOLOGO"
)

# Add SDK library paths
if ($windowsSdkDir -and $windowsSdkVersion) {
    $libUmPath   = Join-Path $windowsSdkDir "Lib\$windowsSdkVersion\um\x64"
    $libUcrtPath = Join-Path $windowsSdkDir "Lib\$windowsSdkVersion\ucrt\x64"
    if (Test-Path $libUmPath)   { $LINKFLAGS += "/LIBPATH:`"$libUmPath`"" }
    if (Test-Path $libUcrtPath) { $LINKFLAGS += "/LIBPATH:`"$libUcrtPath`"" }
}

if ($Config -eq "Release") {
    $LINKFLAGS += "/OPT:REF", "/OPT:ICF", "/LTCG"
} else {
    $LINKFLAGS += "/DEBUG"
}

& $LINK.Source @LINKFLAGS @OBJS kernel32.lib ws2_32.lib user32.lib

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] $BIN\$Target" -ForegroundColor Green
} else {
    throw "Link failed"
}