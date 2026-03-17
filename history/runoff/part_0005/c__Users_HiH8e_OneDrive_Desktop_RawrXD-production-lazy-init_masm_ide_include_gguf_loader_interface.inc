; ============================================================================
; GGUF_LOADER_INTERFACE.INC - Universal GGUF Loader Interface for MASM IDE
; Provides common interface for all GGUF loader variants to access IDE
; ============================================================================

; ============================================================================
; COMMON GGUF LOADER API (All variants must implement these)
; ============================================================================

; Core Loading Functions
GGUF_LoadModel PROTO :DWORD              ; Load model from file path
GGUF_CloseModel PROTO :DWORD             ; Close and free model
GGUF_ValidateModel PROTO :DWORD          ; Validate model structure

; Model Information
GGUF_GetTensorCount PROTO :DWORD         ; Get number of tensors
GGUF_GetTensorName PROTO :DWORD, :DWORD  ; Get tensor name by index
GGUF_GetTensorInfo PROTO :DWORD, :DWORD  ; Get tensor info structure

; Metadata Access
GGUF_GetMetadata PROTO :DWORD, :DWORD    ; Get model metadata
GGUF_GetParameter PROTO :DWORD, :DWORD   ; Get specific parameter
GGUF_GetVersion PROTO :DWORD             ; Get GGUF version

; Error Handling
GGUF_GetLastError PROTO :DWORD           ; Get last error code

; ============================================================================
; IDE REGISTRATION (Loaders call these to register with IDE)
; ============================================================================

; Register loader with IDE
GGUF_IDE_RegisterLoader PROTO :DWORD, :DWORD   ; loader_name, loader_version

; Set IDE callbacks
GGUF_IDE_SetProgressCallback PROTO :DWORD      ; callback function pointer
GGUF_IDE_SetStatusCallback PROTO :DWORD        ; callback function pointer
GGUF_IDE_SetModelLoadedCallback PROTO :DWORD   ; callback function pointer
GGUF_IDE_SetTensorProgressCallback PROTO :DWORD ; callback(tensorIndex,totalTensors,message)
GGUF_IDE_SetMemoryCallback PROTO :DWORD        ; callback(bytesInUse)
GGUF_IDE_SetCancelCallback PROTO :DWORD        ; callback() returns non-zero to cancel

; Notify IDE
GGUF_IDE_NotifyProgress PROTO :DWORD, :DWORD   ; percent, message
GGUF_IDE_NotifyStatus PROTO :DWORD, :DWORD     ; level, message
GGUF_IDE_NotifyModelLoaded PROTO :DWORD, :DWORD ; handle, success
GGUF_IDE_NotifyTensorProgress PROTO :DWORD, :DWORD, :DWORD ; index,total,message
GGUF_IDE_NotifyMemoryUsage PROTO :DWORD        ; bytes
GGUF_IDE_CheckCancel PROTO                     ; returns EAX non-zero if cancel requested

; Get active loader info
GGUF_IDE_GetActiveLoader PROTO                  ; Returns EAX=name, EDX=version

; ============================================================================
; COMMON ERROR CODES (All loaders use these)
; ============================================================================

GGUF_SUCCESS            equ 0
GGUF_ERROR_FILE         equ -1
GGUF_ERROR_FORMAT       equ -2
GGUF_ERROR_MEMORY       equ -3
GGUF_ERROR_VERSION      equ -5
GGUF_ERROR_CORRUPT      equ -6
GGUF_ERROR_UNSUPPORTED  equ -7

; Capability flags
GGUF_CAP_STREAMING      equ 1     ; Supports streaming
GGUF_CAP_COMPRESSION    equ 2     ; Supports compression
GGUF_CAP_QUANTIZATION   equ 4     ; Supports quantization
GGUF_CAP_MMAP           equ 8     ; Supports memory mapping
GGUF_CAP_PROGRESSIVE    equ 16    ; Supports progressive loading
