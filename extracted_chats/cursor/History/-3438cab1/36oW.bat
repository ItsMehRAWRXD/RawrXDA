@echo off
echo 🔍 Smart Project Finder - Find YOUR Actual Projects
echo 📅 Let's find the real projects you've built!
echo.

REM Enable delayed expansion
setlocal enabledelayedexpansion

echo 🎯 Scanning for YOUR projects (excluding system files)...
echo.

REM Initialize counters
set python_count=0
set java_count=0
set csharp_count=0
set cpp_count=0
set web_count=0
set total_projects=0

echo 📊 Found Projects:
echo.

REM Find Python projects (excluding system directories)
echo 🐍 Python Projects:
for /r %%d in (.) do (
    REM Skip system directories
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|venv\|env\|__pycache__\|\.pytest_cache\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.py" (
            REM Check if this looks like a real project (has main or app file)
            set "has_main=0"
            for %%f in (*.py) do (
                findstr /i "if __name__ == \"__main__\":" "%%f" >nul
                if not errorlevel 1 set "has_main=1"
                findstr /i "def main" "%%f" >nul
                if not errorlevel 1 set "has_main=1"
            )
            if "!has_main!"=="1" (
                set /a python_count+=1
                set /a total_projects+=1
                echo   !python_count!. %%d
                if exist "requirements.txt" echo     📦 Has requirements.txt
                if exist "main.py" echo     🚀 Has main.py
                if exist "app.py" echo     📱 Has app.py
            )
        )
        popd
    )
)

echo.

REM Find Java projects
echo ☕ Java Projects:
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|target\|\.m2\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.java" (
            set "has_main=0"
            for %%f in (*.java) do (
                findstr /i "public static void main" "%%f" >nul
                if not errorlevel 1 set "has_main=1"
            )
            if "!has_main!"=="1" (
                set /a java_count+=1
                set /a total_projects+=1
                echo   !java_count!. %%d
                if exist "pom.xml" echo     📦 Maven project
                if exist "build.gradle" echo     🔧 Gradle project
            )
        )
        popd
    )
)

echo.

REM Find C# projects
echo 🔷 C# Projects:
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|bin\|obj\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.cs" (
            set "has_main=0"
            for %%f in (*.cs) do (
                findstr /i "static void Main" "%%f" >nul
                if not errorlevel 1 set "has_main=1"
            )
            if "!has_main!"=="1" (
                set /a csharp_count+=1
                set /a total_projects+=1
                echo   !csharp_count!. %%d
                if exist "*.csproj" echo     🔧 Visual Studio project
                if exist "*.sln" echo     🏗️ Solution file
            )
        )
        popd
    )
)

echo.

REM Find C++ projects
echo ⚡ C++ Projects:
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|build\|Debug\|Release\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.cpp" (
            set "has_main=0"
            for %%f in (*.cpp) do (
                findstr /i "int main" "%%f" >nul
                if not errorlevel 1 set "has_main=1"
            )
            if "!has_main!"=="1" (
                set /a cpp_count+=1
                set /a total_projects+=1
                echo   !cpp_count!. %%d
                if exist "CMakeLists.txt" echo     🔧 CMake project
                if exist "Makefile" echo     🔧 Makefile project
            )
        )
        popd
    )
)

echo.

REM Find Web projects
echo 🌐 Web Projects:
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|dist\|build\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.html" (
            set "is_web_project=0"
            if exist "index.html" set "is_web_project=1"
            if exist "*.js" set "is_web_project=1"
            if exist "*.css" set "is_web_project=1"
            if exist "package.json" set "is_web_project=1"
            
            if "!is_web_project!"=="1" (
                set /a web_count+=1
                set /a total_projects+=1
                echo   !web_count!. %%d
                if exist "package.json" echo     📦 Node.js project
                if exist "*.js" echo     ⚡ Has JavaScript
                if exist "*.css" echo     🎨 Has CSS
            )
        )
        popd
    )
)

echo.
echo 📊 Project Summary:
echo   🐍 Python Projects: %python_count%
echo   ☕ Java Projects: %java_count%
echo   🔷 C# Projects: %csharp_count%
echo   ⚡ C++ Projects: %cpp_count%
echo   🌐 Web Projects: %web_count%
echo   📊 Total Projects: %total_projects%
echo.

if %total_projects%==0 (
    echo ❌ No projects found. Make sure you're in the right directory.
    echo 💡 Try running this from your main development folder.
    pause
    exit /b 1
)

echo 🚀 What would you like to do?
echo.
echo 1. Compile all found projects
echo 2. Compile Python projects only
echo 3. Compile Java projects only
echo 4. Compile C# projects only
echo 5. Compile C++ projects only
echo 6. Package web projects only
echo 7. Open project folders
echo 8. Exit
echo.

set /p choice="Enter your choice (1-8): "

if "%choice%"=="1" (
    echo 🔧 Compiling all projects...
    call :compile_all_projects
) else if "%choice%"=="2" (
    echo 🐍 Compiling Python projects...
    call :compile_python_only
) else if "%choice%"=="3" (
    echo ☕ Compiling Java projects...
    call :compile_java_only
) else if "%choice%"=="4" (
    echo 🔷 Compiling C# projects...
    call :compile_csharp_only
) else if "%choice%"=="5" (
    echo ⚡ Compiling C++ projects...
    call :compile_cpp_only
) else if "%choice%"=="6" (
    echo 🌐 Packaging web projects...
    call :package_web_only
) else if "%choice%"=="7" (
    echo 📁 Opening project folders...
    call :open_project_folders
) else if "%choice%"=="8" (
    echo 👋 Goodbye!
    exit /b 0
) else (
    echo ❌ Invalid choice. Please try again.
    pause
    goto :eof
)

