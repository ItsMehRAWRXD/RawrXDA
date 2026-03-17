@echo off
echo.
echo ╔════════════════════════════════════════════════════════════╗
echo ║                                                            ║
echo ║   QUICK BUILD ALL - Security Research Suite                ║
echo ║                                                            ║
echo ╚════════════════════════════════════════════════════════════╝
echo.

set SUCCESS=0
set FAILED=0
set TOTAL=0

REM ============================================================================
echo [1/5] Building Mirai Windows Bot...
echo.
call "%~dp0quick-build.bat"
if %errorlevel% equ 0 (
    set /a SUCCESS+=1
    echo ✓ Mirai: SUCCESS
) else (
    set /a FAILED+=1
    echo ✗ Mirai: FAILED
)
set /a TOTAL+=1
echo.

REM ============================================================================
echo [2/5] Building RawrZDesktop...
echo.
call "%~dp0quick-build-rawrzdesktop.bat"
if %errorlevel% equ 0 (
    set /a SUCCESS+=1
    echo ✓ RawrZDesktop: SUCCESS
) else (
    set /a FAILED+=1
    echo ✗ RawrZDesktop: FAILED
)
set /a TOTAL+=1
echo.

REM ============================================================================
echo [3/5] Building OhGee...
echo.
call "%~dp0quick-build-ohgee.bat"
if %errorlevel% equ 0 (
    set /a SUCCESS+=1
    echo ✓ OhGee: SUCCESS
) else (
    set /a FAILED+=1
    echo ✗ OhGee: FAILED
)
set /a TOTAL+=1
echo.

REM ============================================================================
echo [4/5] Setting up rawrz-http-encryptor...
echo.
call "%~dp0quick-setup-rawrz-http.bat"
if %errorlevel% equ 0 (
    set /a SUCCESS+=1
    echo ✓ rawrz-http: SUCCESS
) else (
    set /a FAILED+=1
    echo ✗ rawrz-http: FAILED
)
set /a TOTAL+=1
echo.

REM ============================================================================
echo.
echo ========================================
echo  BUILD SUMMARY
echo ========================================
echo Total Projects: %TOTAL%
echo Success: %SUCCESS%
echo Failed: %FAILED%
echo ========================================
echo.

if %FAILED% equ 0 (
    echo ✓ ALL BUILDS SUCCESSFUL!
    echo.
    echo Built executables:
    echo - build\windows\mirai_bot.exe
    echo - D:\Security Research aka GitHub Repos\RawrZDesktop\...\RawrZDesktop.exe
    echo - D:\Security Research aka GitHub Repos\OhGee\...\KimiAppNative.exe
    echo.
) else (
    echo Some builds failed. Check output above for details.
)

pause
