Reverse Engineering C++ → MASM x64

Overview
- reveng-cpp-to-masm.ps1: Invokes MSVC (cl.exe) to emit MASM-style assembly listings (/FAcs), then generates MASM skeletons per function and a demangled names map.
- masm_header_gen.asm: Pure MASM tool to scan .asm files and emit a consolidated .inc with EXTERNDEF/PROTO lines (WIP; builds under src/masm solution).

Prereqs
- Visual Studio 2022 Build Tools with C++ (v143). Ensure VsDevCmd.bat is available.
- Windows x64.

Quick Start
- Generate listing + skeleton from a C++ file:
```
# Windows PowerShell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reveng-cpp-to-masm.ps1 -Input tools/samples/test_reveng.cpp -OutDir bin/reveng-out
```
- Optional include/defines (semicolon-separated):
```
powershell -NoProfile -ExecutionPolicy Bypass -File tools/reveng-cpp-to-masm.ps1 -Input src/qtapp/inference_engine.cpp -OutDir bin/reveng-out -Include "include;src;C:/Qt/6.7.3/msvc2022_64/include" -Defines "WIN32;_WINDOWS;NDEBUG"
```

Outputs
- <file>.asm: MASM-style assembly listing produced by cl.exe
- <file>.skeleton.asm: MASM PROC stubs for each function symbol
- <file>.names.json: Decorated → demangled name map (uses undname.exe if available)
- <file>.cl.log / <file>.cl.err: Compiler logs

Header Generation (WIP)
- Build the pure MASM header generator target:
```
cmake --build . --config Release --target masm_header_gen
```
- Run to emit EXTERNDEF/PROTO for existing .asm files:
```
bin/Release/masm_header_gen.exe src/masm/final-ide bin/masm-externs.inc
```

Notes
- The listing-to-skeleton path is best-effort and emits minimal PROCs with `ret` and a TODO. Port bodies manually or paste from listing.
- For complex Qt sources, pass `-Include` with Qt include roots to allow cl.exe to compile and emit listings.
- If cl.exe is not found, run in a "Developer Command Prompt for VS 2022" or edit the script to hardcode VsDevCmd.bat.
