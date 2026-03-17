;==============================================================================
; resource_guards.asm - RAII-Style Resource Management for RawrXD
; ==============================================================================
; PHASE 4: ERROR HANDLING ENHANCEMENT
; Implements RAII-style (Resource Acquisition Is Initialization) wrappers:
; - Automatic resource cleanup on scope exit
; - File handle guards
; - Memory allocation guards
; - Mutex/lock guards
; - Registry key guards
; - Network socket guards
;==============================================================================

option casemap:none

; ============================================================================
; Local error constants (kept consistent with error_handler.asm)
; ============================================================================
ERROR_SEVERITY_WARNING     EQU 1
ERROR_SEVERITY_ERROR       EQU 2

ERROR_CATEGORY_MEMORY      EQU 0
ERROR_CATEGORY_IO          EQU 1
ERROR_CATEGORY_NETWORK     EQU 2
ERROR_CATEGORY_SYSTEM      EQU 5

;==============================================================================
; Win32 constants
;==============================================================================
INVALID_HANDLE_VALUE    EQU -1
INVALID_SOCKET          EQU -1
SOCKET_ERROR            EQU -1

;==============================================================================
; RESOURCE GUARD TYPES
;==============================================================================
GUARD_TYPE_FILE         EQU 1
GUARD_TYPE_MEMORY       EQU 2
GUARD_TYPE_MUTEX        EQU 3
GUARD_TYPE_REGISTRY     EQU 4
GUARD_TYPE_SOCKET       EQU 5
GUARD_TYPE_EVENT        EQU 6

;==============================================================================
; EXTERNAL DECLARATIONS
;==============================================================================
EXTERN CloseHandle:PROC
EXTERN RegCloseKey:PROC
EXTERN closesocket:PROC
EXTERN asm_malloc:PROC
EXTERN asm_free:PROC
EXTERN asm_mutex_unlock:PROC
EXTERN ErrorHandler_Capture:PROC

;==============================================================================
; STRUCTURES
;==============================================================================
RESOURCE_GUARD STRUCT
    guard_type          DWORD ?
    resource_handle     QWORD ?
    is_active           DWORD ?
    cleanup_func        QWORD ?
    aux_data            QWORD ?
RESOURCE_GUARD ENDS

;==============================================================================
; DATA SEGMENT
;==============================================================================
.data
    szGuardFileClose    BYTE "File handle guard cleanup", 0
    szGuardMemFree      BYTE "Memory guard cleanup", 0
    szGuardMutexUnlock  BYTE "Mutex guard cleanup", 0
    szGuardRegClose     BYTE "Registry key guard cleanup", 0
    szGuardSocketClose  BYTE "Socket guard cleanup", 0
    szGuardEventClose   BYTE "Event handle guard cleanup", 0
    
    szGuardInitFail     BYTE "Resource guard initialization failed", 0
    szGuardCleanupFail  BYTE "Resource guard cleanup failed", 0

;==============================================================================
; CODE SEGMENT
;==============================================================================
.code

;==============================================================================
; PUBLIC: Guard_CreateFile(handle: rcx) -> rax (guard ptr or NULL)
; Creates a file handle guard that will CloseHandle on destruction
;==============================================================================
PUBLIC Guard_CreateFile
ALIGN 16
Guard_CreateFile PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; Save file handle
    
    ; Allocate guard structure
    MOV rcx, SIZE RESOURCE_GUARD
    MOV rdx, 16
    CALL asm_malloc
    TEST rax, rax
    JZ guard_file_fail
    
    ; Initialize guard
    MOV DWORD PTR [rax + RESOURCE_GUARD.guard_type], GUARD_TYPE_FILE
    MOV QWORD PTR [rax + RESOURCE_GUARD.resource_handle], rbx
    MOV DWORD PTR [rax + RESOURCE_GUARD.is_active], 1
    LEA rcx, [Guard_CleanupFile]
    MOV QWORD PTR [rax + RESOURCE_GUARD.cleanup_func], rcx
    MOV QWORD PTR [rax + RESOURCE_GUARD.aux_data], 0
    
    JMP guard_file_done
    
