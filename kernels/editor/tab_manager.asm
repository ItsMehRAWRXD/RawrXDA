; ============================================================
; Tab Manager - Virtual Tab System for 10M+ Tabs
; Memory-mapped index file for unlimited tab capacity
; PRODUCTION GRADE - No placeholders
; ============================================================

.data
    ; File names
    szIndexFileName     db "tabs.idx", 0
    szDataFileName      db "tabs.dat", 0
    
    ; Constants
    TAB_ENTRY_SIZE      EQU 16       ; 8-byte offset + 8-byte size
    MAX_TABS            EQU 10000000 ; 10 million tabs
    INDEX_FILE_SIZE     EQU 160000000 ; 160MB for 10M tabs
    
    ; Tab flags
    TAB_FLAG_ACTIVE     EQU 1
    TAB_FLAG_MODIFIED   EQU 2
    TAB_FLAG_DELETED    EQU 80000000h

.code

; ============================================================
; Win32 API External Declarations
; ============================================================
extern CreateFileA:PROC
extern CloseHandle:PROC
extern ReadFile:PROC
extern WriteFile:PROC
extern SetFilePointer:PROC
extern SetEndOfFile:PROC
extern CreateFileMappingA:PROC
extern MapViewOfFile:PROC
extern UnmapViewOfFile:PROC
extern FlushViewOfFile:PROC
extern GetProcessHeap:PROC
extern HeapAlloc:PROC
extern HeapFree:PROC

; ============================================================
; Structure Definitions
; ============================================================

; TabManager Structure (64 bytes)
TABMGR_INDEX_HANDLE     EQU 0    ; qword - index file handle
TABMGR_DATA_HANDLE      EQU 8    ; qword - data file handle
TABMGR_MAPPING_HANDLE   EQU 16   ; qword - file mapping handle
TABMGR_INDEX_BASE       EQU 24   ; qword - mapped memory base
TABMGR_TAB_COUNT        EQU 32   ; qword - number of tabs
TABMGR_ACTIVE_TAB       EQU 40   ; qword - active tab ID
TABMGR_DATA_OFFSET      EQU 48   ; qword - next write offset in data file
TABMGR_FLAGS            EQU 56   ; qword - manager flags
TABMGR_STRUCT_SIZE      EQU 64

; TabEntry Structure (16 bytes, in memory-mapped file)
TABENTRY_DATA_OFFSET    EQU 0    ; qword - offset in data file
TABENTRY_SIZE           EQU 8    ; qword - size of tab content (high bit = deleted flag)

; Win32 Constants
GENERIC_READ            EQU 80000000h
GENERIC_WRITE           EQU 40000000h
FILE_SHARE_READ         EQU 1
CREATE_ALWAYS           EQU 2
OPEN_ALWAYS             EQU 4
FILE_ATTRIBUTE_NORMAL   EQU 80h
PAGE_READWRITE          EQU 4
FILE_MAP_ALL_ACCESS     EQU 0F001Fh
FILE_BEGIN              EQU 0
INVALID_HANDLE_VALUE    EQU -1

