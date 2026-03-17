@echo off
REM Link with full paths to libraries
cd /d "D:\RawrXD-production-lazy-init\src\masm"

setlocal
set "LIBPATH=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64;C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64"

"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe" ^
    /OUT:RawrXD_IDE_core.exe ^
    /SUBSYSTEM:CONSOLE ^
    /MACHINE:X64 ^
    /DEBUG ^
    /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" ^
    /LIBPATH:"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\lib\x64" ^
    agentic_kernel.obj ^
    language_scaffolders.obj ^
    kernel32.lib ^
    user32.lib ^
    shell32.lib ^
    advapi32.lib

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo SUCCESS: RawrXD_IDE_core.exe created!
    echo ========================================
    echo.
    dir RawrXD_IDE_core.exe
    echo.
    echo RAWRXD IDE READY - CORE FUNCTIONALITY
    echo Core modules linked:
    echo   - Agentic Kernel (40-agent swarm, 800B model)
    echo   - Language Scaffolders (7 base languages)
    echo   - 19-language configuration table
    echo.
) else (
    echo LINKING FAILED
    echo Error code: %errorlevel%
)

endlocal
