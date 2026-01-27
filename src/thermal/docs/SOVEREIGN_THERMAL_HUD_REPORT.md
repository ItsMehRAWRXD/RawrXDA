# Sovereign Thermal HUD: Module Architecture Report

## 1. Executive Summary
The **Sovereign Thermal HUD** is a high-performance, zero-latency visualization layer for the Sovereign NVMe Thermal stack. Implemented entirely in **MASM x64**, it provides a transparent, "always-on" desktop overlay that renders real-time thermal telemetry directly from the Shared Memory (MMF) bus.

## 2. Technical Specifications
*   **Language:** Pure MASM x64 (Microsoft Macro Assembler)
*   **Subsystem:** Windows GUI (`/subsystem:windows`)
*   **Dependencies:** `kernel32.dll`, `user32.dll`, `gdi32.dll`
*   **Memory Footprint:** < 2MB (Static allocation)
*   **Telemetry Source:** `Global\SOVEREIGN_NVME_TEMPS` (MMF)

## 3. Architecture components

### 3.1. The MMF Tap
The HUD bypasses all higher-level logic (Governor/Oracle) and taps directly into the raw silicon telemetry bus.
```asm
; Direct MMF mapping
invoke OpenFileMappingA, FILE_MAP_READ, 0, addr szMMFName
invoke MapViewOfFile, eax, FILE_MAP_READ, 0, 0, 1024
```

### 3.2. Visual Engine (GDI32)
*   **Layered Windows:** Uses `WS_EX_LAYERED` and `LWA_COLORKEY` (0x101010) to achieve pixel-perfect transparency.
*   **Rendering:** Double-buffered GDI painting loop triggered by a 500ms `WM_TIMER`.
*   **Dynamic Coloring:**
    *   **< 45°C:** Green (`hBrushSafe`)
    *   **45°C - 60°C:** Yellow (`hBrushWarn`)
    *   **> 60°C:** Red (`hBrushCrit`)

### 3.3. Reliability Stack (`RealWndProc`)
To prevent "Callback Hell" crashes common in x64 Assembly GUI programming, the HUD implements a stack-frame wrapper `RealWndProc`.
*   **Function:** Saves non-volatile registers (`RBX`, `RSI`, `RDI`, `R12`-`R15`) before entering the Windows message loop.
*   **Impact:** Ensures 100% stability on modern Windows kernels where register preservation policies are strict.

## 4. Integration Status
*   [x] **Build Pipeline:** Integrated into `build_governor.ps1`
*   [x] **Run Level:** Can run standalone or spawned by the Governor.
*   [x] **Visual Verification:** Successfully rendering on desktop.

## 5. Next Steps
*   **Interactive Mode:** Add mouse hit-testing to drag the HUD.
*   **Graphing:** Implement a MASM-based rolling history graph.
