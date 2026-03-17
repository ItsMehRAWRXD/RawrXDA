@echo off
cd /d d:\rawrxd\src
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\ml64.exe" (
    "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\ml64.exe" /c rawrxd_compiler_masm64.asm /Fo rawrxd_compiler_masm64.obj
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\ml64.exe" (
    "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.39.33519\bin\Hostx64\x64\ml64.exe" /c rawrxd_compiler_masm64.asm /Fo rawrxd_compiler_masm64.obj
) else (
    echo ml64.exe not found in expected locations
    pause
    exit /b 1
)

if %errorlevel% neq 0 (
    echo Build failed with error %errorlevel%
    pause
) else (
    echo Build successful - lexer with keyword support compiled!
    echo Test functions available: Lexer_TestSuite, Lexer_TestLanguage, Lexer_TestKeywords, Lexer_TestTokenTypes
)
pause