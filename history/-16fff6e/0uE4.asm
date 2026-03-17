; pifabric_ide_integration.asm - Wire PiFabric to IDE Main Loop
; THIS WEEK Task #1: Complete integration with message loop
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
include user32.inc

includelib kernel32.lib
includelib user32.lib

EXTERN PiFabric_Init:PROC
EXTERN PiFabric_Open:PROC
EXTERN PiFabric_Close:PROC
EXTERN PiFabric_Stream:PROC
EXTERN PiFabric_SetTier:PROC
EXTERN UiStatusbar_UpdateText:PROC
EXTERN UiModelBrowser_Refresh:PROC

PUBLIC IdeIntegration_Init
PUBLIC IdeIntegration_ProcessMessage
PUBLIC IdeIntegration_LoadModel
PUBLIC IdeIntegration_UnloadModel
PUBLIC IdeIntegration_SetQualityTier
PUBLIC IdeIntegration_Shutdown

; Custom window messages
WM_PIFABRIC_LOAD_MODEL      EQU WM_USER + 100
WM_PIFABRIC_UNLOAD_MODEL    EQU WM_USER + 101
WM_PIFABRIC_SET_TIER        EQU WM_USER + 102
WM_PIFABRIC_STREAM_UPDATE   EQU WM_USER + 103

.data
g_hMainWindow       dd 0
g_hPiFabricHandle   dd 0
g_CurrentTier       dd 2        ; Default: TIER_BALANCED
g_bInitialized      dd 0
g_szCurrentModel    db 260 dup(0)

szStatusLoading     db "Loading model...",0
szStatusReady       db "Model ready",0
szStatusUnloading   db "Unloading...",0
szStatusIdle        db "Idle",0
szStatusError       db "Error loading model",0

.code

; ============================================================
; IdeIntegration_Init - Initialize PiFabric in IDE context
; Input:  ECX = main window handle
; Output: EAX = 1 success, 0 failure
; ============================================================
IdeIntegration_Init PROC hWnd:DWORD
    push ebx
    push esi
    
    mov ebx, hWnd
    mov [g_hMainWindow], ebx
    
    ; Initialize PiFabric core
    call PiFabric_Init
    test eax, eax
    jz @fail
    
    ; Mark as initialized
    mov [g_bInitialized], 1
    
    ; Update status bar
    push OFFSET szStatusIdle
    call UiStatusbar_UpdateText
    
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop esi
    pop ebx
    ret
IdeIntegration_Init ENDP

; ============================================================
; IdeIntegration_ProcessMessage - Handle PiFabric messages
; Input:  ECX = hWnd, EDX = uMsg, ESI = wParam, EDI = lParam
; Output: EAX = message handled (1) or pass to DefWindowProc (0)
; ============================================================
IdeIntegration_ProcessMessage PROC hWnd:DWORD, uMsg:DWORD, wParam:DWORD, lParam:DWORD
    push ebx
    push esi
    push edi
    
    mov ebx, uMsg
    
    ; Check for PiFabric messages
    cmp ebx, WM_PIFABRIC_LOAD_MODEL
    je @handle_load
    cmp ebx, WM_PIFABRIC_UNLOAD_MODEL
    je @handle_unload
    cmp ebx, WM_PIFABRIC_SET_TIER
    je @handle_tier
    cmp ebx, WM_PIFABRIC_STREAM_UPDATE
    je @handle_stream
    
    ; Not a PiFabric message
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
    
@handle_load:
    ; wParam = pointer to model path
    mov esi, wParam
    push esi
    call IdeIntegration_LoadModel
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@handle_unload:
    call IdeIntegration_UnloadModel
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@handle_tier:
    ; wParam = new tier (0-4)
    mov esi, wParam
    push esi
    call IdeIntegration_SetQualityTier
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@handle_stream:
    ; Handle streaming update
    ; wParam = tensor index, lParam = progress
    call HandleStreamUpdate
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
IdeIntegration_ProcessMessage ENDP

