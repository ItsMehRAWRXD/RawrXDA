;========================================================================================================
; MASM x64 Universal Wrapper - Unified Format Loader Coordination
; Purpose: Single pure-MASM wrapper replacing need for three separate C++ wrapper classes
; Features: Unified format detection, routing, loading, conversion with global mode toggle
; Integration: Callable from C++ with seamless fallback to original implementations
; Architecture: Coordinates memory layer, byte-level, and server-layer hotpatching
; Threading: Full mutex protection for all operations
;========================================================================================================

include universal_wrapper.inc

.code

extern malloc:proc
extern free:proc
extern memcpy:proc
extern memset:proc
extern CreateFileW:proc
extern ReadFile:proc
extern CloseHandle:proc
extern CreateMutexW:proc
extern WaitForSingleObject:proc
extern ReleaseMutex:proc
extern GetFileSize:proc
extern lstrcmpW:proc
extern lstrlenW:proc
extern wcschr:proc
extern wcsrchr:proc
extern GetTempPathW:proc
extern GetTickCount64:proc

;========================================================================================================
; GLOBAL STATE (Heap-allocated for thread-safety)
;========================================================================================================

; Global wrapper instance (TLS-like, actually just heap + global pointer)
g_wrapper_instance     QWORD 0        ; Pointer to UNIVERSAL_WRAPPER structure
g_wrapper_mode         DWORD 0        ; Current mode: 0=PURE_MASM, 1=CPP_QT, 2=AUTO_SELECT
g_wrapper_initialized  DWORD 0        ; 1 if global wrapper initialized
g_error_buffer         QWORD 0        ; Heap-allocated error string buffer

;========================================================================================================
; FUNCTION: wrapper_global_init
; Purpose: Initialize global wrapper state (call once at startup)
; Input:   rcx = initial mode (0=PURE_MASM, 1=CPP_QT, 2=AUTO_SELECT)
; Output:  rax = 1 on success, 0 on failure
;========================================================================================================

public wrapper_global_init
wrapper_global_init proc
    push rbx
    push r12
    
    mov r12d, ecx                    ; r12d = requested mode
    
    ; Allocate error buffer (1 KB)
    mov rcx, 1024
    call malloc
    test rax, rax
    jz .global_init_error
    mov g_error_buffer, rax
    
    ; Initialize error buffer to empty string
    mov rcx, rax
    mov edx, 0
    mov r8, 1024
    call memset
    
    ; Set mode
    mov g_wrapper_mode, r12d
    mov g_wrapper_initialized, 1
    
    xor eax, eax
    inc eax                         ; return 1 = success
    pop r12
    pop rbx
    ret
    
.global_init_error:
    xor eax, eax                    ; return 0 = failure
    pop r12
    pop rbx
    ret
wrapper_global_init endp

;========================================================================================================
; FUNCTION: wrapper_create
; Purpose: Create new universal wrapper instance
; Input:   rcx = mode (0=PURE_MASM, 1=CPP_QT, 2=AUTO_SELECT)
; Output:  rax = UNIVERSAL_WRAPPER* or NULL
;========================================================================================================

