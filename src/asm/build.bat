@echo off
REM Build script for RawrXD PE Writer
REM Requires MASM64 and Windows SDK

echo Building RawrXD PE Writer...

REM Assemble the PE writer
ml64 /c /Fo:RawrXD_PE_Writer.obj RawrXD_PE_Writer.asm
if errorlevel 1 goto error

REM Assemble the example
ml64 /c /Fo:PE_Writer_Example.obj PE_Writer_Example.asm  
if errorlevel 1 goto error

REM Link the example with PE writer
link /SUBSYSTEM:CONSOLE /ENTRY:main PE_Writer_Example.obj RawrXD_PE_Writer.obj kernel32.lib
if errorlevel 1 goto error

echo Build completed successfully!
echo Run PE_Writer_Example.exe to create hello.exe
goto end

:error
echo Build failed!
exit /b 1

:end