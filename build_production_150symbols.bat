@echo off
REM ============================================================================
REM RawrXD Symbol Batch Resolver - Full Production Build
REM 150 Unlinked Symbols | 10 Batches of 15 | No Stubs
REM ============================================================================

echo.
echo ========================================================================
echo Building Full Production Symbol Batch Resolver
echo 150 symbols across 10 batches - Zero stubs, direct resolution
echo ========================================================================
echo.

REM Step 1: Assemble PE Writer Core
echo [1/4] Assembling PE Writer Core...
ml64 /c /nologo /Fo"RawrXD_PE_Writer_Core_ml64.obj" "RawrXD_PE_Writer_Core_ml64.asm"
if errorlevel 1 goto error

REM Step 2: Assemble Symbol Batch Resolver (150 symbols)
echo [2/4] Assembling Symbol Batch Resolver (150 symbols)...
ml64 /c /nologo /Fo"symbol_batch_resolver_production.obj" "symbol_batch_resolver_production.asm"
if errorlevel 1 goto error

REM Step 3: Assemble Production Driver
echo [3/4] Assembling Production Driver...
ml64 /c /nologo /Fo"production_driver_150symbols.obj" "production_driver_150symbols.asm"
if errorlevel 1 goto error

REM Step 4: Link Production Executable
echo [4/4] Linking Production Executable...
link /NOLOGO /SUBSYSTEM:CONSOLE /ENTRY:main ^
     /OUT:RawrXD_SymbolResolver_Production.exe ^
     production_driver_150symbols.obj ^
     symbol_batch_resolver_production.obj ^
     RawrXD_PE_Writer_Core_ml64.obj ^
     kernel32.lib
if errorlevel 1 goto error

echo.
echo ========================================================================
echo BUILD SUCCESSFUL
echo ========================================================================
echo.
echo Executable: RawrXD_SymbolResolver_Production.exe
echo.
echo Run to generate PE with 150 resolved symbols:
echo   ^> RawrXD_SymbolResolver_Production.exe
echo.
echo Output: production_150symbols.exe
echo ========================================================================
goto end

:error
echo.
echo ========================================================================
echo BUILD FAILED
echo ========================================================================
exit /b 1

:end
