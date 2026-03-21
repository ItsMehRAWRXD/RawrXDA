@echo off
setlocal
set ROOT=%~dp0..
cd /d "%ROOT%"
if not exist build mkdir build

ml64 /c /nologo /Fo build\pe_emitter.obj tools\pe_emitter.asm
if errorlevel 1 exit /b 1

link /nologo /subsystem:console /entry:main build\pe_emitter.obj kernel32.lib /out:build\sovereign_app.exe
if errorlevel 1 exit /b 1

echo Built build\sovereign_app.exe ^(Sovereign Forge — see docs\SOVEREIGN_FORGE_MASM.md^)
exit /b 0
