@echo off
REM Compile Secure IDE GUI Application
REM Real desktop application like Cursor

echo === Secure IDE GUI Compilation ===
echo Building desktop application...

REM Compile the main GUI application
echo Compiling SecureIDEGUI.java...
javac SecureIDEGUI.java

REM Compile supporting classes
echo Compiling supporting classes...
javac -cp . IntegratedASMCompiler.java
javac -cp . PureAssemblyCompiler.java
javac -cp . AssemblyCompiler.java

echo.
echo === Compilation Complete ===
echo Generated class files:
echo   - SecureIDEGUI.class
echo   - IntegratedASMCompiler.class
echo   - PureAssemblyCompiler.class
echo   - AssemblyCompiler.class

echo.
echo === Features ===
echo ^✓ Modern GUI Interface
echo ^✓ Code Editor with Syntax Highlighting
echo ^✓ File Explorer
echo ^✓ AI Assistant Panel
echo ^✓ Integrated Terminal
echo ^✓ Security Status Panel
echo ^✓ Built-in Assembly Compiler
echo ^✓ Local AI Processing
echo ^✓ Security Monitoring

echo.
echo === Usage ===
echo Run GUI: java SecureIDEGUI
echo 
echo The application will open with:
echo   - File explorer on the left
echo   - Code editor in the center
echo   - AI assistant and terminal at the bottom
echo   - Security status panel

echo.
echo Compilation successful!
echo Starting Secure IDE GUI...
java SecureIDEGUI
pause
