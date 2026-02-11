# MMF Diagnostic & Sidecar Notes

## Diagnostic Tool
Build and run the MMF diagnostic tool:

```powershell
cmake --build D:\rawrxd\build --config Release --target mmf_diagnostic
D:\rawrxd\build\tools\Release\mmf_diagnostic.exe
```

## MASM Clean Sidecar
The clean MASM sidecar is in:

```
D:\rawrxd\src\direct_io\nvme_thermal_sidecar_clean.asm
```

You can build it manually:

```powershell
& "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\ml64.exe" /c D:\rawrxd\src\direct_io\nvme_thermal_sidecar_clean.asm /Fo D:\rawrxd\build\nvme_thermal_sidecar_clean.obj
& "C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\link.exe" /subsystem:console /entry:SidecarMain D:\rawrxd\build\nvme_thermal_sidecar_clean.obj kernel32.lib /OUT:D:\rawrxd\build\nvme_thermal_sidecar_clean.exe
```

Then run:

```powershell
D:\rawrxd\build\nvme_thermal_sidecar_clean.exe
D:\rawrxd\build\tests\Release\nvme_thermal_reader.exe
```

## Key Finding
- `Global\` namespace **fails** (Access Denied).
- `Local\` and bare names **work**.

So the MASM sidecar must use `Local\SOVEREIGN_NVME_TEMPS` for non-SYSTEM execution.
