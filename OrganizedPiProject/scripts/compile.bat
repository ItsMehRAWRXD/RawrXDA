@echo off
cd /d "%~dp0"
echo Compiling Java files...
javac -cp . *.java
if %errorlevel% == 0 (
    echo Compilation successful!
    echo Run with: java EditorCLI
) else (
    echo Compilation failed!
)
pause