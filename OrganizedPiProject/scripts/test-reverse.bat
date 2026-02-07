@echo off
echo Testing Reverse π Engine...
echo.
echo 1. Compiling ReverseGrade.java
javac ReverseGrade.java 2>nul
if %errorlevel% neq 0 (
    echo Compilation failed - reverse syntax not valid Java!
    echo Creating working test version...
    copy UniversalGrade.java TestGrade.java >nul
    javac TestGrade.java
    if %errorlevel% equ 0 (
        echo Test version compiled successfully!
        java TestGrade
    )
) else (
    echo Reverse compilation successful!
    java ReverseGrade
)

echo.
echo Test complete!
pause