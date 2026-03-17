$env:PATH += ";C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64";
$env:INCLUDE += ";D:\rawrxd\include;C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\ucrt;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\um;C:\Program Files (x86)\Windows Kits\10\Include\10.0.26100.0\shared;C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\include";
$env:LIB += ";C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\lib\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.26100.0\um\x64;C:\Program Files (x86)\Windows Kits\10\Lib\10.0.22621.0\ucrt\x64";

# Clean up locked file
Rename-Item D:\rawrxd\build\bin\RawrXD_Native_Core.dll D:\rawrxd\build\bin\RawrXD_Native_Core.dll.old -Force -ErrorAction SilentlyContinue

# Assemble
ml64 /c /FoD:\rawrxd\src\RawrXD_Native_Core.obj D:\rawrxd\src\RawrXD_Native_Core.asm;
ml64 /c /FoD:\rawrxd\src\RawrXD_Native_UI.obj D:\rawrxd\src\RawrXD_Native_UI.asm;
ml64 /c /FoD:\rawrxd\src\RawrXD_Titan_Kernel.obj D:\rawrxd\src\RawrXD_Titan_Kernel.asm;
ml64 /c /FoD:\rawrxd\src\RawrXD_SwarmLink.obj D:\rawrxd\src\RawrXD_SwarmLink.asm;
ml64 /c /FoD:\rawrxd\src\RawrXD_CDB_Harden.obj D:\rawrxd\src\RawrXD_CDB_Harden.asm;

# Link
cl /std:c++20 /LD /FeD:\rawrxd\build\bin\RawrXD_Native_Core.dll D:\rawrxd\src\ui\RawrXD_UI_Bridge.cpp D:\rawrxd\src\RawrXD_Prometheus_Export.cpp D:\rawrxd\src\RawrXD_Native_Core.obj D:\rawrxd\src\RawrXD_Native_UI.obj D:\rawrxd\src\RawrXD_Titan_Kernel.obj D:\rawrxd\src\RawrXD_SwarmLink.obj D:\rawrxd\src\RawrXD_CDB_Harden.obj /link /EXPORT:Core_InitializeUI /EXPORT:Core_RunMessageLoop /EXPORT:Core_RunMessageLoopAsync /EXPORT:Core_StartAgenticExplorer /EXPORT:Core_LoadFileIntoEditor /EXPORT:Core_LoadLocalModel /EXPORT:Core_GetModelPointer /EXPORT:Core_GetModelSize /EXPORT:Core_ExportPrometheusMetrics /DEFAULTLIB:user32.lib /DEFAULTLIB:kernel32.lib /DEFAULTLIB:comctl32.lib /DEFAULTLIB:gdi32.lib /NODEFAULTLIB:libcmt.lib /DEFAULTLIB:msvcrt.lib

Write-Host "[SOVEREIGN] v22.4.0-HYBRID MONOLITH READY." -ForegroundColor Green
