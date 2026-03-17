@echo off
REM Build NASM Swarm Bridge for Windows

echo Building NASM Swarm Bridge...

REM Check NASM
nasm -v >nul 2>&1
if errorlevel 1 (
    echo [ERROR] NASM not found! Install NASM first.
    pause
    exit /b 1
)

REM Assemble
echo [1/2] Assembling swarm_bridge.asm...
nasm -f win64 swarm_bridge.asm -o swarm_bridge.obj
if errorlevel 1 (
    echo [ERROR] Assembly failed!
    pause
    exit /b 1
)

REM Link
echo [2/2] Linking with curl...

REM Try MSVC linker first
where link.exe >nul 2>&1
if not errorlevel 1 (
    echo Using MSVC linker...
    link swarm_bridge.obj libcurl.lib /DLL /OUT:swarm_bridge.dll
) else (
    REM Try GCC
    where gcc.exe >nul 2>&1
    if not errorlevel 1 (
        echo Using GCC linker...
        gcc -shared swarm_bridge.obj -lcurl -o swarm_bridge.dll
    ) else (
        echo [ERROR] No linker found! Install MSVC or GCC.
        pause
        exit /b 1
    )
)

if errorlevel 1 (
    echo [ERROR] Linking failed!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build successful!
echo  Output: swarm_bridge.dll
echo ========================================
echo.
pause
