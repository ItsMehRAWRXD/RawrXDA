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

# Source groups
$ASM = Get-ChildItem "native_core\kernels\*.asm" -Recurse
$CPP = Get-ChildItem "native_core\*.cpp" -Recurse

# Output dirs
$OBJ = "obj-native"
$BIN = "bin-native"
New-Item -ItemType Directory -Force -Path $OBJ,$BIN | Out-Null

# Compile MASM
$ASM | ForEach-Object {
    & $ML64 /c /nologo /Fo"$OBJ\$($_.BaseName).obj" $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "MASM failed on $_" }
}

# Compile C++ (native only—no external includes)
$CPPFLAGS = @(
    "/std:c++20", "/EHsc", "/W4",
    "/I.", "/Inative_core",
    "/DUNICODE", "/D_UNICODE",
    "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
    "/DRAWXD_NATIVE_BUILD"
)

if ($Config -eq "Release") {
    $CPPFLAGS += "/O2", "/Ob2", "/Oi", "/Ot", "/GL"
} else {
    $CPPFLAGS += "/Od", "/Zi", "/RTC1"
}

$CPP | ForEach-Object {
    & $CL @CPPFLAGS /c /Fo"$OBJ\$($_.BaseName).obj" $_.FullName
    if ($LASTEXITCODE -ne 0) { throw "CL failed on $_" }
}

# Link (kernel32, ws2_32 only—OS components)
$OBJS = Get-ChildItem "$OBJ\*.obj" | ForEach-Object { $_.FullName }
$LINKFLAGS = @(
    "/OUT:$BIN\$Target",
    "/SUBSYSTEM:CONSOLE",
    "/MACHINE:X64"
)

if ($Config -eq "Release") {
    $LINKFLAGS += "/OPT:REF", "/OPT:ICF", "/LTCG"
} else {
    $LINKFLAGS += "/DEBUG"
}

& $LINK @LINKFLAGS @OBJS kernel32.lib ws2_32.lib user32.lib

if ($LASTEXITCODE -eq 0) {
    Write-Host "[SUCCESS] $BIN\$Target" -ForegroundColor Green
} else {
    throw "Link failed"
}