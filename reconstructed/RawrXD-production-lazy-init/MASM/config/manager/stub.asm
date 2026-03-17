; ============================================================================
; CONFIG MANAGER STUB - Minimal implementation for extended harness
; ============================================================================
; Provides Config functions needed by extended harness linking
; ============================================================================

OPTION CASEMAP:NONE

public Config_Initialize
public Config_Cleanup
public Config_LoadFromFile
public Config_LoadFromEnvironment
public Config_IsFeatureEnabled
public Config_GetModelPath
public Config_GetApiEndpoint
public Config_GetApiKey
public Config_EnableFeature
public Config_DisableFeature

.data
align 8
szModelPath db "ollama:latest", 0
szApiEndpoint db "http://localhost:11434", 0
szApiKey db "default-key", 0
dwFeatureFlags dd 0FFFFFFFFh  ; All features enabled by default
dwReserved dd 0

.code

; ============================================================================
; Config_Initialize
; RCX = pointer to config structure
; Returns: EAX = success (0)
; ============================================================================
Config_Initialize PROC
    lea rax, [dwFeatureFlags]
    mov dword ptr [rax], 0FFFFFFFFh
    xor eax, eax
    ret
Config_Initialize ENDP

; ============================================================================
; Config_Cleanup
; RCX = pointer to config structure
; Returns: EAX = success (0)
; ============================================================================
Config_Cleanup PROC
    xor eax, eax
    ret
Config_Cleanup ENDP

; ============================================================================
; Config_LoadFromFile
; RCX = pointer to config structure
; RDX = file path
; Returns: EAX = success (0)
; ============================================================================
Config_LoadFromFile PROC
    xor eax, eax
    ret
Config_LoadFromFile ENDP

; ============================================================================
; Config_LoadFromEnvironment
; RCX = pointer to config structure
; Returns: EAX = success (0)
; ============================================================================
Config_LoadFromEnvironment PROC
    xor eax, eax
    ret
Config_LoadFromEnvironment ENDP

; ============================================================================
; Config_IsFeatureEnabled
; RCX = pointer to config structure
; RDX = feature flag index
; Returns: EAX = 1 if enabled, 0 if disabled
; ============================================================================
Config_IsFeatureEnabled PROC
    mov eax, 1  ; All features enabled by default
    ret
Config_IsFeatureEnabled ENDP

; ============================================================================
; Config_GetModelPath
; RCX = pointer to config structure
; Returns: RAX = pointer to model path string
; ============================================================================
Config_GetModelPath PROC
    lea rax, [szModelPath]
    ret
Config_GetModelPath ENDP

; ============================================================================
; Config_GetApiEndpoint
; RCX = pointer to config structure
; Returns: RAX = pointer to API endpoint string
; ============================================================================
Config_GetApiEndpoint PROC
    lea rax, [szApiEndpoint]
    ret
Config_GetApiEndpoint ENDP

; ============================================================================
; Config_GetApiKey
; RCX = pointer to config structure
; Returns: RAX = pointer to API key string
; ============================================================================
Config_GetApiKey PROC
    lea rax, [szApiKey]
    ret
Config_GetApiKey ENDP

; ============================================================================
; Config_EnableFeature
; RCX = pointer to config structure
; RDX = feature flag index
; Returns: EAX = success (0)
; ============================================================================
Config_EnableFeature PROC
    xor eax, eax
    ret
Config_EnableFeature ENDP

; ============================================================================
; Config_DisableFeature
; RCX = pointer to config structure
; RDX = feature flag index
; Returns: EAX = success (0)
; ============================================================================
Config_DisableFeature PROC
    xor eax, eax
    ret
Config_DisableFeature ENDP

end