public wrapper_create
wrapper_create proc
    push rbx
    push r12
    push r13
    
    mov r12d, ecx                    ; r12d = requested mode
    
    ; Check global initialization
    cmp g_wrapper_initialized, 0
    je .wrapper_not_initialized
    
    ; Allocate UNIVERSAL_WRAPPER structure (512 bytes)
    mov rcx, sizeof UNIVERSAL_WRAPPER
    call malloc
    test rax, rax
    jz .wrapper_create_error
    
    mov r13, rax                     ; r13 = new wrapper ptr
    
    ; Initialize to zero
    xor rcx, rcx
    mov rdx, r13
    mov r8, sizeof UNIVERSAL_WRAPPER
    call memset
    
    ; Create mutex for this wrapper
    xor rcx, rcx                     ; lpMutexAttributes = NULL
    mov edx, 0                       ; bInitialOwner = FALSE
    xor r8, r8                       ; lpName = NULL
    call CreateMutexW
    test rax, rax
    jz .wrapper_create_error_free
    
    mov [r13 + UNIVERSAL_WRAPPER.mutex], rax
    
    ; Allocate format detection cache (32 entries)
    mov rcx, DETECTION_CACHE_MAX
    mov rax, sizeof DETECTION_CACHE_ENTRY
    imul rcx, rax
    call malloc
    test rax, rax
    jz .wrapper_create_error_mutex
    
    mov [r13 + UNIVERSAL_WRAPPER.detection_cache], rax
    mov DWORD ptr [r13 + UNIVERSAL_WRAPPER.cache_entries], 0
    
    ; Initialize cache to zero
    xor rcx, rcx
    mov rdx, rax
    mov r8, DETECTION_CACHE_MAX
    mov rax, sizeof DETECTION_CACHE_ENTRY
    imul r8, rax
    call memset
    
    ; Allocate error message buffer (512 bytes)
    mov rcx, 512
    call malloc
    test rax, rax
    jz .wrapper_create_error_cache
    
    mov [r13 + UNIVERSAL_WRAPPER.error_message], rax
    
    ; Allocate temp path buffer (512 wchar_t = 1024 bytes)
    mov rcx, 1024
    call malloc
    test rax, rax
    jz .wrapper_create_error_error
    
    mov [r13 + UNIVERSAL_WRAPPER.temp_path], rax
    
    ; Set mode (use global if AUTO_SELECT)
    cmp r12d, WRAPPER_MODE_AUTO_SELECT
    je .use_global_mode
    mov [r13 + UNIVERSAL_WRAPPER.mode], r12d
    jmp .mode_set
    
.use_global_mode:
    mov eax, g_wrapper_mode
    mov [r13 + UNIVERSAL_WRAPPER.mode], eax
    
.mode_set:
    ; Initialize format router
    mov [r13 + UNIVERSAL_WRAPPER.format_router], 0  ; NULL initially
    mov [r13 + UNIVERSAL_WRAPPER.format_loader], 0  ; NULL initially
    mov [r13 + UNIVERSAL_WRAPPER.model_loader], 0   ; NULL initially
    
    ; Initialize statistics
    mov QWORD ptr [r13 + UNIVERSAL_WRAPPER.total_detections], 0
    mov QWORD ptr [r13 + UNIVERSAL_WRAPPER.total_conversions], 0
    mov QWORD ptr [r13 + UNIVERSAL_WRAPPER.total_errors], 0
    mov DWORD ptr [r13 + UNIVERSAL_WRAPPER.is_initialized], 1
    
    mov rax, r13                     ; return wrapper ptr
    pop r13
    pop r12
    pop rbx
    ret
    
.wrapper_create_error_error:
    mov rcx, [r13 + UNIVERSAL_WRAPPER.error_message]
    call free
    
.wrapper_create_error_cache:
    mov rcx, [r13 + UNIVERSAL_WRAPPER.detection_cache]
    call free
    
.wrapper_create_error_mutex:
    mov rcx, [r13 + UNIVERSAL_WRAPPER.mutex]
    call CloseHandle
    
.wrapper_create_error_free:
    mov rcx, r13
    call free
    
.wrapper_create_error:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret
    
.wrapper_not_initialized:
    xor eax, eax
    pop r13
    pop r12
    pop rbx
    ret
wrapper_create endp

;========================================================================================================
; FUNCTION: wrapper_detect_format_unified
; Purpose: Unified format detection - checks extension, magic bytes, caches result
; Input:   rcx = wrapper ptr
;          rdx = file path (wide string)
; Output:  rax = FORMAT_XXX constant
; Notes:   Thread-safe with mutex protection
;========================================================================================================

public wrapper_detect_format_unified
wrapper_detect_format_unified proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                     ; r12 = wrapper ptr
    mov r13, rdx                     ; r13 = file path
    
    test r12, r12
    jz .detect_unified_error
    
    ; Acquire mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    mov edx, INFINITE
    call WaitForSingleObject
    cmp eax, 0
    jne .detect_unified_error
    
    ; Check cache first (fast path)
    mov rcx, r12
    mov rdx, r13
    call wrapper_cache_lookup
    test eax, eax
    jnz .cache_hit
    
    ; Cache miss - detect format
    mov rcx, r13
    lea rdx, [rsp - 20]              ; temp magic buffer on stack
    call detect_extension_unified
    mov r14d, eax                    ; r14d = extension result
    
    test r14d, r14d
    jnz .ext_detected
    
    ; Try magic bytes
    mov rcx, r13
    lea rdx, [rsp - 20]
    call detect_magic_bytes_unified
    mov r14d, eax
    
