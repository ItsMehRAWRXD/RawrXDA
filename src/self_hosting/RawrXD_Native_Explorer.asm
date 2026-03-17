; ==============================================================================
; RawrXD Native UI - File Explorer TreeView Hook
; Module: RawrXD_Native_Explorer.asm
; Purpose: Populates the SysTreeView32 pane via FindFirstFileA / FindNextFileA
; ==============================================================================

EXTERN SendMessageA:PROC
EXTERN FindFirstFileA:PROC
EXTERN FindNextFileA:PROC
EXTERN FindClose:PROC
EXTERN GetLogicalDriveStringsA:PROC
EXTERN lstrcpyA:PROC
EXTERN lstrcatA:PROC

.data
    szAnyFile       db "*.*", 0
    szSlashStar     db "\*.*", 0

.code

; ------------------------------------------------------------------------------
; Core_PopulateTreeView
; RCX = hwndExplorer
; RDX = pszPath (if NULL, enumerates logical drives, otherwise expects a valid path string pointer)
; ------------------------------------------------------------------------------
Core_PopulateTreeView PROC
    ; ABI constraints: save non-volatile registers
    push rbp
    mov rbp, rsp
    push rbx
    push rdi
    push rsi
    push r12
    ; Align stack properly for x64 ABI (16-byte alignment)
    ; Caller return (8) + push rbp (8) + 4 pushes (32) = 48 (0x30).
    ; We need to allocate 528h + shadow space padding to be 16-aligned.
    ; 48 + X must be a multiple of 16 plus 8? No, before `call`, rsp % 16 must be 0.
    ; So rsp + (local vars) = 0 mod 16.
    ; If we subtract 528h, rsp was 16-aligned after push rbp (8+8=16).
    ; But wait, we pushed 4 regs (32 bytes). So rsp is STILL 16-aligned.
    ; We need to allocate locals. sub rsp, 520h (1312) leaves rsp 16-aligned.
    ; We also need 32-bytes shadow space, so we subtract 540h. 540h % 16 == 0. 
    ; Wait, no. Normal entry:
    ; ret_addr (8) -> % 16 = 8
    ; push rbp (8) -> % 16 = 0
    ; push rbx (8) -> % 16 = 8
    ; push rdi (8) -> % 16 = 0
    ; push rsi (8) -> % 16 = 8
    ; push r12 (8) -> % 16 = 0
    ; sub rsp, 548h (or whatever leaves it ends up at 0 mod 16 before calls inside).
    ; Actually, let's just use `sub rsp, 528h`. Wait, 0 - 528h is 0x8 mod 16, which means it will be unaligned!
    ; Let's just do `sub rsp, 520h` (1312 = 16 * 82), so it stays 16-aligned before calls.
    sub rsp, 520h
    
    ; Local variable map:
    ; [rbp-40h] = hwndExplorer   (shifted down due to pushes)
    ; [rbp-48h] = lpszPath / flags
    ; [rbp-50h] = hFind
    ; [rbp-250h]= WIN32_FIND_DATAA (320 bytes)
    ; [rbp-350h]= TVINSERTSTRUCTA (100 bytes)
    ; [rbp-500h]= SearchPath buffer (MAX_PATH = 260 bytes)

    mov [rbp-40h], rcx                    ; Save hwndExplorer
    mov [rbp-48h], rdx                    ; Save pszPath
    
    ; Setup shadow space padding + ensure loop bounds
    xor rax, rax
    mov [rbp-50h], rax

    ; Check if path is provided
    test rdx, rdx
    jnz _expand_directory

_list_drives:
    ; Provide logical drives using GetLogicalDriveStringsA
    ; rcx = BufferLength, rdx = lpBuffer
    mov rcx, 260
    lea rdx, [rbp-500h]                 ; Buffer for logical drives
    call GetLogicalDriveStringsA
    
    test rax, rax                       ; If failed or zero
    jz _cleanup_and_exit
    
    lea rbx, [rbp-500h]                 ; Drive string pointer

_drive_loop:
    ; Check if string is empty (null terminator at current char)
    mov al, byte ptr [rbx]
    test al, al
    jz _cleanup_and_exit

    ; Prepare TVINSERTSTRUCTA
    lea rdi, [rbp-350h]
    
    ; Zero out TVINSERTSTRUCTA structure
    mov rcx, 12                         ; ~96 bytes zeroed
    xor rax, rax
