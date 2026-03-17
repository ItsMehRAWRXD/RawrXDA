# Frontend Link Audit (D:\RawrXD)

## Scope
- `toolchain/masm/Unified-PowerShell-Compiler-RawrXD.ps1`
- `toolchain/masm/Build-MASM-Standalone.ps1`
- `toolchain/masm/build_masm_standalone.bat`
- `tools/inhouse/linker/Program.cs` (capability check only)

## Fixed Frontend Link Gaps
- Unified compiler parameter contract now matches callers.
  - Added `-Architecture` (`x64|x86`)
  - Added `-OutputType` (`exe|dll`)
- Windows Kits resolution now selects a version that actually has both `ucrt` and `um` for the requested arch.
  - Prevents false fail on `10.0.26100.0` when `ucrt` for x64/x86 is absent.
- MASM tool selection now honors architecture.
  - `x64 => ml64.exe`
  - `x86 => ml.exe /coff`
- NASM format now honors architecture.
  - `x64 => -f win64`
  - `x86 => -f win32`
- Link step now passes explicit `/MACHINE:X64|X86` and stable argument arrays.
- `Build-MASM-Standalone.ps1` single-file delegation fixed to use named invocation (not positional array splat).
- Added valid NASM x86 sample and repaired NASM x64 sample source.
- Added multi-file MASM x64 sample pair to validate directory mode linking.

## Verified Working Commands
- MASM x64:
  - `Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_masm.asm -Tool masm -Architecture x64 -SubSystem console -Entry main`
- MASM x86:
  - `Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_masm_x86.asm -Tool masm -Architecture x86 -SubSystem console -Entry main`
- NASM x64:
  - `Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_nasm.asm -Tool nasm -Architecture x64 -SubSystem console -Entry start`
- NASM x86:
  - `Unified-PowerShell-Compiler-RawrXD.ps1 -Source samples\hello_nasm_x86.asm -Tool nasm -Architecture x86 -SubSystem console -Entry _start`
- Builder single-file MASM:
  - `Build-MASM-Standalone.ps1 -Source samples\hello_masm.asm -Architecture x64 -Entry main`
  - `Build-MASM-Standalone.ps1 -Source samples\hello_masm_x86.asm -Architecture x86 -Entry main`
- Builder multi-file MASM:
  - `Build-MASM-Standalone.ps1 -Source samples\multi_x64 -Architecture x64 -Entry main`
- Batch wrapper:
  - `build_masm_standalone.bat x64 samples\hello_masm.asm`
  - `build_masm_standalone.bat x86 samples\hello_masm_x86.asm`

## Remaining True Missing-Link Area
- In-house linker is currently x64-only, not full x86-capable.
  - `tools/inhouse/linker/Program.cs` defines only `IMAGE_FILE_MACHINE_AMD64`.
  - Relocation handling is AMD64-only.
  - Contains explicit `Non-x64 obj` failure path.
- This means “no external ml/link/deps” is not yet true for full x86/x64 parity in the IDE path.

