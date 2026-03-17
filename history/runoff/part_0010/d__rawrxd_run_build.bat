@echo off
cd /d d:\rawrxd\build
echo === BUILD START ===
ninja -j 2 RawrEngine 2>&1
echo === BUILD EXIT CODE: %ERRORLEVEL% ===
