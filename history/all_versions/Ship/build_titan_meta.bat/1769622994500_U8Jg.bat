@echo off
setlocal

:: Find ml64.exe (VS Build Tools, VS Community, etc.)
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe" (
    set "ML64=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\ml64.exe"
    set "LINK=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\link.exe"
) else (
    set "ML64=ml64.exe"
    set "LINK=link.exe"
)

echo ============================================================================
echo Building RawrXD Titan MetaReverse (Zero-Dependency)...
echo ============================================================================

:: Assemble only (produces .obj)
"%ML64%" /c /Zi /O2 /D"METAREV=5" /arch:AVX512 /FoRawrXD_Titan.obj RawrXD_Titan_MetaReverse.asm
if %errorlevel% neq 0 goto error

:: Link with NO DEFAULT LIBS, custom entry, no kernel32.lib dependency
"%LINK%" /SUBSYSTEM:WINDOWS /NODEFAULTLIB /ENTRY:PEBMain /OUT:RawrXD-Titan.exe RawrXD_Titan.obj
if %errorlevel% neq 0 goto error

echo.
echo ============================================================================
echo BUILD SUCCESSFUL
echo ============================================================================
goto :eof

:error
echo.
echo ============================================================================
echo BUILD FAILED
echo ============================================================================
exit /b 1
