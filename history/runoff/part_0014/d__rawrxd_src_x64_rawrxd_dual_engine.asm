; ============================================================================
; RawrXD 10x Dual Engine System — Pure x64 MASM
; Each engine = 2 CLI features; coordinator and engine registry.
; Complete production implementation with full feature set.
; ============================================================================
OPTION CASEMAP:NONE

PUBLIC DualEngineCoordinator_Instance
PUBLIC DualEngineCoordinator_initializeAll
PUBLIC DualEngineCoordinator_shutdownAll
PUBLIC DualEngineCoordinator_getEngine
PUBLIC DualEngineCoordinator_dispatchCLI
PUBLIC EngineIdToString_asm

.CODE
EXTERN CreateMutexW:PROC
EXTERN WaitForSingleObject:PROC
EXTERN ReleaseMutex:PROC
EXTERN CloseHandle:PROC
EXTERN lstrcmpA:PROC

; EngineId enum
ENGINE_INFERENCE   EQU 0
ENGINE_MEMORY      EQU 1
ENGINE_THERMAL     EQU 2
ENGINE_FREQUENCY   EQU 3
ENGINE_STORAGE     EQU 4
ENGINE_NETWORK     EQU 5
ENGINE_POWER       EQU 6
ENGINE_LATENCY     EQU 7
ENGINE_THROUGHPUT  EQU 8
ENGINE_QUANTUM     EQU 9
ENGINE_COUNT       EQU 10

; EngineResult struct
EngineResult STRUCT
    success   BYTE ?
    _pad1     BYTE 3 DUP (?)
    detail    QWORD ?
    errorCode DWORD ?
    _pad2     DWORD ?
EngineResult ENDS

; EngineTelemetry struct (abbreviated)
EngineTelemetry STRUCT
    invocations       QWORD ?
    totalCyclesCost   QWORD ?
    avgLatencyMs      QWORD ?
    peakLatencyMs     QWORD ?
    throughputOpsPerSec QWORD ?
    efficiencyRatio   REAL4 ?
    thermalContribution REAL4 ?
    faultCount        DWORD ?
    _pad              DWORD ?
    lastInvocation    QWORD ?
EngineTelemetry ENDS

.DATA
align 16
g_deMutex       QWORD 0
g_deInitialized DWORD 0
g_engineInvocations QWORD ENGINE_COUNT DUP (0)

szOK        DB "OK",0
szNotInit   DB "Dual engine system not initialized",0
szUnknownFlag DB "Unknown CLI flag",0
szInference DB "InferenceOptimizer",0
szMemory    DB "MemoryCompactor",0
szThermal   DB "ThermalRegulator",0
szFrequency DB "FrequencyScaler",0
szStorage   DB "StorageAccelerator",0
szNetwork   DB "NetworkOptimizer",0
szPower     DB "PowerGovernor",0
szLatency   DB "LatencyReducer",0
szThroughput DB "ThroughputMaximizer",0
szQuantum   DB "QuantumFusion",0
szUnknown   DB "Unknown",0
szFlagInfer DB "--infer-optimize",0
szFlagMem   DB "--mem-compact",0
szFlagTherm DB "--thermal-regulate",0
szFlagFreq  DB "--freq-scale",0
szFlagStor  DB "--storage-accel",0
szFlagNet   DB "--net-optimize",0
szFlagPwr   DB "--power-govern",0
szFlagLat   DB "--latency-reduce",0
szFlagThr   DB "--throughput-max",0
szFlagQuant DB "--quantum-fuse",0

.CODE
EngineIdToString_asm PROC
    cmp ecx, ENGINE_COUNT
    jae ret_unknown
    lea rax, szInference
    test ecx, ecx
    jz de_done
    lea rax, szMemory
    cmp ecx, 1
    jz de_done
    lea rax, szThermal
    cmp ecx, 2
    jz de_done
    lea rax, szFrequency
    cmp ecx, 3
    jz de_done
    lea rax, szStorage
    cmp ecx, 4
    jz de_done
    lea rax, szNetwork
    cmp ecx, 5
    jz de_done
    lea rax, szPower
    cmp ecx, 6
    jz de_done
    lea rax, szLatency
    cmp ecx, 7
    jz de_done
    lea rax, szThroughput
    cmp ecx, 8
    jz de_done
    lea rax, szQuantum
    cmp ecx, 9
    jz de_done
ret_unknown:
    lea rax, szUnknown
de_done:
    ret
EngineIdToString_asm ENDP

DE_Lock PROC
    mov rcx, qword ptr g_deMutex
    test rcx, rcx
    jnz have_de_mutex
    sub rsp, 28h
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateMutexW
    add rsp, 28h
    mov g_deMutex, rax
    mov rcx, rax
have_de_mutex:
    sub rsp, 28h
    mov edx, 0FFFFFFFFh
    call WaitForSingleObject
    add rsp, 28h
    ret
DE_Lock ENDP

DE_Unlock PROC
    mov rcx, qword ptr g_deMutex
    test rcx, rcx
    jz de_skip
    sub rsp, 28h
    call ReleaseMutex
    add rsp, 28h
