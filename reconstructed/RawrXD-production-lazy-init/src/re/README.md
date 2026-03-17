# RawrXD IDE - Reverse Engineering Subsystem

## Overview
This subsystem provides binary analysis, disassembly, decompilation, and dynamic analysis capabilities. It is designed to be modular, scriptable, and agent-ready.

## Components
- **BinaryLoader**: Loads PE/ELF/Mach-O binaries, extracts sections, symbols, and metadata.
- **Disassembler**: Decodes instructions for x86/x64/ARM architectures, produces annotated assembly.
- **Decompiler**: Converts assembly to pseudo-C, recovers structs and control flow.
- **DynamicAnalyzer**: Attaches to running processes, inspects memory, stack, heap, and traces execution.
- **REFeatureFlags**: All RE features are gated behind `RAWRXD_RE_ENABLED` (default OFF).
- **UI Integration**: Adds RE panes to the IDE, supports annotation, reporting, and scripting.

## Roadmap
- v1: Static analysis (binary load, disasm, decomp, UI)
- v2: Dynamic analysis (process attach, live memory, execution trace)
- v3: Agentic RE (autonomous symbol renaming, hypothesis, causal graph)

---
**Status:** Foundation in progress
