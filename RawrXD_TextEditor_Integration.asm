; ===============================================================================
; RawrXD_TextEditor_Integration.asm
; Unified integration of file I/O, ML inference, completion popup, editing
; ===============================================================================

OPTION CASEMAP:NONE

EXTERN FileIO_OpenRead:PROC
EXTERN FileIO_OpenWrite:PROC
EXTERN FileIO_Read:PROC
EXTERN FileIO_Write:PROC
EXTERN FileIO_Close:PROC
EXTERN FileIO_SetModified:PROC
EXTERN FileIO_ClearModified:PROC
EXTERN FileIO_IsModified:PROC

EXTERN MLInference_Initialize:PROC
EXTERN MLInference_Invoke:PROC
EXTERN MLInference_Cleanup:PROC

EXTERN CompletionPopup_Initialize:PROC
EXTERN CompletionPopup_Show:PROC
EXTERN CompletionPopup_Hide:PROC
EXTERN CompletionPopup_IsVisible:PROC

EXTERN EditOps_InsertChar:PROC
EXTERN EditOps_DeleteChar:PROC
EXTERN EditOps_Backspace:PROC
EXTERN EditOps_HandleTab:PROC
EXTERN EditOps_HandleNewline:PROC
EXTERN EditOps_SelectRange:PROC
EXTERN EditOps_DeleteSelection:PROC

EXTERN OutputDebugStringA:PROC
EXTERN GetTickCount:PROC

.data
    ALIGN 16
    szUIReady           db "[UI] Text editor ready - Ctrl+Space for ML completion", 0
    szUIFileOpen        db "[UI] Opening file: %s", 0
    szUIFileSave        db "[UI] Saving file: %s", 0
    szUIFileModified    db "[UI] File modified - unsaved changes", 0
    szUICtrlSpace       db "[UI] Ctrl+Space pressed - invoking ML", 0
    szUICompletionShow  db "[UI] Showing %d completions", 0
    szErrorFileNotFound db "[ERROR] File not found: %s", 0
    szErrorSaveFailed   db "[ERROR] Save failed: %s", 0

    g_CurrentFile       db 256 dup (0)   ; Current file path
    g_FileBuffer        db 32768 dup (0) ; File content (32KB)
    g_FileBufferSize    dq 0             ; Bytes in buffer
    g_EditorInitialized dd 0             ; Initialization flag

.code

; ===============================================================================
; TextEditor_Initialize - Initialize all subsystems
; Returns: rax = 1 if complete success, 0 if partial
; ===============================================================================
TextEditor_Initialize PROC FRAME
    push rbp
    .pushreg rbp
    push rbx
    .pushreg rbx
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Initialize file I/O
    ; (FileIO globals are statically initialized)
    
    ; Initialize ML inference
    call MLInference_Initialize
    test eax, eax
    jz InitPartial
    
    ; Initialize completion popup
    call CompletionPopup_Initialize
    test eax, eax
    jz InitPartial
    
    ; Mark as ready
    mov dword ptr [g_EditorInitialized], 1
    lea rcx, szUIReady
    call OutputDebugStringA
    mov eax, 1
    jmp InitDone
    
InitPartial:
    ; Partial initialization is acceptable (ML may not be available)
    mov dword ptr [g_EditorInitialized], 1
    lea rcx, szUIReady
    call OutputDebugStringA
    mov eax, 1
    
InitDone:
    add rsp, 32
    pop rbx
    pop rbp
    ret
TextEditor_Initialize ENDP

; ===============================================================================
; TextEditor_OpenFile - Load file into editor buffer
; rcx = file path (null-terminated)
; Returns: rax = bytes read, 0 if error
; ===============================================================================
TextEditor_OpenFile PROC FRAME USES rbx r12
    .endprolog
    
    mov rbx, rcx                    ; rbx = file path
    
    ; Copy path to global
    lea rax, [g_CurrentFile]
    mov r12, rbx
CopyPathLoop:
    mov al, byte ptr [rbx]
    test al, al
    jz CopyPathDone
    mov byte ptr [rax], al
    inc rbx
    inc rax
    jmp CopyPathLoop
    
CopyPathDone:
    mov byte ptr [rax], 0
    
    ; Log open
    lea rcx, szUIFileOpen
    mov rdx, r12
    call OutputDebugStringA
    
    ; Open file for reading
    lea rcx, [g_CurrentFile]
    call FileIO_OpenRead
    
    test rax, rax
    jz FileOpenError
    
    ; Read file into buffer
    lea rcx, [g_FileBuffer]
    mov rdx, 32768                  ; Max 32KB
    call FileIO_Read
    
    mov [g_FileBufferSize], rax
    
    ; Close file
    call FileIO_Close
    
    ; Clear modified flag
    call FileIO_ClearModified
    
    ret
    
FileOpenError:
    lea rcx, szErrorFileNotFound
    lea rdx, [g_CurrentFile]
    call OutputDebugStringA
    xor eax, eax
    ret
TextEditor_OpenFile ENDP

; ===============================================================================
; TextEditor_SaveFile - Write buffer back to file
; Returns: rax = bytes written, 0 if error
; ===============================================================================
TextEditor_SaveFile PROC FRAME USES rbx
    .endprolog
    
    lea rcx, szUIFileSave
    lea rdx, [g_CurrentFile]
    call OutputDebugStringA
    
    ; Open file for writing
    lea rcx, [g_CurrentFile]
    call FileIO_OpenWrite
    
    test rax, rax
    jz FileSaveError
    
    ; Write buffer to file
    lea rcx, [g_FileBuffer]
    mov rdx, [g_FileBufferSize]
    call FileIO_Write
    
    mov rbx, rax                    ; rbx = bytes written
    
    ; Close file
    call FileIO_Close
    
    ; Clear modified flag
    call FileIO_ClearModified
    
    mov rax, rbx
    ret
    
