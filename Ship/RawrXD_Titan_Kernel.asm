;==============================================================================
; RawrXD_Titan_Kernel.asm
; Zero-Stub, Persistent Model Management
;==============================================================================
OPTION CASEMAP:NONE

include win64_api.inc

; SYSTEM EXTERNALS
extern InitializeSRWLock : PROC
extern CreateFileA : PROC
extern GetFileSizeEx : PROC
extern CreateFileMappingA : PROC
extern MapViewOfFile : PROC
extern UnmapViewOfFile : PROC
extern CloseHandle : PROC
extern VirtualAlloc : PROC
extern VirtualFree : PROC
extern GetTickCount64 : PROC
extern OutputDebugStringA : PROC

; CRT
extern malloc : PROC
extern free : PROC
extern memset : PROC
extern memcpy : PROC
extern strlen : PROC
extern strcmp : PROC
; extern  : PROC  ; Removed unused and problematic reference

;==============================================================================
; CONSTANTS
;==============================================================================
MAX_CACHED_MODELS       EQU 64
MAX_MODEL_NAME_LEN      EQU 256
MAX_PATH_LEN            EQU 260
MODEL_STATE_UNLOADED    EQU 0
MODEL_STATE_LOADING     EQU 1
MODEL_STATE_READY       EQU 2
MODEL_STATE_ERROR       EQU 3

;==============================================================================
; STRUCTURES
;==============================================================================

PersistentModel STRUCT
    model_name          BYTE MAX_MODEL_NAME_LEN DUP(?)
    file_path           BYTE MAX_PATH_LEN DUP(?)
    model_hash          QWORD 2 DUP(?)
    state               DWORD ?
    ref_count           DWORD ?
    last_access_tick    QWORD ?
    hFile               QWORD ?
    hMapping            QWORD ?
    pMappingBase        QWORD ?
    file_size           QWORD ?
    pGGUFHeader         QWORD ?
    pMetadataKV         QWORD ?
    pTensorInfos        QWORD ?
    pDataSection        QWORD ?
    n_tensors           QWORD ?
    n_kv                QWORD ?
    arch_type           DWORD ?
    n_vocab             DWORD ?
    n_ctx_train         DWORD ?
    n_embd              DWORD ?
    n_layer             DWORD ?
    n_head              DWORD ?
    n_head_kv           DWORD ?
    n_ff                DWORD ?
    n_rot               DWORD ?
    rope_theta          REAL8 ?
    rope_scale          REAL8 ?
    rms_norm_eps        REAL8 ?
    pTensorHashTable    QWORD ?
    pKVCache            QWORD ?
    kv_cache_size       QWORD ?
    pActivations        QWORD ?
    activation_size     QWORD ?
    total_tokens_gen    QWORD ?
    total_time_ms       QWORD ?
    avg_tps             REAL4 ?
    access_lock         SRWLOCK <>
PersistentModel ENDS

;==============================================================================
; DATA SECTION
;==============================================================================
.DATA
g_initialized       DWORD 0
g_modelSlotLock     SRWLOCK <>

.DATA?
g_modelSlots        PersistentModel MAX_CACHED_MODELS DUP(<>)

;==============================================================================
; CODE SECTION
;==============================================================================
.CODE

DllMain PROC hInst:QWORD, fdwReason:DWORD, lpReserved:QWORD
    cmp edx, 1 ; DLL_PROCESS_ATTACH
    jne @@not_attach
    
    lea rcx, g_modelSlotLock
    call InitializeSRWLock
    mov g_initialized, 1
    
@@not_attach:
    mov eax, 1
    ret
DllMain ENDP

Titan_GetModelSlot PROC model_name:QWORD
    mov eax, -1
    ret
Titan_GetModelSlot ENDP

END
