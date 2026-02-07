@echo off
REM Compile ASM IDE with javac
REM No external dependencies - Pure Java implementation

echo === ASM IDE Compilation ===
echo Compiling with javac...

REM Compile the main ASM IDE
echo Compiling ASMIDE.java...
javac ASMIDE.java

REM Compile the integrated compiler
echo Compiling IntegratedASMCompiler.java...
javac IntegratedASMCompiler.java

REM Compile the standalone compiler
echo Compiling PureAssemblyCompiler.java...
javac PureAssemblyCompiler.java

REM Compile the assembly compiler
echo Compiling AssemblyCompiler.java...
javac AssemblyCompiler.java

REM Compile the secure IDE
echo Compiling SecureIDE.java...
javac SecureIDE.java

echo.
echo === Compilation Complete ===
echo Generated class files:
echo   - ASMIDE.class
echo   - IntegratedASMCompiler.class
echo   - PureAssemblyCompiler.class
echo   - AssemblyCompiler.class
echo   - SecureIDE.class

echo.
echo === Usage ===
echo Run ASM IDE: java ASMIDE
echo Run Secure IDE: java SecureIDE
echo Compile Assembly: java PureAssemblyCompiler file.asm

echo.
echo === Features ===
echo ^✓ Built-in Assembly Compiler
echo ^✓ No External Dependencies
echo ^✓ Local AI Processing
echo ^✓ Security Monitoring
echo ^✓ File Management
echo ^✓ Terminal Integration

echo.
echo Compilation successful!
pause
