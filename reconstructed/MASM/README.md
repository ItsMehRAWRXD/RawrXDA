Unified PowerShell Compiler

- `Build-MASM-IDE.ps1`: builds MASM64 GUI skeleton.
- `Unified-PowerShell-Compiler.ps1`: compiles MASM or NASM sources into .exe using MSVC toolchain and Windows Kits.
- Samples in `samples/` for quick verification.

Usage:

```powershell
pwsh -NoProfile -File E:\masm\Unified-PowerShell-Compiler.ps1 -Source E:\masm\samples\hello_masm.asm -Tool masm -SubSystem console
pwsh -NoProfile -File E:\masm\Unified-PowerShell-Compiler.ps1 -Source E:\masm\samples\hello_nasm.asm -Tool nasm -SubSystem console -Entry start
```
