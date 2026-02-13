@echo off
setlocal enabledelayedexpansion

echo ========================================
echo RawrXD Native Model Bridge - COMPLETE BUILD AND TEST
echo ========================================
echo.

REM Change to Ship directory
cd /d D:\RawrXD\Ship

REM ============================================
REM PHASE 1: Generate Test GGUF Files
REM ============================================
echo [1/5] Generating test GGUF files...
python generate_test_gguf.py
if %ERRORLEVEL% neq 0 (
    echo ❌ GGUF generation failed!
    echo Make sure Python with numpy is installed
    pause
    exit /b 1
)
echo ✅ Test files generated
echo.

REM ============================================
REM PHASE 2: Clean Previous Build
REM ============================================
echo [2/5] Cleaning previous build...
if exist *.obj del /q *.obj
if exist RawrXD_NativeModelBridge.dll del /q RawrXD_NativeModelBridge.dll
if exist *.lib del /q *.lib
if exist *.exp del /q *.exp
echo ✅ Clean complete
echo.

REM ============================================
REM PHASE 3: Assemble (Use FIXED version)
REM ============================================
echo [3/5] Assembling RawrXD_NativeModelBridge_v2_FIXED.asm...
ml64 /c ^
     /Zi ^
     /W3 ^
     /errorReport:prompt ^
     /O2 ^
     /arch:AVX2 ^
     /Fo"RawrXD_NativeModelBridge.obj" ^
     /Fl"RawrXD_NativeModelBridge.lst" ^
     RawrXD_NativeModelBridge_v2_FIXED.asm

if %ERRORLEVEL% neq 0 (
    echo.
    echo ❌ Assembly failed!
    echo.
    echo Checking for common errors...
    findstr /C:"error A" RawrXD_NativeModelBridge.lst > nul
    if %ERRORLEVEL% equ 0 (
        echo.
        echo === Compilation Errors ===
        findstr /C:"error A" RawrXD_NativeModelBridge.lst
        echo.
    )
    echo.
    echo 📝 Check RawrXD_NativeModelBridge.lst for details
    pause
    exit /b 1
)

echo ✅ Assembly successful
echo.

REM ============================================
REM PHASE 4: Link
REM ============================================
echo [4/5] Linking DLL...
link /DLL ^
     /OUT:RawrXD_NativeModelBridge.dll ^
     /MACHINE:X64 ^
     /SUBSYSTEM:WINDOWS ^
     /ENTRY:DllMain ^
     /LARGEADDRESSAWARE ^
     /DEBUG:FULL ^
     /OPT:REF ^
     /OPT:ICF ^
     /MAP:RawrXD_NativeModelBridge.map ^
     RawrXD_NativeModelBridge.obj ^
     kernel32.lib ^
     ntdll.lib ^
     msvcrt.lib

if %ERRORLEVEL% neq 0 (
    echo.
    echo ❌ Linking failed!
    echo.
    echo 📝 Check for unresolved externals
    pause
    exit /b 1
)

echo ✅ Linking successful
echo.

REM ============================================
REM PHASE 5: Verify and Display Info
REM ============================================
echo [5/5] Verifying build...

if not exist RawrXD_NativeModelBridge.dll (
    echo ❌ DLL not found!
    exit /b 1
)

for %%F in (RawrXD_NativeModelBridge.dll) do (
    set SIZE=%%~zF
    set /a SIZE_KB=!SIZE! / 1024
)

echo.
echo ========================================
echo 🎉 BUILD SUCCESSFUL
echo ========================================
echo.
echo 📦 Output Files:
dir /b RawrXD_NativeModelBridge.* 2>nul
echo.
echo 📊 Statistics:
echo   DLL Size: %SIZE% bytes (%SIZE_KB% KB)

REM Check exports
dumpbin /exports RawrXD_NativeModelBridge.dll > exports.txt 2>nul
if %ERRORLEVEL% equ 0 (
    findstr /C:"DllMain" exports.txt >nul
    if %ERRORLEVEL% equ 0 (
        echo   ✅ DllMain exported
    ) else (
        echo   ⚠️ DllMain not found in exports
    )
    
    findstr /C:"LoadModelNative" exports.txt >nul
    if %ERRORLEVEL% equ 0 (
        echo   ✅ LoadModelNative exported
    ) else (
        echo   ⚠️ LoadModelNative not found in exports
    )
)

echo.
echo 🧪 Test Files Available:
if exist test_model_1m.gguf (
    for %%F in (test_model_1m.gguf) do (
        set /a TEST_SIZE=%%~zF / 1024
        echo   ✅ test_model_1m.gguf (!TEST_SIZE! KB)
    )
)
if exist test_model_q2k.gguf (
    for %%F in (test_model_q2k.gguf) do (
        set /a TEST_SIZE=%%~zF / 1024
        echo   ✅ test_model_q2k.gguf (!TEST_SIZE! KB)
    )
)

echo.
echo ========================================
echo 🚀 READY FOR TESTING
echo ========================================
echo.
echo Run PowerShell test:
echo   powershell -ExecutionPolicy Bypass -File test_dll_basic.ps1
echo.
echo Or manual test:
echo   $dll = [System.Reflection.Assembly]::LoadFile("$PWD\RawrXD_NativeModelBridge.dll")
echo   $dll.GetExportedTypes()
echo.

pause