guard_file_fail:
    ; Log error
    MOV ecx, 1001
    MOV edx, ERROR_SEVERITY_ERROR
    MOV r8d, ERROR_CATEGORY_MEMORY
    LEA r9, [szGuardInitFail]
    CALL ErrorHandler_Capture
    XOR rax, rax
    
guard_file_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CreateFile ENDP

;==============================================================================
; PUBLIC: Guard_CreateMemory(ptr: rcx) -> rax (guard ptr or NULL)
; Creates a memory guard that will asm_free on destruction
;==============================================================================
PUBLIC Guard_CreateMemory
ALIGN 16
Guard_CreateMemory PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; Save memory pointer
    
    ; Allocate guard structure
    MOV rcx, SIZE RESOURCE_GUARD
    MOV rdx, 16
    CALL asm_malloc
    TEST rax, rax
    JZ guard_mem_fail
    
    ; Initialize guard
    MOV DWORD PTR [rax + RESOURCE_GUARD.guard_type], GUARD_TYPE_MEMORY
    MOV QWORD PTR [rax + RESOURCE_GUARD.resource_handle], rbx
    MOV DWORD PTR [rax + RESOURCE_GUARD.is_active], 1
    LEA rcx, [Guard_CleanupMemory]
    MOV QWORD PTR [rax + RESOURCE_GUARD.cleanup_func], rcx
    MOV QWORD PTR [rax + RESOURCE_GUARD.aux_data], 0
    
    JMP guard_mem_done
    
guard_mem_fail:
    MOV ecx, 1002
    MOV edx, ERROR_SEVERITY_ERROR
    MOV r8d, ERROR_CATEGORY_MEMORY
    LEA r9, [szGuardInitFail]
    CALL ErrorHandler_Capture
    XOR rax, rax
    
guard_mem_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CreateMemory ENDP

;==============================================================================
; PUBLIC: Guard_CreateMutex(mutex_handle: rcx) -> rax (guard ptr or NULL)
; Creates a mutex guard that will unlock on destruction
;==============================================================================
PUBLIC Guard_CreateMutex
ALIGN 16
Guard_CreateMutex PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; Save mutex handle
    
    ; Allocate guard structure
    MOV rcx, SIZE RESOURCE_GUARD
    MOV rdx, 16
    CALL asm_malloc
    TEST rax, rax
    JZ guard_mutex_fail
    
    ; Initialize guard
    MOV DWORD PTR [rax + RESOURCE_GUARD.guard_type], GUARD_TYPE_MUTEX
    MOV QWORD PTR [rax + RESOURCE_GUARD.resource_handle], rbx
    MOV DWORD PTR [rax + RESOURCE_GUARD.is_active], 1
    LEA rcx, [Guard_CleanupMutex]
    MOV QWORD PTR [rax + RESOURCE_GUARD.cleanup_func], rcx
    MOV QWORD PTR [rax + RESOURCE_GUARD.aux_data], 0
    
    JMP guard_mutex_done
    
guard_mutex_fail:
    MOV ecx, 1003
    MOV edx, ERROR_SEVERITY_ERROR
    MOV r8d, ERROR_CATEGORY_SYSTEM
    LEA r9, [szGuardInitFail]
    CALL ErrorHandler_Capture
    XOR rax, rax
    
guard_mutex_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CreateMutex ENDP

