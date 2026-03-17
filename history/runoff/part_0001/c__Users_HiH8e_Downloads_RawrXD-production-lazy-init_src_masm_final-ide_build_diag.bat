@echo off
setlocal enabledelayedexpansion

set MSVC_BIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64
set AS="%MSVC_BIN%\ml64.exe"
set LINK_EXE="%MSVC_BIN%\link.exe"

set AS_FLAGS=/c /Zi /Cp
set LINK_FLAGS=/SUBSYSTEM:CONSOLE /ENTRY:main /DEBUG /LIBPATH:"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"

set LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib comdlg32.lib d2d1.lib dwrite.lib dwmapi.lib uxtheme.lib advapi32.lib

echo Building RawrXD Diagnostic...

echo [1/3] Compiling core utilities...
%AS% %AS_FLAGS% asm_memory.obj asm_string.obj process_manager.obj console_log.obj asm_log.obj asm_events.obj asm_sync.obj

echo [2/3] Compiling UI...
%AS% %AS_FLAGS% ui_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [3/3] Linking diagnostic main...
%LINK_EXE% %LINK_FLAGS% main_diag.obj ui_masm.obj asm_memory.obj asm_string.obj process_manager.obj console_log.obj asm_log.obj asm_events.obj asm_sync.obj %LIBS% /OUT:RawrXD_DIAG.exe
if %errorlevel% neq 0 exit /b %errorlevel%

echo Build successful: RawrXD_DIAG.exe
