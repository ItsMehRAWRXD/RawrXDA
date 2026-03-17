;==============================================================================
; force_loader.asm - Pure MASM64 Force Model Loader (Hardware Bypass)
; ==========================================================================
; Bypasses hardware limits by using virtual memory and on-the-fly quantization.
; Zero C++ runtime dependencies.
;==============================================================================

.686p
.xmm
.model flat, c
option casemap:none

include windows.inc
include memory_virtual.inc
include quantization.inc
include logging.inc

.data
    szBypassingVRAM     BYTE "Bypassing VRAM limits for model: %s",0
    szVirtualMemory     BYTE "Allocating %lld bytes of virtual compressed memory...",0
    szForceLoadSuccess  BYTE "Model force-loaded into virtual space.",0

.code

;==============================================================================
; ForceLoadModel - Loads a model regardless of physical hardware limits
;==============================================================================
ForceLoadModel PROC uses rbx rsi rdi pModelName:QWORD
    LOCAL fileSize:QWORD
    LOCAL pVirtualMem:QWORD
    
    ; 1. Get Model Size
    invoke GetModelFileSize, pModelName
    mov fileSize, rax
    
    ; 2. Log Bypass
    invoke LogWarning, addr szBypassingVRAM, pModelName
    
    ; 3. Allocate Virtual Compressed Memory
    ; (This uses the memory_virtual.asm logic to page to disk if needed)
    invoke LogInfo, addr szVirtualMemory, fileSize
    invoke VirtualCompressedHeapCreate, fileSize, NULL
    .if rax == 0
        ret
    .endif
    mov pVirtualMem, rax
    
    ; 4. Stream Tensors into Virtual Memory
    ; (On-the-fly quantization to Q2/Q4 to save space)
    invoke StreamTensorsToVirtualMem, pModelName, pVirtualMem
    
    invoke LogSuccess, addr szForceLoadSuccess
    mov rax, pVirtualMem
    ret
ForceLoadModel ENDP

END
