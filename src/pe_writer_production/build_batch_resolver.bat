@echo off
REM Production Build: Symbol Batch Resolver
REM Compiles 75 unlinked symbols in 5 batches of 15

echo Building Symbol Batch Resolver (Production)...

REM Assemble core PE writer
ml64 /c /Fo"RawrXD_PE_Writer_Core_ml64.obj" "RawrXD_PE_Writer_Core_ml64.asm"
if errorlevel 1 goto error

REM Assemble symbol batch resolver
ml64 /c /Fo"symbol_batch_resolver.obj" "structures\symbol_batch_resolver.asm"
if errorlevel 1 goto error

REM Assemble driver
ml64 /c /Fo"batch_resolver_driver.obj" "examples\batch_resolver_driver.asm"
if errorlevel 1 goto error

REM Link production executable
link /SUBSYSTEM:CONSOLE /ENTRY:main /OUT:batch_resolver_production.exe ^
     batch_resolver_driver.obj symbol_batch_resolver.obj RawrXD_PE_Writer_Core_ml64.obj ^
     kernel32.lib
if errorlevel 1 goto error

echo.
echo Build successful: batch_resolver_production.exe
echo Run to generate PE with 75 resolved symbols
goto end

:error
echo Build failed!
exit /b 1

:end
