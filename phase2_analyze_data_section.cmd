@echo off
REM ============================================================================
REM PHASE 2 ANALYSIS: Identify .data Section Contents for Runtime Migration
REM ============================================================================
REM
REM This script analyzes the RawrXD binaries to identify what's in the .data
REM section (the 15.9 MB static data bloat) and maps them to source code so
REM they can be migrated to runtime allocation (VirtualAlloc, MapViewOfFile).
REM
REM Expected Result: Identifies candidates for the 15.9 MB .data reduction
REM
REM ============================================================================

setlocal enabledelayedexpansion

set "ROOT=D:\rawrxd"
set "DUMPBIN=C:\VS2022Enterprise\VC\Tools\MSVC\14.50.35717\bin\Hostx64\x64\dumpbin.exe"

echo.
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║ PHASE 2 ANALYSIS: .data SECTION FORENSICS                        ║
echo ║ Target: Identify 15.9 MB static arrays for runtime migration     ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.

cd /d "%ROOT%"

REM First, try to find the release binary built in Phase 1
set "BINARY=RawrXD-Sovereign.exe"
if not exist "%BINARY%" (
    echo ❌ ERROR: %BINARY% not found
    echo Run _build_full_release.cmd first to generate the optimized binary.
    exit /b 1
)

echo [1] Binary Found: %BINARY%
echo.

REM ============================================================================
REM SECTION 1: Dump PE Headers to identify .data section
REM ============================================================================
echo [2] Analyzing PE Section Headers...
echo.

"%DUMPBIN%" /HEADERS "%BINARY%" > _phase2_headers.tmp 2>&1

REM Extract .data section info
echo --- .data Section (Static Data) ---
findstr /R "\.data" _phase2_headers.tmp | head -5
echo.

REM ============================================================================
REM SECTION 2: Extract Symbol Information
REM ============================================================================
echo [3] Extracting Symbol Table (searching for large arrays)...
echo.

"%DUMPBIN%" /SYMBOLS "%BINARY%" > _phase2_symbols.tmp 2>&1

REM Look for patterns that suggest large static data:
REM   - Names with "weights", "buffer", "cache", "data", "pool", "table"
REM   - Symbols in .data section
REM   - Large size indicators

echo --- Suspicious Symbol Patterns (potential static arrays) ---
findstr /I "weight buffer cache data pool table matrix kernel model layer /c:.data" _phase2_symbols.tmp | head -20
echo.

REM ============================================================================
REM SECTION 3: Try to identify initialization patterns in source
REM ============================================================================
echo [4] Searching source code for static array declarations...
echo.

echo --- Candidates for Runtime Migration ---
echo.

REM Search for large static array patterns in C++
findstr /R /N "static.*\[.*\].*=" *.cpp *.h 2>nul | findstr /I "weight model cache buffer matrix" | head -10

if %ERRORLEVEL% NEQ 0 (
    echo (No large static arrays found matching patterns in immediate directory)
)

echo.

REM ============================================================================
REM SECTION 4: Disassembly hint - find initialized data references
REM ============================================================================
echo [5] Analyzing Code References to .data Section...
echo.

"%DUMPBIN%" /DISASM "%BINARY%" > _phase2_disasm.tmp 2>&1

REM Count references to data addresses (this is a heuristic)
echo --- Code Pattern Analysis ---
echo "(Scanning for lea, mov, and other data references...)"
findstr /I "lea mov" _phase2_disasm.tmp | findstr "rdata\|data\|rodata" | wc -l

echo.

REM ============================================================================
REM SECTION 6: Generate recommendations
REM ============================================================================
echo ╔══════════════════════════════════════════════════════════════════╗
echo ║ PHASE 2 FINDINGS & RECOMMENDATIONS                              ║
echo ╚══════════════════════════════════════════════════════════════════╝
echo.

echo Based on the 15.9 MB .data section detected in the audit:
echo.
echo LIKELY CANDIDATES:
echo   1. Model Weight Buffers
echo      → If quantized model weights (e.g., Phi-3-mini, Qwen) are embedded
echo      → Move to: Load from external .gguf or memory-mapped file
echo      → Strategy: MapViewOfFile() with lazy loading
echo.
echo   2. KV-Cache Matrices
echo      → If attention cache is pre-allocated as static array
echo      → Move to: VirtualAlloc() at startup
echo      → Strategy: Allocate per-request in token generation loop
echo.
echo   3. Quantization Tables
echo      → If static lookup tables for dequantization
echo      → Move to: Generate on first use (memoize if perf critical)
echo      → Strategy: Small runtime init cost, huge binary savings
echo.
echo   4. Framework Boilerplate
echo      → Large pre-allocated string pools, event tables, etc.
echo      → Move to: Lazy initialization during first use
echo      → Strategy: Replace static with dynamic allocation
echo.

echo ═════════════════════════════════════════════════════════════════════
echo NEXT STEPS:
echo ═════════════════════════════════════════════════════════════════════
echo.
echo 1. Review the candidates above
echo 2. Inspect the source files to find these patterns
echo 3. For EACH candidate:
echo    a. Create a _runtime_allocate_*.cpp file with VirtualAlloc/mmap
echo    b. Replace static declarations with initialization function calls
echo    c. Update headers to expose the initialization API
echo    d. Rebuild with _build_full_release.cmd
echo    e. Verify .data section is reduced in dumpbin /HEADERS
echo.
echo 4. Expected outcome: 15.9 MB -> 1-2 MB (10-15 MB saved)
echo.
echo ═════════════════════════════════════════════════════════════════════
echo.

REM Cleanup
del /f /q _phase2_headers.tmp 2>nul
del /f /q _phase2_symbols.tmp 2>nul
del /f /q _phase2_disasm.tmp 2>nul

exit /b 0