;==============================================================================
; PUBLIC: Guard_CreateRegistry(hkey: rcx) -> rax (guard ptr or NULL)
; Creates a registry key guard that will RegCloseKey on destruction
;==============================================================================
PUBLIC Guard_CreateRegistry
ALIGN 16
Guard_CreateRegistry PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; Save registry key
    
    ; Allocate guard structure
    MOV rcx, SIZE RESOURCE_GUARD
    MOV rdx, 16
    CALL asm_malloc
    TEST rax, rax
    JZ guard_reg_fail
    
    ; Initialize guard
    MOV DWORD PTR [rax + RESOURCE_GUARD.guard_type], GUARD_TYPE_REGISTRY
    MOV QWORD PTR [rax + RESOURCE_GUARD.resource_handle], rbx
    MOV DWORD PTR [rax + RESOURCE_GUARD.is_active], 1
    LEA rcx, [Guard_CleanupRegistry]
    MOV QWORD PTR [rax + RESOURCE_GUARD.cleanup_func], rcx
    MOV QWORD PTR [rax + RESOURCE_GUARD.aux_data], 0
    
    JMP guard_reg_done
    
guard_reg_fail:
    MOV ecx, 1004
    MOV edx, ERROR_SEVERITY_ERROR
    MOV r8d, ERROR_CATEGORY_SYSTEM
    LEA r9, [szGuardInitFail]
    CALL ErrorHandler_Capture
    XOR rax, rax
    
guard_reg_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CreateRegistry ENDP

;==============================================================================
; PUBLIC: Guard_CreateSocket(socket: rcx) -> rax (guard ptr or NULL)
; Creates a socket guard that will closesocket on destruction
;==============================================================================
PUBLIC Guard_CreateSocket
ALIGN 16
Guard_CreateSocket PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; Save socket
    
    ; Allocate guard structure
    MOV rcx, SIZE RESOURCE_GUARD
    MOV rdx, 16
    CALL asm_malloc
    TEST rax, rax
    JZ guard_socket_fail
    
    ; Initialize guard
    MOV DWORD PTR [rax + RESOURCE_GUARD.guard_type], GUARD_TYPE_SOCKET
    MOV QWORD PTR [rax + RESOURCE_GUARD.resource_handle], rbx
    MOV DWORD PTR [rax + RESOURCE_GUARD.is_active], 1
    LEA rcx, [Guard_CleanupSocket]
    MOV QWORD PTR [rax + RESOURCE_GUARD.cleanup_func], rcx
    MOV QWORD PTR [rax + RESOURCE_GUARD.aux_data], 0
    
    JMP guard_socket_done
    
guard_socket_fail:
    MOV ecx, 1005
    MOV edx, ERROR_SEVERITY_ERROR
    MOV r8d, ERROR_CATEGORY_NETWORK
    LEA r9, [szGuardInitFail]
    CALL ErrorHandler_Capture
    XOR rax, rax
    
guard_socket_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CreateSocket ENDP

;==============================================================================
; PUBLIC: Guard_Destroy(guard: rcx) -> void
; Destroys a resource guard and performs cleanup
;==============================================================================
PUBLIC Guard_Destroy
ALIGN 16
Guard_Destroy PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx            ; guard ptr
    TEST rbx, rbx
    JZ guard_destroy_done
    
    ; Check if guard is still active
    MOV eax, DWORD PTR [rbx + RESOURCE_GUARD.is_active]
    TEST eax, eax
    JZ guard_already_cleaned
    
    ; Call cleanup function
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.cleanup_func]
    TEST rcx, rcx
    JZ skip_cleanup
    
    ; Pass guard as parameter to cleanup function
    MOV rcx, rbx
    CALL QWORD PTR [rbx + RESOURCE_GUARD.cleanup_func]
    
skip_cleanup:
    ; Mark as inactive
    MOV DWORD PTR [rbx + RESOURCE_GUARD.is_active], 0
    
guard_already_cleaned:
    ; Free guard structure itself
    MOV rcx, rbx
    CALL asm_free
    
guard_destroy_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_Destroy ENDP

