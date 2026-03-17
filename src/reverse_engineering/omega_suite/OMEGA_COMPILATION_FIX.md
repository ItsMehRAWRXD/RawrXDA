# MASM64 COMPILATION FIX PATCH for omega.asm

This patch resolves the 3 remaining compilation errors:

1. **Line 620:** movss addressing mode
2. **Line 1916:** undefined symbol g_KickPitch
3. **Line 1967:** undefined symbol g_SnarePitch

## Solution strategy

Replace real4 floating-point constants with DQ (qword) encoded IEEE 754 floats. This avoids forward reference issues during 1st pass assembly.

**IEEE 754 encoding for key values:**

| Value            | Hex (32-bit float) |
|------------------|--------------------|
| 55.0 Hz (Kick)   | 42580000h          |
| 200.0 Hz (Snare) | 43480000h          |
| 8000.0 Hz (HiHat)| 45fa0000h          |
| 0.95 (Kick decay)| 3f733333h          |

Use DWORD aligned allocation with immediate values, or reference via register load before use.

---

## FIX 1: MOVSS addressing at line 620

**OLD:**
```asm
lea rsi, [rdx + rax*4]
movss [rsi], xmm0
```
SSE instructions have strict addressing mode requirements; ML64 may not accept `[rsi]` for movss.

**NEW:** Use MOVD for memory store (MASM64-compatible):
```asm
lea rsi, [rdx + rax*4]
movd dword ptr [rsi], xmm0
```

---

## FIX 2 & 3: Undefined symbols

g_KickPitch, g_SnarePitch defined as real4 in .data but MASM64 may not resolve them in .code.

**Option A — Inline the values:**
```asm
mov edx, 42580000h
movd xmm1, edx
cvtdq2ps xmm1, xmm1
```

**Option B — Equates:**
```asm
g_fKickPitch  equ 42580000h
g_fSnarePitch equ 43480000h
mov edx, g_fKickPitch
movd xmm1, edx
cvtdq2ps xmm1, xmm1
```

**Option C:** Initialize globals at runtime from a lookup table.

---

## Patch instructions

1. **At line 620:** Replace `movss [rsi], xmm0` with `movd dword ptr [rsi], xmm0`.
2. **After .data:** Add `g_fKickPitch equ 42580000h` and `g_fSnarePitch equ 43480000h`.
3. **At lines 1916 and 1967:** Replace `movss xmm1, g_KickPitch` with the equate load + movd/cvtdq2ps sequence above.

**Alternative:** Replace real4 declarations with explicitly initialized DWORD values in .data, then initialize at runtime.
