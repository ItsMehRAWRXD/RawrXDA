@echo off
echo Compiling Java to native executable...

REM Check for GraalVM
if exist "%GRAALVM_HOME%\bin\native-image.exe" (
    echo Using GraalVM Native Image...
    javac *.java
    jar cfe app.jar SimpleCursor *.class
    "%GRAALVM_HOME%\bin\native-image.exe" -jar app.jar --no-fallback SimpleCursorIDE
    echo [OK] Native EXE created: SimpleCursorIDE.exe
) else (
    echo GraalVM not found. Install from: https://www.graalvm.org/
    echo Alternative: Use jpackage with JDK 14+
    jpackage --input . --name SimpleCursorIDE --main-jar CleanCursorIDE.jar --main-class SimpleCursor --type exe
)

pause