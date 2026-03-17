@echo off
REM ============================================================================
REM Pure MASM Crypto Library Build Script
REM Zero dependencies - OpenSSL API compatible
REM ============================================================================

echo Building Pure MASM Crypto Library...
echo =====================================

REM Set paths
set ROOT=%~dp0
set BUILD_DIR=%ROOT%build-crypto
set SRC_DIR=%ROOT%src\crypto
set INCLUDE_DIR=%ROOT%include
set ML_PATH=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x86

REM Create build directory
if exist %BUILD_DIR% rmdir /s /q %BUILD_DIR%
mkdir %BUILD_DIR%
cd %BUILD_DIR%

echo.
echo Step 1: Assembling MASM sources...
echo ------------------------------------

REM Assemble each MASM file
"%ML_PATH%\ml.exe" /c /Zi /Fo%BUILD_DIR%\masm_crypto_rand.obj %SRC_DIR%\masm_crypto_rand.asm
if errorlevel 1 goto :error

echo   [OK] masm_crypto_rand.asm

"%ML_PATH%\ml.exe" /c /Zi /Fo%BUILD_DIR%\masm_crypto_aes.obj %SRC_DIR%\masm_crypto_aes.asm
if errorlevel 1 goto :error

echo   [OK] masm_crypto_aes.asm

"%ML_PATH%\ml.exe" /c /Zi /Fo%BUILD_DIR%\masm_crypto_evp.obj %SRC_DIR%\masm_crypto_evp.asm
if errorlevel 1 goto :error

echo   [OK] masm_crypto_evp.asm

echo.
echo Step 2: Creating static library...
echo ------------------------------------

REM Create static library
lib /OUT:%BUILD_DIR%\masm_crypto.lib %BUILD_DIR%\masm_crypto_rand.obj %BUILD_DIR%\masm_crypto_aes.obj %BUILD_DIR%\masm_crypto_evp.obj
if errorlevel 1 goto :error

echo   [OK] masm_crypto.lib created

echo.
echo Step 3: Running tests...
echo -------------------------

REM Create test program
echo #include "masm_crypto.h" > test_crypto.c
echo #include ^<stdio.h^> >> test_crypto.c
echo. >> test_crypto.c
echo int main() { >> test_crypto.c
echo     unsigned char buf[32]; >> test_crypto.c
echo     if (RAND_bytes(buf, 32)) { >> test_crypto.c
echo         printf("[OK] RAND_bytes test passed\n"); >> test_crypto.c
echo         return 0; >> test_crypto.c
echo     } else { >> test_crypto.c
echo         printf("[FAIL] RAND_bytes test failed\n"); >> test_crypto.c
echo         return 1; >> test_crypto.c
echo     } >> test_crypto.c
echo } >> test_crypto.c

REM Compile test
cl /I%INCLUDE_DIR% /c test_crypto.c
if errorlevel 1 goto :error

REM Link test
link test_crypto.obj %BUILD_DIR%\masm_crypto.lib /OUT:test_crypto.exe
if errorlevel 1 goto :error

REM Run test
test_crypto.exe
if errorlevel 1 goto :error

echo.
echo =====================================
echo Build completed successfully!
echo.
echo Output files:
echo   Library: %BUILD_DIR%\masm_crypto.lib
echo   Header:  %INCLUDE_DIR%\masm_crypto.h
echo   Objects: %BUILD_DIR%\*.obj
echo.
echo The library is ready to use!
echo Replace OpenSSL with masm_crypto.lib
echo =====================================

cd %ROOT%
goto :end

:error
echo.
echo =====================================
echo BUILD FAILED!
echo =====================================
cd %ROOT%
exit /b 1

:end