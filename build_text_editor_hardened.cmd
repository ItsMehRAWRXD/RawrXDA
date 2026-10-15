@echo off
setlocal
where ml64.exe >nul 2>nul || (echo ERROR: run from x64 Native Tools Command Prompt & exit /b 1)
where link.exe >nul 2>nul || (echo ERROR: link.exe unavailable & exit /b 1)
if not exist build_hardened mkdir build_hardened
ml64 /nologo /c /Fo"build_hardened\RawrXD_TextEditorGUI_hardened.obj" "RawrXD_TextEditorGUI_hardened.asm" || exit /b 1
link /nologo /SUBSYSTEM:WINDOWS /ENTRY:main /MACHINE:X64 /DYNAMICBASE /HIGHENTROPYVA /NXCOMPAT /CETCOMPAT /GUARD:CF /OUT:"build_hardened\RawrXD_TextEditorGUI_hardened.exe" "build_hardened\RawrXD_TextEditorGUI_hardened.obj" user32.lib kernel32.lib || exit /b 1
echo Built build_hardened\RawrXD_TextEditorGUI_hardened.exe
echo NOTE: standalone prototype; CFG requires instrumented indirect calls to provide full coverage.
