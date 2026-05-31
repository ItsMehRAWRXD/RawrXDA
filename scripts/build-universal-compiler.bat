@echo off
REM Build and test the Universal Compiler

set COMPILER_SRC=d:\rawrxd\src\tools\universal_compiler.cpp
set OUTPUT_EXE=d:\rawrxd\bin\universal_compiler.exe
set EXAMPLE_IR=d:\rawrxd\examples\factorial.ir
set OUTPUT_CPP=d:\rawrxd\examples\factorial.cpp
set OUTPUT_PY=d:\rawrxd\examples\factorial.py
set OUTPUT_JAVA=d:\rawrxd\examples\Factorial.java

REM Create directories if needed
if not exist d:\rawrxd\bin mkdir d:\rawrxd\bin
if not exist d:\rawrxd\examples mkdir d:\rawrxd\examples

echo [BUILD] Compiling Universal Compiler...
g++ -std=c++20 -O2 "%COMPILER_SRC%" -o "%OUTPUT_EXE%"
if errorlevel 1 (
    echo Error: Compilation failed
    exit /b 1
)

echo [SUCCESS] Compiled: %OUTPUT_EXE%
echo.

echo [TEST] Generating C++ code...
"%OUTPUT_EXE%" "%EXAMPLE_IR%" --lang cpp --output "%OUTPUT_CPP%"
if exist "%OUTPUT_CPP%" (
    echo [OK] Generated: %OUTPUT_CPP%
    type "%OUTPUT_CPP%"
    echo.
)

echo [TEST] Generating Python code...
"%OUTPUT_EXE%" "%EXAMPLE_IR%" --lang python --output "%OUTPUT_PY%"
if exist "%OUTPUT_PY%" (
    echo [OK] Generated: %OUTPUT_PY%
    type "%OUTPUT_PY%"
    echo.
)

echo [TEST] Generating Java code...
"%OUTPUT_EXE%" "%EXAMPLE_IR%" --lang java --output "%OUTPUT_JAVA%"
if exist "%OUTPUT_JAVA%" (
    echo [OK] Generated: %OUTPUT_JAVA%
    type "%OUTPUT_JAVA%"
    echo.
)

echo [SUMMARY] Universal Compiler test completed successfully!
