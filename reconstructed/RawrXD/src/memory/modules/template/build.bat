@echo off
setlocal

:: Configure environment
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat"

:: Create output directory
if not exist "..\..\..\build\memory_modules" mkdir "..\..\..\build\memory_modules"

:: Compile Memory Module
echo Building Memory Module Template...
cl /LD /O2 /EHsc /MD main.cpp /Fe:..\..\..\build\memory_modules\memory_template.dll /link /EXPORT:AllocateContextBuffer /EXPORT:FreeContextBuffer /EXPORT:OptimizeContextBuffer /EXPORT:GetTotalAllocatedBytes

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo Build successful! Output: build\memory_modules\memory_template.dll
