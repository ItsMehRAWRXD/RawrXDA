@echo off
echo Building RawrXD CLI with Agentic Features...
g++ -std=c++17 -D_WIN32 -I./include -I./src src/rawrxd_cli.cpp src/cli_extras_stubs.cpp src/tools/file_ops.cpp -o rawrxd_cli.exe -static -lws2_32 -lshlwapi -lwinhttp
if %errorlevel% neq 0 (
    echo Build Failed!
    exit /b 1
)
echo Build Success!
rawrxd_cli.exe /audit test
