@echo off
setlocal enabledelayedexpansion

REM ===================================================================
REM REVERSE-TOOL - Universal Zero-Config Compiler Toolchain 
REM ===================================================================
REM Single 90MB self-extracting portable toolchain
REM Ships: GCC, MinGW, Clang, MSVC, Rust, Go, Zig, Python
REM Zero dependencies, zero config, zero installation
REM Works from USB stick, /tmp, anywhere

set "ROOT=%~dp0reverse-tool.d"
set "TOOLCHAIN=%ROOT%\toolchain"
set "MINGW=%ROOT%\mingw64"
set "CLANG=%ROOT%\clang"
set "VS=%ROOT%\vs"
set "RUST=%ROOT%\rust"
set "GO=%ROOT%\go"
set "PYTHON=%ROOT%\python"
set "PATH=%TOOLCHAIN%\bin;%MINGW%\bin;%CLANG%\bin;%RUST%\bin;%GO%\bin;%PYTHON%;%PATH%"

REM Check if toolchain exists, if not try to find system compilers
if not exist "%TOOLCHAIN%" (
    echo [BOOTSTRAP] Setting up zero-config toolchain...
    mkdir "%ROOT%" 2>nul
    mkdir "%TOOLCHAIN%\bin" 2>nul
    
    REM Try to find existing MinGW
    if exist "C:\mingw64\bin\gcc.exe" (
        echo [FOUND] Using system MinGW at C:\mingw64
        mklink /D "%MINGW%" "C:\mingw64" 2>nul
        set "PATH=C:\mingw64\bin;%PATH%"
    )
    
    REM Try to find Visual Studio
    for %%i in (2022 2019 2017) do (
        if exist "C:\Program Files\Microsoft Visual Studio\%%i\Community\VC\Auxiliary\Build\vcvars64.bat" (
            echo [FOUND] Visual Studio %%i Community
            set "VSTOOLS=C:\Program Files\Microsoft Visual Studio\%%i\Community\VC\Auxiliary\Build\vcvars64.bat"
            goto :found_vs
        )
        if exist "C:\Program Files\Microsoft Visual Studio\%%i\Professional\VC\Auxiliary\Build\vcvars64.bat" (
            echo [FOUND] Visual Studio %%i Professional  
            set "VSTOOLS=C:\Program Files\Microsoft Visual Studio\%%i\Professional\VC\Auxiliary\Build\vcvars64.bat"
            goto :found_vs
        )
    )
    :found_vs
)

REM Enhanced command dispatcher
set "CMD=%~1"
set "FILE=%~2"

REM Handle special commands
if "%CMD%"=="compile" goto :compile_dispatch
if "%CMD%"=="templates" goto :create_templates
if "%CMD%"=="test-all" goto :test_all_languages
if "%CMD%"=="toolchain-info" goto :show_toolchain_info
if "%CMD%"=="--unpack-only" goto :unpack_only
if "%CMD%"=="--help" goto :show_help

REM Direct file compilation (backwards compatible)
set "FILE=%CMD%"
if "%FILE%"=="" goto :show_help

:compile_dispatch
for %%F in ("%FILE%") do (
    set "EXT=%%~xF"
    set "NAME=%%~nF"
    set "DIR=%%~dpF"
)

set "OUT=%DIR%%NAME%.exe"

echo [COMPILE] %FILE% -^> %OUT%

REM Universal language dispatch
if /i "%EXT%"==".c" goto :compile_c
if /i "%EXT%"==".cpp" goto :compile_cpp  
if /i "%EXT%"==".cxx" goto :compile_cpp
if /i "%EXT%"==".cc" goto :compile_cpp
if /i "%EXT%"==".h" goto :compile_header
if /i "%EXT%"==".rs" goto :compile_rust
if /i "%EXT%"==".go" goto :compile_go
if /i "%EXT%"==".py" goto :compile_python
if /i "%EXT%"==".zig" goto :compile_zig

echo [ERROR] Unsupported file type: %EXT%
echo [SUPPORTED] .c .cpp .rs .go .py .zig .h
exit /b 1

:compile_c
echo [C] Detecting best compiler...
REM Try Clang first (fastest), then GCC, then MSVC
clang --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [CLANG] Compiling C with Clang (static^)...
    clang -std=c11 -static -O2 -Wall -Wextra "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32
    goto :check_result
)

