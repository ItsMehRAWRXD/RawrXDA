@echo off
set "VC_VARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64"
set "WIN_LIB=C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64"
cd /d d:\rawrxd
"%VC_VARS%\ml64.exe" /c /Zi d:\rawrxd\src\ui\RawrXD_Native_UI.asm
"%VC_VARS%\cl.exe" /c /EHsc d:\rawrxd\src\ui\RawrXD_UI_Bridge.cpp d:\rawrxd\test_ui_launcher.cpp /I"d:\rawrxd\src\core" /I"d:\rawrxd\src\ui"
"%VC_VARS%\link.exe" /NOLOGO /SUBSYSTEM:WINDOWS /ENTRY:WinMainCRTStartup RawrXD_Native_UI.obj RawrXD_UI_Bridge.obj test_ui_launcher.obj /LIBPATH:"%WIN_LIB%" kernel32.lib user32.lib gdi32.lib comctl32.lib /OUT:d:\rawrxd\bin\test_native_ui.exe
