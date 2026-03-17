# INCLUDE/RAWRXD Folder Audit - January 23, 2026

## Overview
This subfolder contains RawrXD-specific headers for advanced features and integrations, including nano-slice management, ROCm HMM, and Tencent compression.

## Key Components
- **NanoSliceManager.hpp**: Header for nano-slice memory management and optimization.
- **ROCmHMM.hpp**: (Legacy) ROCm Heterogeneous Memory Management header, retained for compatibility or reference only. All ROCm dependencies should be removed or stubbed.
- **TencentCompression.hpp**: Header for Tencent compression integration and support.

## Status
- RawrXD-specific headers are present and up to date.
- No external dependencies (e.g., Vulkan/ROCm) remain in this subfolder, except for legacy stubs.

## TODO
- Ensure all legacy ROCm code is stubbed or removed.
- Validate header usage in both CLI and GUI builds.
- Confirm all advanced features are documented and robust.
