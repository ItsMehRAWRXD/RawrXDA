# Pure MASM Compiler Project

A zero-dependency, pure MASM compiler implementation that provides full compiler support for multiple programming languages without external dependencies.

## Features

- Pure MASM implementation (no external dependencies)
- Support for 50+ programming languages
- PE header generation and executable creation
- Integration with RawrXD IDE/CLI
- Cross-language compilation support

## Project Structure

```
Pure-MASM-Compiler-Project/
├── src/
│   ├── pe_defs.inc          # PE header definitions
│   ├── core_runtime.asm     # CRT replacement in pure ASM
│   ├── compiler_main.asm    # Universal compiler driver
│   └── language_tables.inc  # Language support definitions
├── build.bat                # Build script
├── test_programs/           # Test programs for each language
└── integration/            # Integration with RawrXD IDE/CLI
```

## Building

Run `build.bat` to compile the MASM compiler:

```batch
build.bat
```

## Integration with RawrXD

This compiler integrates seamlessly with the RawrXD IDE/CLI system, providing:
- Language detection and compilation
- Build/run templates for each supported language
- Agentic swarm integration
- GGUF model loading support

## Supported Languages

See `src/language_tables.inc` for the complete list of supported programming languages.