; ============================================================
; IdeIntegration_LoadModel - Load GGUF model via PiFabric
; Input:  ECX = pointer to model path
; Output: EAX = handle or 0 on failure
; ============================================================
IdeIntegration_LoadModel PROC lpPath:DWORD
    push ebx
    push esi
    push edi
    
    mov esi, lpPath
    
    ; Check if already loaded
    cmp [g_hPiFabricHandle], 0
    jne @already_loaded
    
    ; Update status
    push OFFSET szStatusLoading
    call UiStatusbar_UpdateText
    
    ; Copy model path
    push 260
    push OFFSET g_szCurrentModel
    push esi
    call lstrcpyn
    
    ; Open model via PiFabric
    push esi
    call PiFabric_Open
    test eax, eax
    jz @load_failed
    
    mov [g_hPiFabricHandle], eax
    
    ; Update status
    push OFFSET szStatusReady
    call UiStatusbar_UpdateText
    
    ; Refresh model browser
    call UiModelBrowser_Refresh
    
    mov eax, [g_hPiFabricHandle]
    pop edi
    pop esi
    pop ebx
    ret
    
@already_loaded:
    ; Unload current first
    call IdeIntegration_UnloadModel
    jmp IdeIntegration_LoadModel
    
@load_failed:
    push OFFSET szStatusError
    call UiStatusbar_UpdateText
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
IdeIntegration_LoadModel ENDP

; ============================================================
; IdeIntegration_UnloadModel - Unload current model
; Output: EAX = 1 success
; ============================================================
IdeIntegration_UnloadModel PROC
    push ebx
    
    cmp [g_hPiFabricHandle], 0
    je @not_loaded
    
    ; Update status
    push OFFSET szStatusUnloading
    call UiStatusbar_UpdateText
    
    ; Close via PiFabric
    push [g_hPiFabricHandle]
    call PiFabric_Close
    
    mov [g_hPiFabricHandle], 0
    
    ; Clear model path
    mov byte ptr [g_szCurrentModel], 0
    
    ; Update status
    push OFFSET szStatusIdle
    call UiStatusbar_UpdateText
    
    ; Refresh UI
    call UiModelBrowser_Refresh
    
@not_loaded:
    mov eax, 1
    pop ebx
    ret
IdeIntegration_UnloadModel ENDP

; ============================================================
; IdeIntegration_SetQualityTier - Adjust quality/speed tier
; Input:  ECX = tier (0=fastest, 4=highest quality)
; Output: EAX = 1 success
; ============================================================
IdeIntegration_SetQualityTier PROC tier:DWORD
    push ebx
    
    mov ebx, tier
    
    ; Validate tier (0-4)
    cmp ebx, 4
    ja @invalid
    
    mov [g_CurrentTier], ebx
    
    ; Apply to PiFabric if model loaded
    cmp [g_hPiFabricHandle], 0
    je @done
    
    push ebx
    push [g_hPiFabricHandle]
    call PiFabric_SetTier
    
@done:
    mov eax, 1
    pop ebx
    ret
    
@invalid:
    xor eax, eax
    pop ebx
    ret
IdeIntegration_SetQualityTier ENDP

; ============================================================
; IdeIntegration_Shutdown - Cleanup on IDE exit
; ============================================================
IdeIntegration_Shutdown PROC
    push ebx
    
    ; Unload any active model
    call IdeIntegration_UnloadModel
    
    mov [g_bInitialized], 0
    
    mov eax, 1
    pop ebx
    ret
IdeIntegration_Shutdown ENDP

; ============================================================
; Helper: HandleStreamUpdate - Process streaming updates
; ============================================================
HandleStreamUpdate PROC
    ; Stub: Update progress UI based on streaming status
    mov eax, 1
    ret
HandleStreamUpdate ENDP

END
