@echo off
setlocal enabledelayedexpansion
color 0A

echo.
echo ========================================
echo Qt MASM Compilation Test
echo ========================================
echo.

set success=0
set failed=0
set results=

REM Test qt6_foundation.asm
echo [TEST 1/5] qt6_foundation.asm
ml64.exe /c /Cp /nologo /Zi /Fo obj\qt6_foundation.obj qt6_foundation.asm > nul 2>&1
if !errorlevel! equ 0 (
  echo   Status: PASS
  set /a success+=1
  set results=!results!qt6_foundation.asm: PASS
) else (
  echo   Status: FAIL
  set /a failed+=1
  ml64.exe /c /Cp /nologo qt6_foundation.asm 2>&1 | findstr /r "error"
  set results=!results!qt6_foundation.asm: FAIL
)
echo.

REM Test qt6_main_window.asm
echo [TEST 2/5] qt6_main_window.asm
ml64.exe /c /Cp /nologo /Zi /Fo obj\qt6_main_window.obj qt6_main_window.asm > nul 2>&1
if !errorlevel! equ 0 (
  echo   Status: PASS
  set /a success+=1
  set results=!results!qt6_main_window.asm: PASS
) else (
  echo   Status: FAIL
  set /a failed+=1
  ml64.exe /c /Cp /nologo qt6_main_window.asm 2>&1 | findstr /r "error"
  set results=!results!qt6_main_window.asm: FAIL
)
echo.

REM Test qt6_statusbar.asm
echo [TEST 3/5] qt6_statusbar.asm
ml64.exe /c /Cp /nologo /Zi /Fo obj\qt6_statusbar.obj qt6_statusbar.asm > nul 2>&1
if !errorlevel! equ 0 (
  echo   Status: PASS
  set /a success+=1
  set results=!results!qt6_statusbar.asm: PASS
) else (
  echo   Status: FAIL
  set /a failed+=1
  ml64.exe /c /Cp /nologo qt6_statusbar.asm 2>&1 | findstr /r "error"
  set results=!results!qt6_statusbar.asm: FAIL
)
echo.

REM Test qt6_text_editor.asm
echo [TEST 4/5] qt6_text_editor.asm
ml64.exe /c /Cp /nologo /Zi /Fo obj\qt6_text_editor.obj qt6_text_editor.asm > nul 2>&1
if !errorlevel! equ 0 (
  echo   Status: PASS
  set /a success+=1
  set results=!results!qt6_text_editor.asm: PASS
) else (
  echo   Status: FAIL
  set /a failed+=1
  ml64.exe /c /Cp /nologo qt6_text_editor.asm 2>&1 | findstr /r "error"
  set results=!results!qt6_text_editor.asm: FAIL
)
echo.

REM Test qt6_syntax_highlighter.asm
echo [TEST 5/5] qt6_syntax_highlighter.asm
ml64.exe /c /Cp /nologo /Zi /Fo obj\qt6_syntax_highlighter.obj qt6_syntax_highlighter.asm > nul 2>&1
if !errorlevel! equ 0 (
  echo   Status: PASS
  set /a success+=1
  set results=!results!qt6_syntax_highlighter.asm: PASS
) else (
  echo   Status: FAIL
  set /a failed+=1
  ml64.exe /c /Cp /nologo qt6_syntax_highlighter.asm 2>&1 | findstr /r "error"
  set results=!results!qt6_syntax_highlighter.asm: FAIL
)
echo.

echo ========================================
echo SUMMARY
echo ========================================
echo Success: %success%/5
echo Failed:  %failed%/5
echo.
if %failed% equ 0 (
  echo ALL TESTS PASSED!
  color 0A
) else (
  echo SOME TESTS FAILED
  color 0C
)
echo.
pause
