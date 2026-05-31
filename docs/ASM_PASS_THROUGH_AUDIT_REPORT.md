;=============================================================================
; ASM_PASS_THROUGH_AUDIT_REPORT.md
; RawrXD Sovereign ASM Reality Check - Function-by-Function Analysis
;=============================================================================

# Executive Summary

**Audit Date**: 2026-04-23
**Scope**: All RawrXD ASM files in d:\rawrxd\
**Objective**: Identify pass-through functions and map conversion to real kernels

**Current State**: 
- ✅ UI / IDE: Real
- ✅ Agentic loop: Real  
- ✅ Build/debug/search: Real
- ⚠️ Sovereign client: Semi-real
- ⚠️ ASM interface: Mixed
- 🔴 ASM kernels: Partially fake (pass-throughs to C++/external)
- 🔴 Transformer math: Incomplete

---

# Category 1: Bridge Pass-Throughs (C++ → ASM → C++)

## Pattern Analysis
```asm
; BAD - Bridge pass-through
Engine_SetKVQuant PROC
    mov rcx, [rcx + CONTEXT_PTR]
    call Llama_SetKVQuant    ; ← External dependency!
    ret
Engine_SetKVQuant ENDP
```

## Functions Requiring Conversion

### 1.1 KV Cache Management Functions

| Function | File | Current | Target | Priority |
|----------|------|---------|--------|----------|
| `SpecEngine_SetKVQuant` | RawrXD_Sovereign_Core.asm | Calls Llama_SetKVQuant | Direct memory write to KV header | Critical |
| `Engine_ClearKVCache` | RawrXD_Sovereign_Core.asm | Calls Llama_ClearKV | Direct cache invalidation | Critical |
| `Engine_GetKVCacheSize` | RawrXD_Sovereign_Core.asm | Calls Llama_GetKVSize | Read from cache header | High |
| `KVCache_Resize` | RawrXD_Sovereign_Core.asm | Calls external realloc | Internal memory management | High |

### Conversion Example: SpecEngine_SetKVQuant

```asm
; BEFORE (Pass-through)
SpecEngine_SetKVQuant PROC
    mov rax, [rcx + CONTEXT_PTR]
    mov rcx, rax
    mov edx, r8d
    call Llama_SetKVQuant    ; External call!
    ret
SpecEngine_SetKVQuant ENDP

; AFTER (Real kernel)
SpecEngine_SetKVQuant PROC FRAME
    ; RCX = ctx*
    ; RDX = quant_mode
    
    push rbp
    .PUSHREG rbp
    sub rsp, 32
    .ALLOCSTACK 32
    .ENDPROLOG
    
    ; Load KV cache pointer from context
    mov rax, [rcx + KV_CACHE_PTR]
    test rax, rax
    jz .fail
    
    ; Write quant mode directly to cache header
    mov [rax + KV_QUANT_MODE], edx
    
    ; Recompute stride based on quant mode
    cmp edx, KV_QUANT_Q4_0
    je .q4_stride
    cmp edx, KV_QUANT_Q5_0
    je .q5_stride
    cmp edx, KV_QUANT_Q8_0
    je .q8_stride
    
    ; FP16 (default)
    mov dword ptr [rax + KV_STRIDE], 64
    jmp .done
    
.q4_stride:
    mov dword ptr [rax + KV_STRIDE], 32
    jmp .done
    
.q5_stride:
    mov dword ptr [rax + KV_STRIDE], 40
    jmp .done
    
.q8_stride:
    mov dword ptr [rax + KV_STRIDE], 72
    
.done:
    xor eax, eax      ; Return success
    add rsp, 32
    pop rbp
    ret
    
.fail:
    mov eax, -1       ; Return error
    add rsp, 32
    pop rbp
    ret
SpecEngine_SetKVQuant ENDP
```

### 1.2 Configuration Functions