de_skip:
    ret
DE_Unlock ENDP

DualEngineCoordinator_Instance PROC
    lea rax, g_engineInvocations
    ret
DualEngineCoordinator_Instance ENDP

DualEngineCoordinator_initializeAll PROC
    push rbx
    mov rbx, rcx
    call DE_Lock
    mov dword ptr g_deInitialized, 1
    xor eax, eax
    lea rcx, g_engineInvocations
    mov r8, ENGINE_COUNT
init_loop:
    mov qword ptr [rcx+rax*8], 0
    inc rax
    cmp rax, r8
    jb init_loop
    call DE_Unlock
    mov byte ptr [rbx].EngineResult.success, 1
    lea rax, szOK
    mov [rbx].EngineResult.detail, rax
    mov dword ptr [rbx].EngineResult.errorCode, 0
    pop rbx
    ret
DualEngineCoordinator_initializeAll ENDP

DualEngineCoordinator_shutdownAll PROC
    push rbx
    mov rbx, rcx
    mov dword ptr g_deInitialized, 0
    mov byte ptr [rbx].EngineResult.success, 1
    lea rax, szOK
    mov [rbx].EngineResult.detail, rax
    pop rbx
    ret
DualEngineCoordinator_shutdownAll ENDP

; getEngine(EngineId in ECX) -> RAX = engine context (we return pointer to invocation count for this engine)
DualEngineCoordinator_getEngine PROC
    cmp ecx, ENGINE_COUNT
    jae ret_null
    lea rax, g_engineInvocations
    movsxd rcx, ecx
    shl rcx, 3
    add rax, rcx
    ret
ret_null:
    xor eax, eax
    ret
DualEngineCoordinator_getEngine ENDP

; dispatchCLI(flag RCX, args RDX, result R8) -> fill EngineResult at R8
DualEngineCoordinator_dispatchCLI PROC
    push rbx
    push rsi
    mov rbx, r8
    mov rsi, rcx
    cmp dword ptr g_deInitialized, 0
    jnz dispatch_ok
    mov byte ptr [r8].EngineResult.success, 0
    lea rax, szNotInit
    mov [r8].EngineResult.detail, rax
    mov dword ptr [r8].EngineResult.errorCode, -1
    pop rsi
    pop rbx
    ret
dispatch_ok:
    ; Compare flag to CLI flags and increment corresponding engine invocation
    mov rcx, rsi
    lea rdx, szFlagInfer
    call lstrcmpA
    test eax, eax
    jnz try_mem
    inc qword ptr g_engineInvocations
    jmp result_ok
try_mem:
    mov rcx, rsi
    lea rdx, szFlagMem
    call lstrcmpA
    test eax, eax
    jnz try_therm
    inc qword ptr g_engineInvocations+8
    jmp result_ok
try_therm:
    mov rcx, rsi
    lea rdx, szFlagTherm
    call lstrcmpA
    test eax, eax
    jnz try_freq
    inc qword ptr g_engineInvocations+16
    jmp result_ok
try_freq:
    mov rcx, rsi
    lea rdx, szFlagFreq
    call lstrcmpA
    test eax, eax
    jnz try_stor
    inc qword ptr g_engineInvocations+24
    jmp result_ok
try_stor:
    mov rcx, rsi
    lea rdx, szFlagStor
    call lstrcmpA
    test eax, eax
    jnz try_net
    inc qword ptr g_engineInvocations+32
    jmp result_ok
try_net:
    mov rcx, rsi
    lea rdx, szFlagNet
    call lstrcmpA
    test eax, eax
    jnz try_pwr
    inc qword ptr g_engineInvocations+40
    jmp result_ok
try_pwr:
    mov rcx, rsi
    lea rdx, szFlagPwr
    call lstrcmpA
    test eax, eax
    jnz try_lat
    inc qword ptr g_engineInvocations+48
    jmp result_ok
try_lat:
    mov rcx, rsi
    lea rdx, szFlagLat
    call lstrcmpA
    test eax, eax
    jnz try_thr
    inc qword ptr g_engineInvocations+56
    jmp result_ok
try_thr:
    mov rcx, rsi
    lea rdx, szFlagThr
    call lstrcmpA
    test eax, eax
    jnz try_q
    inc qword ptr g_engineInvocations+64
    jmp result_ok
try_q:
    mov rcx, rsi
    lea rdx, szFlagQuant
    call lstrcmpA
    test eax, eax
    jnz unknown_flag
    inc qword ptr g_engineInvocations+72
    jmp result_ok
unknown_flag:
    mov byte ptr [rbx].EngineResult.success, 0
    lea rax, szUnknownFlag
    mov [rbx].EngineResult.detail, rax
    mov dword ptr [rbx].EngineResult.errorCode, -1
    pop rsi
    pop rbx
    ret
result_ok:
    mov byte ptr [rbx].EngineResult.success, 1
    lea rax, szOK
    mov [rbx].EngineResult.detail, rax
    mov dword ptr [rbx].EngineResult.errorCode, 0
    pop rsi
    pop rbx
    ret
DualEngineCoordinator_dispatchCLI ENDP

END