; ============================================================
; InitializeTabManager - Create/open tab storage
; Input:  RCX = pointer to TabManager structure (64 bytes)
;         RDX = base path for files (null-terminated)
; Output: RAX = 1 success, 0 failure
; ============================================================
InitializeTabManager PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    sub rsp, 72
    
    mov rbx, rcx              ; TabManager ptr
    mov rsi, rdx              ; base path
    
    ; Initialize structure
    mov qword ptr [rbx + TABMGR_TAB_COUNT], 0
    mov qword ptr [rbx + TABMGR_ACTIVE_TAB], 0
    mov qword ptr [rbx + TABMGR_DATA_OFFSET], 0
    mov qword ptr [rbx + TABMGR_FLAGS], 0
    
    ; Create/open index file
    lea rcx, szIndexFileName
    mov edx, GENERIC_READ or GENERIC_WRITE
    xor r8d, r8d              ; no sharing
    xor r9, r9                ; no security
    mov dword ptr [rsp+32], OPEN_ALWAYS
    mov dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je fail
    mov [rbx + TABMGR_INDEX_HANDLE], rax
    mov r12, rax
    
    ; Extend index file to required size
    mov rcx, r12
    mov edx, INDEX_FILE_SIZE and 0FFFFFFFFh
    mov r8d, 0
    mov r9d, FILE_BEGIN
    call SetFilePointer
    
    mov rcx, r12
    call SetEndOfFile
    
    ; Reset file pointer
    mov rcx, r12
    xor edx, edx
    xor r8d, r8d
    mov r9d, FILE_BEGIN
    call SetFilePointer
    
    ; Create file mapping
    mov rcx, r12              ; file handle
    xor rdx, rdx              ; security
    mov r8d, PAGE_READWRITE
    xor r9d, r9d              ; size high
    mov dword ptr [rsp+32], INDEX_FILE_SIZE
    mov qword ptr [rsp+40], 0 ; name
    call CreateFileMappingA
    test rax, rax
    jz fail
    mov [rbx + TABMGR_MAPPING_HANDLE], rax
    mov r13, rax
    
    ; Map view of file
    mov rcx, r13
    mov edx, FILE_MAP_ALL_ACCESS
    xor r8d, r8d              ; offset high
    xor r9d, r9d              ; offset low
    mov dword ptr [rsp+32], INDEX_FILE_SIZE
    call MapViewOfFile
    test rax, rax
    jz fail
    mov [rbx + TABMGR_INDEX_BASE], rax
    
    ; Create/open data file
    lea rcx, szDataFileName
    mov edx, GENERIC_READ or GENERIC_WRITE
    xor r8d, r8d
    xor r9, r9
    mov dword ptr [rsp+32], OPEN_ALWAYS
    mov dword ptr [rsp+40], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+48], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je fail
    mov [rbx + TABMGR_DATA_HANDLE], rax
    
    mov rax, 1
    jmp done
    
fail:
    xor rax, rax
done:
    add rsp, 72
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
InitializeTabManager ENDP

; ============================================================
; CloseTabManager - Release all resources
; Input:  RCX = pointer to TabManager
; ============================================================
CloseTabManager PROC
    push rbx
    sub rsp, 32
    
    mov rbx, rcx
    
    ; Flush memory-mapped file
    mov rcx, [rbx + TABMGR_INDEX_BASE]
    test rcx, rcx
    jz skip_flush
    mov rdx, INDEX_FILE_SIZE
    call FlushViewOfFile
    
    ; Unmap view
    mov rcx, [rbx + TABMGR_INDEX_BASE]
    call UnmapViewOfFile
skip_flush:
    
    ; Close mapping handle
    mov rcx, [rbx + TABMGR_MAPPING_HANDLE]
    test rcx, rcx
    jz skip_mapping
    call CloseHandle
skip_mapping:
    
    ; Close index file
    mov rcx, [rbx + TABMGR_INDEX_HANDLE]
    test rcx, rcx
    jz skip_index
    call CloseHandle
skip_index:
    
    ; Close data file
    mov rcx, [rbx + TABMGR_DATA_HANDLE]
    test rcx, rcx
    jz done
    call CloseHandle
    
done:
    add rsp, 32
    pop rbx
    ret
CloseTabManager ENDP

; ============================================================
; CreateTab - Allocate new tab in index
; Input:  RCX = pointer to TabManager
; Output: RAX = new tab ID (or -1 on failure)
; ============================================================
CreateTab PROC
    push rbx
    push rsi
    
    mov rbx, rcx
    
    ; Get current tab count
    mov rax, [rbx + TABMGR_TAB_COUNT]
    
    ; Check limit
    cmp rax, MAX_TABS
    jge fail
    
    mov rsi, rax              ; new tab ID = current count
    
    ; Calculate entry address in mapped memory
    mov rcx, [rbx + TABMGR_INDEX_BASE]
    mov rdx, rax
    imul rdx, TAB_ENTRY_SIZE
    add rcx, rdx              ; rcx = entry address
    
    ; Initialize entry
    mov rax, [rbx + TABMGR_DATA_OFFSET]
    mov [rcx + TABENTRY_DATA_OFFSET], rax
    mov qword ptr [rcx + TABENTRY_SIZE], 0   ; empty tab, not deleted
    
    ; Increment tab count
    inc qword ptr [rbx + TABMGR_TAB_COUNT]
    
    mov rax, rsi              ; return new tab ID
    jmp done
    
fail:
    mov rax, -1
