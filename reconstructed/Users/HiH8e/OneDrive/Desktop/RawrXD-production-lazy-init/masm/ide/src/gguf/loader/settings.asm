; ============================================================================
; gguf_loader_settings.asm
; GGUF Model Loader Settings System
; Provides toggles, checkboxes, and custom configuration for loader methods
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include mini_winconst.inc

; ============================================================================
; IMPORTS
; ============================================================================

GetProcessHeap PROTO
HeapAlloc PROTO :DWORD,:DWORD,:DWORD
HeapFree PROTO :DWORD,:DWORD,:DWORD

includelib kernel32.lib

; ============================================================================
; CONSTANTS & EXPORTS
; ============================================================================

PUBLIC GGUFSettings_Init
PUBLIC GGUFSettings_Create
PUBLIC GGUFSettings_SetLoaderMethod
PUBLIC GGUFSettings_GetLoaderMethod
PUBLIC GGUFSettings_SetToggle
PUBLIC GGUFSettings_GetToggle
PUBLIC GGUFSettings_SetCustomPath
PUBLIC GGUFSettings_GetCustomPath
PUBLIC GGUFSettings_ApplyDefaults
PUBLIC GGUFSettings_SaveToRegistry
PUBLIC GGUFSettings_LoadFromRegistry
PUBLIC GGUFSettings_Destroy

; Loader methods
LOADER_METHOD_DISC      equ 0       ; Disc-based streaming
LOADER_METHOD_MEMORY    equ 1       ; Full memory load
LOADER_METHOD_MMAP      equ 2       ; Memory-mapped file
LOADER_METHOD_HYBRID    equ 3       ; Hybrid disc+memory
LOADER_METHOD_AUTO      equ 4       ; Auto-select

; Toggle flags
TOGGLE_ENABLE_COMPRESSION      equ 1
TOGGLE_ENABLE_QUANTIZATION     equ 2
TOGGLE_ENABLE_ADAPTIVE_TUNING  equ 4
TOGGLE_ENABLE_TELEMETRY        equ 8
TOGGLE_ENABLE_TRAINING_MODE    equ 16
TOGGLE_ENABLE_VALIDATION       equ 32

; ============================================================================
; STRUCTURES
; ============================================================================

GGUF_LOADER_SETTINGS STRUCT
    dwDefaultMethod         DWORD ?     ; Default loading method
    dwEnabledMethods        DWORD ?     ; Bitmask of enabled methods
    dwToggleFlags           DWORD ?     ; Feature toggles
    dwCompressionLevel      DWORD ?     ; 0-9 compression level
    dwQuantizationBits      DWORD ?     ; 4, 8, 16, or 32
    dwAdaptationTimeout     DWORD ?     ; Milliseconds
    dwTelemetryInterval     DWORD ?     ; Milliseconds
    dwMaxMemoryPercent      DWORD ?     ; Max memory usage %
    szCustomModelPath       BYTE 512 DUP (?) ; Custom model path
    szCustomCachePath       BYTE 512 DUP (?) ; Custom cache path
    dwFlags                 DWORD ?     ; Status flags
    dwVersion               DWORD ?     ; Settings version
GGUF_LOADER_SETTINGS ENDS

; ============================================================================
; DATA SECTION
; ============================================================================

.data

g_pGlobalSettings   DWORD 0             ; Global settings instance
g_dwSettingsCount   DWORD 0             ; Number of settings instances

; Default method priority
g_defaultMethodOrder DWORD \
    LOADER_METHOD_AUTO,         ; Try auto first
    LOADER_METHOD_MEMORY,       ; Then full memory
    LOADER_METHOD_HYBRID,       ; Then hybrid
    LOADER_METHOD_MMAP,         ; Then mmap
    LOADER_METHOD_DISC          ; Finally disc

; Registry key paths
szSettingsRoot      db "Software\PiFabric\GGUFLoader",0
szMethodKey         db "DefaultMethod",0
szToggleKey         db "EnabledFeatures",0
szCompressionKey    db "CompressionLevel",0
szQuantizationKey   db "QuantizationBits",0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; ============================================================================
; GGUFSettings_Init - Initialize global settings system
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

