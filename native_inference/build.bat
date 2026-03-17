@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

if %errorlevel% neq 0 (
    echo Failed to set up Visual Studio environment
    exit /b 1
)

echo Creating build directory...
if not exist build mkdir build

echo Compiling native inference engine...

REM Compile C++ files
cl /c /std:c++20 /EHsc /arch:AVX2 /W4 /permissive- /Zc:__cplusplus ^
    native_inference_engine.cpp ^
    /I. ^
    /Fo:build\native_inference_engine.obj

cl /c /std:c++20 /EHsc /arch:AVX2 /W4 /permissive- /Zc:__cplusplus ^
    native_gguf_loader.cpp ^
    /I. ^
    /Fo:build\native_gguf_loader.obj

cl /c /std:c++20 /EHsc /arch:AVX2 /W4 /permissive- /Zc:__cplusplus ^
    native_tokenizer.cpp ^
    /I. ^
    /Fo:build\native_tokenizer.obj

if %errorlevel% neq 0 (
    echo C++ compilation failed
    exit /b 1
)

REM Compile MASM files
ml64 /c native_quant_q4_0.asm /Fo:build\native_quant_q4_0.obj
ml64 /c native_quant_q8_0.asm /Fo:build\native_quant_q8_0.obj
ml64 /c native_matmul_dx12.asm /Fo:build\native_matmul_dx12.obj
ml64 /c native_tokenizer_masm.asm /Fo:build\native_tokenizer_masm.obj

if %errorlevel% neq 0 (
    echo MASM compilation failed
    exit /b 1
)

REM Link library
lib /OUT:build\native_inference.lib ^
    build\native_inference_engine.obj ^
    build\native_gguf_loader.obj ^
    build\native_tokenizer.obj ^
    build\native_quant_q4_0.obj ^
    build\native_quant_q8_0.obj ^
    build\native_matmul_dx12.obj ^
    build\native_tokenizer_masm.obj

if %errorlevel% neq 0 (
    echo Library linking failed
    exit /b 1
)

REM Compile test executable
cl /std:c++20 /EHsc /arch:AVX2 ^
    test_main.cpp ^
    build\native_inference.lib ^
    /I. ^
    /Fe:build\native_inference_test.exe

if %errorlevel% neq 0 (
    echo Test executable compilation failed
    exit /b 1
)

echo Build completed successfully!
echo Run build\native_inference_test.exe to test the implementation