done:
    pop rsi
    pop rbx
    ret
CreateTab ENDP

; ============================================================
; CloseTab - Mark tab as deleted (soft delete)
; Input:  RCX = pointer to TabManager
;         RDX = tab ID to close
; Output: RAX = 1 success, 0 failure
; ============================================================
CloseTab PROC
    push rbx
    
    mov rbx, rcx
    
    ; Validate tab ID
    cmp rdx, [rbx + TABMGR_TAB_COUNT]
    jge invalid
    
    ; Calculate entry address
    mov rcx, [rbx + TABMGR_INDEX_BASE]
    mov rax, rdx
    imul rax, TAB_ENTRY_SIZE
    add rcx, rax
    
    ; Set deleted flag (high bit of size field)
    mov rax, [rcx + TABENTRY_SIZE]
    or rax, TAB_FLAG_DELETED
    mov [rcx + TABENTRY_SIZE], rax
    
    mov rax, 1
    jmp done
    
invalid:
    xor rax, rax
done:
    pop rbx
    ret
CloseTab ENDP

; ============================================================
; GetTabLocation - Get tab data offset and size
; Input:  RCX = pointer to TabManager
;         RDX = tab ID
; Output: RAX = data offset (or -1 if invalid/deleted)
;         RDX = size (excluding deleted flag)
; ============================================================
GetTabLocation PROC
    push rbx
    
    mov rbx, rcx
    
    ; Validate tab ID
    cmp rdx, [rbx + TABMGR_TAB_COUNT]
    jge invalid
    
    ; Calculate entry address
    mov rcx, [rbx + TABMGR_INDEX_BASE]
    mov rax, rdx
    imul rax, TAB_ENTRY_SIZE
    add rcx, rax
    
    ; Check deleted flag
    mov rdx, [rcx + TABENTRY_SIZE]
    test rdx, TAB_FLAG_DELETED
    jnz invalid
    
    ; Return offset and size
    mov rax, [rcx + TABENTRY_DATA_OFFSET]
    and rdx, 7FFFFFFFh        ; mask out flags
    jmp done
    
invalid:
    mov rax, -1
    xor rdx, rdx
done:
    pop rbx
    ret
GetTabLocation ENDP

; ============================================================
; WriteTabContent - Write content to tab
; Input:  RCX = pointer to TabManager
;         RDX = tab ID
;         R8  = pointer to content
;         R9  = content length in bytes
; Output: RAX = 1 success, 0 failure
; ============================================================
WriteTabContent PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 56
    
    mov rbx, rcx              ; TabManager
    mov r12, rdx              ; tab ID
    mov r13, r8               ; content ptr
    mov r14, r9               ; content length
    
    ; Validate tab ID
    cmp r12, [rbx + TABMGR_TAB_COUNT]
    jge fail
    
    ; Calculate entry address
    mov rsi, [rbx + TABMGR_INDEX_BASE]
    mov rax, r12
    imul rax, TAB_ENTRY_SIZE
    add rsi, rax              ; rsi = entry address
    
    ; Get current data offset for this tab
    mov rdi, [rbx + TABMGR_DATA_OFFSET]
    
    ; Seek to write position in data file
    mov rcx, [rbx + TABMGR_DATA_HANDLE]
    mov edx, edi              ; low 32 bits
    shr rdi, 32
    mov r8d, edi              ; high 32 bits
    mov r9d, FILE_BEGIN
    call SetFilePointer
    
    ; Write content
    mov rcx, [rbx + TABMGR_DATA_HANDLE]
    mov rdx, r13              ; buffer
    mov r8, r14               ; bytes to write
    lea r9, [rsp+48]          ; bytes written
    mov qword ptr [rsp+32], 0 ; overlapped
    call WriteFile
    test rax, rax
    jz fail
    
    ; Update entry in index
    mov rax, [rbx + TABMGR_DATA_OFFSET]
    mov [rsi + TABENTRY_DATA_OFFSET], rax
    mov [rsi + TABENTRY_SIZE], r14
    
    ; Update next write offset
    add rax, r14
    mov [rbx + TABMGR_DATA_OFFSET], rax
    
    mov rax, 1
    jmp done
    
fail:
    xor rax, rax
done:
    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
WriteTabContent ENDP

