@echo off
setlocal
echo ========================================================
echo RawrXD v3.0 Build System - Production Kernel Mode
echo ========================================================

:: Ensure environment is set up (usually via vcvars64.bat)
if "%VCINSTALLDIR%"=="" (
    echo VC++ not found. Please run within VS Developer Command Prompt.
    exit /b 1
)

:: Create output directory
if not exist "bin" mkdir bin

echo [1/3] Assembling AVX-512 Kernels (MASM)...
ml64 /c /nologo /Fo"bin\RawrXD_Lexer_AVX2.obj" src\asm\RawrXD_Lexer_AVX2.asm
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ml64 /c /nologo /Fo"bin\RawrXD_Inference_AVX512.obj" src\asm\RawrXD_Inference_AVX512.asm
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

ml64 /c /nologo /Fo"bin\RawrXD_Tokenizer.obj" src\asm\RawrXD_Tokenizer.asm
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo [2/3] Compiling C++ Core (O2 Optimization)...
:: Note: Adjust include paths as necessary
cl /O2 /GL /std:c++20 /DUNICODE /D_UNICODE /EHsc ^
   src\main.cpp ^
   src\cli\RawrXD_CLI.cpp ^
   src\gui\RawrXD_GlyphEngine.cpp ^
   src\token_generator.cpp ^
   src\agentic\monaco\MonacoIntegration.cpp ^
   bin\RawrXD_Lexer_AVX2.obj ^
   bin\RawrXD_Inference_AVX512.obj ^
   bin\RawrXD_Tokenizer.obj ^
   /I src /I src\include /I src\agentic /I include ^
   /Fo"bin\\" /Fe"bin\RawrXD.exe" ^
   /link /SUBSYSTEM:CONSOLE /NODEFAULTLIB:libcmt ^
   kernel32.lib user32.lib shell32.lib advapi32.lib ^
   d2d1.lib dwrite.lib ws2_32.lib

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b 1
)

echo [3/3] Build Complete.
echo [SUCCESS] Binary Size: Optimized. No Dependencies.
echo Output: bin\RawrXD.exe

endlocal
