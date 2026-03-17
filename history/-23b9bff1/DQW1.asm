; =====================================================================
; File Opener Component - Pure ASM
; Handles file dialogs, loading, saving, and file sharing
; =====================================================================

bits 64
default rel

section .data
    ; File dialog filters
    szFilter        db "All Files (*.*)", 0, "*.*", 0
                    db "ASM Files (*.asm)", 0, "*.asm", 0
                    db "Text Files (*.txt)", 0, "*.txt", 0
                    db 0
    
    szOpenTitle     db "Open File", 0
    szSaveTitle     db "Save File", 0
    
    ; Error messages
    szErrOpen       db "Failed to open file", 0
    szErrSave       db "Failed to save file", 0
    szErrRead       db "Failed to read file", 0
    
section .bss
    ; File handles
    hFile           resq 1
    fileSize        resq 1
    bytesRead       resd 1
    bytesWritten    resd 1
    
    ; File dialog structure
    ofn             resb 152        ; OPENFILENAME structure
    
    ; Shared file buffer (for transferring between front/backend)
    sharedFileBuffer resb 1048576   ; 1MB shared buffer
    sharedFileSize   resq 1
    sharedFileName   resb 260       ; Current shared file name

section .text

extern GetOpenFileNameA
extern GetSaveFileNameA
extern CreateFileA
extern ReadFile
extern WriteFile
extern CloseHandle
extern GetFileSize
extern MessageBoxA

; Import from dx_ide_main.asm
extern hWnd
extern editorBuffer
extern currentFilePath
extern cursorPos

global OpenFileDialog
global SaveFileDialog
global LoadFileToEditor
global SaveEditorToFile
global ShareFileToBackend
global GetSharedFile

; =====================================================================
; Open File Dialog
; Returns: RAX = 1 if file selected, 0 if cancelled
; Output: currentFilePath filled with selected file
; =====================================================================
OpenFileDialog:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    
    ; Zero out OPENFILENAME structure
    lea rdi, [ofn]
    xor eax, eax
    mov ecx, 152/8
    rep stosq
    
    ; Fill OPENFILENAME structure
    lea rbx, [ofn]
    mov dword [rbx], 88             ; lStructSize
    mov rax, [hWnd]
    mov [rbx + 8], rax              ; hwndOwner
    
    extern currentFilePath
    lea rax, [currentFilePath]
    mov [rbx + 16], rax             ; lpstrFile
    mov dword [rbx + 24], 260       ; nMaxFile
    
    lea rax, [szFilter]
    mov [rbx + 32], rax             ; lpstrFilter
    
    lea rax, [szOpenTitle]
    mov [rbx + 48], rax             ; lpstrTitle
    
    mov dword [rbx + 52], 0x00080000 ; Flags: OFN_FILEMUSTEXIST
    
    ; Show dialog
    lea rcx, [ofn]
    call GetOpenFileNameA
    
    pop rsi
    pop rbx
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Save File Dialog
; Returns: RAX = 1 if location selected, 0 if cancelled
; =====================================================================
SaveFileDialog:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    
    ; Zero out OPENFILENAME structure
    lea rdi, [ofn]
    xor eax, eax
    mov ecx, 152/8
    rep stosq
    
    ; Fill OPENFILENAME structure
    lea rbx, [ofn]
    mov dword [rbx], 88
    mov rax, [hWnd]
    mov [rbx + 8], rax
    
    extern currentFilePath
    lea rax, [currentFilePath]
    mov [rbx + 16], rax
    mov dword [rbx + 24], 260
    
    lea rax, [szFilter]
    mov [rbx + 32], rax
    
    lea rax, [szSaveTitle]
    mov [rbx + 48], rax
    
    mov dword [rbx + 52], 0x00000002 ; Flags: OFN_OVERWRITEPROMPT
    
    ; Show dialog
    lea rcx, [ofn]
    call GetSaveFileNameA
    
    pop rbx
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Load File To Editor
; Input: currentFilePath contains file to load
; Returns: RAX = 1 on success, 0 on failure
; =====================================================================
LoadFileToEditor:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    
    ; Open file
    extern currentFilePath
    lea rcx, [currentFilePath]
    mov edx, 0x80000000             ; GENERIC_READ
    mov r8d, 1                      ; FILE_SHARE_READ
    xor r9, r9                      ; No security attributes
    mov dword [rsp + 32], 3         ; OPEN_EXISTING
    mov dword [rsp + 40], 0x80      ; FILE_ATTRIBUTE_NORMAL
    mov qword [rsp + 48], 0         ; No template
    call CreateFileA
    
    cmp rax, -1
    je .error_open
    mov [hFile], rax
    
    ; Get file size
    mov rcx, [hFile]
    xor edx, edx
    call GetFileSize
    mov [fileSize], rax
    
    ; Check if file fits in buffer
    cmp rax, 65536
    jg .error_size
    
    ; Read file into editor buffer
    mov rcx, [hFile]
    extern editorBuffer
    lea rdx, [editorBuffer]
    mov r8d, [fileSize]
    lea r9, [bytesRead]
    mov qword [rsp + 32], 0         ; No overlapped
    call ReadFile
    
    test eax, eax
    jz .error_read
    
    ; Close file
    mov rcx, [hFile]
    call CloseHandle
    
    ; Update cursor position
    extern cursorPos
    mov dword [cursorPos], 0
    
    ; Success
    mov eax, 1
    jmp .exit
    
