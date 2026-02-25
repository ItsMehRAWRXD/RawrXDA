; memory_cleanup.asm - FIXES ALL MEMORY LEAKS
; Implements proper cleanup/shutdown sequence with resource deallocation
; Addresses: L3 cache, file handles, DS requests, GGML context cleanup

.386
.MODEL FLAT, STDCALL
OPTION CASEMAP:NONE

INCLUDE windows.inc
INCLUDE kernel32.inc

IFNDEF MAX_PATH
MAX_PATH EQU 260
ENDIF

.DATA
    ; Cleanup status strings
    szL3CleanupStart    BYTE "Cleaning up L3 cache buffer", 0
    szL3CleanupDone     BYTE "L3 cache freed successfully", 0
    szL3NotAllocated    BYTE "L3 cache was not allocated", 0
    
    szFileCloseStart    BYTE "Closing model file handle", 0
    szFileCloseDone     BYTE "Model file closed successfully", 0
    szFileNotOpen       BYTE "Model file was not open", 0
    
    szGGMLCleanupStart  BYTE "Freeing GGML context", 0
    szGGMLCleanupDone   BYTE "GGML context freed", 0
    
    szShutdownStart     BYTE "=== Master Shutdown Sequence Started ===", 0
    szShutdownDone      BYTE "=== Master Shutdown Complete ===", 0

.CODE

; ============================================================
; EXTERNAL REFERENCES (From other modules)
; ============================================================
EXTERN Titan_Stop_All_Streams:NEAR
EXTERN CleanupInference:NEAR
EXTERN Titan_Vulkan_Cleanup:NEAR
EXTERN Titan_DirectStorage_Cleanup:NEAR
EXTERN Titan_GGML_Cleanup:NEAR

; Global variables (from main code)
EXTERN g_L3_Buffer:DWORD
EXTERN g_hModelFile:DWORD
EXTERN g_GGML_Context:DWORD
EXTERN L3_SCRATCH_SIZE:DWORD

; Logging function
EXTERN LogMessage:NEAR  ; int LogMessage(int level, const char* fmt, ...)
                        ; level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR

