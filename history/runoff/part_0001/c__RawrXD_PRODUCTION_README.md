# RawrXD Production Toolchain v1.0.0

This folder contains the production-ready components for the RawrXD IDE, automatically wired from the D:\RawrXD-Compilers source.

## 📂 Structure

- **Headers/**: C/C++ headers for integration.
  - `RawrXD_PE_Generator.h`: Advanced PE generation & encoding API.
  - `instruction_encoder.h`: x64 instruction encoding engine.
- **Libraries/**: Static libraries (.lib) for linking.
  - `rawrxd_pe_gen_adv.lib`: Advanced PE Generator with AES/ChaCha20/Polymorphic support.
  - `rawrxd_pe_gen.lib`: Lightweight "PROD" PE generator.
  - `rawrxd_encoder.lib`: Combined x64/instruction encoder.
- **pe_generator.exe**: Standalone PE generation utility.

## 🚀 Quick Start for IDE Integration

To generate a PE from your IDE:

```cpp
#include "Headers/RawrXD_PE_Generator.h"

// Initialize context
PE_GEN_CONTEXT ctx;
PeGenInitialize(&ctx, 0x100000, false);

// ... build your PE ...

// Save to disk
PeGenWriteToFile(&ctx, "output.exe");
PeGenCleanup(&ctx);
```

## 🛠️ Maintenance

To rebuild the toolchain from source, run:
`pwsh -File D:\RawrXD-Compilers\Build-And-Wire.ps1`

---
*Built with ❤️ for RawrXD*
