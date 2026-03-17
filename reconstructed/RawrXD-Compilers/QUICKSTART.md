# RawrXD v2.0 Quick-Start Guide

**Time to Integration: 15 minutes ⚡**

---

## 🚀 In 3 Steps

### Step 1: Drop GGUF Parser (2 minutes)

**File:** `gguf_robust_tools.hpp`

**Your code:**
```cpp
#include "gguf_robust_tools.hpp"

int main() {
    // Pre-flight validation
    auto scan = rawrxd::gguf_robust::CorruptionScan::ScanFile("model.gguf");
    if (!scan.is_valid) {
        printf("Error: %s\n", scan.error_msg);
        return 1;
    }
    
    // Safe metadata parsing
    rawrxd::gguf_robust::RobustGguFStream stream("model.gguf");
    rawrxd::gguf_robust::MetadataSurgeon surgeon(stream);
    
    rawrxd::gguf_robust::MetadataSurgeon::ParseConfig cfg;
    cfg.skip_chat_template = true;
    
    if (!surgeon.ParseKvPairs(scan.metadata_kv_count, cfg)) {
        printf("Parse failed\n");
        return 1;
    }
    
    printf("Model loaded safely!\n");
    return 0;
}
```

**Result:** ✅ No more GGUF crashes

---

### Step 2: Link x64 Encoder (5 minutes)

**Files:** 
- `encoder_core_v3.asm` (link with project)

**Your MASM code:**
```asm
EXTERN Encoder_Init:PROC
EXTERN Encode_Inst_MOV_RR:PROC
EXTERN Encode_Inst_ALU_RR:PROC
EXTERN Encode_Inst_MOV_R64_I64:PROC

MyAssembly PROC
    call Encoder_Init
    
    ; MOV RAX, 0x1234567890ABCDEF
    mov dl, REG_RAX
    mov rcx, 0x1234567890ABCDEF
    call Encode_Inst_MOV_R64_I64
    
    ; ADD RBX, RCX
    mov ecx, 0                          ; Operation: ADD
    mov edx, REG_RBX                    ; Dest: RBX
    mov r8d, REG_RCX                    ; Src: RCX
    call Encode_Inst_ALU_RR
    
    ; More instructions...
    
    ret
MyAssembly ENDP
```

**Result:** ✅ Machine code generated inline

---

### Step 3: Configure & Run (8 minutes)

**Build command (Visual Studio):**
```batch
cl.exe /c your_code.cpp
ml64.exe /c encoder_core_v3.asm
link.exe your_code.obj encoder_core_v3.obj kernel32.lib
```

**Run:**
```
your_app.exe model.gguf
→ Model loaded safely!
```

**Result:** ✅ Bulletproof model loader with inline assembler

---

## 📖 Full Documentation

| Goal | Document |
|------|----------|
| **Deep integration** | `GGUF_ENCODER_INTEGRATION_GUIDE.md` |
| **Phase 1 context** | `FINAL_STATUS_REPORT.md` |
| **Bytecode reference** | `BYTECODE_REFERENCE.md` |
| **Everything** | `V2_COMPLETE_SUMMARY.md` |

---

## ✅ Sanity Checks

**GGUF Parser:**
```cpp
auto scan = CorruptionScan::ScanFile("test.gguf");
assert(scan.is_valid);              // File OK?
assert(scan.tensor_count < 1000000); // Corruption detected?
```

**x64 Encoder:**
```asm
mov ecx, 0          ; Operation index
mov edx, 0          ; Dest = RAX
mov r8d, 1          ; Src = RCX
call Encode_Inst_ALU_RR
; Check memory at g_EncoderCtx.output_ptr has bytecode
```

---

## 🎯 What Works Now

✅ Load corrupted models without crashing  
✅ Generate x64 machine code inline  
✅ No external compiler needed  
✅ 64-bit file operations  
✅ Automatic toxic key filtering  

---

## 🚧 Coming in Phase 3

🔄 100+ instruction types (vs. current 10)  
🔄 Two-pass assembly (forward references)  
🔄 PE executable generation  
🔄 Debugging support

---

## 💡 Pro Tips

1. **Always check `scan.is_valid`** before parsing
2. **Set `skip_chat_template = true`** (prevents crashes)
3. **Use `SkipString()` for untrusted fields** (zero allocations)
4. **Let REX prefix auto-calculate** (register ID >= 8? REX.B/R set automatically)

---

**That's it! You're now ready to:**
- Load any model (even corrupted ones)
- Generate x64 code inline
- Build a self-hosted assembler

**Questions? See `GGUF_ENCODER_INTEGRATION_GUIDE.md` for complete docs.**

