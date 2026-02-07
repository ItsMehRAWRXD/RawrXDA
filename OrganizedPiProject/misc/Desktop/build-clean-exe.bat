@echo off
echo Building Clean Encrypted IDE...

REM Compile with encryption
javac CleanEncryption.java CursorLikeIDE.java InlineAssistant.java ChatInterface.java SimpleCursor.java

REM Create clean JAR
jar cfe CleanCursorIDE.jar SimpleCursor CleanEncryption.class CursorLikeIDE.class InlineAssistant.class ChatInterface.class SimpleCursor.class

REM Create Windows launcher
echo @echo off > CleanCursorIDE.bat
echo java -jar CleanCursorIDE.jar >> CleanCursorIDE.bat

echo [OK] Clean encrypted IDE created: CleanCursorIDE.jar
echo [OK] Launcher created: CleanCursorIDE.bat
pause