; ================================================
; RawrXD ASM Constants Include File
; Shared constants for all MASM assembly modules
; ================================================

IFNDEF RAWRXD_ASM_CONSTANTS_INC
RAWRXD_ASM_CONSTANTS_INC EQU 1

; ================================================
; Global System Constants
; ================================================
RAWRXD_VERSION_MAJOR EQU 7
RAWRXD_VERSION_MINOR EQU 4
RAWRXD_VERSION_PATCH EQU 0

; ================================================
; Error Codes
; ================================================
RAWRXD_SUCCESS EQU 0
RAWRXD_ERROR_INVALID_PARAM EQU 1
RAWRXD_ERROR_OUT_OF_MEMORY EQU 2
RAWRXD_ERROR_TIMEOUT EQU 3
RAWRXD_ERROR_NOT_IMPLEMENTED EQU 4
RAWRXD_ERROR_ACCESS_DENIED EQU 5

; ================================================
; Memory Layout Constants
; ================================================
CACHE_LINE_SIZE EQU 64
PAGE_SIZE EQU 4096
STACK_ALIGNMENT EQU 16

; ================================================
; SIMD Constants
; ================================================
AVX512_VECTOR_SIZE EQU 512
AVX256_VECTOR_SIZE EQU 256
SSE_VECTOR_SIZE EQU 128

; ================================================
; Cognitive Processing Constants
; ================================================
MAX_COGNITIVE_DEPTH EQU 16
DEFAULT_PATTERN_THRESHOLD EQU 0.75

; ================================================
; Token Processing Constants
; ================================================
MAX_VOCAB_SIZE EQU 50000
MAX_SEQUENCE_LENGTH EQU 4096
TOKEN_EMBEDDING_SIZE EQU 4096

; ================================================
; Buffer Size Constants
; ================================================
SMALL_BUFFER_SIZE EQU 1024
MEDIUM_BUFFER_SIZE EQU 4096
LARGE_BUFFER_SIZE EQU 16384
HUGE_BUFFER_SIZE EQU 65536

ENDIF ; RAWRXD_ASM_CONSTANTS_INC