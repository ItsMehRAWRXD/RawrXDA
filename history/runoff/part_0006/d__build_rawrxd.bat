@echo off
cd /d D:\RawrXD-production-lazy-init\build
echo Building RawrXD-QtShell...
cmake --build . --target RawrXD-QtShell --config Release > D:\build_log_full.txt 2>&1
echo Build complete with exit code: %ERRORLEVEL%
type D:\build_log_full.txt | findstr /i "error"
