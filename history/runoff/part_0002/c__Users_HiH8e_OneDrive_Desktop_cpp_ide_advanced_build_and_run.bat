@echo off
echo ========================================================
echo ADVANCED C++ IDE - RAPID BUILD SYSTEM
echo ========================================================
echo.

echo [1/4] Checking compiler...
where g++ >nul 2>nul
if %errorlevel% neq 0 (
    echo ERROR: g++ compiler not found!
    echo Please install MinGW-w64 or MSYS2
    echo Download from: https://www.mingw-w64.org/
    pause
    exit /b 1
)
echo ✅ g++ compiler found

echo [2/4] Cleaning previous builds...
if exist advanced_ide.exe del advanced_ide.exe
if exist *.o del *.o
echo ✅ Clean complete

echo [3/4] Compiling Advanced C++ IDE...
g++ -std=c++17 -O2 -Wall -Wextra -o advanced_ide.exe main.cpp -mwindows -lcomctl32 -lcomdlg32 -lshell32 -lshlwapi -lole32 -luuid

if %errorlevel% neq 0 (
    echo ❌ Compilation failed!
    echo Check the error messages above.
    pause
    exit /b 1
)
echo ✅ Compilation successful!

echo [4/4] Starting IDE...
if exist advanced_ide.exe (
    echo ✅ Build complete: advanced_ide.exe
    echo.
    echo ========================================================
    echo FEATURES IMPLEMENTED:
    echo ✅ Multi-tab text editor with syntax highlighting
    echo ✅ File operations: New, Open, Save, Save As
    echo ✅ Real compilation system (g++ integration)
    echo ✅ Program execution with console output
    echo ✅ Drag & drop file support
    echo ✅ Full menu system and toolbar
    echo ✅ Resizable panes and status bar
    echo ✅ File tree explorer
    echo ✅ Output window for compilation results
    echo ✅ Keyboard shortcuts (Ctrl+N, Ctrl+O, Ctrl+S, F5, F7)
    echo ✅ Professional Windows GUI
    echo ========================================================
    echo.
    echo Starting Advanced C++ IDE...
    echo.
    advanced_ide.exe
) else (
    echo ❌ Build failed - executable not created!
    pause
    exit /b 1
)

echo.
echo IDE session ended.
pause