# 🔧 GGUF Tensor Resolver - Integration Guide

## ✅ What This Fixes

Your GGUF loader currently:
- ✅ Loads file into memory
- ✅ Parses header
- ✅ Parses KV metadata
- ✅ Reads tensor count
- ❌ **STOPS HERE - Never resolves tensor pointers**
- ❌ Returns null model

This patch adds:
- ✅ Data section offset calculation
- ✅ Tensor pointer resolution
- ✅ Bounds validation
- ✅ Size computation
- ✅ Model struct population

---

## 📦 Files Added (NO FILES REMOVED)

### 1. `gguf_tensor_resolver.asm` (Core resolver)
- `GGUF_ComputeDataSectionOffset` - Calculates data section start
- `GGUF_ComputeTensorByteSize` - Computes tensor sizes
- `GGUF_ResolveTensorPointers` - Resolves all tensor data pointers
- `GGUF_ValidateTensorIntegrity` - Validates bounds and integrity
- `GGUF_PopulateModelStruct` - Sets all model struct fields
- `GGUF_ResolverComplete` - Master function (call this once)

### 2. `gguf_loader_integration.asm` (Integration bridge)
- `GGUF_LoaderWithResolver` - Complete loader with resolver
- `GGUF_PatchExistingLoader` - One-line fix for existing loader
- `GGUF_IntegrationTest` - Test with real Ollama models
- `GGUF_LogTensorInfo` - Debug helper

---

## 🔧 Integration Point - EXACT LOCATION

Find this in your existing GGUF loader:

```asm
; Your existing loader code:
GGUFLoader_Load proc pszFilePath:DWORD
    ; ... load file ...
    ; ... parse header ...
    ; ... parse KV metadata ...
    ; ... parse tensor metadata ...
    
    ; ===== INSERT RESOLVER CALL HERE =====
    ; (Right after tensor metadata parsing completes)
    
    ; OLD CODE: returns null because tensors not resolved
    mov eax, pModel
    ret
GGUFLoader_Load endp
```

**ADD THIS ONE LINE:**

```asm
; Your existing loader code:
GGUFLoader_Load proc pszFilePath:DWORD
    LOCAL pModel:DWORD
    LOCAL base_ptr:DWORD
    LOCAL file_size:DWORD
    ; ... other locals ...
    
    ; ... load file ...
    ; ... parse header ...
    ; ... parse KV metadata ...
    ; ... parse tensor metadata ...
    
    ; ===== NEW: CALL RESOLVER =====
    invoke GGUF_ResolverComplete, \
           pModel, base_ptr, header_size, kv_size, tensor_meta_size, \
           tensor_array_ptr, tensor_count, file_size
    test eax, eax
    jz @LoadFailed
    ; ===== END NEW CODE =====
    
    ; Now model is fully populated
    mov eax, pModel
    ret
    
@LoadFailed:
    xor eax, eax
    ret
GGUFLoader_Load endp
```

---

## 📋 Prerequisites - What You Need

The resolver needs these values from your existing loader:

| Value | Description | Where It Comes From |
|-------|-------------|---------------------|
| `base_ptr` | Memory pointer to loaded file | Your `LoadFileToMemory` function |
| `file_size` | Total file size | GetFileSize() or file info |
| `header_size` | Size of GGUF header | Computed during header parse (typically 32-64 bytes) |
| `kv_size` | Size of KV metadata block | Computed during KV parse |
| `tensor_meta_size` | Size of tensor metadata | Computed during tensor metadata parse |
| `tensor_array_ptr` | Pointer to tensor metadata array | Allocated during tensor parse |
| `tensor_count` | Number of tensors | Read from header |
| `pModel` | Pointer to model struct | Your model allocation |

**If you don't have these values**, add them to your existing parsing functions:

```asm
; Example: Modify your header parser to return size
GGUFLoader_ParseHeader proc base_ptr:DWORD, pModel:DWORD
    ; ... parse header ...
    
    ; Compute header size
    mov eax, 32                  ; Base header
    add eax, kv_count_bytes
    ; ... add other fields ...
    
    ret                          ; EAX = header size
GGUFLoader_ParseHeader endp
```

---

## 🔍 Debugging - Verify It Works

### Test 1: Load Ollama 350M model
```asm
invoke GGUF_LoaderWithResolver, \
       offset szModel350M, addr model
test eax, eax
jz @Failed

; Check tensor count
mov edi, eax
mov ecx, [edi+8]               ; model.tensor_count
; Should be > 0 (typically 100-200 tensors)
```

