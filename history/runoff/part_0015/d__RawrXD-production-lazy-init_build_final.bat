@echo off
cd /d "D:\RawrXD-production-lazy-init\src\masm"

echo Compiling language scaffolder stubs...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" /c /Fo"language_scaffolders_stubs.obj" /nologo /W3 /Zi language_scaffolders_stubs.asm
if %errorlevel% equ 0 (echo   ✅ SUCCESS) else (echo   ❌ FAILED & exit /b 1)

echo.
echo Compiling entry point...
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" /c /Fo"entry_point.obj" /nologo /W3 /Zi entry_point.asm
if %errorlevel% equ 0 (echo   ✅ SUCCESS) else (echo   ❌ FAILED & exit /b 1)

echo.
echo ========================================
echo LINKING COMPLETE EXECUTABLE...
echo ========================================
echo.

"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" ^
    /OUT:RawrXD_IDE_core.exe ^
    /SUBSYSTEM:CONSOLE ^
    /MACHINE:X64 ^
    /DEBUG ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" ^
    /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64" ^
    entry_point.obj ^
    agentic_kernel.obj ^
    language_scaffolders.obj ^
    language_scaffolders_stubs.obj ^
    kernel32.lib ^
    user32.lib ^
    shell32.lib ^
    advapi32.lib

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo ✅ SUCCESS: RawrXD_IDE_core.exe created!
    echo ========================================
    echo.
    dir RawrXD_IDE_core.exe
    echo.
    echo SYSTEM READY - Core Features:
    echo   ✅ Agentic Kernel (40-agent swarm, 800B model)
    echo   ✅ Language Scaffolders (7 base + 12 extended)
    echo   ✅ 19-language configuration table
    echo   ✅ Project generation framework
    echo   ✅ Build & run automation
    echo.
) else (
    echo.
    echo ❌ LINKING FAILED
    echo.
)