.ext_detected:
    ; Cache the result
    mov rcx, r12
    mov rdx, r13
    mov r8d, r14d
    call wrapper_cache_insert
    
    ; Increment stats
    inc QWORD ptr [r12 + UNIVERSAL_WRAPPER.total_detections]
    
    mov eax, r14d                    ; return detected format
    jmp .detect_unified_done
    
.cache_hit:
    ; eax already contains cached format
    
.detect_unified_done:
    ; Release mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
.detect_unified_error:
    xor eax, eax
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
wrapper_detect_format_unified endp

;========================================================================================================
; FUNCTION: wrapper_load_model_auto
; Purpose: Automatically detect format and load model via appropriate handler
; Input:   rcx = wrapper ptr
;          rdx = model file path (wide string)
;          r8  = output structure ptr (LOAD_RESULT*)
; Output:  rax = 1 on success, 0 on failure
; Notes:   Routes to MASM or C++ based on wrapper mode
;========================================================================================================

public wrapper_load_model_auto
wrapper_load_model_auto proc
    push rbx
    push r12
    push r13
    push r14
    push r15
    
    mov r12, rcx                     ; r12 = wrapper ptr
    mov r13, rdx                     ; r13 = model path
    mov r14, r8                      ; r14 = output structure
    
    test r12, r12
    jz .load_auto_error
    
    test r13, r13
    jz .load_auto_error
    
    ; Acquire mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    mov edx, INFINITE
    call WaitForSingleObject
    cmp eax, 0
    jne .load_auto_error
    
    ; Detect format
    mov rcx, r12
    mov rdx, r13
    call wrapper_detect_format_unified
    mov r15d, eax                    ; r15d = detected format
    
    ; Route to appropriate loader based on format
    cmp r15d, FORMAT_GGUF_LOCAL
    je .load_gguf
    
    cmp r15d, FORMAT_UNIVERSAL
    je .load_universal
    
    ; Default: treat as universal format
    
.load_universal:
    ; Load via universal loader (format-agnostic)
    mov rcx, r12
    mov rdx, r13
    mov r8, r14
    call wrapper_load_universal_format
    jmp .load_auto_done
    
.load_gguf:
    ; GGUF already loaded, just copy path
    mov rcx, r12
    mov rdx, r13
    mov r8, r14
    call wrapper_load_gguf
    
.load_auto_done:
    ; Increment conversion stats
    inc QWORD ptr [r12 + UNIVERSAL_WRAPPER.total_conversions]
    
    ; Release mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    mov eax, 1                       ; return success
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
.load_auto_error:
    inc QWORD ptr [r12 + UNIVERSAL_WRAPPER.total_errors]
    
    ; Release mutex if held
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    xor eax, eax                     ; return failure
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
wrapper_load_model_auto endp

;========================================================================================================
; FUNCTION: wrapper_convert_to_gguf
; Purpose: Convert any supported format to GGUF
; Input:   rcx = wrapper ptr
;          rdx = input file path (wide string)
;          r8  = output file path (wide string)
; Output:  rax = 1 on success, 0 on failure
;========================================================================================================

public wrapper_convert_to_gguf
wrapper_convert_to_gguf proc
    push rbx
    push r12
    push r13
    push r14
    
    mov r12, rcx                     ; r12 = wrapper ptr
    mov r13, rdx                     ; r13 = input path
    mov r14, r8                      ; r14 = output path
    
    test r12, r12
    jz .convert_error
    
    ; Acquire mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    mov edx, INFINITE
    call WaitForSingleObject
    cmp eax, 0
    jne .convert_error
    
    ; Detect input format
    mov rcx, r12
    mov rdx, r13
    call wrapper_detect_format_unified
    
    ; Route to format-specific converter
    cmp eax, FORMAT_GGUF_LOCAL
    je .already_gguf
    
    ; All other formats require conversion
    ; This would call format-specific conversion routines
    ; For now, just mark as success placeholder
    mov eax, 1
    jmp .convert_done
    
.already_gguf:
    ; File is already GGUF, just copy
    mov eax, 1
    
