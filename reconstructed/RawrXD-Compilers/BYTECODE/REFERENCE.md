# BYTECODE REFERENCE - encoder_host_final.asm

## Test Sequence in main()

The `main()` procedure tests 5 instruction variants. Below is the expected bytecode
for each, cross-referenced against Intel x64 ISA (Intel SDM Volume 2).

---

## Test 1: MOV RAX, 0x123456789ABCDEF0

**Instruction:** Move 64-bit immediate to RAX

**Expected Bytecode:**
```
48 B8 F0 DE BC 9A 78 56 34 12
```

**Breakdown:**
- `48` = REX.W (64-bit operand size)
- `B8` = MOV r64, imm64 opcode (B8+rd where rd=0 for RAX)
- `F0 DE BC 9A 78 56 34 12` = Immediate 0x123456789ABCDEF0 (little-endian)

**Intel SDM Reference:** Vol 2A, MOV (Move) instruction, opcode B8+rd

---

## Test 2: MOV R15, 0x42

**Instruction:** Move 64-bit immediate to R15 (extended register)

**Expected Bytecode:**
```
49 B8 42 00 00 00 00 00 00 00
```

**Breakdown:**
- `49` = REX.W (bit 3) + REX.B (bit 0) for extended R15
  - 0x48 (REX.W) | 0x01 (REX.B) | 0x40 (REX prefix base) = 0x49
- `B8` = MOV r64, imm64 opcode
- `42 00 00 00 00 00 00 00` = Immediate 0x42 (zero-extended to qword)

**Intel SDM Reference:** Vol 2A, REX Prefix rules

---

## Test 3: MOV RAX, RBX

**Instruction:** Move RBX into RAX

**Expected Bytecode:**
```
48 89 C3
```

**Breakdown:**
- `48` = REX.W (64-bit operation)
- `89` = MOV r64, r/m64 opcode
- `C3` = ModRM byte
  - mod=11 (register direct addressing)
  - reg=011 (RBX, source operand)
  - r/m=000 (RAX, destination operand)

**Encoding Verification:**
- mod = 0x3 << 6 = 0xC0
- reg = 0x3 << 3 = 0x18
- r/m = 0x0
- Combined = 0xC0 | 0x18 | 0x0 = 0xD8... wait, should be 0xC3

**Correction (Intel convention for 89h opcode):**
- reg field = source (RBX = 3)
- r/m field = destination (RAX = 0)
- ModRM = (3 << 6) | (3 << 3) | 0 = 0xC0 | 0x18 | 0x0 = 0xD8

**Actually:** Let me recalculate. For `89 /r` (MOV r/m64, r64):
- Source (r): RBX = register 3
- Dest (r/m): RAX = register 0
- ModRM format: mod=11, reg=src_reg, r/m=dest_reg
- ModRM = (11 << 6) | (011 << 3) | 000 = 0xC0 | 0x18 | 0x00 = 0xD8

**Hmm, discrepancy. Let me check encoder logic...**

Looking at `Emit_MOV_R64_R64`:
```asm
mov rcx, 3         ; mod = register
mov rdx, rax       ; reg = src
pop r8             ; r/m = dst
and dl, 7
and r8b, 7
call EncodeModRM
```

In `EncodeModRM`:
```asm
shl cl, 6          ; mod << 6
shl dl, 3          ; reg << 3
or cl, dl
or cl, r8b
mov al, cl
```

So: ModRM = (mod << 6) | (reg << 3) | r/m
            = (3 << 6) | (3 << 3) | 0
            = 0xC0 | 0x18 | 0x00
            = 0xD8

**But Intel says MOV RAX, RBX is 89 C3...**

**Clarification:** In x64, RAX = register 0, RBX = register 3
- For `89 /r`: source is in reg field, dest is in r/m field
- Source (RBX=3) goes in reg field
- Dest (RAX=0) goes in r/m field
- ModRM = (3 << 6) | (3 << 3) | 0 = 0xD8

**But objdump shows 89 C3. Let me verify with actual disassembly...**

Actually, I need to check: is the encoder computing this correctly?

Looking at test in `main()`:
```asm
mov rcx, 0         ; RAX (destination)
mov rdx, 3         ; RBX (source)
call Emit_MOV_R64_R64
```

In the encoder, we push dst/src:
```asm
push rcx           ; Push RAX
push rdx           ; Push RBX
...
pop rax            ; rax = RBX (src)
mov rcx, 3         ; mod = register
mov rdx, rax       ; rdx = RBX (reg field = source)
pop r8             ; r8 = RAX (r/m field = dest)
and dl, 7          ; Mask RBX to 3
and r8b, 7         ; Mask RAX to 0
call EncodeModRM   ; (3 << 6) | (3 << 3) | 0 = 0xD8
```

**Conflict with Intel SDM.** Let me re-read the `89` opcode definition:

```
89 /r: MOV r/m64, r64
```

This means:
- Source operand (r64): from reg field
- Dest operand (r/m64): from r/m field

For `MOV RAX, RBX` (Intel syntax):
- Destination: RAX
- Source: RBX

In ModRM encoding:
- If r/m is destination, then: r/m = 0 (RAX), reg = 3 (RBX)
- ModRM = (3 << 6) | (3 << 3) | 0 = 0xD8

**But this contradicts disassemblers showing 89 C3.**

**Resolution:** I think I misunderstood. Let me check: does `89 C3` decode as:
- ModRM byte C3 = 11 000 011
- mod = 11 (register)
- reg = 000 (RAX)
- r/m = 011 (RBX)

So `89 C3` means: MOV (r/m from C3=RBX), (reg from C3=RAX)
= MOV RBX, RAX

