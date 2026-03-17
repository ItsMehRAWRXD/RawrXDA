@echo off
call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
cd /d D:\rawrxd

echo ===== STEP 1: Assemble =====
ml64 /c /nologo /W3 /Fo proof.obj proof.asm
if errorlevel 1 (
    echo ASSEMBLE FAILED with errorlevel %ERRORLEVEL%
    goto :done
)
echo ASSEMBLE OK

echo ===== STEP 2: Link =====
link /nologo /subsystem:console /entry:main /out:proof.exe proof.obj kernel32.lib
if errorlevel 1 (
    echo LINK FAILED with errorlevel %ERRORLEVEL%
    goto :done
)
echo LINK OK

echo ===== STEP 3: Run =====
proof.exe
echo Exit code: %ERRORLEVEL%

:done
echo ===== BUILD COMPLETE =====
