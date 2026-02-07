@echo off
echo Testing Universal Grade π Engine...
echo.
echo 1. Compiling UniversalGrade.java
javac UniversalGrade.java
if %errorlevel% neq 0 (
    echo Compilation failed!
    pause
    exit /b 1
)

echo 2. Starting Universal Grade
java UniversalGrade

echo.
echo Test complete!
pause