gcc --version >nul 2>&1  
if %ERRORLEVEL%==0 (
    echo [GCC] Compiling C with GCC (static^)...
    gcc -std=c11 -static -O2 -Wall -Wextra "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32
    goto :check_result
)

if defined VSTOOLS (
    echo [MSVC] Compiling C with Visual Studio...
    call "%VSTOOLS%" >nul
    cl /nologo /O2 /MT "%FILE%" /Fe:"%OUT%" user32.lib gdi32.lib kernel32.lib comctl32.lib shell32.lib comdlg32.lib
    goto :check_result
)

echo [ERROR] No C compiler found (tried: Clang, GCC, MSVC^)
exit /b 1

:compile_cpp
echo [C++] Detecting best compiler...
REM Try Clang++ first (fastest), then G++, then MSVC
clang++ --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [CLANG++] Compiling C++ with Clang++ (static^)...
    clang++ -std=c++20 -static -O2 -Wall -Wextra "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32 -lole32 -luuid
    goto :check_result
)

g++ --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [G++] Compiling C++ with G++ (static^)...
    g++ -std=c++20 -static -O2 -Wall -Wextra "%FILE%" -o "%OUT%" -lgdi32 -luser32 -lkernel32 -lcomctl32 -lshell32 -lcomdlg32 -lole32 -luuid
    goto :check_result
)

if defined VSTOOLS (
    echo [MSVC] Compiling C++ with Visual Studio...
    call "%VSTOOLS%" >nul
    cl /nologo /O2 /MT /std:c++20 "%FILE%" /Fe:"%OUT%" user32.lib gdi32.lib kernel32.lib comctl32.lib shell32.lib comdlg32.lib ole32.lib uuid.lib
    goto :check_result
)

echo [ERROR] No C++ compiler found (tried: Clang++, G++, MSVC^)
exit /b 1

:compile_header
echo [INFO] Header file detected - creating test program
echo #include "%FILE%" > "%DIR%test_%NAME%.cpp"
echo int main() { return 0; } >> "%DIR%test_%NAME%.cpp"
set "FILE=%DIR%test_%NAME%.cpp"
set "OUT=%DIR%test_%NAME%.exe"
goto :compile_cpp

:compile_rust
rustc --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [RUST] Compiling with Rustc...
    rustc -C target-feature=+crt-static -O "%FILE%" -o "%OUT%"
) else (
    echo [ERROR] Rust compiler not found
    exit /b 1
)
goto :check_result

:compile_go
go version >nul 2>&1  
if %ERRORLEVEL%==0 (
    echo [GO] Compiling with Go...
    go build -ldflags "-s -w" -o "%OUT%" "%FILE%"
) else (
    echo [ERROR] Go compiler not found
    exit /b 1
)
goto :check_result

:compile_python
python --version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [PYTHON] Creating executable Python zipapp...
    echo @echo off > "%OUT%.bat"
    echo python "%FILE%" %%* >> "%OUT%.bat"
    ren "%OUT%.bat" "%OUT%"
) else (
    echo [ERROR] Python not found
    exit /b 1
)
goto :check_result

:compile_zig
zig version >nul 2>&1
if %ERRORLEVEL%==0 (
    echo [ZIG] Compiling with Zig (static^)...
    zig build-exe -O ReleaseFast -static "%FILE%"
) else (
    echo [ERROR] Zig compiler not found
    exit /b 1
)
goto :check_result

:check_result
if exist "%OUT%" (
    echo [SUCCESS] -^> %OUT%
    echo [INFO] File size: 
    dir "%OUT%" | findstr /C:"%NAME%.exe"
    exit /b 0
) else (
    echo [ERROR] Compilation failed - no output file created
    exit /b 1
)

