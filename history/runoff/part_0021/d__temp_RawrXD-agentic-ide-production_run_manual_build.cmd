@echo off
setlocal

call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1

set "QT_ROOT=C:\Qt\6.7.3\msvc2022_64"
set "PATH=%QT_ROOT%\bin;%PATH%"
set "QMAKE=%QT_ROOT%\bin\qmake.exe"

cd /d D:\temp\RawrXD-agentic-ide-production\build-sovereign-static
if exist build_with_env.bat (call build_with_env.bat) else (echo build_with_env.bat missing & exit /b 1)

cd /d D:\temp\RawrXD-agentic-ide-production\RawrXD-IDE
if exist Makefile.nmake (
    nmake clean
    del /Q /F Makefile*
)
"%QMAKE%" RawrXD-IDE.pro CONFIG+=release || exit /b 1
set CL=/MP16
nmake || exit /b 1

copy /Y ..\build-sovereign-static\bin\RawrXD-SovereignLoader.dll release\ || exit /b 1

cd /d D:\temp\RawrXD-agentic-ide-production
python smoke_test.py

endlocal
