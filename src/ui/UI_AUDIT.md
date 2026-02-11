# UI_AUDIT.md

## Folder: `src/ui/`

### Summary
This folder contains user interface logic for the IDE/CLI, including dialogs, panels, widgets, and integration with model loading and inference. The code is implemented in C++/Qt and relies on Qt for GUI components. Some routines are stubs or placeholders for future MASM migration, but Qt dependencies will be kept for GUI functionality.

### Key Files Audited
- Various dialogs, panels, and widgets (e.g., `chat_panel.cpp`, `chromatic_window.cpp`, `diff_dock.cpp`, `InferenceSettingsDialog.cpp`, `tokenizer_selector.cpp`).
- `UI_FOLDER_AUDIT.md`: Existing audit file (to be updated with MASM migration plan).

### External Dependencies
- Qt (for GUI components, signals, slots)
- C++ standard library (memory, threading, chrono)
- Model loader and inference engine logic

### Stub/Placeholder Routines
- Some UI routines may be stubbed or simplified for unsupported features.
- Integration with MASM routines may be incomplete or placeholder.

### MASM Migration Targets
- All backend logic (model loading, inference, compression) to be ported to MASM and integrated with UI.
- Qt dependencies will be kept for GUI functionality.
- Refactor UI logic for MASM64 compatibility where needed.

### Implementation Status
- [x] MASM backend routines (tensor ops, quantization, compression) ready for integration
- [x] C++ bridge headers (ggml_masm_bridge.h, net_masm_bridge.h) available for UI integration
- [x] Backend integration files (ggml_masm_backend.cpp, net_backend.cpp) created
- [x] Regression tests created for MASM routines
- [x] Audit documentation updated

### Next Steps
- [ ] Integrate MASM backend routines with UI components (InferenceSettingsDialog, chat_panel, etc.)
- [ ] Refactor UI logic to call MASM routines for model loading, inference, and compression
- [ ] Test MASM-integrated UI workflows (model download, inference, chat)
- [ ] Optimize UI responsiveness for MASM backend operations
- [ ] Document UI-MASM integration and user-facing features
