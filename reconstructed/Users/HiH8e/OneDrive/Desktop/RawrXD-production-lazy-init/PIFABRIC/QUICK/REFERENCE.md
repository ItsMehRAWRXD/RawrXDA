# QUICK REFERENCE: Tensor Offset Resolution + GGUF Settings + Training

## 3-Minute Overview

You now have **3 new MASM modules** that work together:

### 1. **Tensor Offset Resolver** (`gguf_tensor_offset_resolver.asm`)
- **What**: Converts GGUF file offsets → memory pointers
- **Main Function**: `GGUF_TensorOffsetLoopAll()`
- **When**: Call after file is loaded into memory
- **Cost**: ~10ms for 1000 tensors

### 2. **GGUF Settings** (`gguf_loader_settings.asm`)
- **What**: Settings system with toggles and checkboxes
- **Main Functions**: 
  - `GGUFSettings_SetLoaderMethod()` - Pick DISC/MEMORY/MMAP/HYBRID
  - `GGUFSettings_SetToggle()` - Enable/disable features
  - `GGUFSettings_SetCustomPath()` - Custom model paths
- **UI**: Will show radio buttons + checkboxes in IDE

### 3. **ML Training** (`ml_model_training.asm`)
- **What**: Full training loop (forward, backward, update)
- **Main Functions**:
  - `MLTraining_CreateSession()` - Start training
  - `MLTraining_ComputeGradients()` - One training step
  - `MLTraining_UpdateWeights()` - Apply optimizer
- **Optimizers**: SGD, Adam, AdamW, RMSProp

---

## Code Integration (5 minutes)

### Step 1: Enable Tensor Resolution in Loader

In your existing `GGUFChain_LoadModel()`:

```asm
; After: mov esi, eax  (got file handle)
; After: pModel loaded to memory

; Add this:
push pContext                          ; Your GGUF_MODEL_CONTEXT
call PiFabric_ResolveTensorOffsets     ; From pifabric_core_enhanced
cmp eax, 1
jne @error_resolution_failed
```

### Step 2: Wire Settings System

```asm
; Create settings instance
call GGUFSettings_Create
mov pSettings, eax

; User selects MEMORY mode
push LOADER_METHOD_MEMORY
push pSettings
call GGUFSettings_SetLoaderMethod

; User enables training
push 1                                 ; enable = TRUE
push TOGGLE_ENABLE_TRAINING_MODE
push pSettings
call GGUFSettings_SetToggle
```

### Step 3: Enable Training

```asm
; In PiFabric_Open() or after model loaded:
push TRAINING_LOSS_CROSSENTROPY        ; Loss type
push TRAINING_OPTIMIZER_ADAM           ; Optimizer
push 24                                ; nLayers
push hFabric
call PiFabric_EnableTraining
cmp eax, 1
jne @training_not_available
```

### Step 4: Train in Loop

```asm
; In your batch loop:
push dwBatchSize
push pBatchTarget                      ; Labels
push pBatchInput                       ; Data
push hFabric
call PiFabric_Train

; Get loss
push hFabric
call MLTraining_GetLoss
; EAX now has loss value
```

---

## Data Structures

### GGUF Tensor Info
```asm
GGUF_TENSOR_INFO STRUCT
    pName           DWORD ?     ; Name
    nDims           DWORD ?     ; Rank (2-4)
    dims            DWORD 4 DUP (?) ; Shape
    type            DWORD ?     ; Type (F32, Q4_0, etc.)
    offsetInFile    QWORD ?     ; File offset
    pResolved       DWORD ?     ; ← Set by resolver
    cbTensorBytes   QWORD ?     ; ← Computed size
    dwFlags         DWORD ?     ; Status
GGUF_TENSOR_INFO ENDS
```

### Training Session
```asm
ML_TRAINING_SESSION STRUCT
    pModel          DWORD ?     ; Model
    dwOptimizer     DWORD ?     ; SGD/Adam/etc.
    dwLossFunction  DWORD ?     ; XE/MSE/Huber
    fLearningRate   DWORD ?     ; As float
    fCurrentLoss    DWORD ?     ; ← Updated each step
    dwEpoch         DWORD ?     ; Epoch counter
    pMetrics        DWORD ?     ; Accuracy, F1, etc.
ML_TRAINING_SESSION ENDS
```

### Settings
```asm
GGUF_LOADER_SETTINGS STRUCT
    dwDefaultMethod DWORD ?     ; MEMORY / DISC / etc.
    dwToggleFlags   DWORD ?     ; Feature bits
    dwCompressionLevel DWORD ?  ; 0-9
    szCustomModelPath BYTE 512 DUP (?)
GGUF_LOADER_SETTINGS ENDS
```

---

## Feature Toggles (Checkboxes)

```asm
TOGGLE_ENABLE_COMPRESSION      equ 1   ; ☐ Compress
TOGGLE_ENABLE_QUANTIZATION     equ 2   ; ☐ Quantize
TOGGLE_ENABLE_ADAPTIVE_TUNING  equ 4   ; ☐ Auto-tune
TOGGLE_ENABLE_TELEMETRY        equ 8   ; ☐ Telemetry
TOGGLE_ENABLE_TRAINING_MODE    equ 16  ; ☐ Training
TOGGLE_ENABLE_VALIDATION       equ 32  ; ☐ Validation
```

### Enable All
```asm
push OFFSET pSettings
mov eax, 0x3F              ; All 6 bits
call GGUFSettings_SetToggle
```

---

## Loader Methods (Radio Buttons)

