# Powershell Build Script for RawrXD Agentic IDE
# Links restored MASM64 kernels with C++ Win32 Frontend

$ErrorActionPreference = "Stop"

# --- Configuration ---
$ProjectRoot = "D:\rawrxd"
$SrcDir = "d:\rawrxd\Full Source\src"
$Win32AppDir = "$SrcDir\win32app"
$ModulesDir = "$SrcDir\modules"
$AsmDir = "d:\rawrxd\src"
$BuildDir = "$ProjectRoot\build_unified"

# Toolchain Paths (Explicit from discovery)
$MSVC_BASE = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207"
$WIN_SDK_INC = "C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0"
$WIN_SDK_LIB = "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0"

$MasmExe = "$MSVC_BASE\bin\Hostx64\x64\ml64.exe"
$ClExe = "$MSVC_BASE\bin\Hostx64\x64\cl.exe"

# Identify all C++ files for the 26k LOC integration
$CppFiles = @()
if (Test-Path $Win32AppDir) { $CppFiles += Get-ChildItem -Path "$Win32AppDir" -Filter "*.cpp" | Select-Object -ExpandProperty FullName }
if (Test-Path $ModulesDir) { $CppFiles += Get-ChildItem -Path "$ModulesDir" -Filter "*.cpp" -ErrorAction SilentlyContinue | Select-Object -ExpandProperty FullName }
if (Test-Path $SrcDir) { $CppFiles += Get-ChildItem -Path "$SrcDir" -Filter "*.cpp" | Select-Object -ExpandProperty FullName }

# Filter out test harnesses and stubs that might conflict
$CppSources = $CppFiles | Where-Object { 
    $_ -notmatch "test_runner" -and 
    $_ -notmatch "minimal" -and 
    $_ -notmatch "simple_test" -and
    $_ -notmatch "digestion_test_harness" -and
    $_ -notmatch "main_old_cli"
}

# --- Preparation ---
if (Test-Path $BuildDir) {
    Remove-Item -Path "$BuildDir\*" -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $BuildDir | Out-Null
}

# Assemble strictly valid kernels ONLY
$AsmFiles = @(
    "RawrXD_AVX512_Real.asm"
)

$AsmObjs = @()
foreach ($asmName in $AsmFiles) {
    $asmPath = Join-Path $AsmDir $asmName
    $objPath = Join-Path $BuildDir ($asmName.Replace(".asm", ".obj"))
    
    if (Test-Path $asmPath) {
        Write-Host "    -> Assembling $asmName" -ForegroundColor Cyan
        & "$MasmExe" /c /Zi /Cp /Fo "$objPath" "$asmPath" | Out-Default
        if ($LASTEXITCODE -eq 0) {
            $AsmObjs += "`"$objPath`""
        }
    }
}

# --- 2. Compile C++ Modules & Win32 Frontend ---
Write-Host "[*] Compiling 26k LOC C++ Frontend (MSVC)..." -ForegroundColor Yellow

$Libs = @(
    "user32.lib", "gdi32.lib", "comctl32.lib", "shell32.lib", "ole32.lib", "oleaut32.lib",
    "shlwapi.lib", "advapi32.lib", "ws2_32.lib", "winhttp.lib", "imm32.lib", "version.lib",
    "dwrite.lib", "d2d1.lib", "windowscodecs.lib", "kernel32.lib", "crypt32.lib", "msvcrt.lib"
)

$RspContent = @(
    "/EHsc", "/Zi", "/W3", "/std:c++17", "/D_AMD64_", "/DUNICODE", "/D_UNICODE", "/DWIN32_LEAN_AND_MEAN", "/DNOMINMAX",
    "/D_HAS_ITERATOR_DEBUGGING=0", "/D_ITERATOR_DEBUG_LEVEL=0", "/D_CRT_SECURE_NO_WARNINGS",
    "/I`"d:\rawrxd\Full Source\src`"",
    "/I`"d:\rawrxd\Full Source\src\config`"",
    "/I`"d:\rawrxd\Full Source\src\win32app`"",
    "/I`"d:\rawrxd\include`"",
    "/I`"$MSVC_BASE\include`"",
    "/I`"$WIN_SDK_INC\um`"",
    "/I`"$WIN_SDK_INC\shared`"",
    "/I`"$WIN_SDK_INC\ucrt`"",
    "/I`"$WIN_SDK_INC\cppwinrt`"",
    "/X",
    "/D_WIN32_WINNT=0x0A00"
)

foreach ($src in $CppSources) { $RspContent += "`"$src`"" }
foreach ($obj in $AsmObjs) { $RspContent += $obj }

$RspContent += "/Fe`"$BuildDir\RawrXD_Win32IDE.exe`""

$RspContent | Out-File -FilePath "$BuildDir\cl.rsp" -Encoding ASCII

# Compile and Link
$env:LIB = "$MSVC_BASE\lib\x64;$WIN_SDK_LIB\um\x64;$WIN_SDK_LIB\ucrt\x64"
Write-Host "[*] Executing cl.exe with response file..." -ForegroundColor Yellow
& "$ClExe" "@$BuildDir\cl.rsp" $Libs /link /SUBSYSTEM:WINDOWS /MACHINE:X64 /FORCE:MULTIPLE /NODEFAULTLIB:LIBCMT

if ($LASTEXITCODE -ne 0) {
    Write-Host "[!] Build Failed!" -ForegroundColor Red
} else {
    Write-Host "[+] Build Successful: $BuildDir\RawrXD_Win32IDE.exe" -ForegroundColor Green
}
