@echo off
echo Compiling Extended π-Engine...
javac PiEngine-Extended.java
if %errorlevel% == 0 (
    echo Compilation successful!
    echo.
    echo Running Extended π-Engine...
    java PiEngine
) else (
    echo Compilation failed!
)
pause