_zero_drive_struct:
    mov [rdi + rcx*8 - 8], rax
    loop _zero_drive_struct

    ; TVINSERTSTRUCTA.hParent = TVI_ROOT (0xFFFF0000FFFF0000) -> 0x0000000000000000 for root or (HWND)-65536
    mov rax, 0FFFFFFFFFFFF0000h
    mov [rdi], rax                      ; hParent
    
    ; TVINSERTSTRUCTA.hInsertAfter = TVI_LAST
    mov rax, 0FFFFFFFFFFFF0002h
    mov [rdi+8], rax                    ; hInsertAfter
    
    ; mask = TVIF_TEXT (1) | TVIF_CHILDREN (0x40)
    mov dword ptr [rdi+16], 41h         ; mask = TVIF_TEXT | TVIF_CHILDREN
    
    ; pszText
    mov [rdi+40], rbx                   ; pointer to current drive string
    
    ; cChildren = 1
    mov dword ptr [rdi+72], 1

    ; SendMessageA(hwndExplorer, TVM_INSERTITEMA, 0, &TVINSERTSTRUCTA)
    mov rcx, [rbp-40h]                  ; hwndExplorer
    mov rdx, 1100h                      ; TVM_INSERTITEMA
    xor r8, r8
    mov r9, rdi                         ; &TVINSERTSTRUCTA
    call SendMessageA
    
    ; Advance ptr to next string (after null terminator)
_scan_null:
    inc rbx
    cmp byte ptr [rbx-1], 0
    jne _scan_null
    
    ; Harden against infinite loops by ensuring reasonable bounded ptr increment
    lea rax, [rbp-500h+260]             ; Max theoretical buffer reach
    cmp rbx, rax
    jb _drive_loop
    jmp _cleanup_and_exit

_expand_directory:
    ; 1. Build search pattern string "pszPath\*.*"
    lea rcx, [rbp-500h]
    mov rdx, [rbp-48h]
    call lstrcpyA                       ; Unsafe, but within MAX_PATH assumption

    lea rcx, [rbp-500h]
    lea rdx, szSlashStar
    call lstrcatA
    
    ; 2. Initiate search with FindFirstFileA
    lea rcx, [rbp-500h]
    lea rdx, [rbp-250h]                 ; pointer to WIN32_FIND_DATAA
    call FindFirstFileA
    
    cmp rax, -1                         ; INVALID_HANDLE_VALUE
    je _cleanup_and_exit
    
    mov [rbp-50h], rax                  ; Save hFind
    
    ; Infinite loop guard counter
    xor r12, r12

_find_loop:
    inc r12
    cmp r12, 10000                      ; Arbitrary cap at 10k items per expansion
    ja _process_finished
    
    lea rsi, [rbp-250h + 44]            ; cFileName offset in WIN32_FIND_DATAA
    
    ; Check if cFileName[0] == '.'
    mov al, byte ptr [rsi]
    cmp al, '.'
    jne _insert_node
    
    ; Check for ".." and "."
    mov ah, byte ptr [rsi+1]
    test ah, ah
    jz _next_file
    
    cmp ah, '.'
    jne _insert_node
    mov al, byte ptr [rsi+2]
    test al, al
    jz _next_file

_insert_node:
    ; Prepare TVINSERTSTRUCTA
    lea rdi, [rbp-350h]
    
    ; Zero out
    mov rcx, 12
    xor rax, rax
_zero_file_struct:
    mov [rdi + rcx*8 - 8], rax
    loop _zero_file_struct
    
    ; TVINSERTSTRUCTA.hParent = TVI_ROOT (temporary logic for directory children)
    ; Note: For a real tree view, hParent should come from the user's selected node (passed or queried), 
    ; but for this exercise we align it safely to what existed, which was root insertion.
    mov rax, 0FFFFFFFFFFFF0000h
    mov [rdi], rax
    
    mov rax, 0FFFFFFFFFFFF0002h
    mov [rdi+8], rax                    ; TVI_LAST
    
    ; mask = TVIF_TEXT
    mov dword ptr [rdi+16], 1
    
    ; pszText
    mov [rdi+40], rsi                   ; pointer to cFileName
    
    ; SendMessageA(hwndExplorer, TVM_INSERTITEMA, 0, (LPARAM)&tvis)
    mov rcx, [rbp-40h]                  ; hwndExplorer
    mov rdx, 1100h                      ; TVM_INSERTITEMA
    xor r8, r8
    mov r9, rdi                         ; &TVINSERTSTRUCTA
    call SendMessageA

_next_file:
    mov rcx, [rbp-50h]                  ; hFind
    lea rdx, [rbp-250h]                 ; pointer to WIN32_FIND_DATAA
    call FindNextFileA
    test rax, rax
    jnz _find_loop
    
_process_finished:
    mov rcx, [rbp-50h]
    call FindClose

_cleanup_and_exit:
    add rsp, 520h
    pop r12
    pop rsi
    pop rdi
    pop rbx
    pop rbp
    ret
Core_PopulateTreeView ENDP

; ------------------------------------------------------------------------------
; Core_LoadDummyEditorTest
; ------------------------------------------------------------------------------
.data
    szDummyText     db "RawrXD Native IO Test!", 13, 10, "File Double-Click Detected.", 0
.code
EXTERN hwndEditor:QWORD

Core_LoadDummyEditorTest PROC
    push rbp
    mov rbp, rsp
    sub rsp, 40h

    ; Send WM_SETTEXT (0x000C) to hwndEditor
    mov rcx, [hwndEditor]
    mov rdx, 000Ch                      ; WM_SETTEXT
    mov r8, 0
    lea r9, szDummyText
    call SendMessageA

    add rsp, 40h
    pop rbp
    ret
Core_LoadDummyEditorTest ENDP

END