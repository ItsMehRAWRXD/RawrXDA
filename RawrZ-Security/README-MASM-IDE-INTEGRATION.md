# RawrZ Security – MASM x64 IDE Integration (Camellia)

This folder contains **all RawrZ Security content** integrated with the RawrXD MASM x64 IDE and **Camellia** (cipher + assembly).

## Contents (all available from this repo)

| Path | Description |
|------|-------------|
| **RawrZ-Payload-Builder/** | Full Electron Payload Builder: hash, encrypt, compress, steganography, obfuscation, network tools, all engines |
| **RawrZ-Core/** | Backend/API and test endpoints |
| **RawrZ-Runtimes/** | RawrZ Python/JS/Lua runtimes |
| **RawrZ-Runtimes-Complete/** | Extended runtimes (Python, Perl, PHP, Ruby, Lua, JavaScript) |
| **Camellia-Engine/** | Camellia JS engine + NASM .asm (from RawrZ; used by Payload Builder) |

## Camellia (Camilla) in the MASM IDE

- **Native MASM x64 kernel** (build from IDE or CLI):
  - `d:\rawrxd\Ship\RawrZ_Camellia_MASM_x64.asm`
  - Exports: `RawrZ_Camellia_Init`, `RawrZ_Camellia_EncryptBlock`, `RawrZ_Camellia_DecryptBlock`, `RawrZ_Camellia_EncryptCBC`, `RawrZ_Camellia_DecryptCBC`
- **Build (from repo root or Ship):**
  ```bat
  ml64 /c /Fo RawrZ_Camellia_MASM_x64.obj Ship\RawrZ_Camellia_MASM_x64.asm
  link /DLL /OUT:RawrZ_Camellia_x64.dll RawrZ_Camellia_MASM_x64.obj kernel32.lib
  ```
- **NASM/Camellia JS engine** (original RawrZ): `d:\rawrxd\itsmehrawrxd-master\src\engines\camellia-assembly.asm` and `camellia-assembly.js`.

## Running RawrZ Security from the IDE

1. **Payload Builder (full UI)**  
   From this folder:
   ```powershell
   cd d:\rawrxd\RawrZ-Security\RawrZ-Payload-Builder
   npm install
   npm run dev
   ```

2. **MASM CLI (IDE terminal)**  
   In the IDE’s MASM CLI pane:
   - `asm Ship\RawrZ_Camellia_MASM_x64.asm` to assemble the Camellia kernel.
   - Use the same CLI for other Ship/kernels.

3. **Launcher script**  
   Run from repo root:
   ```powershell
   .\RawrZ-Security\run_rawrz_from_ide.ps1
   ```
   This starts the RawrZ Payload Builder so all security content is available while you work in the MASM IDE.

## Engine list (Payload Builder)

All engines under `RawrZ-Payload-Builder\src\engines\` are loaded by the Payload Builder, including:

- **security.engine.js** – Security & Auth (tokens, permissions, audit)
- **real-encryption-engine.js** – AES-256-GCM encrypt/decrypt
- **stealth-engine.js**, **polymorphic-engine.js**, **evasion-engine.js**
- **stub-generator.js**, **template-generator.js**, **unified-compiler-engine.js**
- **full-assembly.js** – Assembly generation
- **network-engine.js**, **sysinfo.engine.js**, **payload-manager.js**
- Plus hashes, compression, binary analysis, and the rest of the RawrZ Security feature set.

## Reverse engineer / kernel workflow

1. Open the MASM x64 IDE (RawrXD Titan IDE or Win32 IDE with MASM CLI).
2. Assemble `Ship\RawrZ_Camellia_MASM_x64.asm` for native Camellia in x64.
3. Run `run_rawrz_from_ide.ps1` to bring up the full RawrZ Security stack (Payload Builder + engines).
4. Use the same workspace for both MASM kernels and RawrZ Security tooling (Camellia, crypto, stego, obfuscation, etc.).

All RawrZ Security content lives under `d:\rawrxd\RawrZ-Security` and is used with the MASM x64 IDE and Camellia as above.
