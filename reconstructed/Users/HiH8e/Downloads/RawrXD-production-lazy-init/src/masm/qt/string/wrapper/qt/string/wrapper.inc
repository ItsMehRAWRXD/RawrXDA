; =============================================================================
; Qt String Wrapper Definitions - Pure MASM
; =============================================================================
; Purpose: Structure definitions, constants, and external declarations for
;          pure MASM Qt string, byte array, and file operations
; Author: AI Toolkit / GitHub Copilot
; Date: December 29, 2025
; Status: Production Ready
; =============================================================================

; =============================================================================
; STRUCTURE DEFINITIONS
; =============================================================================

; QtStringOpResult - Return value for string operations
QtStringOpResult STRUCT
    success         DWORD       0       ; Success flag (1=success, 0=failure)
    error_code      DWORD       0       ; Error code (0=no error)
    length          DWORD       0       ; Output length or error detail
    reserved        QWORD       0       ; Reserved for future use
QtStringOpResult ENDS

; QtStringBuffer - Managed buffer structure
QtStringBuffer STRUCT
    data_ptr        QWORD       0       ; Pointer to buffer data
    capacity        DWORD       0       ; Total capacity in bytes
    length          DWORD       0       ; Current valid length
    flags           DWORD       0       ; Flags (bit 0: owned, bit 1: locked)
QtStringBuffer ENDS

; QtStringOpContext - Context for ongoing operations
QtStringOpContext STRUCT
    operation_type  DWORD       0       ; Type of operation
    source_ptr      QWORD       0       ; Source pointer
    dest_ptr        QWORD       0       ; Destination pointer
    encoding        DWORD       0       ; Encoding type
    options         DWORD       0       ; Operation options
    timestamp_ms    QWORD       0       ; Timestamp for cache/timeout
QtStringOpContext ENDS

; QtFileHandle - Opaque file handle structure
QtFileHandle STRUCT
    handle_value    QWORD       0       ; File handle value
    open_mode       DWORD       0       ; Open mode flags
    access_rights   DWORD       0       ; Access rights
    reserved        QWORD       0       ; Reserved
QtFileHandle ENDS

; QtStringStatistics - Performance and operation metrics
QtStringStatistics STRUCT
    total_allocations   QWORD   0       ; Total allocations
    total_deallocations QWORD   0       ; Total deallocations
    current_allocated   QWORD   0       ; Current bytes allocated
    string_conversions  QWORD   0       ; String conversions performed
    file_operations     QWORD   0       ; File operations performed
    cache_hits          QWORD   0       ; Cache hits
    cache_misses        QWORD   0       ; Cache misses
    last_error_time_ms  QWORD   0       ; Last error timestamp
QtStringStatistics ENDS

; =============================================================================
; ENUMERATION CONSTANTS
; =============================================================================

; String encoding types
ENCODING_UTF8           EQU 0
ENCODING_UTF16LE       EQU 1
ENCODING_UTF16BE       EQU 2
ENCODING_ASCII         EQU 3
ENCODING_LATIN1        EQU 4
ENCODING_UTF32LE       EQU 5
ENCODING_UTF32BE       EQU 6
ENCODING_WINDOWS1252   EQU 7
ENCODING_ISO88591      EQU 8

; File open modes
FILE_MODE_READ         EQU 1
FILE_MODE_WRITE        EQU 2
FILE_MODE_APPEND       EQU 4
FILE_MODE_BINARY       EQU 8
FILE_MODE_TEXT         EQU 16

; File access rights
FILE_ACCESS_READ       EQU 0x20000000
FILE_ACCESS_WRITE      EQU 0x40000000
FILE_ACCESS_APPEND     EQU 0x00000004

; Compression algorithms
COMPRESS_ZLIB          EQU 0
COMPRESS_LZMA          EQU 1
COMPRESS_BROTLI        EQU 2
COMPRESS_LZ4           EQU 3

; Network protocol types
NET_PROTOCOL_HTTP      EQU 0
NET_PROTOCOL_HTTPS     EQU 1
NET_PROTOCOL_FTP       EQU 2
NET_PROTOCOL_WEBSOCKET EQU 3

; Async I/O modes
ASYNC_MODE_OVERLAPPED  EQU 0
ASYNC_MODE_IOURING     EQU 1
ASYNC_MODE_CALLBACK    EQU 2

; String operation types
OP_CONCAT              EQU 0
OP_SUBSTRING           EQU 1
OP_REPLACE             EQU 2
OP_TRIM                EQU 3
OP_REVERSE             EQU 4
OP_UPPERCASE           EQU 5
OP_LOWERCASE           EQU 6
OP_FIND                EQU 7
OP_SPLIT               EQU 8

; Error codes
ERROR_SUCCESS          EQU 0
ERROR_INVALID_PTR      EQU 1
ERROR_BUFFER_TOO_SMALL EQU 2
ERROR_OUT_OF_MEMORY    EQU 3
ERROR_INVALID_ENCODING EQU 4
ERROR_FILE_NOT_FOUND   EQU 5
ERROR_FILE_ACCESS_DENY EQU 6
ERROR_FILE_LOCKED      EQU 7
ERROR_OPERATION_TIMEOUT EQU 8
ERROR_INVALID_PARAMETER EQU 9
ERROR_UNKNOWN          EQU 255

