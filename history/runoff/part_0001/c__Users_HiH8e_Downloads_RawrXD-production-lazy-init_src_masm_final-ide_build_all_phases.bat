@echo off
setlocal enabledelayedexpansion

echo ============================================================
echo RAWRXD MASM64 IDE - FULL BUILD (PHASES 1-4)
echo ============================================================

set AS="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"
set LINK="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe"
set LIBS=user32.lib kernel32.lib gdi32.lib advapi32.lib comctl32.lib shell32.lib

:: 1. Build Infrastructure
echo [1/4] Building Infrastructure...
%AS% /c /Zi dialog_system.asm
%AS% /c /Zi tab_control.asm
%AS% /c /Zi listview_control.asm
%AS% /c /Zi registry_persistence.asm
%AS% /c /Zi qt6_settings_dialog.asm

:: 2. Build 10 Core Components
echo [2/4] Building 10 Core Components...
%AS% /c /Zi cpp_to_masm_terminal_manager.asm
%AS% /c /Zi cpp_to_masm_theme_manager.asm
%AS% /c /Zi cpp_to_masm_ai_chat_panel.asm
%AS% /c /Zi cpp_to_masm_compliance_logger.asm
%AS% /c /Zi cpp_to_masm_model_loader_thread.asm
%AS% /c /Zi cpp_to_masm_metrics_collector.asm
%AS% /c /Zi cpp_to_masm_backup_manager.asm
%AS% /c /Zi cpp_to_masm_bpe_tokenizer.asm
%AS% /c /Zi cpp_to_masm_inference_engine.asm
%AS% /c /Zi cpp_to_masm_streaming_inference.asm

:: 3. Build Test Runners
echo [3/4] Building Test Runners...
%AS% /c /Zi test_phase4.asm

:: 4. Link Everything
echo [4/4] Linking Phase 4 Test Runner...
%LINK% /SUBSYSTEM:WINDOWS /ENTRY:main test_phase4.obj ^
    qt6_settings_dialog.obj registry_persistence.obj ^
    dialog_system.obj tab_control.obj listview_control.obj ^
    cpp_to_masm_terminal_manager.obj cpp_to_masm_theme_manager.obj ^
    cpp_to_masm_ai_chat_panel.obj cpp_to_masm_compliance_logger.obj ^
    cpp_to_masm_model_loader_thread.obj cpp_to_masm_metrics_collector.obj ^
    cpp_to_masm_backup_manager.obj cpp_to_masm_bpe_tokenizer.obj ^
    cpp_to_masm_inference_engine.obj cpp_to_masm_streaming_inference.obj ^
    %LIBS% /OUT:test_phase4.exe

if %ERRORLEVEL% EQU 0 (
    echo ============================================================
    echo BUILD SUCCESSFUL: test_phase4.exe created.
    echo ============================================================
) else (
    echo ============================================================
    echo BUILD FAILED! Check errors above.
    echo ============================================================
    exit /b 1
)
