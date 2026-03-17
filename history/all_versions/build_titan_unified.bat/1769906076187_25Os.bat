@echo off
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
)
setlocal enabledelayedexpansion

:: RawrXD_Titan_UNIFIED Build Script
:: Assembles and links native GGUF inference engine with no external server
:: Produces: RawrXD-Agent.exe (standalone, 120B capable)

echo [*] RawrXD Titan Unified Inference Engine
echo [*] No llama-server.exe required - Direct GGUF inference
echo.

:: Find ml64.exe from VS Build Tools or Community
set "ML64="
set "LINKER="

echo [+] Searching for ml64.exe...
for /f "delims=" %%i in ('where /r "%ProgramFiles(x86)%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

for /f "delims=" %%i in ('where /r "%ProgramFiles%\Microsoft Visual Studio" ml64.exe 2^>nul ^| findstr /i "x64"') do (
    set "ML64=%%i"
    goto :found_ml64
)

echo [-] ml64.exe not found
exit /b 1

:found_ml64
echo [+] Found ml64.exe: !ML64!

:: Find link.exe
for %%D in ("!ML64!") do (
    set "LINKER=%%~dpD\link.exe"
)

if not exist "!LINKER!" (
    echo [-] link.exe not found at !LINKER!
    exit /b 1
)

echo [+] Found link.exe: !LINKER!
echo.

:: Paths
set "SRCDIR=D:\rawrxd\src"
set "OUTDIR=D:\rawrxd\bin"
set "OUTEXE=!OUTDIR!\RawrXD-Agent.exe"

if not exist "!SRCDIR!" (
    echo [-] Source directory not found: !SRCDIR!
    exit /b 1
)

if not exist "!OUTDIR!" (
    echo [+] Creating output directory: !OUTDIR!
    mkdir "!OUTDIR!"
)

echo [1/4] Assembling RawrXD_Titan_UNIFIED.asm...
"!ML64!" /c /Zi /O2 /Fo"!OUTDIR!\RawrXD_Titan_UNIFIED.obj" "!SRCDIR!\RawrXD_Titan_UNIFIED.asm"
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)
echo [+] Titan core assembled

echo [2/4] Assembling RawrXD_Titan_MetaReverse.asm...
"!ML64!" /c /Zi /Fo"!OUTDIR!\RawrXD_Titan_MetaReverse.obj" "!SRCDIR!\RawrXD_Titan_MetaReverse.asm"
if errorlevel 1 (
    echo [-] Assembly failed
    exit /b 1
)
echo [+] PEB bootstrap assembled

echo [3/4] Assembling CLI wrapper...
"!ML64!" /c /Zi /Fo"!OUTDIR!\RawrXD_CLI_Main.obj" "!SRCDIR!\RawrXD_CLI_Main.asm"
if errorlevel 1 (
    echo [-] Assembly failed (optional - continuing)
)

echo [4/4] Linking with no external server dependencies...
"!LINKER!" /SUBSYSTEM:CONSOLE /OUT:"!OUTEXE!" ^
    /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64" ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64" ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\ucrt\x64" ^
    "!OUTDIR!\RawrXD_Titan_UNIFIED.obj" ^
    "!OUTDIR!\RawrXD_Titan_MetaReverse.obj" ^
    "!OUTDIR!\RawrXD_CLI_Main.obj" ^
    kernel32.lib ntdll.lib user32.lib advapi32.lib
if errorlevel 1 (
    echo [-] Linking failed
    exit /b 1
)

echo.
echo ============================================================
echo [SUCCESS] Built RawrXD-Agent.exe
echo          Native GGUF inference engine
echo          AVX-512 Q2_K dequantization
echo          No llama-server.exe, Python, or CUDA needed
echo ============================================================
echo.
echo Usage:
echo   RawrXD-Agent.exe model.gguf "prompt text"
echo.
echo Features:
echo   - Loads 120B models directly into ring buffer
echo   - AVX-512 quantized matmul (90GB/s practical)
echo   - RoPE position embeddings (128k context)
echo   - GQA attention with KV-cache
echo   - Zero-copy ring buffer streaming to CLI/GUI
echo.

endlocal
exit /b 0
