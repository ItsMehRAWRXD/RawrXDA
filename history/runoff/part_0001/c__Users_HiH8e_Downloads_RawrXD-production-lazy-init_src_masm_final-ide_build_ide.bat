@echo off
setlocal enabledelayedexpansion

set MSVC_BIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64
set AS="%MSVC_BIN%\ml64.exe"
set LINK_EXE="%MSVC_BIN%\link.exe"

set AS_FLAGS=/c /Zi /Cp
set LINK_FLAGS=/SUBSYSTEM:CONSOLE /ENTRY:main /DEBUG /LIBPATH:"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64" /LIBPATH:"C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64"

set LIBS=kernel32.lib user32.lib gdi32.lib shell32.lib comdlg32.lib d2d1.lib dwrite.lib dwmapi.lib uxtheme.lib advapi32.lib

echo Building RawrXD AI IDE...

echo [1/10] Compiling core utilities...
%AS% %AS_FLAGS% asm_memory.asm
%AS% %AS_FLAGS% asm_string.asm
%AS% %AS_FLAGS% process_manager.asm
%AS% %AS_FLAGS% console_log.asm
%AS% %AS_FLAGS% asm_log.asm
%AS% %AS_FLAGS% asm_events.asm
%AS% %AS_FLAGS% asm_sync.asm

echo [2/10] Compiling main_masm.asm...
%AS% %AS_FLAGS% main_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [2a/10] Compiling main_window_masm_simple.asm...
%AS% %AS_FLAGS% main_window_masm_simple.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [3/10] Compiling ui_masm.asm...
%AS% %AS_FLAGS% ui_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [3a/10] Compiling ui_helpers_masm.asm...
%AS% %AS_FLAGS% ui_helpers_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [4/10] Compiling agentic_masm.asm...
%AS% %AS_FLAGS% agentic_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [5/10] Compiling gui_designer_complete.asm...
%AS% %AS_FLAGS% gui_designer_complete.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [6/10] Compiling ml_masm.asm...
%AS% %AS_FLAGS% ml_masm.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [7/10] Compiling Rawr1024 engine...
%AS% %AS_FLAGS% rawr1024_dual_engine_custom.asm

echo [8/10] Compiling IDE components (File Tree, Editor, Tabs, Minimap, Command Palette, Split Panes)...
%AS% %AS_FLAGS% ide_components.asm

echo [9/10] Compiling feature harness...
%AS% %AS_FLAGS% rawrxd_feature_harness.asm
if %errorlevel% neq 0 exit /b %errorlevel%

echo [10/10] Compiling hotpatch and agentic engine...
%AS% %AS_FLAGS% unified_masm_hotpatch.asm
%AS% %AS_FLAGS% agentic_engine.asm
%AS% %AS_FLAGS% agentic_failure_detector.asm
%AS% %AS_FLAGS% agentic_puppeteer.asm

echo [12/12] Linking...
%LINK_EXE% %LINK_FLAGS% main_masm.obj main_window_masm_simple.obj ui_masm.obj ui_helpers_masm.obj agentic_masm.obj gui_designer_complete.obj ide_components.obj ml_masm.obj asm_memory.obj asm_string.obj process_manager.obj console_log.obj rawr1024_dual_engine_custom.obj rawrxd_feature_harness.obj unified_masm_hotpatch.obj agentic_engine.obj asm_log.obj asm_events.obj asm_sync.obj agentic_failure_detector.obj agentic_puppeteer.obj %LIBS% /OUT:RawrXD_IDE.exe
if %errorlevel% neq 0 exit /b %errorlevel%

echo Build successful: RawrXD_IDE.exe
