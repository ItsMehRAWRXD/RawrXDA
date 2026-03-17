@echo off
echo Building Native HTTP Server...

REM Try different compilers
where g++ >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using g++...
    g++ -std=c++17 -O2 native_http_server.cpp -o native_http_server.exe -lws2_32
    goto :done
)

where clang++ >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using clang++...
    clang++ -std=c++17 -O2 native_http_server.cpp -o native_http_server.exe -lws2_32
    goto :done
)

REM Try MSVC
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cl >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using MSVC...
    cl /std:c++17 /O2 native_http_server.cpp /Fe:native_http_server.exe ws2_32.lib
    goto :done
)

REM Try MinGW
"C:\MinGW\bin\g++.exe" --version >nul 2>&1
if %ERRORLEVEL% == 0 (
    echo Using MinGW...
    "C:\MinGW\bin\g++.exe" -std=c++17 -O2 native_http_server.cpp -o native_http_server.exe -lws2_32
    goto :done
)

echo ERROR: No C++ compiler found!
echo Please install Visual Studio, MinGW, or Clang
pause
exit /b 1

:done
if exist native_http_server.exe (
    echo SUCCESS: native_http_server.exe created!
    echo Run: native_http_server.exe [port]
) else (
    echo ERROR: Build failed
)
pause