# MASM Folder Audit - January 23, 2026

## Overview
This folder contains all MASM (x64 assembly) code, hotpatch systems, orchestration, and integration logic for the RawrXD IDE. It is central to the agentic, hotpatch, and performance-critical features of both CLI and GUI.

## Key Components
- **agentic_kernel.asm, agentic_tools.asm, agentic_inference_stream.asm**: Core agentic and inference logic in MASM.
- **masm_hotpatch_*.asm, unified_hotpatch_manager.asm**: Hotpatch and runtime patching systems.
- **masm_orchestration_*.asm, orchestration/bridge files**: Orchestration and bridge logic for MASM integration.
- **build_*.bat, CMakeLists.txt**: Build scripts and configuration for MASM modules.
- **README_*.md, VERIFICATION_CHECKLIST.md**: Documentation and verification for MASM features.

## Status
- MASM agentic, hotpatch, and orchestration features are present and integrated.
- Build scripts and CMake configuration exist for MASM modules.
- No external dependencies (e.g., Vulkan/ROCm) remain in this folder.

## TODO
- Ensure all MASM features are accessible from both CLI and GUI.
- Validate hotpatch and orchestration logic for new/changed IDE features.
- Update documentation and verification checklists as features evolve.
