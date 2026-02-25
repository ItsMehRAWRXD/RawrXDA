@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
echo INCLUDE=%INCLUDE%
echo LIB=%LIB%
echo.
where mt.exe
where rc.exe
