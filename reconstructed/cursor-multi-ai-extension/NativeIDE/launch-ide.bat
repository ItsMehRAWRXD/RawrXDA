@echo off
echo ===============================================
echo    NATIVE IDE - UNIVERSAL TOOLCHAIN
echo ===============================================
echo.
echo [LAUNCH] Starting Native IDE System...
echo.

REM Check if GUI is available
if exist "..\native-ide\btnBuilder.java" (
    echo [GUI] Starting Java Build Interface...
    cd ..\native-ide
    java --source 21 btnBuilder.java
    cd ..\NativeIDE
) else (
    echo [CLI] Native IDE Command Line Ready
    echo.
    echo Universal Zero-Config Toolchain:
    echo   Supports: C, C++, Rust, Go, Python, Zig
    echo   Usage: reverse-tool-simple.bat [file]
    echo.
    echo Available templates:
    if exist templates\*.* (
        dir templates\*.* /b
    ) else (
        echo   Creating sample templates...
        mkdir templates 2>nul
        echo #include ^<stdio.h^> > templates\hello.c
        echo int main() { printf("Hello World!\\n"); return 0; } >> templates\hello.c
        
        echo #include ^<iostream^> > templates\hello.cpp
        echo int main() { std::cout ^<^< "Hello C++!" ^<^< std::endl; return 0; } >> templates\hello.cpp
        
        echo fn main() { println!("Hello Rust!"); } > templates\hello.rs
        echo print("Hello Python!") > templates\hello.py
        
        echo   Templates created: hello.c, hello.cpp, hello.rs, hello.py
    )
    echo.
    echo [TEST] Try compiling a sample:
    echo   reverse-tool-simple.bat templates\hello.c
    echo   reverse-tool-simple.bat templates\hello.cpp
    echo.
)

echo ===============================================
echo Native IDE Ready - Zero Config Compilation!
echo ===============================================
pause