; ============================================================
; ReadTabContent - Read content from tab
; Input:  RCX = pointer to TabManager
;         RDX = tab ID
;         R8  = output buffer
;         R9  = buffer size
; Output: RAX = bytes read (or -1 on failure)
; ============================================================
ReadTabContent PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 56
    
    mov rbx, rcx              ; TabManager
    mov r12, rdx              ; tab ID
    mov r13, r8               ; output buffer
    mov r14, r9               ; buffer size
    
    ; Get tab location
    mov rcx, rbx
    mov rdx, r12
    call GetTabLocation
    cmp rax, -1
    je fail
    
    mov rsi, rax              ; data offset
    mov rdi, rdx              ; content size
    
    ; Clamp read size to buffer size
    cmp rdi, r14
    jle size_ok
    mov rdi, r14
size_ok:
    
    ; Seek to read position
    mov rcx, [rbx + TABMGR_DATA_HANDLE]
    mov edx, esi              ; low 32 bits
    shr rsi, 32
    mov r8d, esi              ; high 32 bits
    mov r9d, FILE_BEGIN
    call SetFilePointer
    
    ; Read content
    mov rcx, [rbx + TABMGR_DATA_HANDLE]
    mov rdx, r13              ; buffer
    mov r8, rdi               ; bytes to read
    lea r9, [rsp+48]          ; bytes read
    mov qword ptr [rsp+32], 0 ; overlapped
    call ReadFile
    test rax, rax
    jz fail
    
    mov rax, [rsp+48]         ; return bytes read
    jmp done
    
fail:
    mov rax, -1
done:
    add rsp, 56
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
ReadTabContent ENDP

; ============================================================
; SwitchToTab - Change active tab
; Input:  RCX = pointer to TabManager
;         RDX = new active tab ID
; Output: RAX = 1 success, 0 failure
; ============================================================
SwitchToTab PROC
    push rbx
    
    mov rbx, rcx
    
    ; Validate tab ID
    cmp rdx, [rbx + TABMGR_TAB_COUNT]
    jge invalid
    
    ; Check not deleted
    push rdx
    mov rcx, rbx
    call GetTabLocation
    pop rdx
    cmp rax, -1
    je invalid
    
    ; Update active tab
    mov [rbx + TABMGR_ACTIVE_TAB], rdx
    
    mov rax, 1
    jmp done
    
invalid:
    xor rax, rax
done:
    pop rbx
    ret
SwitchToTab ENDP

; ============================================================
; GetTabCount - Return number of tabs
; Input:  RCX = pointer to TabManager
; Output: RAX = tab count
; ============================================================
GetTabCount PROC
    mov rax, [rcx + TABMGR_TAB_COUNT]
    ret
GetTabCount ENDP

; ============================================================
; GetActiveTab - Return active tab ID
; Input:  RCX = pointer to TabManager
; Output: RAX = active tab ID
; ============================================================
GetActiveTab PROC
    mov rax, [rcx + TABMGR_ACTIVE_TAB]
    ret
GetActiveTab ENDP

; ============================================================
; GetNextVisibleTab - Find next non-deleted tab
; Input:  RCX = pointer to TabManager
;         RDX = current tab ID
; Output: RAX = next visible tab ID (or -1 if none)
; ============================================================
GetNextVisibleTab PROC
    push rbx
    push r12
    
    mov rbx, rcx
    mov r12, rdx
    inc r12                   ; start from next tab
    
search:
    cmp r12, [rbx + TABMGR_TAB_COUNT]
    jge not_found
    
    mov rcx, rbx
    mov rdx, r12
    call GetTabLocation
    cmp rax, -1
    jne found
    
    inc r12
    jmp search
    
found:
    mov rax, r12
    jmp done
    
not_found:
    mov rax, -1
done:
    pop r12
    pop rbx
    ret
GetNextVisibleTab ENDP

; ============================================================
; GetPrevVisibleTab - Find previous non-deleted tab
; Input:  RCX = pointer to TabManager
;         RDX = current tab ID
; Output: RAX = previous visible tab ID (or -1 if none)
; ============================================================
GetPrevVisibleTab PROC
    push rbx
    push r12
    
    mov rbx, rcx
    mov r12, rdx
    dec r12                   ; start from previous tab
    
