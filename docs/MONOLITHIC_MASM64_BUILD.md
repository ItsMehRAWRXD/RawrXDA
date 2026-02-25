# Monolithic MASM64 Kernel — Build and Layout

Single EXE, no CRT dependency: all 7 modules assemble with ML64 and link into **build\monolithic\RawrXD.exe**. Beacon/inference/agent in-process; Win32 UI.

---

## Layout: `src/asm/monolithic/`

| Module | File | Role |
|--------|------|------|
| Entry | **main.asm** | WinMain / WinMainCRTStartup, heap init, calls Beacon → Inference → Agent → UIMainLoop |
| Inference | **inference.asm** | InferenceEngineInit (AVX check), RunInference, TokenGenerate; 64KB KV placeholder, align 16 |
| UI | **ui.asm** | UIMainLoop (RegisterClassExW, CreateWindowExW, message loop), WndProc (WM_DESTROY / WM_PAINT), CreateEditorPane (EDIT child). Uses WINMSG / PAINTST; locals fixed for ML64 |
| Beacon | **beacon.asm** | BeaconRouterInit, BeaconSend / BeaconRecv, RegisterAgent; ring buffers via g_hHeap, align 16 |
| LSP | **lsp.asm** | LSPBridgeInit, LSPSendRequest (stubs; no FRAME) |
| Agent | **agent.asm** | AgentCoreInit (RegisterAgent + heap task queue), SpawnTask → BeaconSend; EXTERN BeaconSend/Recv, RunInference, RegisterAgent, g_hHeap |
| Model | **model_loader.asm** | LoadModel (CreateFileW → CreateFileMappingW → MapViewOfFile, GGUF magic), GetTensor, UnloadModel; FRAME only on LoadModel |

---

## Build and link

| Script | Purpose |
|--------|--------|
| **scripts/build_monolithic.ps1** | Finds ml64/link (vswhere or `C:\VS2022Enterprise`), finds Windows Kits `um\x64` for libs, assembles all 7, then calls the link script |
| **scripts/genesis_final_link.ps1** | Links the 7 .obj files with /SUBSYSTEM:WINDOWS, /ENTRY:WinMainCRTStartup, optional /LIBPATH; default obj dir **build\monolithic\obj**, output **build\monolithic\RawrXD.exe** |

---

## Fixes applied (ML64-safe)

- **main / inference / beacon:** Removed FRAME/.endprolog where no unwind was needed.
- **inference / beacon:** `align 4096` in .data? → **align 16** (ML64 limitation).
- **ui:** MSG → **WINMSG**, PAINTSTRUCT → **PAINTST**; locals `msgBuf` / `wndClass` / `paintStruct`; WndProc uses `mov rdx, rsp` for paint struct; return uses `msgBuf.wParam` (full width).
- **agent / model_loader:** In .data?, `dd 0`/`dq 0` → **dd ?** / **dq ?** to clear BSS warnings.
- **Link:** Lib path from Windows Kits (um\x64) when LIB isn’t set; genesis_final_link.ps1 takes **-LibPath** and uses /LIBPATH.

---

## How to build

From repo root (VS or Windows Kits available):

```powershell
.\scripts\build_monolithic.ps1
```

- If **ml64** / **link** aren’t on PATH, use a **Developer Command Prompt** or set PATH to VC Hostx64\x64; the script also tries `C:\VS2022Enterprise` and Windows Kits for libs.

**Result:** `build\monolithic\RawrXD.exe` — single process, one EXE, no CRT, Beacon in-process, Win32 UI.
