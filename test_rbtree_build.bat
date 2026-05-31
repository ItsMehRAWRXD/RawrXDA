@echo off
REM test_rbtree_build.bat — Build and run RB-tree unit test

call "C:\VS2022Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64
if errorlevel 1 (
    echo [RawrXD] FAILED: vcvarsall.bat
    exit /b 1
)

cd /d D:\rawrxd

set LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\um\x64;C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;%LIB%

"C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\cl.exe" /EHsc /Zi /I. test_rbtree.cpp build-ninja\Debug\RawrXD_RBTree.obj /Fe:test_rbtree.exe
if errorlevel 1 (
    echo [RawrXD] FAILED: Compilation error
    exit /b 1
)

echo [RawrXD] Running unit tests...
test_rbtree.exe
