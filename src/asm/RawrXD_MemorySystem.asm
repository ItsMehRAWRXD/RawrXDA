; =============================================================================
; RawrXD_MemorySystem.asm - Three-Layer Persistent Memory Architecture
; Based on Claude Code's MEMORY.md + Topic Files + Transcript Index
; =============================================================================

OPTION CASemap:NONE
; OPTION WIN64:3

INCLUDE win64.inc
INCLUDELIB \masm64\lib64\kernel32.lib
INCLUDELIB \masm64\lib64\user32.lib
INCLUDELIB \masm64\lib64\ntdll.lib

; Constants
MEMORY_LAYER_INDEX      EQU 1
MEMORY_LAYER_TOPICS     EQU 2
MEMORY_LAYER_TRANSCRIPT EQU 3

MEM_TYPE_CONTEXT        EQU 1
MEM_TYPE_DECISION       EQU 2
MEM_TYPE_PATTERN        EQU 3

MAX_TOPICS              EQU 256
HASH_TABLE_SIZE         EQU 1024
MEMORY_MAGIC            EQU 0544D5852h  ; 'RXMT'

; Structures
MEMORY_INDEX_ENTRY STRUCT
    entryType       DWORD ?
    priority        DWORD ?
    topicHash       DWORD ?
    lastAccessed    QWORD ?
    accessCount     DWORD ?
    title           BYTE 128 DUP(?)
    summary         BYTE 512 DUP(?)
MEMORY_INDEX_ENTRY ENDS

MEMORY_SYSTEM_CONTEXT STRUCT
    indexEntries    QWORD ?
    indexCount      DWORD ?
    indexCapacity   DWORD ?
    indexModified   BYTE ?
    topicCache      QWORD ?
    topicCacheSize  DWORD ?
    topicHashTable  QWORD ?
    hMutex          QWORD ?
    hIndexEvent     QWORD ?
    basePath        BYTE 260 DUP(?)
    indexPath       BYTE 260 DUP(?)
    autoSaveInterval DWORD ?
MEMORY_SYSTEM_CONTEXT ENDS

.DATA
g_szMemorySubdir    BYTE "memory", 0
g_szIndexFile       BYTE "MEMORY.md", 0
g_szTopicsDir       BYTE "topics", 0
g_szIndexHeader     BYTE "# RawrXD Memory Index", 13, 10, 0

.CODE

MemorySystem_Initialize PROC FRAME
    LOCAL hHeap:QWORD
    push rbx
    push rsi
    push rdi
    push r12
    push r14
    
    mov r12, rcx                    ; HWND (placeholder use)
    mov r13, rdx                    ; Project path
    mov r14, r8                     ; Context
    
    call GetProcessHeap
    mov hHeap, rax
    
    ; Setup paths
    lea rdi, [r14].MEMORY_SYSTEM_CONTEXT.basePath
    mov rsi, r13
@@copy:
    lodsb
    stosb
    test al, al
    jnz @@copy
    
    ; Create index path
    lea rdi, [r14].MEMORY_SYSTEM_CONTEXT.indexPath
    lea rsi, [r14].MEMORY_SYSTEM_CONTEXT.basePath
@@copy2:
    lodsb
    stosb
    test al, al
    jnz @@copy2
    
    ; Allocate Layer 1 Index
    mov rcx, hHeap
    xor rdx, rdx
    mov r8, MAX_TOPICS * SIZEOF MEMORY_INDEX_ENTRY
    call HeapAlloc
    mov [r14].MEMORY_SYSTEM_CONTEXT.indexEntries, rax
    
    mov [r14].MEMORY_SYSTEM_CONTEXT.indexCapacity, MAX_TOPICS
    
    ; Sync primitives
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    call CreateMutexW
    mov [r14].MEMORY_SYSTEM_CONTEXT.hMutex, rax
    
    xor ecx, ecx
    xor edx, edx
    xor r8d, r8d
    xor r9d, r9d
    call CreateEventW
    mov [r14].MEMORY_SYSTEM_CONTEXT.hIndexEvent, rax
    
    mov rax, TRUE
    pop r14
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
MemorySystem_Initialize ENDP

PUBLIC MemorySystem_Initialize

; =============================================================================
; MemorySystem_AddEntry - MASM module for adding a memory entry
; =============================================================================
MemorySystem_AddEntry PROC FRAME
    ; [Placeholder for logic to insert into memory index and topics]
    mov rax, 1
    ret
MemorySystem_AddEntry ENDP

PUBLIC MemorySystem_AddEntry

END
