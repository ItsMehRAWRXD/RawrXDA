@echo off
REM Test compilation of qt6_text_editor and qt6_syntax_highlighter
setlocal enabledelayedexpansion

cd /d "C:\Users\HiH8e\Downloads\RawrXD-production-lazy-init\src\masm\final-ide"

set ML64="C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe"

echo Testing qt6_text_editor.asm...
%ML64% /c /Cp /nologo /Zi /Fo test_text_editor.obj qt6_text_editor.asm 2>&1
if %errorlevel% equ 0 (
    echo qt6_text_editor.asm: PASS
) else (
    echo qt6_text_editor.asm: FAIL with exit code %errorlevel%
)

echo.
echo Testing qt6_syntax_highlighter.asm...
%ML64% /c /Cp /nologo /Zi /Fo test_syntax_highlighter.obj qt6_syntax_highlighter.asm 2>&1
if %errorlevel% equ 0 (
    echo qt6_syntax_highlighter.asm: PASS
) else (
    echo qt6_syntax_highlighter.asm: FAIL with exit code %errorlevel%
)
