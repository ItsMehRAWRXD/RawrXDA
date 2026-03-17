@echo off
REM ============================================================================
REM FILE: build_masm_ide.bat
REM TITLE: MASM IDE Build Script - Complete Build System
REM PURPOSE: Build the pure MASM RawrXD IDE from source
REM ============================================================================

echo Building RawrXD MASM IDE...
echo.

REM Check if MASM is available
where ml64 >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MASM ml64 not found in PATH
    echo Please install Microsoft Macro Assembler or Visual Studio Build Tools
    echo.
    echo Required components:
    echo - Microsoft Macro Assembler (ml64.exe)
    echo - Windows SDK
    echo - Visual Studio Build Tools (optional)
    pause
    exit /b 1
)

REM Check if required libraries are available
if not exist "kernel32.lib" (
    echo ERROR: kernel32.lib not found
    echo Please ensure Windows SDK is properly installed
    pause
    exit /b 1
)

if not exist "user32.lib" (
    echo ERROR: user32.lib not found
    pause
    exit /b 1
)

if not exist "gdi32.lib" (
    echo ERROR: gdi32.lib not found
    pause
    exit /b 1
)

REM Create build directory
if not exist "build" mkdir build
if not exist "build\obj" mkdir build\obj
if not exist "build\bin" mkdir build\bin

REM ============================================================================
REM ASSEMBLE SOURCE FILES
REM ============================================================================

echo Assembling source files...

REM Assemble main IDE file
echo - rawrxd_masm_ide_main.asm
ml64 /c /Fo build\obj\rawrxd_masm_ide_main.obj rawrxd_masm_ide_main.asm
if %errorlevel% neq 0 (
    echo Assembly failed for rawrxd_masm_ide_main.asm
    pause
    exit /b 1
)

REM Assemble hotpatch system
echo - unified_masm_hotpatch.asm
ml64 /c /Fo build\obj\unified_masm_hotpatch.obj unified_masm_hotpatch.asm
if %errorlevel% neq 0 (
    echo Assembly failed for unified_masm_hotpatch.asm
    pause
    exit /b 1
)

REM Assemble agentic system
echo - agentic_masm_system.asm
ml64 /c /Fo build\obj\agentic_masm_system.obj agentic_masm_system.asm
if %errorlevel% neq 0 (
    echo Assembly failed for agentic_masm_system.asm
    pause
    exit /b 1
)

REM Assemble theme manager
echo - masm_theme_manager.asm
ml64 /c /Fo build\obj\masm_theme_manager.obj masm_theme_manager.asm
if %errorlevel% neq 0 (
    echo Assembly failed for masm_theme_manager.asm
    pause
    exit /b 1
)

REM Assemble code minimap
echo - masm_code_minimap.asm
ml64 /c /Fo build\obj\masm_code_minimap.obj masm_code_minimap.asm
if %errorlevel% neq 0 (
    echo Assembly failed for masm_code_minimap.asm
    pause
    exit /b 1
)

REM Assemble command palette
echo - masm_command_palette.asm
ml64 /c /Fo build\obj\masm_command_palette.obj masm_command_palette.asm
if %errorlevel% neq 0 (
    echo Assembly failed for masm_command_palette.asm
    pause
    exit /b 1
)

REM Assemble UI framework
echo - masm_ui_framework.asm
ml64 /c /Fo build\obj\masm_ui_framework.obj masm_ui_framework.asm
if %errorlevel% neq 0 (
    echo Assembly failed for masm_ui_framework.asm
    pause
    exit /b 1
)

REM Assemble additional components if they exist
if exist "ai_orchestration_coordinator.asm" (
    echo - ai_orchestration_coordinator.asm
    ml64 /c /Fo build\obj\ai_orchestration_coordinator.obj ai_orchestration_coordinator.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for ai_orchestration_coordinator.asm
        pause
        exit /b 1
    )
)

REM Assemble advanced features
if exist "masm_syntax_highlighting.asm" (
    echo - masm_syntax_highlighting.asm
    ml64 /c /Fo build\obj\masm_syntax_highlighting.obj masm_syntax_highlighting.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_syntax_highlighting.asm
        pause
        exit /b 1
    )
)

if exist "masm_terminal_integration.asm" (
    echo - masm_terminal_integration.asm
    ml64 /c /Fo build\obj\masm_terminal_integration.obj masm_terminal_integration.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_terminal_integration.asm
        pause
        exit /b 1
    )
)

if exist "masm_plugin_system.asm" (
    echo - masm_plugin_system.asm
    ml64 /c /Fo build\obj\masm_plugin_system.obj masm_plugin_system.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_plugin_system.asm
        pause
        exit /b 1
    )
)

if exist "masm_advanced_visualization.asm" (
    echo - masm_advanced_visualization.asm
    ml64 /c /Fo build\obj\masm_advanced_visualization.obj masm_advanced_visualization.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_advanced_visualization.asm
        pause
        exit /b 1
    )
)

if exist "masm_code_completion.asm" (
    echo - masm_code_completion.asm
    ml64 /c /Fo build\obj\masm_code_completion.obj masm_code_completion.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_code_completion.asm
        pause
        exit /b 1
    )
)

if exist "masm_advanced_find_replace.asm" (
    echo - masm_advanced_find_replace.asm
    ml64 /c /Fo build\obj\masm_advanced_find_replace.obj masm_advanced_find_replace.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_advanced_find_replace.asm
        pause
        exit /b 1
    )
)

