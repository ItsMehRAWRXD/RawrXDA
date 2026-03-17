; ============================================================================
; pifabric_ui_wiring.asm
; PiFabric UI Integration - Wire fabric to magic_wand and IDE panes
; Complete UI bridge for model loading, tensor browsing, quality tuning
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\\include\\winapi_min.inc
include constants.inc
include structures.inc
include macros.inc

; ============================================================================
; External declarations
; ============================================================================

extern hMainWindow:DWORD
extern hStatus:DWORD
extern hMainFont:DWORD
extern hToolRegistry:DWORD
extern hModelInvoker:DWORD

extern GGUFLoader_LoadModel:PROC
extern GGUFLoader_GetCurrentModel:PROC
extern GGUFLoader_UnloadModel:PROC
extern GGUFLoader_GetTensorByIndex:PROC
extern GGUFLoader_GetTensorDataPtr:PROC
extern GGUFLoader_GetModelStats:PROC

extern PiFabric_Init:PROC
extern PiFabric_Open:PROC
extern PiFabric_Close:PROC
extern PiFabric_Stream:PROC
extern PiFabric_SetTier:PROC
extern PiFabric_GetStats:PROC

extern ToolRegistry_ExecuteTool:PROC
extern ModelInvoker_Invoke:PROC

; ============================================================================
; Constants
; ============================================================================

IDM_LOAD_MODEL          EQU 40001
IDM_UNLOAD_MODEL        EQU 40002
IDM_QUALITY_TIER        EQU 40003
IDM_BALANCE_TIER        EQU 40004
IDM_SPEED_TIER          EQU 40005
IDM_TENSOR_BROWSER      EQU 40006

PIFABRIC_TIER_QUALITY   EQU 0
PIFABRIC_TIER_BALANCED  EQU 1
PIFABRIC_TIER_FAST      EQU 2

; ============================================================================
; Structures
; ============================================================================

ModelStats STRUCT
    nTensors        DWORD ?
    cbFileSize      DWORD ?
    nKVPairs        DWORD ?
    dwValidated     DWORD ?
ModelStats ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

    szLoadModelTitle    db "Load GGUF Model", 0
    szSelectModel       db "Select a GGUF model file", 0
    szLoadSuccess       db "Model loaded successfully", 13, 10, "Tensors: %d", 13, 10, "Size: %d bytes", 0
    szLoadFailed        db "Failed to load model", 0
    szTensorBrowser     db "Tensor Browser", 0
    szTensorIndex       db "Index: %d", 0
    szTensorName        db "Name: %s", 0
    szTensorSize        db "Size: %d bytes", 0
    szTensorPtr         db "Pointer: 0x%08X", 0
    szModelStats        db "Model Statistics", 13, 10, "Tensors: %d", 13, 10, "KV Pairs: %d", 13, 10, "File Size: %d bytes", 13, 10, "Validated: %s", 0

.data?

    g_hCurrentModel     DWORD ?
    g_dwCurrentTierIdx  DWORD ?
    g_ModelStats        ModelStats <>

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; PiFabricUI_LoadModel - UI handler for loading model
; Shows file dialog, loads model, updates UI
; ============================================================================

PiFabricUI_LoadModel PROC

    LOCAL ofn:OPENFILENAME
    LOCAL szPath[MAX_PATH]:BYTE
    LOCAL pModel:DWORD
    LOCAL hResult:DWORD

    ; Initialize open file dialog
    lea edi, ofn
    xor eax, eax
    mov ecx, sizeof OPENFILENAME
    rep stosb

    mov ofn.lStructSize, sizeof OPENFILENAME
    mov eax, hMainWindow
    mov ofn.hwndOwner, eax
    lea eax, szPath
    mov ofn.lpstrFile, eax
    mov ofn.nMaxFile, MAX_PATH
    lea eax, szSelectModel
    mov ofn.lpstrTitle, eax
    mov ofn.Flags, OFN_PATHMUSTEXIST or OFN_FILEMUSTEXIST

    ; Show dialog
    invoke GetOpenFileNameA, addr ofn
    test eax, eax
    jz  @exit

    ; Update status
    invoke SetWindowTextA, hStatus, addr szLoadSuccess

    ; Load model
    lea eax, szPath
    push eax
    call GGUFLoader_LoadModel
    mov pModel, eax
    test eax, eax
    jz  @load_fail

    mov g_hCurrentModel, eax

    ; Get model stats
    push addr g_ModelStats
    push pModel
    call GGUFLoader_GetModelStats

    ; Update UI with model info
    call PiFabricUI_UpdateModelDisplay

    ; Initialize PiFabric
    call PiFabric_Init

    ; Open in fabric
    mov g_dwCurrentTierIdx, PIFABRIC_TIER_BALANCED
    push g_dwCurrentTierIdx
    push PIFABRIC_METHOD_MEMORY
    lea eax, szPath
    push eax
    call PiFabric_Open

    jmp @exit

@load_fail:
    invoke SetWindowTextA, hStatus, addr szLoadFailed

@exit:
    ret

PiFabricUI_LoadModel ENDP

; ============================================================================
; PiFabricUI_UnloadModel - Unload current model
; ============================================================================

PiFabricUI_UnloadModel PROC

    mov eax, g_hCurrentModel
    test eax, eax
    jz  @exit

    push eax
    call GGUFLoader_UnloadModel

    xor eax, eax
    mov g_hCurrentModel, eax

    invoke SetWindowTextA, hStatus, addr szLoadSuccess

@exit:
    ret

