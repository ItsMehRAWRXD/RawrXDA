# RawrXD IDE - Integrated Environment Instructions

## Overview
The RawrXD IDE has been fully scaffolded and the backend logic implemented. The system now consists of:
1. **Frontend**: A React-based Single Page Application (SPA) with Monaco Editor, Memory Viewer, and Tool Panels.
2. **Backend**: A C++ REST API Bridge (`react_server`) running inside the RawrXD runtime.
3. **Core**: Native implementations of Inference (AVX-512), Process Memory Management, and Assembler invocation.

## Setup & Launch

### 1. Generate the Frontend
From the `rawrxd>` shell, run:
```bash
!generate_ide my_workspace
```
This will create a `my_workspace` directory containing the full Node.js/React source code.

### 2. Install Dependencies
Open a terminal in the generated folder:
```powershell
cd my_workspace
npm install
```

### 3. Start the Backend
Back in the `rawrxd>` shell, start the API bridge:
```bash
!server 8080
```
*Note: Keep the shell open. The server runs in a background thread.*

### 4. Run the Client
In your terminal:
```powershell
npm start
```
The IDE will launch at `http://localhost:3000`.

## Features

### 🧠 Inference Engine
- **Panel**: Engine Manager
- **Function**: Switch between `Sovereign-800B` (Simulated Pro), `Sovereign-Small` (Fast), and `RawrXD-AVX512` (CPU Native).
- **Backend**: `src/modules/inference_engine.cpp` & `src/engine/sovereign_engines.cpp`.

### 💾 Memory Viewer
- **Panel**: Memory Viewer
- **Function**: See real-time token usage of the internal Ring Buffer.
- **Backend**: `src/memory_core.cpp` (ContextTier logic).

### 🛠 Reverse Engineering Tools
- **Panel**: Tool Output
- **Compiler**: Writes ASM to temp file -> Runs `ml64.exe` -> Returns output.
- **Hotpatch**: Patches process memory in real-time provided a hex address and opcodes.
- **Backend**: `src/re_tools.cpp` & `src/hot_patcher.cpp`.

## Architecture Note
The frontend communicates entirely via JSON/HTTP to `localhost:8080/api/*`. The C++ backend parses these requests and invokes native functions directly.