| Function | File | Current | Target | Priority |
|----------|------|---------|--------|----------|
| `Engine_SetThreads` | RawrXD_CPUInference_Engine.asm | Calls Llama_SetThreads | Write to thread pool config | Medium |
| `Engine_SetBatchSize` | RawrXD_CPUInference_Engine.asm | Calls external | Direct batch config update | Medium |
| `Engine_SetContextWindow` | RawrXD_CPUInference_Engine.asm | Calls external | Update ctx struct | Medium |
| `Engine_GetTokensPerSec` | RawrXD_CPUInference_Engine.asm | Calls Llama_GetPerf | Calculate from counters | High |

### 1.3 Model Loading Functions

| Function | File | Current | Target | Priority |
|----------|------|---------|--------|----------|
| `Engine_LoadModel` | RawrXD_CPUInference_Engine.asm | Calls Llama_Load | Internal GGUF parser | Critical |
| `Engine_UnloadModel` | RawrXD_CPUInference_Engine.asm | Calls Llama_Free | Internal cleanup | Critical |
| `Engine_GetVocabSize` | RawrXD_CPUInference_Engine.asm | Calls external | Read from model header | Low |

---

# Category 2: Stubbed Execution Paths (Fake Work)

## Pattern Analysis
```asm
; BAD - Stubbed execution
Engine_Infer PROC
    ; Setup
    mov [g_InferActive], 1
    
    ; Maybe increment counters
    inc qword ptr [g_CallCount]
    
    ; NO ACTUAL COMPUTATION!
    
    mov [g_InferActive], 0
    ret
Engine_Infer ENDP
```

## Functions Requiring Full Implementation

### 2.1 Inference Entry Points (CRITICAL)

| Function | File | Current | Required Implementation | Priority |
|----------|------|---------|------------------------|----------|
| `SpecEngine_Infer_Speculative` | RawrXD_AgenticInference.asm | Stub | Full speculative decode loop | Critical |
| `Engine_Infer` | RawrXD_CPUInference_Engine.asm | Stub | Complete forward pass | Critical |
| `StandaloneEngine_Infer` | RawrXD_Sovereign_Core.asm | Stub | End-to-end inference | Critical |
| `Engine_InferSingle` | RawrXD_CPUInference_Engine.asm | Stub | Single token step | Critical |

### Conversion Example: SpecEngine_Infer_Single