echo.
echo ✅ Done! Check the 'compiled_projects' folder for results.
pause
exit /b 0

REM ========================================
REM Compilation Functions
REM ========================================

:compile_all_projects
if not exist "compiled_projects" mkdir compiled_projects
call :compile_python_only
call :compile_java_only
call :compile_csharp_only
call :compile_cpp_only
call :package_web_only
goto :eof

:compile_python_only
echo   Creating Python executables...
python -c "import PyInstaller" 2>nul
if errorlevel 1 (
    echo   Installing PyInstaller...
    pip install pyinstaller
)

for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|venv\|env\|__pycache__\|\.pytest_cache" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.py" (
            for %%f in (*.py) do (
                findstr /i "if __name__ == \"__main__\":" "%%f" >nul
                if not errorlevel 1 (
                    echo   Compiling: %%f
                    pyinstaller --onefile --windowed --name "%%~nf" --distpath compiled_projects "%%f" >nul 2>&1
                    if not errorlevel 1 (
                        echo   ✅ Created: %%~nf.exe
                    ) else (
                        echo   ❌ Failed: %%~nf
                    )
                )
            )
        )
        popd
    )
)
goto :eof

:compile_java_only
echo   Creating Java JAR files...
java -version >nul 2>&1
if errorlevel 1 (
    echo   ❌ Java not found. Please install Java JDK.
    goto :eof
)

for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|target\|\.m2\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.java" (
            for %%f in (*.java) do (
                findstr /i "public static void main" "%%f" >nul
                if not errorlevel 1 (
                    echo   Compiling: %%d
                    javac *.java 2>nul
                    if not errorlevel 1 (
                        jar cfe "compiled_projects\%%~nf.jar" "%%~nf" *.class 2>nul
                        if not errorlevel 1 (
                            echo   ✅ Created: %%~nf.jar
                        )
                    ) else (
                        echo   ❌ Compilation failed: %%~nf
                    )
                    goto :next_java
                )
            )
            :next_java
        )
        popd
    )
)
goto :eof

:compile_csharp_only
echo   Creating C# executables...
csc /? >nul 2>&1
if errorlevel 1 (
    echo   ❌ C# compiler not found. Please install Visual Studio or .NET SDK.
    goto :eof
)

for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|bin\|obj\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.cs" (
            for %%f in (*.cs) do (
                findstr /i "static void Main" "%%f" >nul
                if not errorlevel 1 (
                    echo   Compiling: %%d
                    csc /out:"compiled_projects\%%~nf.exe" *.cs 2>nul
                    if not errorlevel 1 (
                        echo   ✅ Created: %%~nf.exe
                    ) else (
                        echo   ❌ Compilation failed: %%~nf
                    )
                    goto :next_csharp
                )
            )
            :next_csharp
        )
        popd
    )
)
goto :eof

:compile_cpp_only
echo   Creating C++ executables...
g++ --version >nul 2>&1
if errorlevel 1 (
    echo   ❌ C++ compiler not found. Please install MinGW or Visual Studio.
    goto :eof
)

for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|build\|Debug\|Release\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.cpp" (
            for %%f in (*.cpp) do (
                findstr /i "int main" "%%f" >nul
                if not errorlevel 1 (
                    echo   Compiling: %%d
                    g++ -o "compiled_projects\%%~nf.exe" *.cpp 2>nul
                    if not errorlevel 1 (
                        echo   ✅ Created: %%~nf.exe
                    ) else (
                        echo   ❌ Compilation failed: %%~nf
                    )
                    goto :next_cpp
                )
            )
            :next_cpp
        )
        popd
    )
)
goto :eof

:package_web_only
echo   Packaging web projects...
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|dist\|build\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        pushd "%%d"
        if exist "*.html" (
            set "is_web_project=0"
            if exist "index.html" set "is_web_project=1"
            if exist "*.js" set "is_web_project=1"
            if exist "*.css" set "is_web_project=1"
            if exist "package.json" set "is_web_project=1"
            
            if "!is_web_project!"=="1" (
                echo   Packaging: %%d
                for %%p in ("%%d") do set "project_name=%%~np"
                if not exist "compiled_projects\!project_name!_web" mkdir "compiled_projects\!project_name!_web"
                copy "*.html" "compiled_projects\!project_name!_web\" >nul 2>&1
                copy "*.js" "compiled_projects\!project_name!_web\" >nul 2>&1
                copy "*.css" "compiled_projects\!project_name!_web\" >nul 2>&1
                copy "*.json" "compiled_projects\!project_name!_web\" >nul 2>&1
                echo   ✅ Packaged: !project_name!_web
            )
        )
        popd
    )
)
goto :eof

:open_project_folders
echo   Opening project folders...
for /r %%d in (.) do (
    echo "%%d" | findstr /i "node_modules\|\.vscode\|\.git\|venv\|env\|__pycache__\|\.pytest_cache\|target\|\.m2\|bin\|obj\|build\|Debug\|Release\|dist\|Config\.Msi\|Windows\|Program Files\|ProgramData\|System Volume Information\|$RECYCLE\.BIN" >nul
    if errorlevel 1 (
        if exist "%%d\*.py" (
            start "" "%%d"
        ) else if exist "%%d\*.java" (
            start "" "%%d"
        ) else if exist "%%d\*.cs" (
            start "" "%%d"
        ) else if exist "%%d\*.cpp" (
            start "" "%%d"
        ) else if exist "%%d\*.html" (
            start "" "%%d"
        )
    )
)
goto :eof