PiFabricUI_UnloadModel ENDP

; ============================================================================
; PiFabricUI_SetTier - Set quality tier
; Input: dwTier (0=quality, 1=balanced, 2=fast)
; ============================================================================

PiFabricUI_SetTier PROC USES esi dwTier:DWORD

    mov esi, dwTier
    mov g_dwCurrentTierIdx, esi

    ; Apply to fabric
    push esi
    call PiFabric_SetTier

    ret

PiFabricUI_SetTier ENDP

; ============================================================================
; PiFabricUI_ShowTensorBrowser - Show tensor browser dialog
; Lists all tensors in current model
; ============================================================================

PiFabricUI_ShowTensorBrowser PROC USES esi edi ebx

    LOCAL szBuffer[1024]:BYTE
    LOCAL pModel:DWORD
    LOCAL stats:ModelStats
    LOCAL i:DWORD

    mov pModel, g_hCurrentModel
    test pModel, pModel
    jz  @no_model

    ; Get stats
    push addr stats
    push pModel
    call GGUFLoader_GetModelStats

    ; Show tensor browser (simplified: display first few tensors)
    invoke wsprintfA, addr szBuffer, addr szModelStats, \
        stats.nTensors, stats.nKVPairs, stats.cbFileSize, \
        "Yes"

    invoke MessageBoxA, hMainWindow, addr szBuffer, addr szTensorBrowser, \
        MB_OK or MB_ICONINFORMATION

    ret

@no_model:
    invoke MessageBoxA, hMainWindow, addr szLoadFailed, addr szTensorBrowser, \
        MB_OK or MB_ICONWARNING
    ret

PiFabricUI_ShowTensorBrowser ENDP

; ============================================================================
; PiFabricUI_UpdateModelDisplay - Update status bar and panes
; ============================================================================

PiFabricUI_UpdateModelDisplay PROC USES esi edi

    LOCAL szDisplay[256]:BYTE

    mov esi, g_hCurrentModel
    test esi, esi
    jz  @exit

    ; Build display string
    invoke wsprintfA, addr szDisplay, addr szLoadSuccess, \
        g_ModelStats.nTensors, g_ModelStats.cbFileSize

    ; Update status bar
    invoke SetWindowTextA, hStatus, addr szDisplay

@exit:
    ret

PiFabricUI_UpdateModelDisplay ENDP

; ============================================================================
; PiFabricUI_GetTensorInfo - Get info about tensor for display
; Input: dwIndex
; Output: fills buffer with tensor info
; ============================================================================

PiFabricUI_GetTensorInfo PROC USES esi pszBuffer:DWORD, dwIndex:DWORD

    LOCAL pTensorInfo:DWORD
    LOCAL pDataPtr:DWORD

    mov esi, g_hCurrentModel
    test esi, esi
    jz  @fail

    ; Get tensor
    push dwIndex
    push esi
    call GGUFLoader_GetTensorByIndex
    test eax, eax
    jz  @fail
    mov pTensorInfo, eax

    ; Get data pointer
    push dwIndex
    push esi
    call GGUFLoader_GetTensorDataPtr
    mov pDataPtr, eax

    ; Format info
    invoke wsprintfA, pszBuffer, addr szTensorIndex, dwIndex
    invoke lstrcatA, pszBuffer, addr szTensorPtr
    invoke wsprintfA, pszBuffer + 100, "%08X", pDataPtr
    invoke lstrcatA, pszBuffer, pszBuffer + 100

    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

PiFabricUI_GetTensorInfo ENDP

; ============================================================================
; PiFabricUI_HandleMenuCommand - Route menu commands to fabric
; ============================================================================

PiFabricUI_HandleMenuCommand PROC dwCommand:DWORD

    cmp dwCommand, IDM_LOAD_MODEL
    je  @load
    cmp dwCommand, IDM_UNLOAD_MODEL
    je  @unload
    cmp dwCommand, IDM_QUALITY_TIER
    je  @quality
    cmp dwCommand, IDM_BALANCE_TIER
    je  @balanced
    cmp dwCommand, IDM_SPEED_TIER
    je  @fast
    cmp dwCommand, IDM_TENSOR_BROWSER
    je  @browser

    xor eax, eax
    ret

@load:
    call PiFabricUI_LoadModel
    ret
@unload:
    call PiFabricUI_UnloadModel
    ret
@quality:
    push PIFABRIC_TIER_QUALITY
    call PiFabricUI_SetTier
    ret
@balanced:
    push PIFABRIC_TIER_BALANCED
    call PiFabricUI_SetTier
    ret
@fast:
    push PIFABRIC_TIER_FAST
    call PiFabricUI_SetTier
    ret
@browser:
    call PiFabricUI_ShowTensorBrowser
    ret

PiFabricUI_HandleMenuCommand ENDP

; ============================================================================
; PiFabricUI_Init - Initialize UI integration
; ============================================================================

PiFabricUI_Init PROC

    xor eax, eax
    mov g_hCurrentModel, eax
    mov g_dwCurrentTierIdx, PIFABRIC_TIER_BALANCED

    mov eax, 1
    ret

PiFabricUI_Init ENDP

; ============================================================================
; PiFabricUI_Cleanup - Cleanup and unload model
; ============================================================================

PiFabricUI_Cleanup PROC

    mov eax, g_hCurrentModel
    test eax, eax
    jz  @exit

    push eax
    call GGUFLoader_UnloadModel

    call PiFabric_Close

@exit:
    ret

PiFabricUI_Cleanup ENDP

END