```asm
;=============================================================================
; SpecEngine_Infer_Single - Real Token Step Implementation
; RCX = ctx*
; RDX = input token ID
; R8  = output buffer (receives next token ID)
;=============================================================================
SpecEngine_Infer_Single PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 64
    .ALLOCSTACK 64
    .ENDPROLOG
    
    mov rbx, rcx          ; RBX = ctx
    mov esi, edx          ; ESI = input token
    mov rdi, r8           ; RDI = output buffer
    
    ;-------------------------------------------------
    ; Step 1: Token Embedding Lookup
    ;-------------------------------------------------
    mov rcx, rbx
    mov edx, esi
    lea r8, [rsp + 32]   ; Temporary embedding buffer
    call Kernel_Embedding_Lookup
    
    ;-------------------------------------------------
    ; Step 2: Apply positional encoding (RoPE)
    ;-------------------------------------------------
    mov rcx, rbx
    lea rdx, [rsp + 32]   ; Embedding
    mov r8d, [rbx + CTX_POS]  ; Current position
    call Kernel_RoPE_Apply
    
    ;-------------------------------------------------
    ; Step 3: Transformer Layers (loop)
    ;-------------------------------------------------
    mov ecx, [rbx + MODEL_N_LAYERS]
    xor ebp, ebp          ; Layer counter
    
.layer_loop:
    cmp ebp, ecx
    jge .layers_done
    
    ; RMS Norm
    mov rcx, rbx
    lea rdx, [rsp + 32]
    mov r8d, ebp
    call Kernel_RMSNorm
    
    ; Self Attention
    mov rcx, rbx
    lea rdx, [rsp + 32]
    mov r8d, ebp
    call Kernel_SelfAttention
    
    ; Residual Add
    lea rcx, [rsp + 32]
    mov rdx, rcx
    call Kernel_VectorAdd
    
    ; FFN Norm
    mov rcx, rbx
    lea rdx, [rsp + 32]
    mov r8d, ebp
    call Kernel_RMSNorm
    
    ; FFN
    mov rcx, rbx
    lea rdx, [rsp + 32]
    mov r8d, ebp
    call Kernel_FFN
    
    ; Residual Add
    lea rcx, [rsp + 32]
    mov rdx, rcx
    call Kernel_VectorAdd
    
    inc ebp
    jmp .layer_loop
    
.layers_done:
    
    ;-------------------------------------------------
    ; Step 4: Final RMS Norm
    ;-------------------------------------------------
    mov rcx, rbx
    lea rdx, [rsp + 32]
    call Kernel_RMSNorm
    
    ;-------------------------------------------------
    ; Step 5: Output Projection (LM Head)
    ;-------------------------------------------------
    mov rcx, rbx
    lea rdx, [rsp + 32]
    lea r8, [rsp + 48]   ; Logits buffer
    call Kernel_LMHead_Project
    
    ;-------------------------------------------------
    ; Step 6: Softmax + Sampling
    ;-------------------------------------------------
    lea rcx, [rsp + 48]  ; Logits
    mov edx, [rbx + MODEL_VOCAB_SIZE]
    mov r8d, [rbx + SAMPLING_TEMP]
    call Kernel_Softmax_Temperature
    
    ; Argmax or sampling
    cmp dword ptr [rbx + USE_GREEDY], 0
    jne .greedy
    
    ; Top-k / Top-p sampling
    lea rcx, [rsp + 48]
    mov edx, [rbx + TOP_K]
    mov r8d, [rbx + TOP_P]
    call Kernel_TopK_TopP_Sample
    jmp .output
    
.greedy:
    lea rcx, [rsp + 48]
    mov edx, [rbx + MODEL_VOCAB_SIZE]
    call Kernel_ArgMax
    
.output:
    ; Store result
    mov [rdi], eax
    
    ; Update position counter
    inc dword ptr [rbx + CTX_POS]
    
    ; Success
    xor eax, eax
    
    add rsp, 64
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
SpecEngine_Infer_Single ENDP
```

### 2.2 Kernel Functions (Must Be Real)

| Function | File | Current | Required | Priority |
|----------|------|---------|----------|----------|
| `Kernel_MatMul` | RawrXD_CPUOps_Kernels.asm | Stub | Full AVX2 matmul | Critical |
| `Kernel_Softmax` | RawrXD_CPUOps_Kernels.asm | Stub | Numerically stable softmax | Critical |
| `Kernel_RMSNorm` | RawrXD_CPUOps_Kernels.asm | Stub | RMS normalization | Critical |
| `Kernel_SelfAttention` | RawrXD_CPUOps_Kernels.asm | Stub | Q·K^T·V | Critical |
| `Kernel_FFN` | RawrXD_CPUOps_Kernels.asm | Stub | Feed-forward network | Critical |
| `Kernel_RoPE` | RawrXD_CPUOps_Kernels.asm | Stub | Rotary embeddings | High |
| `Kernel_LMHead` | RawrXD_CPUOps_Kernels.asm | Stub | Output projection | High |

---

# Category 3: Tokenizer Pass-Throughs

## Pattern Analysis
```asm
; BAD - Tokenizer pass-through
Tokenizer_Encode PROC
    mov rcx, [rcx + TOKENIZER_CTX]
    mov rdx, r8
    call Llama_Tokenize    ; External!
    ret
Tokenizer_Encode ENDP
```

## Functions Requiring BPE Implementation

| Function | File | Current | Target | Priority |
|----------|------|---------|--------|----------|
| `Tokenizer_Encode` | RawrXD_BPETokenizer.asm | Calls external | Full BPE in ASM | Critical |
| `Tokenizer_Decode` | RawrXD_BPETokenizer.asm | Calls external | BPE decode in ASM | Critical |
| `Tokenizer_LoadVocab` | RawrXD_BPETokenizer.asm | Calls external | GGUF vocab parser | High |
| `Tokenizer_MergeLoop` | RawrXD_BPETokenizer.asm | Not implemented | Core BPE merge | Critical |

