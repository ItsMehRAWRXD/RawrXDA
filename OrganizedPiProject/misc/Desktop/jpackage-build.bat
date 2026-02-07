@echo off
echo Building with jpackage (Java 14+)...

REM Compile and create JAR first
call build-exe.bat

REM Use jpackage to create native installer
jpackage --input . --name "SimpleCursorIDE" --main-jar SimpleCursorIDE.jar --main-class SimpleCursor --type exe --win-console

echo ✅ Native installer created in current directory
pause