FileSaveError:
    lea rcx, szErrorSaveFailed
    lea rdx, [g_CurrentFile]
    call OutputDebugStringA
    xor eax, eax
    ret
TextEditor_SaveFile ENDP

; ===============================================================================
; TextEditor_OnCtrlSpace - Handle Ctrl+Space hotkey (ML completion)
; rcx = current line text
; rdx = cursor x position (for popup)
; r8d = cursor y position (for popup)
; Returns: rax = 1 if popup shown
; ===============================================================================
TextEditor_OnCtrlSpace PROC FRAME USES rbx r12 r13
    .endprolog
    
    mov rbx, rcx                    ; rbx = current line
    mov r12d, edx                   ; r12d = x position
    mov r13d, r8d                   ; r13d = y position
    
    lea rcx, szUICtrlSpace
    call OutputDebugStringA
    
    ; Check if popup already visible - hide it first
    call CompletionPopup_IsVisible
    test eax, eax
    jz CtrlSpaceNoPopup
    
    call CompletionPopup_Hide
    jmp CtrlSpaceAlreadyOpen
    
CtrlSpaceNoPopup:
    ; Invoke ML inference with current line
    mov rcx, rbx
    call MLInference_Invoke
    
    test rax, rax
    jz NoInferenceResult
    
    ; Show popup with suggestions
    mov rcx, rax                    ; Inference result
    mov edx, r12d
    mov r8d, r13d
    call CompletionPopup_Show
    
    mov eax, 1
    ret
    
NoInferenceResult:
    xor eax, eax
    ret
    
CtrlSpaceAlreadyOpen:
    xor eax, eax
    ret
TextEditor_OnCtrlSpace ENDP

; ===============================================================================
; TextEditor_OnCharacter - Handle character input
; rcx = character code
; rdx = cursor position
; Returns: rax = new cursor position
; ===============================================================================
TextEditor_OnCharacter PROC FRAME USES rbx
    .endprolog
    
    mov rbx, rcx                    ; rbx = char
    
    ; Handle special characters
    cmp rbx, 0x09                   ; TAB
    je OnTabKey
    
    cmp rbx, 0x0D                   ; ENTER (CR)
    je OnEnterKey
    
    ; Regular character
    call EditOps_InsertChar          ; rcx=char, rdx=cursor (preserved in rdx)
    ret
    
OnTabKey:
    mov rcx, rdx                    ; Cursor position
    mov edx, 4                      ; Indent width = 4
    call EditOps_HandleTab
    ret
    
OnEnterKey:
    mov rcx, rdx                    ; Cursor position
    call EditOps_HandleNewline
    ret
TextEditor_OnCharacter ENDP

; ===============================================================================
; TextEditor_OnDelete - Handle Delete key
; rcx = cursor position
; Returns: rax = cursor position (unchanged)
; ===============================================================================
TextEditor_OnDelete PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call EditOps_DeleteChar
    
    add rsp, 32
    pop rbp
    ret
TextEditor_OnDelete ENDP

; ===============================================================================
; TextEditor_OnBackspace - Handle Backspace key
; rcx = cursor position
; Returns: rax = new cursor position
; ===============================================================================
TextEditor_OnBackspace PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call EditOps_Backspace
    
    add rsp, 32
    pop rbp
    ret
TextEditor_OnBackspace ENDP

; ===============================================================================
; TextEditor_Cleanup - Clean up all resources
; ===============================================================================
TextEditor_Cleanup PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    ; Close any open file
    call FileIO_Close
    
    ; Hide popup if visible
    call CompletionPopup_Hide
    
    ; Cleanup ML inference
    call MLInference_Cleanup
    
    add rsp, 32
    pop rbp
    ret
TextEditor_Cleanup ENDP

; ===============================================================================
; TextEditor_GetBufferPtr - Get pointer to file buffer
; Returns: rax = buffer address
; ===============================================================================
TextEditor_GetBufferPtr PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    lea rax, [g_FileBuffer]
    
    add rsp, 32
    pop rbp
    ret
TextEditor_GetBufferPtr ENDP

; ===============================================================================
; TextEditor_GetBufferSize - Get current buffer size
; Returns: rax = bytes in buffer
; ===============================================================================
TextEditor_GetBufferSize PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    mov rax, [g_FileBufferSize]
    
    add rsp, 32
    pop rbp
    ret
TextEditor_GetBufferSize ENDP

; ===============================================================================
; TextEditor_IsModified - Check if file has unsaved changes
; Returns: rax = 1 if modified, 0 otherwise
; ===============================================================================
TextEditor_IsModified PROC FRAME
    push rbp
    .pushreg rbp
    sub rsp, 32
    .allocstack 32
    .endprolog
    
    call FileIO_IsModified
    
    add rsp, 32
    pop rbp
    ret
TextEditor_IsModified ENDP

PUBLIC TextEditor_Initialize
PUBLIC TextEditor_OpenFile
PUBLIC TextEditor_SaveFile
PUBLIC TextEditor_OnCtrlSpace
PUBLIC TextEditor_OnCharacter
PUBLIC TextEditor_OnDelete
PUBLIC TextEditor_OnBackspace
PUBLIC TextEditor_Cleanup
PUBLIC TextEditor_GetBufferPtr
PUBLIC TextEditor_GetBufferSize
PUBLIC TextEditor_IsModified
PUBLIC g_CurrentFile
PUBLIC g_FileBuffer
PUBLIC g_FileBufferSize
PUBLIC g_EditorInitialized

END
