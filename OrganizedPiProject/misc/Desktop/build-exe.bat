@echo off
echo Building IDE to EXE...

REM Compile Java files
javac -cp "picocli-4.7.5.jar:javax.json-1.1.4.jar" *.java

REM Create JAR
jar cfe SimpleCursorIDE.jar SimpleCursor *.class

REM Build native executable with GraalVM (if installed)
if exist "%GRAALVM_HOME%\bin\native-image.exe" (
    "%GRAALVM_HOME%\bin\native-image.exe" -jar SimpleCursorIDE.jar SimpleCursorIDE.exe
    echo ✅ Native EXE created: SimpleCursorIDE.exe
) else (
    echo ⚠️  GraalVM not found. Creating launcher...
    call create-launcher.bat
)

pause