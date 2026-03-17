; RawrXD_DualEngineManager.asm
; Coordinates FP32 and Quantized engines based on load

include masm_hotpatch.inc

.data
g_currentEngine QWORD 0          ; 0 = FP32, 1 = Quantized
g_loadThreshold QWORD 80         ; Switch if CPU > 80%
g_engineSwitchCount QWORD 0

.code

EXTERN RawrXD_AtomicPatch:PROC
EXTERN RawrXD_GetSystemLoad:PROC ; From asm_utils.asm or similar

; Initialize manager
RawrXD_ManagerInit PROC FRAME
    .endprolog
    mov g_currentEngine, 0       ; Default to FP32
    mov g_engineSwitchCount, 0
    ret
RawrXD_ManagerInit ENDP

; Check system load and switch engines if needed
; Returns: 1 if switched, 0 if stable
RawrXD_CheckAndSwitch PROC FRAME
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Get CPU load (stubbed for now, returns rax=load%)
    call RawrXD_GetSystemLoad
    
    cmp rax, g_loadThreshold
    jg SWITCH_TO_QUANTIZED
    
    ; If load low, maybe switch back to FP32?
    cmp rax, 20
    jl SWITCH_TO_FP32
    
    jmp NO_SWITCH

SWITCH_TO_QUANTIZED:
    cmp g_currentEngine, 1
    je NO_SWITCH                 ; Already quantized
    
    ; Perform hotpatch to quantized engine
    ; lea rcx, g_computeFunctionPtr
    ; lea rdx, RawrXD_QuantizedCompute
    ; call RawrXD_AtomicPatch
    
    mov g_currentEngine, 1
    inc g_engineSwitchCount
    mov eax, 1
    jmp EXIT_CHECK

SWITCH_TO_FP32:
    cmp g_currentEngine, 0
    je NO_SWITCH                 ; Already FP32
    
    ; Perform hotpatch to FP32 engine
    ; lea rcx, g_computeFunctionPtr
    ; lea rdx, RawrXD_FP32Compute
    ; call RawrXD_AtomicPatch
    
    mov g_currentEngine, 0
    inc g_engineSwitchCount
    mov eax, 1
    jmp EXIT_CHECK

NO_SWITCH:
    xor eax, eax

EXIT_CHECK:
    add rsp, 32
    pop rbx
    ret
RawrXD_CheckAndSwitch ENDP

; Get current engine status
RawrXD_GetEngineStatus PROC
    mov rax, g_currentEngine
    ret
RawrXD_GetEngineStatus ENDP

END
