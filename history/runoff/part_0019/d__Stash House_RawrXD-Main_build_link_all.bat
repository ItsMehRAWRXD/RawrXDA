@echo off
REM link_all.bat — Initialize VS env and link RawrXD assembly objects
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1

set OBJDIR=D:\Stash House\RawrXD-Main\build\obj
set BINDIR=D:\Stash House\RawrXD-Main\build\bin
set LIBS=kernel32.lib user32.lib gdi32.lib advapi32.lib ole32.lib oleaut32.lib shell32.lib comdlg32.lib ws2_32.lib wininet.lib comctl32.lib shlwapi.lib uuid.lib dbghelp.lib

echo.
echo === Linking rawrxd_scc.exe ===
link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /MACHINE:X64 /NODEFAULTLIB /OUT:"%BINDIR%\rawrxd_scc.exe" /PDB:"%BINDIR%\rawrxd_scc.pdb" "%OBJDIR%\rawrxd_scc.obj" %LIBS%
if exist "%BINDIR%\rawrxd_scc.exe" (echo   OK) else (echo   FAILED)

echo.
echo === Linking rawrxd_link.exe ===
link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /MACHINE:X64 /NODEFAULTLIB /OUT:"%BINDIR%\rawrxd_link.exe" /PDB:"%BINDIR%\rawrxd_link.pdb" "%OBJDIR%\rawrxd_link.obj" %LIBS%
if exist "%BINDIR%\rawrxd_link.exe" (echo   OK) else (echo   FAILED)

echo.
echo === Linking gguf_dump.exe ===
link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /MACHINE:X64 /NODEFAULTLIB /OUT:"%BINDIR%\gguf_dump.exe" /PDB:"%BINDIR%\gguf_dump.pdb" "%OBJDIR%\gguf_dump.obj" %LIBS%
if exist "%BINDIR%\gguf_dump.exe" (echo   OK) else (echo   FAILED)

echo.
echo === Linking RawrXD-AgenticIDE.exe (all 83 objects, FORCE:MULTIPLE) ===
link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main /MACHINE:X64 /NODEFAULTLIB /FORCE:MULTIPLE /OUT:"%BINDIR%\RawrXD-AgenticIDE.exe" /PDB:"%BINDIR%\RawrXD-AgenticIDE.pdb" /MAP:"%BINDIR%\RawrXD-AgenticIDE.map" "%OBJDIR%\*.obj" %LIBS%
if exist "%BINDIR%\RawrXD-AgenticIDE.exe" (echo   OK) else (echo   FAILED)

echo.
echo === Results ===
dir /B "%BINDIR%\*.exe" 2>nul