GGUFSettings_Init PROC

    cmp g_pGlobalSettings, 0
    jne @already_init

    ; Allocate global settings
    invoke GetProcessHeap
    mov edx, eax
    invoke HeapAlloc, edx, 8h, SIZEOF GGUF_LOADER_SETTINGS
    test eax, eax
    jz  @fail

    mov g_pGlobalSettings, eax
    mov g_dwSettingsCount, 0

    ; Apply defaults
    push eax
    call GGUFSettings_ApplyDefaults

    mov eax, 1
    ret

@already_init:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUF_Settings_Init ENDP

; ============================================================================
; GGUFSettings_Create - Create new settings instance
; out:
;   EAX             = pointer to GGUF_LOADER_SETTINGS, or 0
; ============================================================================

GGUFSettings_Create PROC

    invoke GetProcessHeap
    mov edx, eax
    invoke HeapAlloc, edx, 8h, SIZEOF GGUF_LOADER_SETTINGS
    test eax, eax
    jz  @fail

    ; Initialize with defaults
    push eax
    call GGUFSettings_ApplyDefaults
    pop eax

    mov edx, g_dwSettingsCount
    inc edx
    mov g_dwSettingsCount, edx

    ret

@fail:
    xor eax, eax
    ret

GGUFSettings_Create ENDP

; ============================================================================
; GGUFSettings_ApplyDefaults - Apply default settings
; in:
;   [ESP+4]         = pointer to GGUF_LOADER_SETTINGS
; ============================================================================

GGUFSettings_ApplyDefaults PROC uses esi pSettings:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @done

    ; Set defaults
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwDefaultMethod, LOADER_METHOD_AUTO
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwEnabledMethods, 01Fh  ; All 5 methods
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwToggleFlags, 3Fh      ; All toggles
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwCompressionLevel, 6    ; Medium compression
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwQuantizationBits, 4    ; 4-bit quant
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwAdaptationTimeout, 5000
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwTelemetryInterval, 500
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwMaxMemoryPercent, 80
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwFlags, 0
    mov dword ptr [esi].GGUF_LOADER_SETTINGS.dwVersion, 1

@done:
    ret

GGUFSettings_ApplyDefaults ENDP

; ============================================================================
; GGUFSettings_SetLoaderMethod - Set preferred loading method
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
;   dwMethod        = LOADER_METHOD_*
; out:
;   EAX             = 1 success, 0 invalid method
; ============================================================================

GGUFSettings_SetLoaderMethod PROC uses esi pSettings:DWORD, dwMethod:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @fail

    mov eax, dwMethod
    cmp eax, LOADER_METHOD_AUTO
    ja  @fail

    mov [esi].GGUF_LOADER_SETTINGS.dwDefaultMethod, eax
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUFSettings_SetLoaderMethod ENDP

; ============================================================================
; GGUFSettings_GetLoaderMethod - Get preferred loading method
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
; out:
;   EAX             = LOADER_METHOD_*
; ============================================================================

GGUFSettings_GetLoaderMethod PROC uses esi pSettings:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @error

    mov eax, [esi].GGUF_LOADER_SETTINGS.dwDefaultMethod
    ret

@error:
    mov eax, LOADER_METHOD_AUTO
    ret

GGUFSettings_GetLoaderMethod ENDP

; ============================================================================
; GGUFSettings_SetToggle - Set feature toggle
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
;   dwToggle        = TOGGLE_* constant
;   bEnable         = 1 enable, 0 disable
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

GGUFSettings_SetToggle PROC uses esi pSettings:DWORD, dwToggle:DWORD, bEnable:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @fail

    mov eax, dwToggle
    mov edx, bEnable

    test edx, edx
    jz  @disable_toggle

    ; Enable toggle
    or [esi].GGUF_LOADER_SETTINGS.dwToggleFlags, eax
    mov eax, 1
    ret