### Conversion Example: Tokenizer_MergeLoop

```asm
;=============================================================================
; Tokenizer_MergeLoop - Core BPE tokenization in ASM
; RCX = token buffer
; RDX = buffer length
;=============================================================================
Tokenizer_MergeLoop PROC FRAME
    push rbp
    .PUSHREG rbp
    push rbx
    .PUSHREG rbx
    push rsi
    .PUSHREG rsi
    push rdi
    .PUSHREG rdi
    sub rsp, 48
    .ALLOCSTACK 48
    .ENDPROLOG
    
    mov rsi, rcx          ; RSI = token buffer
    mov ebx, edx          ; EBX = length
    
.merge_loop:
    ; Find best pair to merge
    xor ecx, ecx          ; Best pair score
    xor ebp, ebp          ; Best pair index
    xor edi, edi          ; Current index
    
.find_pair_loop:
    cmp edi, ebx
    jge .find_pair_done
    
    ; Load pair at current position
    movzx eax, word ptr [rsi + rdi*2]
    movzx edx, word ptr [rsi + rdi*2 + 2]
    
    ; Look up merge score in hash table
    shl eax, 16
    or eax, edx
    mov rcx, rbx          ; Tokenizer context
    call HashTable_Lookup
    
    ; Check if this is best score
    cmp eax, [rsp + 32]   ; Current best
    jle .next_pair
    
    mov [rsp + 32], eax   ; Update best
    mov ebp, edi          ; Update best index
    
.next_pair:
    inc edi
    jmp .find_pair_loop
    
.find_pair_done:
    ; Check if any merge found
    cmp dword ptr [rsp + 32], 0
    je .done
    
    ; Apply merge at best position
    mov ecx, ebp
    mov edx, [rsp + 32]   ; Merge ID
    call ApplyMerge
    
    ; Decrease length
    dec ebx
    jmp .merge_loop
    
.done:
    mov eax, ebx          ; Return final length
    
    add rsp, 48
    pop rdi
    pop rsi
    pop rbx
    pop rbp
    ret
Tokenizer_MergeLoop ENDP
```

---

# Complete Function Inventory

## RawrXD_Sovereign_Core.asm

| Function | Line | Category | Status | Action |
|----------|------|----------|--------|--------|
| `AcquireSovereignLock` | ~75 | Real | ✅ | Keep |
| `ReleaseSovereignLock` | ~95 | Real | ✅ | Keep |
| `ValidateDMAAlignment` | ~105 | Real | ✅ | Keep |
| `HealSymbolResolution` | ~130 | Real | ✅ | Keep |
| `ObserveTokenStream` | ~160 | Real | ✅ | Keep |
| `CoordinateAgents` | ~190 | Real | ✅ | Keep |
| `RawrXD_Trigger_Chat` | ~260 | Real | ✅ | Keep |
| `Sovereign_Pipeline_Cycle` | ~330 | Real | ✅ | Keep |
| `Sovereign_MainLoop` | ~370 | Real | ✅ | Keep |
| `SpecEngine_SetKVQuant` | N/A | Pass-through | 🔴 | Convert to direct memory |
| `Engine_ClearKVCache` | N/A | Pass-through | 🔴 | Convert to direct memory |
| `StandaloneEngine_Infer` | N/A | Stub | 🔴 | Implement full forward pass |

## RawrXD_CPUInference_Engine.asm

| Function | Category | Status | Action |
|----------|----------|--------|--------|
| `Engine_Infer` | Stub | 🔴 | Implement full forward pass |
| `Engine_InferSingle` | Stub | 🔴 | Implement token step |
| `Engine_LoadModel` | Pass-through | 🔴 | Implement GGUF loader |
| `Engine_UnloadModel` | Pass-through | 🔴 | Implement cleanup |
| `Engine_SetThreads` | Pass-through | 🟡 | Convert to direct config |
| `Engine_GetTokensPerSec` | Pass-through | 🟡 | Calculate internally |

