# Ship audit (available vs missing)

Date: 2026-02-16/17
Location audited: `D:\rawrxd\Ship`

## 1) What is actually in `Ship` (top-level)

### Top-level folders
- `__pycache__`
- `build`
- `build_complete`
- `build_ide`
- `include`
- `logs`
- `src`
- `tools`
- `webview2`

### Top-level EXEs present
| EXE | Size (bytes) |
|---|---:|
| `RawrXD_Agent.exe` | 621,568 |
| `RawrXD_CLI.exe` | 288,256 |
| `RawrXD_FoundationTest.exe` | 14,848 |
| `RawrXD_IDE.exe` | 168,448 |
| `RawrXD_IDE_Production.exe` | 168,448 |
| `RawrXD_IDE_Ship.exe` | 377,856 |
| `RawrXD_Win32_IDE.exe` | 2,806,287 |
| `RawrXD.exe` | 331,776 |
| `RawrXD-Agent.exe` | 4,096 |
| `RawrXD-Titan.exe` | 3,072 |
| `test_dll.exe` | 136,704 |
| `test_file_operations.exe` | 304,128 |
| `test_inference_engine.exe` | 145,408 |
| `test_integration.exe` | 143,872 |
| `test_suite.exe` | 362,496 |

### Top-level DLLs present
(Subset shown here is the full top-level list captured during audit)
| DLL | Size (bytes) |
|---|---:|
| `RawrXD_AICompletion.dll` | 141,312 |
| `RawrXD_Core.dll` | 141,312 |
| `RawrXD_FileBrowser.dll` | 140,800 |
| `RawrXD_FileManager_Win32.dll` | 112,128 |
| `RawrXD_FileOperations.dll` | 132,096 |
| `RawrXD_InferenceEngine.dll` | 116,224 |
| `RawrXD_InferenceEngine_Win32.dll` | 128,000 |
| `RawrXD_LSPClient.dll` | 151,552 |
| `RawrXD_ModelLoader.dll` | 154,112 |
| `RawrXD_ModelRouter.dll` | 150,016 |
| `RawrXD_TerminalMgr.dll` | 139,264 |
| `RawrXD_TerminalManager_Win32.dll` | 122,880 |
| `RawrXD_TextEditor_Win32.dll` | 131,584 |

(There are many more `RawrXD_*.dll` files in the top level; see `dir *.dll` output in the terminal history if you need the complete list.)

## 2) Model files shipped

Searched recursively for: `*.gguf`, `*.onnx`, `*.safetensors`, `*.bin`, `*.pt`

Result:
- **No actual model files are shipped in `Ship`.**
- The only match found was a build artifact: `build\\CMakeFiles\\4.2.0\\CMakeDetermineCompilerABI_CXX.bin` (this is **not** a language model).

This alone explains why “Load GGUF model” has nothing *in the Ship folder itself* to load by default.

## 3) What the Win32 IDE *tries* to load at runtime (and what’s missing)

The file `RawrXD_Win32_IDE.cpp` explicitly calls `LoadLibraryW` for these DLL names:

| Expected DLL name | Present in `Ship`? | Notes |
|---|---:|---|
| `RawrXD_Titan_Kernel.dll` | **No** | The IDE will report Titan kernel missing/unavailable.
| `RawrXD_NativeModelBridge.dll` | **No** | The IDE will report Native Model Bridge missing/unavailable.
| `RawrXD_InferenceEngine.dll` | Yes | Present.
| `RawrXD_InferenceEngine_Win32.dll` | Yes | Present.

So if you see messages like:
- “Titan Kernel not found (AI features limited).”
- “Native Model Bridge not found.”

…that aligns with the actual filesystem state: those two DLLs are not in `Ship`.

## 4) Explorer + Terminal: implemented vs placeholder (based on `RawrXD_Win32_IDE.cpp`)

### File explorer
- There *is* a real Win32 TreeView (`IDC_FILETREE`).
- The source contains `PopulateFileTree(...)` and a recursive enumerator using `FindFirstFileW`.
- The source also calls `PopulateFileTree(GetExeDirectory())` during initialization (so it will try to show the EXE folder contents).

If you’re not seeing it, that’s a UI/state bug (visibility/layout) rather than “no code exists”.

### Terminal
- The source contains `StartTerminal()` that launches `powershell.exe -NoLogo -NoProfile` using `CreateProcessW` and pipes.
- It reads stdout in a background thread and posts `WM_TERMINAL_OUTPUT` back to the UI.

If the terminal is “all black” / input text is invisible, that’s likely formatting/default RichEdit charformat (UI bug), not proof that there is no terminal process code.

## 5) Things that are *claimed* in comments/UI text but not present as shipped binaries

Inside `RawrXD_Win32_IDE.cpp` there’s a banner/comment line listing items such as:
- `RawrXD_Titan_Kernel.dll` (missing)
- `RawrXD_NativeModelBridge.dll` (missing)
- `RawrXD_AgenticIDE.exe` (not present as a top-level EXE in `Ship`)

Treat that banner as documentation/aspiration; the actual shipped binaries list in section (1) is the ground truth.