@disable_toggle:
    not eax
    and [esi].GGUF_LOADER_SETTINGS.dwToggleFlags, eax
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUFSettings_SetToggle ENDP

; ============================================================================
; GGUFSettings_GetToggle - Get feature toggle state
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
;   dwToggle        = TOGGLE_* constant
; out:
;   EAX             = 1 enabled, 0 disabled
; ============================================================================

GGUFSettings_GetToggle PROC uses esi pSettings:DWORD, dwToggle:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @disabled

    mov eax, [esi].GGUF_LOADER_SETTINGS.dwToggleFlags
    and eax, dwToggle
    test eax, eax
    jz  @disabled

    mov eax, 1
    ret

@disabled:
    xor eax, eax
    ret

GGUFSettings_GetToggle ENDP

; ============================================================================
; GGUFSettings_SetCustomPath - Set custom model or cache path
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
;   lpPath          = path string
;   bModelPath      = 1 for model path, 0 for cache path
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

GGUFSettings_SetCustomPath PROC uses esi edi ecx pSettings:DWORD, lpPath:DWORD, bModelPath:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @fail

    mov edi, lpPath
    test edi, edi
    jz  @fail

    ; Choose destination
    cmp bModelPath, 0
    jne @copy_to_model

    lea edi, [esi].GGUF_LOADER_SETTINGS.szCustomCachePath
    jmp @do_copy

@copy_to_model:
    lea edi, [esi].GGUF_LOADER_SETTINGS.szCustomModelPath

@do_copy:
    mov esi, lpPath
    mov ecx, 511                ; Max 512 chars

@copy_loop:
    cmp ecx, 0
    je  @copy_done
    mov al, [esi]
    mov [edi], al
    test al, al
    jz  @copy_done
    inc esi
    inc edi
    dec ecx
    jmp @copy_loop

@copy_done:
    mov eax, 1
    ret

@fail:
    xor eax, eax
    ret

GGUFSettings_SetCustomPath ENDP

; ============================================================================
; GGUFSettings_GetCustomPath - Get custom path
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
;   bModelPath      = 1 for model path, 0 for cache path
; out:
;   EAX             = pointer to path string
; ============================================================================

GGUFSettings_GetCustomPath PROC uses esi pSettings:DWORD, bModelPath:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @error

    cmp bModelPath, 0
    jne @get_model

    lea eax, [esi].GGUF_LOADER_SETTINGS.szCustomCachePath
    ret

@get_model:
    lea eax, [esi].GGUF_LOADER_SETTINGS.szCustomModelPath
    ret

@error:
    xor eax, eax
    ret

GGUFSettings_GetCustomPath ENDP

; ============================================================================
; GGUFSettings_SaveToRegistry - Save settings to Windows registry
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
; out:
;   EAX             = 1 success, 0 failure
; ============================================================================

GGUFSettings_SaveToRegistry PROC uses esi pSettings:DWORD

    ; TODO: Implement registry save
    ; This is a stub for future implementation
    mov eax, 1
    ret

GGUFSettings_SaveToRegistry ENDP

; ============================================================================
; GGUFSettings_LoadFromRegistry - Load settings from Windows registry
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
; out:
;   EAX             = 1 success, 0 failure or not found
; ============================================================================

GGUFSettings_LoadFromRegistry PROC uses esi pSettings:DWORD

    ; TODO: Implement registry load
    ; This is a stub for future implementation
    mov eax, 1
    ret

GGUFSettings_LoadFromRegistry ENDP

; ============================================================================
; GGUFSettings_Destroy - Free settings instance
; in:
;   pSettings       = pointer to GGUF_LOADER_SETTINGS
; ============================================================================

GGUFSettings_Destroy PROC uses esi pSettings:DWORD

    mov esi, pSettings
    test esi, esi
    jz  @done

    invoke GetProcessHeap
    invoke HeapFree, eax, 0, esi

    mov edx, g_dwSettingsCount
    dec edx
    mov g_dwSettingsCount, edx

@done:
    ret

GGUFSettings_Destroy ENDP

END