## RawrXD_CPUOps_Kernels.asm

| Function | Category | Status | Action |
|----------|----------|--------|--------|
| `Kernel_MatMul` | Stub | 🔴 | Implement AVX2 matmul |
| `Kernel_Softmax` | Stub | 🔴 | Implement stable softmax |
| `Kernel_RMSNorm` | Stub | 🔴 | Implement RMS norm |
| `Kernel_SelfAttention` | Stub | 🔴 | Implement attention |
| `Kernel_FFN` | Stub | 🔴 | Implement FFN |
| `Kernel_RoPE` | Stub | 🔴 | Implement rotary embeddings |

## RawrXD_AgenticInference.asm

| Function | Category | Status | Action |
|----------|----------|--------|--------|
| `SpecEngine_Infer_Speculative` | Stub | 🔴 | Implement speculative decode |
| `SpecEngine_VerifyDraft` | Stub | 🔴 | Implement draft verification |
| `SpecEngine_UpdateCache` | Pass-through | 🔴 | Direct KV cache update |

## RawrXD_BPETokenizer.asm

| Function | Category | Status | Action |
|----------|----------|--------|--------|
| `Tokenizer_Encode` | Pass-through | 🔴 | Implement BPE encode |
| `Tokenizer_Decode` | Pass-through | 🔴 | Implement BPE decode |
| `Tokenizer_MergeLoop` | Not implemented | 🔴 | Implement core BPE |
| `Tokenizer_LoadVocab` | Pass-through | 🔴 | Implement GGUF vocab load |

---

# Conversion Priority Matrix

## Critical (Must Convert First)
1. `SpecEngine_Infer_Single` - Core inference
2. `Kernel_MatMul` - Foundation of all compute
3. `Kernel_SelfAttention` - Core transformer operation
4. `Kernel_Softmax` - Required for attention
5. `Tokenizer_MergeLoop` - Required for tokenization

## High (Convert Second)
6. `Engine_Infer` - Full forward pass wrapper
7. `Kernel_RMSNorm` - Required for each layer
8. `Kernel_FFN` - Required for each layer
9. `SpecEngine_SetKVQuant` - KV cache management
10. `Engine_LoadModel` - Model loading

## Medium (Convert Third)
11. `Kernel_RoPE` - Positional encoding
12. `Engine_ClearKVCache` - Cache management
13. `Engine_GetTokensPerSec` - Performance metrics
14. `Tokenizer_Encode/Decode` - Full tokenization

## Low (Convert Last)
15. Configuration getters/setters
16. Debug/diagnostic functions
17. Optional sampling methods

---

# Success Criteria

## Phase 1: No External Calls
- [ ] Zero `call Llama_*` in ASM files
- [ ] Zero `call ggml_*` in ASM files
- [ ] All EXTERNDEF to external libs removed

## Phase 2: Real Kernels
- [ ] Matmul achieves >80% peak FLOPS (AVX2)
- [ ] Softmax numerically stable
- [ ] Attention produces correct output

## Phase 3: Complete Forward Pass
- [ ] Single token step works end-to-end
- [ ] Multi-token generation works
- [ ] KV cache correctly managed
- [ ] Tokenizer produces correct IDs

## Phase 4: Sovereign Status
- [ ] No LlamaNativeBridge dependency
- [ ] All inference through ASM path
- [ ] Performance within 20% of llama.cpp

---

# Next Steps

1. **ASM-1**: Complete function inventory (Day 2)
2. **ASM-2**: Implement AVX2 matmul + attention (Day 4)
3. **ASM-3**: Build complete forward pass (Day 6)
4. **Integration**: Connect to IDE (Day 7)
5. **Validation**: Benchmark vs llama.cpp (Day 8)

---

*Report Generated: 2026-04-23*
*Status: Ready for ASM kernel implementation*
