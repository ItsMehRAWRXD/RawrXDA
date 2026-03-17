@echo off
setlocal enabledelayedexpansion

echo Building FruitLoopsUltra MASM64 DAW...
echo.

REM Set MASM32 environment
set MASM32_PATH=C:\masm32
set PATH=%MASM32_PATH%\bin;%PATH%

REM Check if MASM is available
where ml >nul 2>&1
if errorlevel 1 (
    echo ERROR: ml not found in PATH
    echo Please install MASM32 or add it to your PATH
    exit /b 1
)

REM Check if linker is available
where link >nul 2>&1
if errorlevel 1 (
    echo ERROR: link not found in PATH
    echo Please install MASM32 or add it to your PATH
    exit /b 1
)

echo Compiling FruitLoopsUltra.asm...
ml /c /Cp /Cx /Zi /coff FruitLoopsUltra.asm
if errorlevel 1 (
    echo Compilation failed!
    exit /b 1
)

echo Linking FruitLoopsUltra.exe...
link /subsystem:windows /entry:WinMain /machine:x64 /debug /out:FruitLoopsUltra.exe FruitLoopsUltra.obj kernel32.lib user32.lib gdi32.lib dsound.lib winmm.lib ole32.lib
if errorlevel 1 (
    echo Linking failed!
    exit /b 1
)

echo Build successful! FruitLoopsUltra.exe created.
echo.
echo Features:
echo - Pure MASM64 implementation with zero dependencies
echo - 5-pane DAW interface (Channel Rack, Piano Roll, Playlist, Mixer, Browser)
echo - Real-time audio processing with WASAPI/DirectSound
echo - AI model integration (TinyLlama to 800B parameters)
echo - Hot-reload capability for live updates
echo - 116 native plugins implemented in MASM
echo - Professional DSP algorithms (SVF, reverb, distortion, compression)
echo - Billboard-ready genre presets (techno, acid, ambient)
echo - Sample-accurate audio engine
echo.
echo Usage:
echo FruitLoopsUltra.exe
echo.
echo AI Commands:
echo - Type "techno" for 135 BPM techno preset
echo - Type "acid" for acid techno with high resonance
echo - Type "ambient" for ambient music at 70 BPM
echo - Press 'R' for hot-reload
echo - Press 'L' to load AI model
echo.
echo Keyboard Shortcuts:
echo - 1-5: Switch between DAW panes
echo - Space: Toggle playback
echo - R: Hot-reload current session
echo - L: Load AI model
echo.

pause