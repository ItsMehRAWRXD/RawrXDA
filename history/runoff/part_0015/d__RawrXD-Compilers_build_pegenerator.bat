@echo off
REM ================================================================================
REM RawrXD PE Generator - Windows Build Script
REM ================================================================================
REM Assembles PE generator, creates library, and builds test executable
REM ================================================================================

setlocal enabledelayedexpansion

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║         RawrXD PE Generator - Build Pipeline                   ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

REM ================================================================================
REM CONFIGURATION
REM ================================================================================

set MASM=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe
set LINK=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\link.exe
set CL=cl.exe
set LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64
set INCLUDE=C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um

if not exist "%MASM%" (
    echo [-] Error: MASM64 not found at:
    echo     %MASM%
    echo.
    echo Please ensure Visual Studio 2022 Enterprise is installed with MASM tools.
    exit /b 1
)

echo [+] Build tools found:
echo     MASM: %MASM%
echo     Link: %LINK%
echo     CL:   %CL%
echo.

REM ================================================================================
REM STEP 1: Assemble PE Generator
REM ================================================================================

echo [*] Step 1: Assembling PE Generator (RawrXD_PE_Generator_FULL.asm)
echo ================================================================
echo.

if not exist RawrXD_PE_Generator_FULL.asm (
    echo [-] Error: RawrXD_PE_Generator_FULL.asm not found in current directory
    exit /b 1
)

"%MASM%" /c /nologo /W3 /Zd /Zi /Fo"PeGen.obj" RawrXD_PE_Generator_FULL.asm
if errorlevel 1 (
    echo [-] Assembly failed!
    exit /b 1
)

echo [+] Assembly successful: PeGen.obj
echo.

REM ================================================================================
REM STEP 2: Create Static Library
REM ================================================================================

echo [*] Step 2: Creating Static Library (RawrXD_PeGen.lib)
echo ================================================================
echo.

lib /nologo /out:"RawrXD_PeGen.lib" PeGen.obj
if errorlevel 1 (
    echo [-] Library creation failed!
    exit /b 1
)

echo [+] Library created: RawrXD_PeGen.lib
echo.

REM ================================================================================
REM STEP 3: Build Test Executable
REM ================================================================================

echo [*] Step 3: Building Test Examples (PeGen_Examples.cpp)
echo ================================================================
echo.

if not exist PeGen_Examples.cpp (
    echo [-] Error: PeGen_Examples.cpp not found
    exit /b 1
)

if not exist RawrXD_PE_Generator.h (
    echo [-] Error: RawrXD_PE_Generator.h not found
    exit /b 1
)

%CL% /nologo /W4 /O2 /MT /I"%INCLUDE%" /Fe"PeGen_Examples.exe" PeGen_Examples.cpp RawrXD_PeGen.lib "%LIB%\kernel32.lib"
if errorlevel 1 (
    echo [-] Compilation failed!
    exit /b 1
)

echo [+] Examples compiled: PeGen_Examples.exe
echo.

REM ================================================================================
REM STEP 4: Summary
REM ================================================================================

echo.
echo ╔════════════════════════════════════════════════════════════════╗
echo ║               BUILD SUCCESSFUL!                               ║
echo ╚════════════════════════════════════════════════════════════════╝
echo.

echo [+] Build Artifacts:
echo     Object:      PeGen.obj
echo     Library:     RawrXD_PeGen.lib
echo     Executable:  PeGen_Examples.exe
echo.

echo [+] File Sizes:
for %%A in (PeGen.obj RawrXD_PeGen.lib PeGen_Examples.exe) do (
    if exist "%%A" (
        for /f "tokens=5" %%B in ('dir %%A ^| findstr /V "Directory"') do (
            if "%%B" neq "" (
                echo     %%A: %%B bytes
            )
        )
    )
)
echo.

echo [*] To run the examples:
echo     .\PeGen_Examples.exe
echo.

echo [*] To use in your project:
echo     1. Include RawrXD_PE_Generator.h in your C/C++ code
echo     2. Link against RawrXD_PeGen.lib
echo     3. Call PeGen_Initialize, PeGen_CreateDosHeader, etc.
echo     4. See PeGen_Examples.cpp for usage patterns
echo.

echo [+] Build complete! All components ready for deployment.
echo.

endlocal
exit /b 0
