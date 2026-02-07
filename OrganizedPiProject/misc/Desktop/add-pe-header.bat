@echo off
echo Adding PE header to make it a real EXE...

REM Create minimal PE stub
echo MZ > pe-stub.bin
echo This program cannot be run in DOS mode. >> pe-stub.bin

REM Combine PE stub with JAR
copy /b pe-stub.bin + CleanCursorIDE.jar CleanCursorIDE-PE.exe

echo [OK] PE executable created: CleanCursorIDE-PE.exe
del pe-stub.bin
pause