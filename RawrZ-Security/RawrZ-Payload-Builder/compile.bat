@echo off
echo Compiling C++ stub to executable...

rem Try MinGW if available
where gcc >nul 2>&1
if %errorlevel%==0 (
    echo Using system GCC...
    gcc -o calc_stub.exe calc_aes-256-gcm_stub.cpp -static -s -lstdc++
    goto :done
)

rem Try Visual Studio Build Tools
where cl >nul 2>&1
if %errorlevel%==0 (
    echo Using Visual Studio compiler...
    cl /Fe:calc_stub.exe calc_aes-256-gcm_stub.cpp /MT
    goto :done
)

rem Try portable toolchain
if exist "d:\MyCoPilot-Complete-Portable\portable-toolchains\gcc-portable\bin\gcc.exe" (
    echo Using portable GCC...
    "d:\MyCoPilot-Complete-Portable\portable-toolchains\gcc-portable\bin\gcc.exe" -o calc_stub.exe calc_aes-256-gcm_stub.cpp -static -s -lstdc++
    goto :done
)

echo No compiler found. Install MinGW or Visual Studio Build Tools.
exit /b 1

:done
if exist calc_stub.exe (
    echo Successfully compiled: calc_stub.exe
    dir calc_stub.exe
) else (
    echo Compilation failed
)