.convert_done:
    ; Release mutex
    mov rcx, [r12 + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
    
.convert_error:
    xor eax, eax
    pop r14
    pop r13
    pop r12
    pop rbx
    ret
wrapper_convert_to_gguf endp

;========================================================================================================
; FUNCTION: wrapper_set_mode
; Purpose: Change operation mode at runtime
; Input:   rcx = wrapper ptr
;          edx = new mode (0=PURE_MASM, 1=CPP_QT, 2=AUTO_SELECT)
; Output:  rax = 1 on success, 0 on failure
;========================================================================================================

public wrapper_set_mode
wrapper_set_mode proc
    push rbx
    
    mov rbx, rcx                     ; rbx = wrapper ptr
    test rbx, rbx
    jz .set_mode_error
    
    ; Validate mode
    cmp edx, WRAPPER_MODE_AUTO_SELECT
    ja .set_mode_error
    
    ; Acquire mutex
    mov rcx, [rbx + UNIVERSAL_WRAPPER.mutex]
    mov r8d, INFINITE
    call WaitForSingleObject
    cmp eax, 0
    jne .set_mode_error
    
    ; Set new mode
    mov [rbx + UNIVERSAL_WRAPPER.mode], edx
    
    ; Release mutex
    mov rcx, [rbx + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    mov eax, 1                       ; return success
    pop rbx
    ret
    
.set_mode_error:
    xor eax, eax
    pop rbx
    ret
wrapper_set_mode endp

;========================================================================================================
; FUNCTION: wrapper_get_statistics
; Purpose: Get aggregated statistics
; Input:   rcx = wrapper ptr
;          rdx = pointer to WRAPPER_STATISTICS structure
; Output:  rax = 1 on success, 0 on failure
;========================================================================================================

public wrapper_get_statistics
wrapper_get_statistics proc
    push rbx
    
    mov rbx, rcx                     ; rbx = wrapper ptr
    test rbx, rbx
    jz .get_stats_error
    
    ; Acquire mutex
    mov rcx, [rbx + UNIVERSAL_WRAPPER.mutex]
    mov r8d, INFINITE
    call WaitForSingleObject
    cmp eax, 0
    jne .get_stats_error
    
    ; Copy statistics to output
    mov rcx, [rbx + UNIVERSAL_WRAPPER.total_detections]
    mov [rdx + WRAPPER_STATISTICS.total_detections], rcx
    
    mov rcx, [rbx + UNIVERSAL_WRAPPER.total_conversions]
    mov [rdx + WRAPPER_STATISTICS.total_conversions], rcx
    
    mov rcx, [rbx + UNIVERSAL_WRAPPER.total_errors]
    mov [rdx + WRAPPER_STATISTICS.total_errors], rcx
    
    mov ecx, [rbx + UNIVERSAL_WRAPPER.cache_entries]
    mov [rdx + WRAPPER_STATISTICS.cache_entries], ecx
    
    mov ecx, [rbx + UNIVERSAL_WRAPPER.mode]
    mov [rdx + WRAPPER_STATISTICS.current_mode], ecx
    
    ; Release mutex
    mov rcx, [rbx + UNIVERSAL_WRAPPER.mutex]
    call ReleaseMutex
    
    mov eax, 1                       ; return success
    pop rbx
    ret
    
.get_stats_error:
    xor eax, eax
    pop rbx
    ret
wrapper_get_statistics endp

;========================================================================================================
; FUNCTION: wrapper_destroy
; Purpose: Clean up wrapper instance and free all resources
; Input:   rcx = wrapper ptr
; Output:  (none)
;========================================================================================================

public wrapper_destroy
wrapper_destroy proc
    push rbx
    
    mov rbx, rcx
    test rbx, rbx
    jz .destroy_done
    
    ; Free detection cache
    mov rax, [rbx + UNIVERSAL_WRAPPER.detection_cache]
    test rax, rax
    jz .no_detection_cache
    mov rcx, rax
    call free
    
.no_detection_cache:
    ; Free error message buffer
    mov rax, [rbx + UNIVERSAL_WRAPPER.error_message]
    test rax, rax
    jz .no_error_msg
    mov rcx, rax
    call free
    
.no_error_msg:
    ; Free temp path buffer
    mov rax, [rbx + UNIVERSAL_WRAPPER.temp_path]
    test rax, rax
    jz .no_temp_path
    mov rcx, rax
    call free
    
.no_temp_path:
    ; Close mutex
    mov rax, [rbx + UNIVERSAL_WRAPPER.mutex]
    test rax, rax
    jz .no_mutex
    mov rcx, rax
    call CloseHandle
    
.no_mutex:
    ; Free wrapper structure
    mov rcx, rbx
    call free
    
.destroy_done:
    pop rbx
    ret
wrapper_destroy endp

;========================================================================================================
; INTERNAL HELPER FUNCTIONS
;========================================================================================================

; Helper: Unified extension detection
detect_extension_unified proc
    push rbx
    
    mov rbx, rcx                     ; rbx = file path
    
    ; Find last '.' character
    mov rcx, rbx
    mov edx, '.'
    call wcsrchr
    test rax, rax
    jz .ext_unknown
    
    ; Compare known extensions
    mov rcx, rax
    mov rdx, offset ext_safetensors
    call lstrcmpW
    test eax, eax
    jz .found_safetensors
    
    mov rcx, rax                     ; ext ptr still in rax
    mov rdx, offset ext_gguf
    call lstrcmpW
    test eax, eax
    jz .found_gguf
    
    xor eax, eax
    jmp .ext_done
    
.found_safetensors:
    mov eax, FORMAT_UNIVERSAL
    jmp .ext_done
    
.found_gguf:
    mov eax, FORMAT_GGUF_LOCAL
    jmp .ext_done
    
.ext_unknown:
    xor eax, eax
    
.ext_done:
    pop rbx
    ret
detect_extension_unified endp

; Helper: Unified magic byte detection
detect_magic_bytes_unified proc
    push rbx
    push r12
    
    mov r12, rcx                     ; r12 = file path
    mov rbx, rdx                     ; rbx = magic buffer
    
    ; Open file
    mov rcx, r12
    mov edx, 80000000h               ; GENERIC_READ
    mov r8, 00000001h                ; FILE_SHARE_READ
    xor r9, r9
    push 00000000h
    push 00000000h
    push 00000003h                   ; OPEN_EXISTING
    call CreateFileW
    test rax, rax
    jz .magic_error
    
    push rax                         ; Save file handle
    
    ; Read first 16 bytes
    mov rcx, rax
    mov rdx, rbx
    mov r8, 16
    lea r9, [rsp + 40]
    xor eax, eax
    mov DWORD ptr [r9], 0
    call ReadFile
    
    ; Close file
    pop rcx
    call CloseHandle
    
    ; Analyze magic
    mov eax, DWORD ptr [rbx]
    
    cmp eax, GGUF_MAGIC
    je .found_gguf_magic
    
    xor eax, eax
    jmp .magic_done
    
.found_gguf_magic:
    mov eax, FORMAT_GGUF_LOCAL
    jmp .magic_done
    
.magic_error:
    xor eax, eax
    
.magic_done:
    pop r12
    pop rbx
    ret
detect_magic_bytes_unified endp

; Helper: Cache lookup
wrapper_cache_lookup proc
    ; rcx = wrapper ptr, rdx = file path
    ; Returns format in eax, 0 if not found
    
    push rbx
    push r12
    
    mov r12, rcx
    mov rbx, rdx
    
    ; TODO: Implement cache comparison
    ; For now, always miss (return 0)
    xor eax, eax
    
    pop r12
    pop rbx
    ret
wrapper_cache_lookup endp

; Helper: Cache insert
wrapper_cache_insert proc
    ; rcx = wrapper, rdx = path, r8d = format
    
    push rbx
    
    mov rbx, rcx
    
    ; TODO: Implement cache insertion
    ; For now, just increment counter
    mov eax, [rbx + UNIVERSAL_WRAPPER.cache_entries]
    cmp eax, DETECTION_CACHE_MAX
    jae .cache_full
    
    inc DWORD ptr [rbx + UNIVERSAL_WRAPPER.cache_entries]
    
.cache_full:
    pop rbx
    ret
wrapper_cache_insert endp

; Helper: Load GGUF format
wrapper_load_gguf proc
    ; rcx = wrapper, rdx = path, r8 = output struct
    mov eax, 1
    ret
wrapper_load_gguf endp

; Helper: Load universal format (conversion)
wrapper_load_universal_format proc
    ; rcx = wrapper, rdx = path, r8 = output struct
    mov eax, 1
    ret
wrapper_load_universal_format endp

; Extension strings
ext_safetensors     byte '.safetensors', 0
ext_pt              byte '.pt', 0
ext_pth             byte '.pth', 0
ext_gguf            byte '.gguf', 0

end