### Test 2: Verify tensor pointers
```asm
invoke GGUF_LogTensorInfo, \
       tensor_array_ptr, tensor_count
test eax, eax
jz @Failed
; All tensors should have non-zero data_ptr and byte_size
```

### Test 3: Load 1B model
```asm
invoke GGUF_LoaderWithResolver, \
       offset szModel1B, addr model
test eax, eax
jz @Failed
; Should succeed with larger model
```

---

## 📊 What Gets Fixed

### Before (Current State)
```
GGUF file loaded into memory:
[Header: 32 bytes]
[KV metadata: 1,024 bytes]
[Tensor metadata: 10,240 bytes]
[Data section: 700 MB]
         ↑
         ❌ Loader stops here - never resolves pointers
         ❌ Returns null model
```

### After (With Resolver)
```
GGUF file loaded into memory:
[Header: 32 bytes] ✅ Parsed
[KV metadata: 1,024 bytes] ✅ Parsed
[Tensor metadata: 10,240 bytes] ✅ Parsed
[Data section: 700 MB] ✅ Tensor pointers resolved
         ↑
         ✅ data_section_ptr computed
         ✅ All tensor.data_ptr set
         ✅ All tensor.byte_size computed
         ✅ Bounds validated
         ✅ Model struct populated
         ✅ Returns valid model
```

---

## 🚀 Testing With Real Ollama Models

Once integrated, test with your actual models:

### 350M Model (Small, Fast)
```
Path: D:\Franken\BackwardsUnlock\350m\unlock-350M-Q3_K_M.gguf
Size: ~350 MB
Tensors: ~100-150
Expected load time: <1 second
```

### 1B Model (Medium)
```
Path: D:\Franken\BackwardsUnlock\1b\unlock-1B-Q4_K_M.gguf
Size: ~1 GB
Tensors: ~150-200
Expected load time: 2-3 seconds
```

### Llama Vocab (Metadata Only)
```
Path: D:\llama.cpp\models\ggml-vocab-llama.gguf
Size: ~500 KB
Tensors: 0 (vocab only)
Expected load time: <100 ms
```

---

## ⚠️ Common Issues

### Issue 1: "Bounds error on first tensor"
**Cause:** `header_size`, `kv_size`, or `tensor_meta_size` not computed correctly  
**Fix:** Add size tracking to your existing parsers

### Issue 2: "All tensor pointers are null"
**Cause:** `data_section_ptr` calculation wrong  
**Fix:** Verify: `data_section_ptr = base_ptr + header_size + kv_size + tensor_meta_size`

### Issue 3: "Tensor byte_size is 0"
**Cause:** Tensor dimensions or type invalid  
**Fix:** Verify dims[] array populated during tensor metadata parse

### Issue 4: "Model struct not populated"
**Cause:** Field offsets in `GGUF_PopulateModelStruct` don't match your struct  
**Fix:** Adjust offsets in `gguf_tensor_resolver.asm` lines 300-320

---

## 📝 Checklist

- [ ] Add `gguf_tensor_resolver.asm` to project
- [ ] Add `gguf_loader_integration.asm` to project
- [ ] Update your loader to track `header_size`, `kv_size`, `tensor_meta_size`
- [ ] Insert `GGUF_ResolverComplete` call after tensor metadata parsing
- [ ] Compile and test with 350M model
- [ ] Verify tensor count > 0
- [ ] Verify all tensor pointers non-null
- [ ] Test with 1B model
- [ ] Test with vocab-only GGUF
- [ ] Enable π-RAM compression hooks
- [ ] Test with models > 10GB

---

## ✅ Success Criteria

Your loader is FIXED when:
- ✅ `GGUF_LoaderWithResolver` returns non-null model
- ✅ `model.tensor_count` > 0
- ✅ `model.tensor_array_ptr` is valid
- ✅ All `tensor[i].data_ptr` are non-null
- ✅ All `tensor[i].byte_size` match expected sizes
- ✅ Bounds checks pass (no overflow)
- ✅ Works with real Ollama models from D:\

---

## 🎯 Next Steps After This Works

Once tensor resolution is working:
1. Wire to settings UI for model selection
2. Add vocab loading (for tokenization)
3. Add tensor compression hooks (π-RAM)
4. Add multi-pass loading for >64GB models
5. Add inference engine integration

---

## 📞 Support

If resolver fails:
1. Check `GGUF_LogTensorInfo` output
2. Verify `data_section_ptr` is correct (should be > header+kv+meta)
3. Check first tensor offset (should be 0 or small value)
4. Verify file loaded completely (file_size matches actual)

---

**Status: Ready to integrate - NO EXISTING CODE NEEDS TO BE REMOVED**