.error_open:
    xor eax, eax
    jmp .exit
    
.error_size:
    mov rcx, [hFile]
    call CloseHandle
    xor eax, eax
    jmp .exit
    
.error_read:
    mov rcx, [hFile]
    call CloseHandle
    xor eax, eax
    
.exit:
    pop rbx
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Save Editor To File
; Input: currentFilePath contains target file
; Returns: RAX = 1 on success, 0 on failure
; =====================================================================
SaveEditorToFile:
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    
    ; Calculate content length - count bytes until null terminator
    extern editorBuffer
    lea rsi, [editorBuffer]
    xor rbx, rbx                    ; RBX = byte counter
    
.find_end:
    cmp byte [rsi + rbx], 0
    je .found_end
    inc rbx
    cmp rbx, 65536
    jl .find_end
    
.found_end:
    ; RBX now contains content length
    
    ; Create/open file for writing
    extern currentFilePath
    lea rcx, [currentFilePath]
    mov edx, 0x40000000             ; GENERIC_WRITE
    xor r8, r8                      ; No sharing
    xor r9, r9
    mov dword [rsp + 32], 2         ; CREATE_ALWAYS
    mov dword [rsp + 40], 0x80      ; FILE_ATTRIBUTE_NORMAL
    mov qword [rsp + 48], 0
    call CreateFileA
    
    cmp rax, -1
    je .error
    mov [hFile], rax
    
    ; Write editor buffer to file
    mov rcx, [hFile]
    lea rdx, [editorBuffer]
    mov r8d, ebx                    ; Content length
    lea r9, [bytesWritten]
    mov qword [rsp + 32], 0
    call WriteFile
    
    ; Close file
    push rax                        ; Save result
    mov rcx, [hFile]
    call CloseHandle
    pop rax
    
    test eax, eax
    jz .error
    
    mov eax, 1
    jmp .exit
    
.error:
    xor eax, eax
    
.exit:
    pop rbx
    add rsp, 64
    pop rbp
    ret

; =====================================================================
; Share File To Backend
; Copies current editor content to shared buffer for agent access
; Returns: RAX = size of shared data
; =====================================================================
ShareFileToBackend:
    push rbp
    mov rbp, rsp
    push rsi
    push rdi
    
    ; Calculate editor content size
    extern editorBuffer
    lea rsi, [editorBuffer]
    xor ecx, ecx
    
.count_loop:
    cmp byte [rsi + rcx], 0
    je .count_done
    inc ecx
    cmp ecx, 65536
    jge .count_done
    jmp .count_loop
    
.count_done:
    ; Copy to shared buffer
    lea rdi, [sharedFileBuffer]
    lea rsi, [editorBuffer]
    rep movsb
    
    ; Store size
    mov [sharedFileSize], rcx
    
    ; Copy current file name
    extern currentFilePath
    lea rsi, [currentFilePath]
    lea rdi, [sharedFileName]
    mov ecx, 260
    rep movsb
    
    ; Return size
    mov rax, [sharedFileSize]
    
    pop rdi
    pop rsi
    pop rbp
    ret

; =====================================================================
; Get Shared File
; Retrieves file data from shared buffer (for backend/agent access)
; Output: RAX = pointer to buffer, RDX = size, R8 = filename pointer
; =====================================================================
GetSharedFile:
    push rbp
    mov rbp, rsp
    
    lea rax, [sharedFileBuffer]
    mov rdx, [sharedFileSize]
    lea r8, [sharedFileName]
    
    pop rbp
    ret

; =====================================================================
; File Menu Handlers
; =====================================================================

global HandleFileNew
global HandleFileOpen
global HandleFileSave
global HandleFileSaveAs

HandleFileNew:
    push rbp
    mov rbp, rsp
    
    ; Clear editor buffer
    extern editorBuffer
    lea rdi, [editorBuffer]
    xor eax, eax
    mov ecx, 65536/8
    rep stosq
    
    ; Clear file path
    extern currentFilePath
    lea rdi, [currentFilePath]
    mov ecx, 260/8
    rep stosq
    
    ; Reset cursor
    extern cursorPos
    mov dword [cursorPos], 0
    
    pop rbp
    ret

HandleFileOpen:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    call OpenFileDialog
    test eax, eax
    jz .exit
    
    call LoadFileToEditor
    
.exit:
    add rsp, 32
    pop rbp
    ret

HandleFileSave:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Check if we have a file path
    extern currentFilePath
    cmp byte [currentFilePath], 0
    je .save_as
    
    ; Save to current path
    call SaveEditorToFile
    jmp .exit
    
.save_as:
    call HandleFileSaveAs
    
.exit:
    add rsp, 32
    pop rbp
    ret

HandleFileSaveAs:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    call SaveFileDialog
    test eax, eax
    jz .exit
    
    call SaveEditorToFile
    
.exit:
    add rsp, 32
    pop rbp
    ret