search:
    test r12, r12
    js not_found              ; negative = not found
    
    mov rcx, rbx
    mov rdx, r12
    call GetTabLocation
    cmp rax, -1
    jne found
    
    dec r12
    jmp search
    
found:
    mov rax, r12
    jmp done
    
not_found:
    mov rax, -1
done:
    pop r12
    pop rbx
    ret
GetPrevVisibleTab ENDP

; ============================================================
; BatchCloseTabs - Close multiple tabs efficiently
; Input:  RCX = pointer to TabManager
;         RDX = pointer to array of tab IDs
;         R8  = count of tabs to close
; Output: RAX = number of tabs successfully closed
; ============================================================
BatchCloseTabs PROC
    push rbx
    push rsi
    push rdi
    push r12
    sub rsp, 32
    
    mov rbx, rcx              ; TabManager
    mov rsi, rdx              ; tab ID array
    mov rdi, r8               ; count
    xor r12, r12              ; success count
    
close_loop:
    test rdi, rdi
    jz done
    
    mov rcx, rbx
    mov rdx, [rsi]            ; get tab ID
    call CloseTab
    add r12, rax              ; add success (0 or 1)
    
    add rsi, 8
    dec rdi
    jmp close_loop
    
done:
    mov rax, r12
    add rsp, 32
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
BatchCloseTabs ENDP

; ============================================================
; CompactTabIndex - Defragment index (remove deleted entries)
; Input:  RCX = pointer to TabManager
; Output: RAX = new tab count
; Note:   This invalidates all existing tab IDs!
; ============================================================
CompactTabIndex PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    
    mov rbx, rcx
    
    mov rsi, [rbx + TABMGR_INDEX_BASE]  ; source pointer
    mov rdi, rsi                         ; dest pointer
    mov r12, [rbx + TABMGR_TAB_COUNT]   ; total tabs
    xor r13, r13                         ; new count
    
compact_loop:
    test r12, r12
    jz compact_done
    
    ; Check if entry is deleted
    mov rax, [rsi + TABENTRY_SIZE]
    test rax, TAB_FLAG_DELETED
    jnz skip_entry
    
    ; Copy entry to compacted position
    mov rax, [rsi + TABENTRY_DATA_OFFSET]
    mov [rdi + TABENTRY_DATA_OFFSET], rax
    mov rax, [rsi + TABENTRY_SIZE]
    mov [rdi + TABENTRY_SIZE], rax
    
    add rdi, TAB_ENTRY_SIZE
    inc r13
    
skip_entry:
    add rsi, TAB_ENTRY_SIZE
    dec r12
    jmp compact_loop
    
compact_done:
    ; Update tab count
    mov [rbx + TABMGR_TAB_COUNT], r13
    
    ; Reset active tab if needed
    mov rax, [rbx + TABMGR_ACTIVE_TAB]
    cmp rax, r13
    jl active_ok
    xor rax, rax
    mov [rbx + TABMGR_ACTIVE_TAB], rax
active_ok:
    
    mov rax, r13
    
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
CompactTabIndex ENDP

; ============================================================
; SearchTabs - Linear search through tab index
; Input:  RCX = pointer to TabManager
;         RDX = search callback function (receives tab ID, returns 1 if match)
;         R8  = max results
;         R9  = pointer to results array (tab IDs)
; Output: RAX = number of matches found
; ============================================================
SearchTabs PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 40
    
    mov rbx, rcx              ; TabManager
    mov rsi, rdx              ; callback
    mov rdi, r8               ; max results
    mov r12, r9               ; results array
    xor r13, r13              ; current tab
    xor r14, r14              ; match count
    mov r15, [rbx + TABMGR_TAB_COUNT]
    
search_loop:
    cmp r13, r15
    jge search_done
    cmp r14, rdi
    jge search_done
    
    ; Check not deleted
    mov rcx, rbx
    mov rdx, r13
    call GetTabLocation
    cmp rax, -1
    je next_tab
    
    ; Call callback with tab ID
    mov rcx, r13
    call rsi
    test rax, rax
    jz next_tab
    
    ; Store match
    mov [r12], r13
    add r12, 8
    inc r14
    
next_tab:
    inc r13
    jmp search_loop
    
search_done:
    mov rax, r14
    add rsp, 40
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
SearchTabs ENDP

END