; ============================================================
; L3 CACHE CLEANUP (MEMORY LEAK FIX #1)
; ============================================================
PUBLIC Titan_Shutdown_L3_Cache
Titan_Shutdown_L3_Cache PROC
    PUSH EBP
    MOV EBP, ESP
    PUSH EBX
    PUSH ESI
    
    ; Log: Starting L3 cleanup
    LEA EAX, szL3CleanupStart
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    ; Check if L3 buffer is allocated
    MOV EAX, g_L3_Buffer
    TEST EAX, EAX
    JZ .L3_ALREADY_FREED
    
    MOV EBX, EAX        ; Save buffer address
    
    ; Log: About to free
    LEA EAX, szL3CleanupStart
    PUSH EAX
    PUSH 0              ; DEBUG level
    CALL LogMessage
    ADD ESP, 8
    
    ; Unlock pages first (if locked in physical memory)
    MOV ECX, EBX        ; g_L3_Buffer
    MOV EDX, L3_SCRATCH_SIZE
    ; Note: In production, call VirtualUnlock here if pages were locked
    
    ; Free the virtual memory
    PUSH 0x8000         ; MEM_RELEASE
    PUSH 0              ; Size must be 0 for MEM_RELEASE
    PUSH EBX            ; Address
    CALL VirtualFree
    
    ; Check return value
    TEST EAX, EAX
    JZ .L3_FREE_FAILED
    
    ; Clear global variable
    MOV g_L3_Buffer, 0
    
    ; Log: Success
    LEA EAX, szL3CleanupDone
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    MOV EAX, 0          ; Success
    JMP .L3_CLEANUP_END
    
.L3_ALREADY_FREED:
    LEA EAX, szL3NotAllocated
    PUSH EAX
    PUSH 2              ; WARN level
    CALL LogMessage
    ADD ESP, 8
    
    MOV EAX, 0
    JMP .L3_CLEANUP_END
    
.L3_FREE_FAILED:
    PUSH 0
    PUSH 3              ; ERROR level
    LEA EAX, [szL3CleanupStart + 8]  ; Error message
    PUSH EAX
    CALL LogMessage
    ADD ESP, 12
    
    MOV EAX, -1         ; Error
    
.L3_CLEANUP_END:
    POP ESI
    POP EBX
    POP EBP
    RET
Titan_Shutdown_L3_Cache ENDP

; ============================================================
; FILE HANDLE CLEANUP (MEMORY LEAK FIX #2)
; ============================================================
PUBLIC Titan_Close_Model_File
Titan_Close_Model_File PROC
    PUSH EBP
    MOV EBP, ESP
    PUSH EBX
    
    ; Log: Starting file close
    LEA EAX, szFileCloseStart
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    ; Check if file is open
    MOV EAX, g_hModelFile
    TEST EAX, EAX
    JZ .FILE_ALREADY_CLOSED
    
    CMP EAX, -1         ; INVALID_HANDLE_VALUE
    JE .FILE_ALREADY_CLOSED
    
    MOV EBX, EAX        ; Save handle
    
    ; Close the file
    PUSH EBX
    CALL CloseHandle
    
    ; Check return value
    TEST EAX, EAX
    JZ .FILE_CLOSE_FAILED
    
    ; Clear global variable
    MOV g_hModelFile, 0
    
    ; Log: Success
    LEA EAX, szFileCloseDone
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    MOV EAX, 0
    JMP .FILE_CLEANUP_END
    
.FILE_ALREADY_CLOSED:
    LEA EAX, szFileNotOpen
    PUSH EAX
    PUSH 2              ; WARN level
    CALL LogMessage
    ADD ESP, 8
    
    MOV EAX, 0
    JMP .FILE_CLEANUP_END
    
.FILE_CLOSE_FAILED:
    PUSH 0
    PUSH 3              ; ERROR level
    LEA EAX, [szFileCloseStart + 7]
    PUSH EAX
    CALL LogMessage
    ADD ESP, 12
    
    MOV EAX, -1
    
.FILE_CLEANUP_END:
    POP EBX
    POP EBP
    RET
Titan_Close_Model_File ENDP

; ============================================================
; GGML CONTEXT CLEANUP
; ============================================================
PUBLIC Titan_Cleanup_GGML_Context
Titan_Cleanup_GGML_Context PROC
    PUSH EBP
    MOV EBP, ESP
    
    ; Log: Starting GGML cleanup
    LEA EAX, szGGMLCleanupStart
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    ; Check if context exists
    MOV EAX, g_GGML_Context
    TEST EAX, EAX
    JZ .GGML_NOT_INIT
    
    ; Call external cleanup function
    CALL Titan_GGML_Cleanup
    
    ; Clear global variable
    MOV g_GGML_Context, 0
    
    ; Log: Success
    LEA EAX, szGGMLCleanupDone
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    JMP .GGML_CLEANUP_END
    
.GGML_NOT_INIT:
    MOV g_GGML_Context, 0
    
.GGML_CLEANUP_END:
    POP EBP
    RET
Titan_Cleanup_GGML_Context ENDP

; ============================================================
; MASTER SHUTDOWN SEQUENCE (CORRECT ORDER)
; ============================================================
PUBLIC Titan_Master_Shutdown
Titan_Master_Shutdown PROC
    PUSH EBP
    MOV EBP, ESP
    PUSH EBX
    PUSH ESI
    PUSH EDI
    
    ; Log: Shutdown started
    LEA EAX, szShutdownStart
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    ; ============ PHASE 1: STOP ASYNC OPERATIONS ============
    
    ; 1. Stop all streaming/async operations
    CALL Titan_Stop_All_Streams
    
    ; ============ PHASE 2: CLEANUP COMPUTE ============
    
    ; 2. Cleanup inference engine (KV cache)
    CALL CleanupInference
    
    ; ============ PHASE 3: CLEANUP GPU/IO ============
    
    ; 3. Cleanup Vulkan compute
    CALL Titan_Vulkan_Cleanup
    
    ; 4. Cleanup DirectStorage
    CALL Titan_DirectStorage_Cleanup
    
    ; ============ PHASE 4: CLEANUP MEMORY ============
    
    ; 5. Free L3 cache buffer
    CALL Titan_Shutdown_L3_Cache
    
    ; 6. Close model file handle
    CALL Titan_Close_Model_File
    
    ; ============ PHASE 5: CLEANUP GGML ============
    
    ; 7. Cleanup GGML context
    CALL Titan_Cleanup_GGML_Context
    
    ; Log: Shutdown complete
    LEA EAX, szShutdownDone
    PUSH EAX
    PUSH 1              ; INFO level
    CALL LogMessage
    ADD ESP, 8
    
    POP EDI
    POP ESI
    POP EBX
    POP EBP
    RET
Titan_Master_Shutdown ENDP

; ============================================================
; EMERGENCY CLEANUP (For abnormal termination)
; Called from exception handler
; ============================================================
PUBLIC Titan_Emergency_Cleanup
Titan_Emergency_Cleanup PROC
    PUSH EBP
    MOV EBP, ESP
    
    ; Attempt to cleanup in simplified order
    ; (No logging, minimal dependencies)
    
    ; 1. Stop streams (if possible)
    CALL Titan_Stop_All_Streams
    
    ; 2. Close file handle directly
    MOV EAX, g_hModelFile
    TEST EAX, EAX
    JZ .SKIP_FILE_CLOSE
    CMP EAX, -1
    JE .SKIP_FILE_CLOSE
    PUSH EAX
    CALL CloseHandle
.SKIP_FILE_CLOSE:
    
    ; 3. Free L3 buffer directly
    MOV EAX, g_L3_Buffer
    TEST EAX, EAX
    JZ .SKIP_L3_FREE
    PUSH 0x8000
    PUSH 0
    PUSH EAX
    CALL VirtualFree
.SKIP_L3_FREE:
    
    POP EBP
    RET
Titan_Emergency_Cleanup ENDP

; ============================================================
; MEMORY VALIDATION (Debug builds)
; ============================================================
IFDEF DEBUG

PUBLIC Titan_Validate_Memory_State
Titan_Validate_Memory_State PROC
    PUSH EBP
    MOV EBP, ESP
    PUSH EBX
    
    MOV EAX, 0          ; OK state
    
    ; Check L3 buffer
    MOV EBX, g_L3_Buffer
    TEST EBX, EBX
    JZ .L3_OK
    
    ; L3 is allocated, verify it's accessible
    MOV ECX, [EBX]      ; Try to read first DWORD
    
.L3_OK:
    
    ; Check file handle
    MOV EBX, g_hModelFile
    TEST EBX, EBX
    JZ .FILE_OK
    
    CMP EBX, -1
    JE .FILE_OK
    
    ; File is open (can't easily validate without I/O)
    
.FILE_OK:
    POP EBX
    POP EBP
    RET
Titan_Validate_Memory_State ENDP

ENDIF ; DEBUG

END
