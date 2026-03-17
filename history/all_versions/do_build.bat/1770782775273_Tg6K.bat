@echo off
cd /d D:\rawrxd\build
ninja -j2
echo EXIT_CODE=%ERRORLEVEL%
