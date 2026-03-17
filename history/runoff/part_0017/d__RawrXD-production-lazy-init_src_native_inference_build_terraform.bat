@echo off
REM Build script for TerraForm-enhanced Native GGUF Loader
REM Compatible with RXUC-TerraForm universal compiler

echo Building Native GGUF Loader with TerraForm...

REM Check if terraform.exe exists
if not exist "..\..\terraform.exe" (
    echo Error: terraform.exe not found in parent directory
    echo Please ensure TerraForm compiler is built and available
    pause
    exit /b 1
)

REM Compile with TerraForm
echo Compiling native_gguf_loader.tf...
..\..\terraform.exe native_gguf_loader.tf

if %errorlevel% neq 0 (
    echo Error: TerraForm compilation failed
    pause
    exit /b 1
)

echo.
echo Build completed successfully!
echo Output: native_gguf_loader.exe
echo.

REM Test the binary if model.gguf exists
if exist "..\..\..\model.gguf" (
    echo Testing with model.gguf...
    native_gguf_loader.exe
) else (
    echo model.gguf not found, skipping test
)

echo.
pause