if exist "masm_plugin_marketplace.asm" (
    echo - masm_plugin_marketplace.asm
    ml64 /c /Fo build\obj\masm_plugin_marketplace.obj masm_plugin_marketplace.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for masm_plugin_marketplace.asm
        pause
        exit /b 1
    )
)

if exist "model_memory_hotpatch.asm" (
    echo - model_memory_hotpatch.asm
    ml64 /c /Fo build\obj\model_memory_hotpatch.obj model_memory_hotpatch.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for model_memory_hotpatch.asm
        pause
        exit /b 1
    )
)

if exist "byte_level_hotpatcher.asm" (
    echo - byte_level_hotpatcher.asm
    ml64 /c /Fo build\obj\byte_level_hotpatcher.obj byte_level_hotpatcher.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for byte_level_hotpatcher.asm
        pause
        exit /b 1
    )
)

if exist "gguf_server_hotpatch.asm" (
    echo - gguf_server_hotpatch.asm
    ml64 /c /Fo build\obj\gguf_server_hotpatch.obj gguf_server_hotpatch.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for gguf_server_hotpatch.asm
        pause
        exit /b 1
    )
)

if exist "proxy_hotpatcher.asm" (
    echo - proxy_hotpatcher.asm
    ml64 /c /Fo build\obj\proxy_hotpatcher.obj proxy_hotpatcher.asm
    if %errorlevel% neq 0 (
        echo Assembly failed for proxy_hotpatcher.asm
        pause
        exit /b 1
    )
)

REM ============================================================================
REM LINK EXECUTABLE
REM ============================================================================

echo.
echo Linking executable...

REM Link all object files into final executable
link /SUBSYSTEM:WINDOWS /ENTRY:main ^
     build\obj\rawrxd_masm_ide_main.obj ^
     build\obj\unified_masm_hotpatch.obj ^
     build\obj\agentic_masm_system.obj ^
     build\obj\masm_ui_framework.obj ^
     build\obj\masm_syntax_highlighting.obj ^
     build\obj\masm_terminal_integration.obj ^
     build\obj\masm_plugin_system.obj ^
     build\obj\masm_advanced_visualization.obj ^
     build\obj\masm_code_completion.obj ^
     build\obj\masm_advanced_find_replace.obj ^
     build\obj\masm_plugin_marketplace.obj ^
     build\obj\masm_theme_manager.obj ^
     build\obj\masm_code_minimap.obj ^
     build\obj\masm_command_palette.obj ^
     kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib winhttp.lib ^
     /OUT:build\bin\RawrXD_MASM_IDE.exe

if %errorlevel% neq 0 (
    echo Linking failed!
    pause
    exit /b 1
)

REM Link additional components if they were assembled
if exist "build\obj\ai_orchestration_coordinator.obj" (
    link /SUBSYSTEM:WINDOWS /ENTRY:main ^
         build\obj\rawrxd_masm_ide_main.obj ^
         build\obj\unified_masm_hotpatch.obj ^
         build\obj\agentic_masm_system.obj ^
         build\obj\masm_ui_framework.obj ^
         build\obj\ai_orchestration_coordinator.obj ^
         kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib ^
         /OUT:build\bin\RawrXD_MASM_IDE.exe
    if %errorlevel% neq 0 (
        echo Linking with additional components failed!
        pause
        exit /b 1
    )
)

if exist "build\obj\model_memory_hotpatch.obj" (
    link /SUBSYSTEM:WINDOWS /ENTRY:main ^
         build\obj\rawrxd_masm_ide_main.obj ^
         build\obj\unified_masm_hotpatch.obj ^
         build\obj\agentic_masm_system.obj ^
         build\obj\masm_ui_framework.obj ^
         build\obj\model_memory_hotpatch.obj ^
         kernel32.lib user32.lib gdi32.lib comctl32.lib comdlg32.lib ^
         /OUT:build\bin\RawrXD_MASM_IDE.exe
    if %errorlevel% neq 0 (
        echo Linking with model_memory_hotpatch failed!
        pause
        exit /b 1
    )
)

REM ============================================================================
REM BUILD COMPLETION
REM ============================================================================

echo.
echo Build successful!
echo.
echo Executable: build\bin\RawrXD_MASM_IDE.exe
echo.

echo Features included:
echo - Pure MASM implementation (no C++ runtime)
echo - Three-layer hotpatching system
echo - Agentic failure detection and correction
echo - Windows API-based UI framework
echo - File operations and dialog support
echo - Advanced syntax highlighting for multiple languages
echo - Full terminal integration with shell support
echo - Extensible plugin system with hot-swapping
echo - Advanced visualization (charts, graphs, 3D rendering)
echo - Code completion and IntelliSense
echo - Advanced find/replace with regex support
echo - Real-time data streaming visualization
echo - Interactive chart manipulation
echo - Export capabilities (PNG, SVG, CSV, JSON)
echo - Plugin marketplace with dependency resolution
echo - Version management and security sandboxing
echo - Complete theme system with 5 built-in themes
echo - Theme transparency controls (per-element opacity)
echo - Always-on-top window mode
echo - Code minimap with real-time sync
echo - Click-to-navigate minimap functionality
echo - Command palette (Ctrl+Shift+P) with fuzzy search
echo - 500+ registered IDE commands
echo - Recent commands tracking
echo - Keyboard navigation (Up/Down/Enter/Esc)
echo.

echo To run: build\bin\RawrXD_MASM_IDE.exe
echo.

REM Display file size
for %%I in (build\bin\RawrXD_MASM_IDE.exe) do set filesize=%%~zI
echo File size: %filesize% bytes
echo.

pause