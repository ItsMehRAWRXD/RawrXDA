@echo off
cd /d d:\rawrxd-ci-bootstrap
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\ml64.exe" /c /nologo /W3 /Fo pe_writer.obj Sovereign_PE_Writer.asm
if errorlevel 1 (
    echo Assembly failed!
    exit /b 1
)
"C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\link.exe" /nologo /subsystem:console /entry:main /out:pe_writer.exe pe_writer.obj "C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64\kernel32.lib"
if errorlevel 1 (
    echo Link failed!
    exit /b 1
)
echo Build complete: pe_writer.exe