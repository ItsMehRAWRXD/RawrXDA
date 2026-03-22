# Disassembly provider abstraction (E11)

**Done when:** UI depends on **`IDisasmBackend`** (name illustrative); swap **Capstone-shaped** vs **existing** backend without rewriting the panel.

## Interface sketch (language-agnostic)

- `openArtifact(path) -> expected<void, Error>`
- `listSymbols() -> vector<Symbol>`
- `xrefsTo(va|symbol) -> vector<Xref>`
- `disasmRange(rva, len) -> lines`

## Backends

- **Stub:** fixed sample table for UI wiring.
- **Future:** Capstone / LLVM-MC / vendor SDK — behind the same interface.

## Editor lane

- Monaco (Electron) or native editor: **read-only** disasm buffer fed by provider; no execution.
