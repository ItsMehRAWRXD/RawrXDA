@echo off
echo [BUILD] Starting minimal Native IDE compilation...

REM Compile just the essential files to get a basic working IDE
echo [COMPILE] Building minimal IDE core...

g++ -std=c++20 -DWIN32_LEAN_AND_MEAN -DNOMINMAX ^
    -I./include ^
    -o NativeIDE.exe ^
    src/main.cpp ^
    src/ide_application.cpp ^
    -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32

if %ERRORLEVEL% == 0 (
    echo [OK] Native IDE compiled successfully!
    echo [INFO] Executable: NativeIDE.exe
    echo.
    echo [TEST] Running basic functionality test...
    NativeIDE.exe --version 2>nul
    echo [COMPLETE] Build process finished.
) else (
    echo [ERROR] Compilation failed with error code %ERRORLEVEL%
    echo [INFO] Check the error messages above for details.
)

pause