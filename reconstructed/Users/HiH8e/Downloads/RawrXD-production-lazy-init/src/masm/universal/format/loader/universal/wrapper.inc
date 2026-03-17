; MASM Header File for Universal Wrapper
; Location: src/masm/universal_format_loader/universal_wrapper.inc
; Purpose: Structure definitions and extern C declarations for unified MASM wrapper

IFNDEF UNIVERSAL_WRAPPER_INC
UNIVERSAL_WRAPPER_INC EQU 1

;========================================================================================================
; STRUCTURE DEFINITIONS (must match C++ side exactly)
;========================================================================================================

; Detection cache entry (32 bytes)
DETECTION_CACHE_ENTRY struct
    path            QWORD ?         ; file path (heap-allocated string)
    format          DWORD ?         ; detected format (FORMAT_XXX)
    compression     DWORD ?         ; compression type
    timestamp       QWORD ?         ; GetTickCount64() when cached
    valid           DWORD ?         ; 1 if entry is valid
    _padding        DWORD 3 dup(?)  ; padding to 32 bytes
DETECTION_CACHE_ENTRY ends

; Format detection result (32 bytes)
DETECTION_RESULT struct
    format          DWORD ?         ; FORMAT_XXX constant
    compression     DWORD ?         ; COMPRESSION_XXX constant
    reason          QWORD ?         ; error/status message pointer
    confidence      DWORD ?         ; 0-100 confidence score
    valid           DWORD ?         ; 1 if result valid
DETECTION_RESULT ends

; Model load result (64 bytes)
LOAD_RESULT struct
    success         DWORD ?         ; 1 if load succeeded
    format          DWORD ?         ; detected format
    output_path     QWORD ?         ; path to output file
    error_msg       QWORD ?         ; error message
    duration_ms     QWORD ?         ; operation duration
    bytes_read      QWORD ?         ; total bytes read
    _padding        QWORD 2 dup(?)  ; alignment
LOAD_RESULT ends

; Statistics structure (64 bytes)
WRAPPER_STATISTICS struct
    total_detections    QWORD ?     ; total format detections performed
    total_conversions   QWORD ?     ; total model conversions
    total_errors        QWORD ?     ; total errors encountered
    cache_hits          QWORD ?     ; cache lookup hits
    cache_misses        QWORD ?     ; cache lookup misses
    cache_entries       DWORD ?     ; current cache size
    current_mode        DWORD ?     ; current operating mode
    _padding            QWORD 3 dup(?)
WRAPPER_STATISTICS ends

; Universal wrapper state (512 bytes)
UNIVERSAL_WRAPPER struct
    ; Threading & Synchronization
    mutex               QWORD ?         ; Windows HANDLE to mutex
    
    ; Caching
    detection_cache     QWORD ?         ; pointer to cache array
    cache_entries       DWORD ?         ; number of entries in cache
    cache_hits          QWORD ?         ; statistics counter
    cache_misses        QWORD ?         ; statistics counter
    
    ; Configuration & State
    mode                DWORD ?         ; current wrapper mode
    is_initialized      DWORD ?         ; 1 if initialized
    last_detection      QWORD ?         ; timestamp of last detection
    
    ; Format Handlers (pointers to FORMAT_ROUTER, UNIVERSAL_LOADER, ENHANCED_LOADER)
    format_router       QWORD ?         ; cached router instance
    format_loader       QWORD ?         ; cached universal loader
    model_loader        QWORD ?         ; cached enhanced loader
    
    ; Error Handling
    error_message       QWORD ?         ; heap-allocated error buffer (512 bytes)
    error_code          DWORD ?         ; Windows error code from last operation
    _error_pad          DWORD ?         ; alignment
    
    ; Paths & Buffers
    temp_path           QWORD ?         ; heap-allocated temp path buffer (1024 bytes)
    
    ; Statistics
    total_detections    QWORD ?         ; total detections since creation
    total_conversions   QWORD ?         ; total conversions since creation
    total_errors        QWORD ?         ; total errors since creation
    
    ; Padding for future expansion
    _reserved           QWORD 28 dup(?) ; reserve space for future fields
UNIVERSAL_WRAPPER ends

;========================================================================================================
; FORMAT TYPE ENUMERATIONS
;========================================================================================================

; Format type constants (must match FORMAT_XXX from individual loaders)
FORMAT_UNKNOWN          EQU 0
FORMAT_GGUF_LOCAL       EQU 1
FORMAT_HF_REPO          EQU 2
FORMAT_HF_FILE          EQU 3
FORMAT_OLLAMA           EQU 4
FORMAT_MASM_COMP        EQU 5
FORMAT_UNIVERSAL        EQU 6
FORMAT_SAFETENSORS      EQU 7
FORMAT_PYTORCH          EQU 8
FORMAT_TENSORFLOW       EQU 9
FORMAT_ONNX             EQU 10
FORMAT_NUMPY            EQU 11