;==============================================================================
; PUBLIC: Guard_Release(guard: rcx) -> void
; Releases ownership without cleanup (for successful transfers)
;==============================================================================
PUBLIC Guard_Release
ALIGN 16
Guard_Release PROC
    TEST rcx, rcx
    JZ guard_release_done
    
    ; Mark as inactive to prevent cleanup
    MOV DWORD PTR [rcx + RESOURCE_GUARD.is_active], 0
    
guard_release_done:
    RET
Guard_Release ENDP

;==============================================================================
; PRIVATE: Guard_CleanupFile(guard: rcx) -> void
; Cleanup function for file handles
;==============================================================================
ALIGN 16
Guard_CleanupFile PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.resource_handle]
    
    ; Validate handle
    CMP rcx, INVALID_HANDLE_VALUE
    JE cleanup_file_done
    TEST rcx, rcx
    JZ cleanup_file_done
    
    ; Close file handle
    CALL CloseHandle
    TEST eax, eax
    JNZ cleanup_file_done
    
    ; Log cleanup failure
    MOV ecx, 2001
    MOV edx, ERROR_SEVERITY_WARNING
    MOV r8d, ERROR_CATEGORY_IO
    LEA r9, [szGuardCleanupFail]
    CALL ErrorHandler_Capture
    
cleanup_file_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CleanupFile ENDP

;==============================================================================
; PRIVATE: Guard_CleanupMemory(guard: rcx) -> void
; Cleanup function for memory allocations
;==============================================================================
ALIGN 16
Guard_CleanupMemory PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.resource_handle]
    
    ; Validate pointer
    TEST rcx, rcx
    JZ cleanup_mem_done
    
    ; Free memory
    CALL asm_free
    
cleanup_mem_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CleanupMemory ENDP

;==============================================================================
; PRIVATE: Guard_CleanupMutex(guard: rcx) -> void
; Cleanup function for mutex locks
;==============================================================================
ALIGN 16
Guard_CleanupMutex PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.resource_handle]
    
    ; Validate handle
    TEST rcx, rcx
    JZ cleanup_mutex_done
    
    ; Unlock mutex
    CALL asm_mutex_unlock
    
cleanup_mutex_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CleanupMutex ENDP

;==============================================================================
; PRIVATE: Guard_CleanupRegistry(guard: rcx) -> void
; Cleanup function for registry keys
;==============================================================================
ALIGN 16
Guard_CleanupRegistry PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.resource_handle]
    
    ; Validate handle
    TEST rcx, rcx
    JZ cleanup_reg_done
    
    ; Close registry key
    CALL RegCloseKey
    TEST eax, eax
    JE cleanup_reg_done
    
    ; Log cleanup failure
    MOV ecx, 2004
    MOV edx, ERROR_SEVERITY_WARNING
    MOV r8d, ERROR_CATEGORY_SYSTEM
    LEA r9, [szGuardCleanupFail]
    CALL ErrorHandler_Capture
    
cleanup_reg_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CleanupRegistry ENDP

;==============================================================================
; PRIVATE: Guard_CleanupSocket(guard: rcx) -> void
; Cleanup function for sockets
;==============================================================================
ALIGN 16
Guard_CleanupSocket PROC
    PUSH rbx
    SUB rsp, 32
    
    MOV rbx, rcx
    MOV rcx, QWORD PTR [rbx + RESOURCE_GUARD.resource_handle]
    
    ; Validate socket
    CMP rcx, INVALID_SOCKET
    JE cleanup_socket_done
    
    ; Close socket
    CALL closesocket
    CMP eax, SOCKET_ERROR
    JNE cleanup_socket_done
    
    ; Log cleanup failure
    MOV ecx, 2005
    MOV edx, ERROR_SEVERITY_WARNING
    MOV r8d, ERROR_CATEGORY_NETWORK
    LEA r9, [szGuardCleanupFail]
    CALL ErrorHandler_Capture
    
cleanup_socket_done:
    ADD rsp, 32
    POP rbx
    RET
Guard_CleanupSocket ENDP

END
