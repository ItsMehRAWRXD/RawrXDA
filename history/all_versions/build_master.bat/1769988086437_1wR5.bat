@echo off
REM RawrXD Complete Build System
REM Builds: Inference Core, GUI IDE, CLI, Deobfuscator

echo ============================================
echo  RawrXD Complete Build System
echo ============================================
echo.

set ML64=ml64.exe
set LINK=link.exe
set OUTDIR=build

if not exist %OUTDIR% mkdir %OUTDIR%

if "%1"=="all" goto build_all
if "%1"=="inference" goto build_inference
if "%1"=="gui" goto build_gui
if "%1"=="cli" goto build_cli
if "%1"=="deobf" goto build_deobf
if "%1"=="clean" goto clean

echo Usage: build.bat [all^|inference^|gui^|cli^|deobf^|clean]
goto done

:build_all
echo [Building All Components]
goto build_inference

:build_inference
echo.
echo [1/4] Building Inference Core...
%ML64% /c /Fo%OUTDIR%\InferenceCore.obj /W3 /nologo src\masm\RawrXD_InferenceCore.asm
if errorlevel 1 goto error
%LINK% /DLL /OUT:%OUTDIR%\RawrXD_Inference.dll /NOLOGO %OUTDIR%\InferenceCore.obj kernel32.lib
if errorlevel 1 goto error
echo     OK: RawrXD_Inference.dll
if not "%1"=="all" goto done
goto build_gui

:build_gui
echo.
echo [2/4] Building GUI IDE...
%ML64% /c /Fo%OUTDIR%\GUI_IDE.obj /W3 /nologo src\masm\RawrXD_GUI_IDE.asm
if errorlevel 1 goto error
%LINK% /SUBSYSTEM:WINDOWS /OUT:%OUTDIR%\RawrXD_IDE.exe /NOLOGO %OUTDIR%\GUI_IDE.obj kernel32.lib user32.lib gdi32.lib comdlg32.lib comctl32.lib shell32.lib
if errorlevel 1 goto error
echo     OK: RawrXD_IDE.exe
if not "%1"=="all" goto done
goto build_cli

:build_cli
echo.
echo [3/4] Building CLI...
%ML64% /c /Fo%OUTDIR%\CLI.obj /W3 /nologo src\masm\RawrXD_CLI.asm
if errorlevel 1 goto error
%LINK% /SUBSYSTEM:CONSOLE /OUT:%OUTDIR%\RawrXD_CLI.exe /NOLOGO %OUTDIR%\CLI.obj kernel32.lib
if errorlevel 1 goto error
echo     OK: RawrXD_CLI.exe
if not "%1"=="all" goto done
goto build_deobf

:build_deobf
echo.
echo [4/4] Building Deobfuscator...
%ML64% /c /Fo%OUTDIR%\OmegaDeobf.obj /W3 /nologo RawrXD_OmegaDeobfuscator.asm
if errorlevel 1 goto error
%ML64% /c /Fo%OUTDIR%\MetaReverse.obj /W3 /nologo RawrXD_MetaReverse.asm
if errorlevel 1 goto error
%LINK% /DLL /OUT:%OUTDIR%\RawrXD_Deobfuscator.dll /NOLOGO %OUTDIR%\OmegaDeobf.obj %OUTDIR%\MetaReverse.obj kernel32.lib
if errorlevel 1 goto error
echo     OK: RawrXD_Deobfuscator.dll
if not "%1"=="all" goto done
goto build_done

:build_done
echo.
echo ============================================
echo  BUILD COMPLETE
echo ============================================
echo.
echo Output files in %OUTDIR%\:
echo   - RawrXD_Inference.dll    (GGUF/LLM inference engine)
echo   - RawrXD_IDE.exe          (GUI IDE - Win32 native)
echo   - RawrXD_CLI.exe          (Command-line IDE)
echo   - RawrXD_Deobfuscator.dll (Anti-obfuscation engine)
echo.
echo Performance Targets:
echo   - 8,500+ tokens/second
echo   - ^<800 microseconds latency
echo   - 3-5x faster than GitHub Copilot
echo   - 2-3x faster than Cursor
echo.
goto done

:clean
echo Cleaning build directory...
if exist %OUTDIR% rmdir /S /Q %OUTDIR%
echo Done.
goto done

:error
echo.
echo ============================================
echo  BUILD FAILED
echo ============================================
exit /b 1

:done
