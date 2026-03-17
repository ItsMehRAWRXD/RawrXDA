; MASM Header File for Enhanced Model Loader
; Provides extern C declarations for C++ integration
; Location: src/masm/universal_format_loader/enhanced_model_loader.inc

IFNDEF ENHANCED_MODEL_LOADER_INC
ENHANCED_MODEL_LOADER_INC EQU 1

; ========================================================================================================
; STRUCTURES (must match C++ side exactly)
; ========================================================================================================

LOAD_RESULT struct
    success         DWORD ?
    format          DWORD ?
    output_path     QWORD ?
    error_msg       QWORD ?
    duration_ms     QWORD ?
    _padding        DWORD 2 dup(?)
LOAD_RESULT ends

ENHANCED_LOADER struct
    mutex           QWORD ?
    temp_dir        QWORD ?
    model_buffer    QWORD ?
    buffer_size     QWORD ?
    gguf_buffer     QWORD ?
    gguf_size       QWORD ?
    format_type     DWORD ?
    is_initialized  DWORD ?
    last_error      QWORD ?
    _padding        DWORD 86 dup(?)
ENHANCED_LOADER ends

TEMP_MANIFEST struct
    file_count      DWORD ?
    files           QWORD 32 dup(?)
    manifest_mutex  QWORD ?
    _padding        DWORD 6 dup(?)
TEMP_MANIFEST ends

; ========================================================================================================
; EXPORTED FUNCTIONS - Called from C++
; ========================================================================================================

; enhanced_loader_init() -> ENHANCED_LOADER*
public enhanced_loader_init:proc

; load_model_universal_format(loader: ENHANCED_LOADER*, model_path: wchar_t*, result: LOAD_RESULT*) -> int
public load_model_universal_format:proc

; read_file_chunked(file_path: wchar_t*, buffer_ptr: void**, size: size_t*) -> int
public read_file_chunked:proc

; generate_temp_gguf_path(temp_dir: wchar_t*, model_path: wchar_t*, output_buffer: wchar_t*) -> wchar_t*
public generate_temp_gguf_path:proc

; write_buffer_to_file(file_path: wchar_t*, buffer: void*, size: size_t) -> int
public write_buffer_to_file:proc

; enhanced_loader_shutdown(loader: ENHANCED_LOADER*)
public enhanced_loader_shutdown:proc

; ========================================================================================================
; FORMAT TYPE CONSTANTS
; ========================================================================================================

FORMAT_UNKNOWN      EQU 0
FORMAT_SAFETENSORS  EQU 1
FORMAT_PYTORCH_ZIP  EQU 2
FORMAT_PYTORCH_PKL  EQU 3
FORMAT_TENSORFLOW   EQU 4
FORMAT_ONNX         EQU 5
FORMAT_NUMPY        EQU 6
FORMAT_GGUF_OUTPUT  EQU 7

; Constants
TEMP_MANIFEST_SIZE  EQU 256
TEMP_FILES_MAX      EQU 32
CHUNK_SIZE          EQU 1048576

ENDIF