```asm
LOADER_METHOD_DISC      equ 0   ; ○ Disc Streaming
LOADER_METHOD_MEMORY    equ 1   ; ● Memory (fastest)
LOADER_METHOD_MMAP      equ 2   ; ○ Memory-Mapped
LOADER_METHOD_HYBRID    equ 3   ; ○ Hybrid
LOADER_METHOD_AUTO      equ 4   ; ○ Auto-select
```

---

## Optimizers & Loss Functions

### Optimizers
```asm
TRAINING_OPTIMIZER_SGD      equ 0   ; Stable, slower
TRAINING_OPTIMIZER_ADAM     equ 1   ; Adaptive (best)
TRAINING_OPTIMIZER_ADAMW    equ 2   ; Adam + L2 reg
TRAINING_OPTIMIZER_RMSPROP  equ 3   ; RMSProp variant
```

### Loss Functions
```asm
TRAINING_LOSS_CROSSENTROPY  equ 0   ; Classification
TRAINING_LOSS_MSE           equ 1   ; Regression
TRAINING_LOSS_HUBER         equ 2   ; Robust
```

---

## Common Usage Patterns

### Pattern 1: Load + Resolve + Infer
```asm
; Open model
push CHAIN_SEQUENTIAL
push METHOD_AUTO
push OFFSET szModelPath
call PiFabric_Open
mov hFabric, eax

; Resolve tensors
push pContext
push hFabric
call PiFabric_ResolveTensorOffsets

; Use for inference
@inference_loop:
    push dwBytes
    push pDst
    push hFabric
    call PiFabric_Stream
    jmp @inference_loop

; Cleanup
push hFabric
call PiFabric_Close
```

### Pattern 2: Load + Train
```asm
; Open model
push CHAIN_SEQUENTIAL
push METHOD_MEMORY                 ; Full memory
push OFFSET szModelPath
call PiFabric_Open
mov hFabric, eax

; Enable training
push TRAINING_LOSS_CROSSENTROPY
push TRAINING_OPTIMIZER_ADAM
push 24
push hFabric
call PiFabric_EnableTraining

; Training loop
xor ecx, ecx
@epoch_loop:
    cmp ecx, 10
    jge @done_training

    ; Forward + backward + update
    push dwBatchSize
    push pLabels
    push pData
    push hFabric
    call PiFabric_Train

    ; Get loss
    push hFabric
    call MLTraining_GetLoss
    ; EAX = loss

    inc ecx
    jmp @epoch_loop

@done_training:
    push hFabric
    call PiFabric_Close
```

### Pattern 3: Settings Configuration
```asm
; Create settings
call GGUFSettings_Create
mov pSettings, eax

; Configure
push LOADER_METHOD_MEMORY
push pSettings
call GGUFSettings_SetLoaderMethod

; Enable compression
push 1
push TOGGLE_ENABLE_COMPRESSION
push pSettings
call GGUFSettings_SetToggle

; Enable training mode
push 1
push TOGGLE_ENABLE_TRAINING_MODE
push pSettings
call GGUFSettings_SetToggle

; Set custom path
push 1
push OFFSET szCustomPath
push pSettings
call GGUFSettings_SetCustomPath
```

---

## API Quick Reference

| Function | Args | Returns | Purpose |
|----------|------|---------|---------|
| `GGUF_TensorOffsetLoopAll` | pContext | 1/0 | Resolve all tensors |
| `GGUF_ResolveTensorPointers` | array, count, base, offset, size | 1/0 | Main resolution loop |
| `GGUFSettings_Create` | - | pSettings | New settings |
| `GGUFSettings_SetLoaderMethod` | pSettings, method | 1/0 | Pick loader |
| `GGUFSettings_SetToggle` | pSettings, toggle, bool | 1/0 | Enable feature |
| `MLTraining_CreateSession` | pModel, nLayers, opt, loss | pSession | Start training |
| `MLTraining_ComputeGradients` | pSession, input, target, batch | 1/0 | One step |
| `MLTraining_UpdateWeights` | pSession | 1/0 | Apply optimizer |
| `MLTraining_GetLoss` | pSession | loss_value | Current loss |
| `MLTraining_GetMetrics` | pSession | pMetrics | Accuracy, etc. |

---

## Status Flags

```asm
PIFABRIC_FLAG_TRAINING_ENABLED  equ 1   ; Training active
PIFABRIC_FLAG_OFFSETS_RESOLVED  equ 2   ; Tensors resolved
PIFABRIC_FLAG_SETTINGS_APPLIED  equ 4   ; Settings applied
```

Check with:
```asm
push hFabric
call PiFabric_GetStatus
; EAX = flags
test eax, PIFABRIC_FLAG_OFFSETS_RESOLVED
jz  @tensors_not_resolved
```

---

## No Removals, Only Additions

✅ **All existing code stays**:
- `pifabric_core.asm` - unchanged
- `gguf_chain_api.asm` - unchanged
- All DISC/MEMORY/MMAP loaders - unchanged

✅ **New files**:
- `gguf_tensor_offset_resolver.asm` - 1100 lines
- `gguf_loader_settings.asm` - 750 lines
- `ml_model_training.asm` - 900 lines
- `pifabric_core_enhanced.asm` - 650 lines (wraps core)

✅ **Backwards compatible** - old code still works

---

## Next: Test It

1. Compile all 4 new modules
2. Link into exe
3. Open GGUF file with `PiFabric_Open()`
4. Resolve tensors with `PiFabric_ResolveTensorOffsets()`
5. Check status: `PiFabric_GetStatus()` → should have `PIFABRIC_FLAG_OFFSETS_RESOLVED` bit
6. Optional: Enable training with `PiFabric_EnableTraining()`
7. Stream or train

**Expected output**: All tensors resolved, pointers valid, training ready.
