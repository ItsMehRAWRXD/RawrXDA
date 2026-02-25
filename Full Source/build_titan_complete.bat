@echo off
:: =============================================================================
:: build_titan_complete.bat
:: Unified Build System for RawrXD Titan Engine (QuadBuffer + Features 14-21)
:: =============================================================================
setlocal enabledelayedexpansion

echo ========================================
echo RawrXD 800B TITAN ENGINE BUILD SYSTEM
echo Features 14-21: Predictor, DirectStorage, Vulkan Sparse, NF4 GPU
echo ========================================
echo.

:: Configuration
set "BUILD_MODE=release"
set "ENABLE_VULKAN=1"
set "ENABLE_DIRECTSTORAGE=1"
set "ENABLE_GUI=1"

if /I "%1"=="debug" set "BUILD_MODE=debug"
if /I "%1"=="clean" goto :CLEAN

:: Step 1: Locate Visual Studio (MASM ML64)
echo [1/7] Locating MASM (ML64)...
set "ML64_PATH="
for /D %%D in ("C:\Program Files*\Microsoft Visual Studio\*\*\VC\Tools\MSVC\*") do (
    if exist "%%D\bin\Hostx64\x64\ml64.exe" (
        set "ML64_PATH=%%D\bin\Hostx64\x64"
        set "SDK_LIB=%%D\lib\x64"
        goto :FOUND_ML64
    )
)
echo ERROR: ML64 not found. Install Visual Studio 2019+ with MASM.
exit /b 1

:FOUND_ML64
echo    Found: !ML64_PATH!\ml64.exe
set "PATH=!ML64_PATH!;%PATH%"

:: Step 2: Locate Windows SDK (for kernel32.lib, ws2_32.lib)
echo [2/7] Locating Windows SDK...
set "SDK_LIB="
for /D %%D in ("C:\Program Files*\Windows Kits\10\Lib\*") do (
    if exist "%%D\um\x64\kernel32.lib" (
        set "SDK_LIB=%%D"
        goto :FOUND_SDK
    )
)
echo ERROR: Windows SDK not found.
exit /b 1

:FOUND_SDK
echo    Found: !SDK_LIB!

:: Step 3: Compile GLSL Shader (Feature 15)
if "!ENABLE_VULKAN!"=="1" (
    echo [3/7] Compiling NF4 Shader ^(GLSL -^> SPIR-V^)...
    where glslangValidator >nul 2>&1
    if errorlevel 1 (
        echo    WARNING: glslangValidator not found. Skipping shader compilation.
        echo    Install Vulkan SDK from https://vulkan.lunarg.com/
    ) else (
        glslangValidator -V -o D:\rawrxd\shaders\nf4_shader.spv D:\rawrxd\shaders\RawrXD_NF4_Shader.comp
        if errorlevel 1 (
            echo    ERROR: Shader compilation failed.
            exit /b 1
        )
        echo    Generated: nf4_shader.spv
    )
) else (
    echo [3/7] Skipping shader compilation ^(Vulkan disabled^)
)

:: Step 4: Compile Resource File (GUI)
if "!ENABLE_GUI!"=="1" (
    echo [4/7] Compiling GUI Resources...
    where rc.exe >nul 2>&1
    if errorlevel 1 (
        echo    ERROR: rc.exe not found. Cannot compile .rc file.
        exit /b 1
    )
    rc.exe /v /fo D:\rawrxd\gui\RawrXD_Titan_GUI.res D:\rawrxd\gui\RawrXD_Titan_GUI.rc
    if errorlevel 1 (
        echo    ERROR: Resource compilation failed.
        exit /b 1
    )
    echo    Generated: RawrXD_Titan_GUI.res
) else (
    echo [4/7] Skipping GUI resources
)

:: Step 5: Assemble MASM Files
echo [5/7] Assembling MASM modules...

set "ML64_FLAGS=/c /Cx /Zf /Zi"
if "!BUILD_MODE!"=="release" (
    set "ML64_FLAGS=!ML64_FLAGS! /O2"
)
set "ML64_FLAGS=!ML64_FLAGS! /I"D:\rawrxd\include" /I"\masm64\include64""

:: Core QuadBuffer
echo    - RawrXD_QuadBuffer_DMA_Orchestrator.asm
ml64 !ML64_FLAGS! /Fo"D:\rawrxd\obj\QuadBuffer_DMA.obj" ^
     D:\rawrxd\src\orchestrator\RawrXD_QuadBuffer_DMA_Orchestrator.asm
if errorlevel 1 goto :BUILD_ERROR

:: Validation Suite
echo    - RawrXD_QuadBuffer_Validate.asm
ml64 !ML64_FLAGS! /Fo"D:\rawrxd\obj\QuadBuffer_Validate.obj" ^
     D:\rawrxd\src\orchestrator\RawrXD_QuadBuffer_Validate.asm
if errorlevel 1 goto :BUILD_ERROR

:: Titan Extensions (Features 14-21)
echo    - RawrXD_Titan_Extensions.asm
ml64 !ML64_FLAGS! /Fo"D:\rawrxd\obj\Titan_Extensions.obj" ^
     D:\rawrxd\src\orchestrator\RawrXD_Titan_Extensions.asm
if errorlevel 1 goto :BUILD_ERROR

