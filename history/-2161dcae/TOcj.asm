; ============================================================================
; GGUF_IDE_BRIDGE.ASM - IDE Integration Bridge for GGUF Loaders
; Manages communication between MASM GGUF loaders and the IDE
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc
include gguf_loader_interface.inc

; ============================================================================
; EXPORTS
; ============================================================================

PUBLIC GGUF_IDE_RegisterLoader
PUBLIC GGUF_IDE_SetProgressCallback
PUBLIC GGUF_IDE_SetStatusCallback
PUBLIC GGUF_IDE_SetModelLoadedCallback
PUBLIC GGUF_IDE_NotifyProgress
PUBLIC GGUF_IDE_NotifyStatus
PUBLIC GGUF_IDE_NotifyModelLoaded
PUBLIC GGUF_IDE_GetActiveLoader

.data
    szBridgeInit    db "[GGUF-IDE-Bridge] Initialized",13,10,0
    szLoaderReg     db "[GGUF-IDE-Bridge] Registered loader: ",0
    szCRLF          db 13,10,0

.data?
    g_BridgeInit    dd ?

.code

; ============================================================================
; GGUF_IDE_RegisterLoader - Register a loader with the IDE
; Input:  pLoaderName:DWORD - pointer to loader name string
;         dwVersion:DWORD   - loader version
; Output: EAX = loader ID (index) or -1 on failure
; ============================================================================
GGUF_IDE_RegisterLoader PROC pLoaderName:DWORD, dwVersion:DWORD
    push esi
    push edi
    push ebx

    ; Check if already have max loaders
    mov eax, g_RegisteredLoaderCount
    cmp eax, 16
    jge @@fail

    ; Get next slot
    mov ebx, eax
    
    ; Calculate offset: ebx * sizeof(GGUF_LOADER_INFO)
    mov eax, ebx
    mov ecx, sizeof GGUF_LOADER_INFO
    mul ecx
    lea edi, g_RegisteredLoaders
    add edi, eax

    ; Fill loader info
    mov eax, pLoaderName
    mov [edi].GGUF_LOADER_INFO.loaderName, eax
    
    mov eax, dwVersion
    mov [edi].GGUF_LOADER_INFO.loaderVersion, eax
    
    ; Default capabilities (can be extended later)
    mov [edi].GGUF_LOADER_INFO.capabilities, 0

    ; Increment count
    inc g_RegisteredLoaderCount

    ; Set as active loader
    mov eax, pLoaderName
    mov g_ActiveLoaderName, eax
    mov eax, dwVersion
    mov g_ActiveLoaderVersion, eax

    ; Return loader ID
    mov eax, ebx
    jmp @@exit

@@fail:
    mov eax, -1

@@exit:
    pop ebx
    pop edi
    pop esi
    ret
GGUF_IDE_RegisterLoader ENDP

; ============================================================================
; GGUF_IDE_SetProgressCallback - Set progress callback
; ============================================================================
GGUF_IDE_SetProgressCallback PROC pCallback:DWORD
    mov eax, pCallback
    mov g_IDE_ProgressCallback, eax
    ret
GGUF_IDE_SetProgressCallback ENDP

; ============================================================================
; GGUF_IDE_SetStatusCallback - Set status callback
; ============================================================================
GGUF_IDE_SetStatusCallback PROC pCallback:DWORD
    mov eax, pCallback
    mov g_IDE_StatusCallback, eax
    ret
GGUF_IDE_SetStatusCallback ENDP

; ============================================================================
; GGUF_IDE_SetModelLoadedCallback - Set model loaded callback
; ============================================================================
GGUF_IDE_SetModelLoadedCallback PROC pCallback:DWORD
    mov eax, pCallback
    mov g_IDE_ModelLoadedCallback, eax
    ret
GGUF_IDE_SetModelLoadedCallback ENDP

; ============================================================================
; GGUF_IDE_NotifyProgress - Notify IDE of progress
; Input:  dwPercent:DWORD - progress percentage (0-100)
;         pMessage:DWORD  - message string
; ============================================================================
GGUF_IDE_NotifyProgress PROC dwPercent:DWORD, pMessage:DWORD
    push ebx
    
    mov eax, g_IDE_ProgressCallback
    test eax, eax
    jz @@no_callback

    ; Call the callback
    push pMessage
    push dwPercent
    call eax
    add esp, 8

@@no_callback:
    pop ebx
    ret
GGUF_IDE_NotifyProgress ENDP

; ============================================================================
; GGUF_IDE_NotifyStatus - Notify IDE of status
; Input:  dwLevel:DWORD  - status level (0=info, 1=warning, 2=error)
;         pMessage:DWORD - message string
; ============================================================================
GGUF_IDE_NotifyStatus PROC dwLevel:DWORD, pMessage:DWORD
    push ebx
    
    mov eax, g_IDE_StatusCallback
    test eax, eax
    jz @@no_callback

    ; Call the callback
    push pMessage
    push dwLevel
    call eax
    add esp, 8

@@no_callback:
    pop ebx
    ret
GGUF_IDE_NotifyStatus ENDP

; ============================================================================
; GGUF_IDE_NotifyModelLoaded - Notify IDE that model loaded
; Input:  hModel:DWORD   - model handle
;         bSuccess:DWORD - success flag
; ============================================================================
GGUF_IDE_NotifyModelLoaded PROC hModel:DWORD, bSuccess:DWORD
    push ebx
    
    mov eax, g_IDE_ModelLoadedCallback
    test eax, eax
    jz @@no_callback

    ; Call the callback
    push bSuccess
    push hModel
    call eax
    add esp, 8

@@no_callback:
    pop ebx
    ret
GGUF_IDE_NotifyModelLoaded ENDP

; ============================================================================
; GGUF_IDE_GetActiveLoader - Get info about active loader
; Output: EAX = pointer to loader name, EDX = version
; ============================================================================
GGUF_IDE_GetActiveLoader PROC
    mov eax, g_ActiveLoaderName
    mov edx, g_ActiveLoaderVersion
    ret
GGUF_IDE_GetActiveLoader ENDP

END