; =============================================================================
; EXTERN C DECLARATIONS
; =============================================================================

; String operations
extern wrapper_qstring_create:proc
extern wrapper_qstring_delete:proc
extern wrapper_qstring_append:proc
extern wrapper_qstring_clear:proc
extern wrapper_qstring_length:proc
extern wrapper_qstring_to_utf8:proc
extern wrapper_qstring_from_utf8:proc
extern wrapper_qstring_to_latin1:proc
extern wrapper_qstring_from_latin1:proc
extern wrapper_qstring_substring:proc
extern wrapper_qstring_find:proc
extern wrapper_qstring_replace:proc
extern wrapper_qstring_split:proc
extern wrapper_qstring_trim:proc
extern wrapper_qstring_uppercase:proc
extern wrapper_qstring_lowercase:proc
extern wrapper_qstring_compare:proc

; QByteArray operations
extern wrapper_qbytearray_create:proc
extern wrapper_qbytearray_delete:proc
extern wrapper_qbytearray_append:proc
extern wrapper_qbytearray_clear:proc
extern wrapper_qbytearray_length:proc
extern wrapper_qbytearray_at:proc
extern wrapper_qbytearray_set:proc
extern wrapper_qbytearray_contains:proc
extern wrapper_qbytearray_find:proc
extern wrapper_qbytearray_from_hex:proc
extern wrapper_qbytearray_to_hex:proc
extern wrapper_qbytearray_compress:proc
extern wrapper_qbytearray_decompress:proc

; QFile operations
extern wrapper_qfile_create:proc
extern wrapper_qfile_delete:proc
extern wrapper_qfile_open:proc
extern wrapper_qfile_close:proc
extern wrapper_qfile_read:proc
extern wrapper_qfile_write:proc
extern wrapper_qfile_seek:proc
extern wrapper_qfile_tell:proc
extern wrapper_qfile_read_all:proc
extern wrapper_qfile_exists:proc
extern wrapper_qfile_remove:proc
extern wrapper_qfile_rename:proc
extern wrapper_qfile_size:proc

; Utility operations
extern wrapper_qt_alloc:proc
extern wrapper_qt_free:proc
extern wrapper_qt_mutex_create:proc
extern wrapper_qt_mutex_delete:proc
extern wrapper_qt_mutex_lock:proc
extern wrapper_qt_mutex_unlock:proc
extern wrapper_qt_get_temp_path:proc
extern wrapper_qt_get_statistics:proc
extern wrapper_qt_clear_statistics:proc

; POSIX file operations
extern wrapper_posix_open:proc
extern wrapper_posix_close:proc
extern wrapper_posix_read:proc
extern wrapper_posix_write:proc
extern wrapper_posix_stat:proc

; Additional encoding conversions
extern wrapper_qstring_to_utf16:proc
extern wrapper_qstring_from_utf16:proc
extern wrapper_qstring_to_utf32:proc
extern wrapper_qstring_from_utf32:proc
extern wrapper_qstring_to_windows1252:proc
extern wrapper_qstring_from_windows1252:proc
extern wrapper_qstring_to_iso88591:proc
extern wrapper_qstring_from_iso88591:proc

; String formatting operations
extern wrapper_qstring_sprintf:proc
extern wrapper_qstring_format:proc
extern wrapper_qstring_replace_placeholder:proc

; Advanced compression
extern wrapper_qbytearray_compress_lzma:proc
extern wrapper_qbytearray_decompress_lzma:proc
extern wrapper_qbytearray_compress_brotli:proc
extern wrapper_qbytearray_decompress_brotli:proc
extern wrapper_qbytearray_compress_lz4:proc
extern wrapper_qbytearray_decompress_lz4:proc

; Network file operations
extern wrapper_net_http_get:proc
extern wrapper_net_http_post:proc
extern wrapper_net_ftp_get:proc
extern wrapper_net_ftp_put:proc
extern wrapper_net_websocket_open:proc
extern wrapper_net_websocket_send:proc
extern wrapper_net_websocket_recv:proc
extern wrapper_net_websocket_close:proc

; Async file operations
extern wrapper_qfile_open_async:proc
extern wrapper_qfile_read_async:proc
extern wrapper_qfile_write_async:proc
extern wrapper_qfile_close_async:proc
extern wrapper_async_wait:proc
extern wrapper_async_cancel:proc

; =============================================================================
; HELPER MACROS
; =============================================================================

; Macro to safely dereference with null check
CHECK_PTR macro ptr_reg
    cmp ptr_reg, 0
    je error_invalid_ptr
endm

; Macro to set error result
SET_ERROR macro error_code_val
    mov [rax].QtStringOpResult.error_code, error_code_val
    mov [rax].QtStringOpResult.success, 0
endm

; Macro to set success result
SET_SUCCESS macro
    mov [rax].QtStringOpResult.success, 1
    mov [rax].QtStringOpResult.error_code, ERROR_SUCCESS
endm

; Macro to allocate buffer
ALLOC_BUFFER macro size_reg
    mov rcx, size_reg
    call wrapper_qt_alloc
endm

; Macro to free buffer
FREE_BUFFER macro ptr_reg
    mov rcx, ptr_reg
    call wrapper_qt_free
endm

; =============================================================================
; ENDMARKER
; =============================================================================