; Compression type constants
COMPRESSION_NONE        EQU 0
COMPRESSION_GZIP        EQU 1
COMPRESSION_ZSTD        EQU 2
COMPRESSION_LZ4         EQU 3

;========================================================================================================
; WRAPPER MODE ENUMERATION
;========================================================================================================

WRAPPER_MODE_PURE_MASM      EQU 0   ; Use pure MASM implementation
WRAPPER_MODE_CPP_QT         EQU 1   ; Use C++/Qt implementation
WRAPPER_MODE_AUTO_SELECT    EQU 2   ; Auto-select based on performance/availability

;========================================================================================================
; CACHE CONFIGURATION
;========================================================================================================

DETECTION_CACHE_MAX     EQU 32      ; Maximum detection cache entries
DETECTION_CACHE_TTL_MS  EQU 300000  ; 5 minute cache time-to-live

;========================================================================================================
; MAGIC BYTE CONSTANTS
;========================================================================================================

GGUF_MAGIC              EQU 0x46554747  ; "GGUF" (0x47 0x47 0x55 0x46 LE)
GZIP_MAGIC              EQU 0x8B1F      ; gzip header (0x1F 0x8B)
ZSTD_MAGIC              EQU 0xFD2FB528  ; Zstandard (0x28 0xB5 0x2F 0xFD)
LZ4_MAGIC               EQU 0x184D2204  ; LZ4 (0x04 0x22 0x4D 0x18)

;========================================================================================================
; ERROR CODES
;========================================================================================================

WRAPPER_OK                  EQU 0       ; Success
WRAPPER_ERROR_INVALID_PTR   EQU 1       ; Invalid pointer
WRAPPER_ERROR_NOT_INIT      EQU 2       ; Not initialized
WRAPPER_ERROR_ALLOC_FAIL    EQU 3       ; Memory allocation failed
WRAPPER_ERROR_MUTEX_FAIL    EQU 4       ; Mutex operation failed
WRAPPER_ERROR_FILE_NOT_FOUND EQU 5      ; File not found
WRAPPER_ERROR_FORMAT_UNKNOWN EQU 6      ; Format not recognized
WRAPPER_ERROR_LOAD_FAILED   EQU 7       ; Format loading failed
WRAPPER_ERROR_CACHE_FULL    EQU 8       ; Cache is full
WRAPPER_ERROR_MODE_INVALID  EQU 9       ; Invalid mode specified

;========================================================================================================
; EXPORTED FUNCTIONS - Called from C++
;========================================================================================================

; Initialize global wrapper state (call once)
; Input: ecx = mode (WRAPPER_MODE_XXX)
; Output: eax = 1 success, 0 failure
public wrapper_global_init:proc

; Create new wrapper instance
; Input: ecx = mode (WRAPPER_MODE_XXX)
; Output: rax = UNIVERSAL_WRAPPER* or NULL
public wrapper_create:proc

; Unified format detection with caching
; Input: rcx = wrapper ptr, rdx = file path (wide string)
; Output: eax = FORMAT_XXX constant
public wrapper_detect_format_unified:proc

; Auto-detect and load model
; Input: rcx = wrapper, rdx = model path, r8 = LOAD_RESULT*
; Output: eax = 1 success, 0 failure
public wrapper_load_model_auto:proc

; Convert any format to GGUF
; Input: rcx = wrapper, rdx = input path, r8 = output path
; Output: eax = 1 success, 0 failure
public wrapper_convert_to_gguf:proc

; Change operating mode at runtime
; Input: rcx = wrapper, edx = new mode
; Output: eax = 1 success, 0 failure
public wrapper_set_mode:proc

; Get aggregated statistics
; Input: rcx = wrapper, rdx = WRAPPER_STATISTICS*
; Output: eax = 1 success, 0 failure
public wrapper_get_statistics:proc

; Destroy wrapper and free resources
; Input: rcx = wrapper ptr
; Output: (none)
public wrapper_destroy:proc

;========================================================================================================
; HELPER FUNCTIONS (internal, not typically called from C++)
;========================================================================================================

; Detect format by extension
; Input: rcx = file path (wide string)
; Output: eax = FORMAT_XXX
public detect_extension_unified:proc

; Detect format by magic bytes
; Input: rcx = file path, rdx = magic buffer
; Output: eax = FORMAT_XXX or COMPRESSION_XXX
public detect_magic_bytes_unified:proc

; Format-agnostic conversion to GGUF
; Input: rcx = input file path, rdx = output file path
; Output: eax = 1 success, 0 failure
public wrapper_convert_generic:proc

ENDIF
