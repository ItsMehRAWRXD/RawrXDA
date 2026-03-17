@echo off
REM Build Ollama Native Bridge as DLL for Python FFI
echo ========================================
echo Building Ollama Native Bridge DLL
echo ========================================

REM Ensure build directory exists
if not exist build mkdir build
if not exist lib mkdir lib

REM Step 1: Compile ollama_native_v2.asm
echo.
echo [1/3] Compiling ollama_native_v2.asm...
nasm -f win64 -DPLATFORM_WIN src\ollama_native_v2.asm -o build\ollama_native_v2.obj
if %ERRORLEVEL% neq 0 (
    echo ERROR: NASM compilation failed
    exit /b 1
)
echo ✓ Compilation successful

REM Step 2: Compile json_parser.asm
echo.
echo [2/3] Compiling json_parser.asm...
nasm -f win64 -DPLATFORM_WIN src\json_parser.asm -o build\json_parser.obj
if %ERRORLEVEL% neq 0 (
    echo ERROR: JSON parser compilation failed
    exit /b 1
)
echo ✓ JSON parser compiled

REM Step 3: Link into DLL
echo.
echo [3/3] Linking DLL...
link /DLL /OUT:lib\ollama_native.dll ^
     /SUBSYSTEM:CONSOLE ^
     /ENTRY:DllMain ^
     /EXPORT:ollama_init ^
     /EXPORT:ollama_connect ^
     /EXPORT:ollama_generate ^
     /EXPORT:ollama_list_models ^
     /EXPORT:ollama_close ^
     /EXPORT:parse_http_response ^
     /EXPORT:strlen_impl ^
     build\ollama_native_v2.obj ^
     build\json_parser.obj ^
     kernel32.lib ws2_32.lib

if %ERRORLEVEL% neq 0 (
    echo ERROR: Linking failed
    echo.
    echo Attempting minimal link without DllMain...
    link /DLL /OUT:lib\ollama_native.dll ^
         /SUBSYSTEM:CONSOLE ^
         /NOENTRY ^
         /EXPORT:ollama_init ^
         /EXPORT:ollama_connect ^
         /EXPORT:ollama_generate ^
         /EXPORT:ollama_list_models ^
         /EXPORT:ollama_close ^
         /EXPORT:parse_http_response ^
         /EXPORT:strlen_impl ^
         build\ollama_native_v2.obj ^
         build\json_parser.obj ^
         kernel32.lib ws2_32.lib
    
    if %ERRORLEVEL% neq 0 (
        echo ERROR: Minimal linking also failed
        exit /b 1
    )
)

echo.
echo ========================================
echo ✓ BUILD SUCCESSFUL
echo ========================================
echo.
echo DLL created: lib\ollama_native.dll
dir lib\ollama_native.dll
echo.
echo Test with: python -c "from src.ollama_bridge import OllamaBridge; b = OllamaBridge()"
