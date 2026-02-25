@echo off
cd /d d:\rawrxd\build
ninja -j 4 > d:\rawrxd\build_final_result.txt 2>&1
echo BUILD_EXIT_CODE=%ERRORLEVEL% >> d:\rawrxd\build_final_result.txt