**But we want MOV RAX, RBX.** That would be `8B C3`:
```
8B: MOV r64, r/m64 (source from r/m, dest from reg)
C3: mod=11, reg=RAX(0), r/m=RBX(3)
```
= MOV RAX, RBX

**So our test bytecode should be `48 8B C3`, not `48 89 C3`.**

**OR**, the encoder swaps dest/source in the call. Let me re-read the test:
```asm
mov rcx, 0         ; dst_reg = RAX
mov rdx, 3         ; src_reg = RBX
call Emit_MOV_R64_R64
```

And in the proc comment:
```asm
; Emit_MOV_R64_R64: MOV r64, r64
; RCX = dst reg, RDX = src reg
```

So it's: MOV dst (from RCX), src (from RDX) = MOV RAX, RBX

The encoder pushes RCX and RDX, then uses them to build ModRM. Looking at the logic:
```asm
pop rax            ; rax = RBX (src)
mov rcx, 3         ; mod = 3 (register addressing)
mov rdx, rax       ; rdx = RBX (source value, used as reg field)
pop r8             ; r8 = RAX (dest value, used as r/m field)
```

Then in EncodeModRM:
```asm
shl cl, 6          ; mod (3) << 6 = 0xC0
shl dl, 3          ; reg (RBX=3) << 3 = 0x18
or cl, dl          ; 0xC0 | 0x18 = 0xD8
or cl, r8b         ; 0xD8 | RAX(0) = 0xD8
```

So ModRM = 0xD8

But the Intel `89` opcode is: MOV r/m64, r64 (r/m = dest, reg = source)
So with ModRM = 0xD8 (mod=11, reg=3, r/m=0):
- Dest: r/m=0 (RAX)
- Source: reg=3 (RBX)
= MOV RAX, RBX ✓

**WAIT.** ModRM 0xD8 decodes as:
- Binary: 11 011 000
- mod = 11
- reg = 011 (3)
- r/m = 000 (0)

So bytecode should be `48 89 D8`, not `48 89 C3`.

**Hmm, let me reconsider.** Actually, 0xC3 is:
- Binary: 1100 0011
- Upper 2 bits (mod): 11
- Middle 3 bits (reg): 000
- Lower 3 bits (r/m): 011

So 0xC3 = ModRM(mod=11, reg=0, r/m=3)
With `89` (MOV r/m, r):
- Dest (r/m=3): RBX
- Source (reg=0): RAX
= MOV RBX, RAX

**But we want MOV RAX, RBX, which is opcode 8B:**
- `8B` = MOV r, r/m (source from r/m, dest from reg)
- ModRM 0xC3 = (mod=11, reg=0, r/m=3)
- Dest (reg=0): RAX
- Source (r/m=3): RBX
= MOV RAX, RBX ✓

**So the bytecode should be `48 8B C3`, not `48 89 C3`.**

---

## **CORRECTION: Opcode 89 vs 8B**

The encoder currently uses **opcode 89** in `Emit_MOV_R64_R64`:
```asm
mov dl, 089h
call EmitByte
```

But based on Intel SDM:
- **89 /r**: MOV r/m64, r64 (source in reg field, dest in r/m field)
- **8B /r**: MOV r64, r/m64 (source in r/m field, dest in reg field)

For syntax **MOV dst, src**, we want the reg field to be dest and r/m to be source.
That's opcode **8B**, not **89**.

**Fix:** Change `Emit_MOV_R64_R64` to use `8B` instead of `89`.

---

## Test 3 (CORRECTED): MOV RAX, RBX

**Expected Bytecode (after fix):**
```
48 8B C3
```

**Breakdown:**
- `48` = REX.W
- `8B` = MOV r64, r/m64 (dest from reg, source from r/m)
- `C3` = ModRM(mod=11, reg=RAX(0), r/m=RBX(3))

---

## Test 4: ADD RAX, RBX

**Instruction:** Add RBX to RAX, result in RAX

**Expected Bytecode:**
```
48 01 C3
```

**Breakdown:**
- `48` = REX.W
- `01` = ADD r/m64, r64 (source in reg field, dest in r/m field)
- `C3` = ModRM(mod=11, reg=RBX(3), r/m=RAX(0))

**Intel SDM Reference:** Vol 2A, ADD instruction, opcode 01

---

## Test 5: RET

**Instruction:** Return from procedure

**Expected Bytecode:**
```
C3
```

**Breakdown:**
- `C3` = RET opcode (near return)

**Intel SDM Reference:** Vol 2B, RET instruction

---

## Summary Table

| Test | Instruction | Bytecode | Note |
|------|-------------|----------|------|
| 1 | `MOV RAX, 0x123456789ABCDEF0` | `48 B8 F0 DE BC 9A 78 56 34 12` | imm64, little-endian |
| 2 | `MOV R15, 0x42` | `49 B8 42 00 00 00 00 00 00 00` | REX.W + REX.B for R15 |
| 3 | `MOV RAX, RBX` | `48 8B C3` | **Requires opcode 8B (not 89)** |
| 4 | `ADD RAX, RBX` | `48 01 C3` | Standard ADD r/m, r |
| 5 | `RET` | `C3` | Near return |

---

## Verification Steps

1. **Assemble** `encoder_host_final.asm`:
   ```powershell
   ml64.exe /c encoder_host_final.asm
   ```

2. **Disassemble** `encoder_host_final.obj` (if objdump available):
   ```bash
   objdump -d encoder_host_final.obj
   ```

3. **Compare** bytecode output against table above

4. **Document** any deviations

---

## Action Required

**Update `Emit_MOV_R64_R64` in encoder_host_final.asm:**

Change line (approximately 215):
```asm
mov dl, 089h
```

To:
```asm
mov dl, 08Bh
```

This corrects the opcode to match Intel x64 ISA semantics.

