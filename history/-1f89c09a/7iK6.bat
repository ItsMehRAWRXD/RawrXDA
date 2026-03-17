@echo off@echo off

setlocal enableextensionssetlocal enableextensions

echo === ;(O);btnBuilder - Zero-Config Build GUI ===echo === ;(O);btnBuilder - Zero-Config Build GUI ===

echo.echo.



where java >nul 2>&1where java >nul 2>&1

if %ERRORLEVEL% NEQ 0 (if %ERRORLEVEL% NEQ 0 (

  echo [ERROR] Java runtime not found.	echo [ERROR] Java runtime not found.

  echo This GUI requires Java 21 or later to run: java --source 21 btnBuilder.java	echo This GUI requires Java 21 or later to run: java --source 21 btnBuilder.java

  echo.	echo.

  echo Quick install options (choose one):	echo Quick install options (choose one):

  echo   - Winget:  winget install EclipseAdoptium.Temurin.21.JDK	echo   - Winget:  winget install EclipseAdoptium.Temurin.21.JDK

  echo   - Oracle:  https://www.oracle.com/java/technologies/downloads/	echo   - Oracle:  https://www.oracle.com/java/technologies/downloads/

  echo   - Adoptium: https://adoptium.net/temurin/releases/?version=21	echo   - Adoptium: https://adoptium.net/temurin/releases/?version=21

  echo.	echo.

  echo Alternatively, use the native Windows builder:	echo Alternatively, use the native Windows builder:

  echo   - Run:  ..\nativeide\build-working.bat  (or ..\NativeIDE\build-working.bat)	echo   - Run:  ..\nativeide\build-working.bat  (or ..\NativeIDE\build-working.bat)

  goto :end	goto :end

))



REM Launch the GUI builderREM Launch the GUI builder

java --versionjava --version

java --source 21 btnBuilder.javajava --source 21 btnBuilder.java



:end:end

pausepause