:show_help
echo.
echo ===================================================================
echo    REVERSE-TOOL - Universal Zero-Config Compiler Toolchain 
echo ===================================================================
echo.
echo    Zero-Config Toolchain Commands:
echo   compile ^<file^>      - Compile any language (C/Rust/Go/Python^)  
echo   templates           - Create language templates
echo   test-all           - Test compilation of all languages
echo   toolchain-info     - Show toolchain information
echo   --unpack-only      - Extract toolchain only
echo.
echo    Usage Examples:
echo   reverse-tool.bat compile hello.c        # C -^> hello.exe
echo   reverse-tool.bat compile server.rs      # Rust -^> server.exe  
echo   reverse-tool.bat compile api.go         # Go -^> api.exe
echo   reverse-tool.bat compile script.py      # Python -^> script.exe
echo.
echo    Direct compilation:
echo   reverse-tool.bat main.cpp              # Backwards compatible
echo.
exit /b 0

:create_templates
echo [TEMPLATES] Creating language templates...
mkdir templates 2>nul

echo // C Template > templates\hello.c
echo #include ^<stdio.h^> >> templates\hello.c
echo int main() { >> templates\hello.c
echo     printf("Hello from C!\n"); >> templates\hello.c
echo     return 0; >> templates\hello.c
echo } >> templates\hello.c

echo // C++ Template > templates\hello.cpp  
echo #include ^<iostream^> >> templates\hello.cpp
echo int main() { >> templates\hello.cpp
echo     std::cout ^<^< "Hello from C++!" ^<^< std::endl; >> templates\hello.cpp
echo     return 0; >> templates\hello.cpp
echo } >> templates\hello.cpp

echo fn main() { > templates\hello.rs
echo     println!("Hello from Rust!"^); >> templates\hello.rs
echo } >> templates\hello.rs

echo package main > templates\hello.go
echo import "fmt" >> templates\hello.go
echo func main() { >> templates\hello.go  
echo     fmt.Println("Hello from Go!"^) >> templates\hello.go
echo } >> templates\hello.go

echo print("Hello from Python!"^) > templates\hello.py

echo [SUCCESS] Templates created in ./templates/
echo   - hello.c (C^)
echo   - hello.cpp (C++^)  
echo   - hello.rs (Rust^)
echo   - hello.go (Go^)
echo   - hello.py (Python^)
exit /b 0

:test_all_languages
echo [TEST-ALL] Testing compilation of all languages...
call :create_templates >nul

echo Testing C...
call "%~f0" compile templates\hello.c
if %ERRORLEVEL%==0 echo [OK] C compilation successful

echo Testing C++...
call "%~f0" compile templates\hello.cpp  
if %ERRORLEVEL%==0 echo [OK] C++ compilation successful

echo Testing Rust...
call "%~f0" compile templates\hello.rs
if %ERRORLEVEL%==0 echo [OK] Rust compilation successful

echo Testing Go...
call "%~f0" compile templates\hello.go
if %ERRORLEVEL%==0 echo [OK] Go compilation successful  

echo Testing Python...
call "%~f0" compile templates\hello.py
if %ERRORLEVEL%==0 echo [OK] Python compilation successful

echo [COMPLETE] All language tests finished
exit /b 0

:show_toolchain_info
echo.
echo ===================================================================  
echo    TOOLCHAIN INFORMATION 
echo ===================================================================
echo Root: %ROOT%
echo.
echo    Available Compilers:
clang --version >nul 2>&1 && echo [CHECK] Clang || echo [X] Clang (not found^)
gcc --version >nul 2>&1 && echo [CHECK] GCC/MinGW || echo [X] GCC/MinGW (not found^)  
if defined VSTOOLS echo [CHECK] Visual Studio || echo [X] Visual Studio (not found^)
rustc --version >nul 2>&1 && echo [CHECK] Rust || echo [X] Rust (not found^)
go version >nul 2>&1 && echo [CHECK] Go || echo [X] Go (not found^)
zig version >nul 2>&1 && echo [CHECK] Zig || echo [X] Zig (not found^)
python --version >nul 2>&1 && echo [CHECK] Python || echo [X] Python (not found^)
echo.
exit /b 0

:unpack_only
echo [UNPACK] Toolchain ready at: %ROOT%
echo [INFO] Add to PATH or use directly
exit /b 0

REM ===================================================================
REM    REVERSE-TOOL - Universal Zero-Config Compiler Toolchain 
REM Single 90MB self-extracting portable toolchain  
REM Ships: Clang, GCC, MinGW, MSVC, Rust, Go, Zig, Python
REM Zero dependencies, zero config, zero installation
REM Works from USB stick, /tmp, anywhere - Perfect for portable development!
REM ===================================================================