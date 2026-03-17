@echo off
REM RawrZ Camellia MASM x64 - Build for IDE integration
setlocal
set ASM=RawrZ_Camellia_MASM_x64.asm
set OBJ=RawrZ_Camellia_MASM_x64.obj
set DLL=RawrZ_Camellia_x64.dll

where ml64.exe >nul 2>&1
if errorlevel 1 (
    echo [ERROR] ml64.exe not in PATH. Use "Developer Command Prompt for VS" or add MSVC bin to PATH.
    exit /b 1
)

echo [*] Building RawrZ Camellia x64 kernel...
ml64 /nologo /c /Fo:%OBJ% %ASM%
if errorlevel 1 (
    echo [ERROR] Assembly failed.
    exit /b 1
)

link /nologo /DLL /OUT:%DLL% %OBJ% kernel32.lib
if errorlevel 1 (
    echo [ERROR] Link failed.
    exit /b 1
)

echo [OK] %DLL% built successfully.
exit /b 0
