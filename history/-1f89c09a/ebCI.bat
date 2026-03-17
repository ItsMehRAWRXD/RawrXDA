@echo off
setlocal enableextensions
echo === ;(O);btnBuilder - Zero-Config Build GUI ===
echo.

where java >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
	echo [ERROR] Java runtime not found.
	echo This GUI requires Java 21 or later to run: java --source 21 btnBuilder.java
	echo.
	echo Quick install options (choose one):
	echo   - Winget:  winget install EclipseAdoptium.Temurin.21.JDK
	echo   - Oracle:  https://www.oracle.com/java/technologies/downloads/
	echo   - Adoptium: https://adoptium.net/temurin/releases/?version=21
	echo.
	echo Alternatively, use the native Windows builder:
	echo   - Run:  ..\nativeide\build-working.bat  (or ..\NativeIDE\build-working.bat)
	goto :end
)

REM Launch the GUI builder
java --version
java --source 21 btnBuilder.java

:end
pause