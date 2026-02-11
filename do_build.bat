@echo off
cd /d D:\rawrxd\build
ninja -j2 2>&1
echo EXIT_CODE=%ERRORLEVEL%
echo EXIT_CODE=%ERRORLEVEL% > D:\rawrxd\build_exit.txt