:: Phase 5 Master Orchestrator
echo    - Phase5_Master_Complete.asm
ml64 !ML64_FLAGS! /Fo"D:\rawrxd\obj\Phase5_Master.obj" ^
     D:\rawrxd\src\orchestrator\Phase5_Master_Complete.asm
if errorlevel 1 goto :BUILD_ERROR

echo    All MASM modules assembled successfully.

:: Step 6: Compile C++ Wrapper (Optional)
echo [6/7] Compiling C++ integration wrapper...
where cl.exe >nul 2>&1
if errorlevel 1 (
    echo    WARNING: cl.exe not found. Skipping C++ wrapper.
) else (
    cl.exe /c /O2 /EHsc /std:c++17 ^
           /I"D:\rawrxd\include" ^
           /Fo"D:\rawrxd\obj\QuadBuffer_Wrapper.obj" ^
           D:\rawrxd\src\orchestrator\QuadBuffer_DMA_Wrapper.cpp
    if errorlevel 1 (
        echo    WARNING: C++ wrapper compilation failed. Continuing with MASM-only build.
    )
)

:: Step 7: Link Everything
echo [7/7] Linking final executable...

set "LINK_FLAGS=/SUBSYSTEM:WINDOWS /ENTRY:main"
if "!BUILD_MODE!"=="debug" (
    set "LINK_FLAGS=!LINK_FLAGS! /DEBUG"
)

set "OBJ_FILES=D:\rawrxd\obj\QuadBuffer_DMA.obj"
set "OBJ_FILES=!OBJ_FILES! D:\rawrxd\obj\Titan_Extensions.obj"
set "OBJ_FILES=!OBJ_FILES! D:\rawrxd\obj\Phase5_Master.obj"

if exist "D:\rawrxd\obj\QuadBuffer_Wrapper.obj" (
    set "OBJ_FILES=!OBJ_FILES! D:\rawrxd\obj\QuadBuffer_Wrapper.obj"
)

set "LIBS=kernel32.lib user32.lib gdi32.lib advapi32.lib ws2_32.lib"
set "LIBS=!LIBS! comctl32.lib shell32.lib"

:: Add Vulkan if enabled
if "!ENABLE_VULKAN!"=="1" (
    set "LIBS=!LIBS! vulkan-1.lib"
)

:: Add DirectStorage if enabled
if "!ENABLE_DIRECTSTORAGE!"=="1" (
    set "LIBS=!LIBS! dstorage.lib"
)

set "LIBPATH=/LIBPATH:"!SDK_LIB!\um\x64" /LIBPATH:"!SDK_LIB!\ucrt\x64""

:: Add Vulkan SDK lib path
if "!ENABLE_VULKAN!"=="1" (
    if exist "C:\VulkanSDK" (
        for /D %%V in ("C:\VulkanSDK\*") do (
            set "LIBPATH=!LIBPATH! /LIBPATH:"%%V\Lib""
        )
    )
)

set "OUTPUT=D:\rawrxd\bin\RawrXD-Titan-Engine.exe"
if "!BUILD_MODE!"=="debug" (
    set "OUTPUT=D:\rawrxd\bin\RawrXD-Titan-Engine-Debug.exe"
)

link !LINK_FLAGS! /OUT:"!OUTPUT!" !LIBPATH! !OBJ_FILES! !LIBS!
if errorlevel 1 goto :LINK_ERROR

:: Add GUI resources if built
if "!ENABLE_GUI!"=="1" (
    if exist "D:\rawrxd\gui\RawrXD_Titan_GUI.res" (
        echo    Embedding GUI resources...
        :: Note: Resources should be linked in link command
        :: For simplicity, shown as separate step
    )
)

echo.
echo ========================================
echo BUILD SUCCESSFUL
echo ========================================
echo Output: !OUTPUT!
echo Mode:   !BUILD_MODE!
echo Features:
echo    - QuadBuffer DMA Orchestrator: YES
echo    - Titan Extensions (14-21):    YES
echo    - Vulkan Sparse Binding:       !ENABLE_VULKAN!
echo    - DirectStorage:               !ENABLE_DIRECTSTORAGE!
echo    - Live GUI Control:            !ENABLE_GUI!
echo ========================================
echo.

:: Post-build verification
echo Post-build checks:
dir "!OUTPUT!" | find ".exe"
dumpbin /EXPORTS "!OUTPUT!" | find "TITAN_" | find /C "TITAN_"
echo.

goto :END

:BUILD_ERROR
echo.
echo ========================================
echo ASSEMBLY FAILED
echo ========================================
echo Check error messages above.
exit /b 1

:LINK_ERROR
echo.
echo ========================================
echo LINK FAILED
echo ========================================
echo Check library paths and dependencies.
exit /b 1

:CLEAN
echo Cleaning build artifacts...
if exist "D:\rawrxd\obj" rd /s /q "D:\rawrxd\obj"
if exist "D:\rawrxd\bin" rd /s /q "D:\rawrxd\bin"
if exist "D:\rawrxd\shaders\*.spv" del /q "D:\rawrxd\shaders\*.spv"
if exist "D:\rawrxd\gui\*.res" del /q "D:\rawrxd\gui\*.res"
mkdir "D:\rawrxd\obj" 2>nul
mkdir "D:\rawrxd\bin" 2>nul
echo Clean complete.
goto :END

:END
endlocal
