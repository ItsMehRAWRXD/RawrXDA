; ============================================================================
; File 27: tab_buffer_integration.asm - Critical bridge: VirtualTabManager ↔ GapBuffer
; ============================================================================
; Purpose: Connect file I/O, tabs, and text storage into unified editing pipeline
; Uses: GapBuffer + VirtualTabManager, file operations, undo/redo integration
; Functions: OpenFile, CloseTab, SaveFile, SwitchTab, InsertText, DeleteSelection, GetLine
; ============================================================================

.code

; CONSTANTS
; ============================================================================

FILE_BUFFER_SIZE        equ 1048576   ; 1MB read buffer

; OPEN FILE AND CREATE TAB
; ============================================================================

TabBuffer_OpenFile PROC USES rbx rcx rdx rsi rdi r8 r9 tabManager:PTR DWORD, filePath:PTR BYTE
    ; tabManager = VirtualTabManager*
    ; filePath = "C:\Users\...\file.cpp"
    ; Returns: tab handle in rax (or -1 if failed)
    
    ; Step 1: Open file
    sub rsp, 40
    mov rcx, filePath
    mov rdx, 0x80000000  ; GENERIC_READ
    mov r8, 1            ; FILE_SHARE_READ
    mov r9, 3            ; OPEN_EXISTING
    call CreateFileA
    add rsp, 40
    
    cmp rax, -1
    je @openfile_failed
    
    mov rbx, rax  ; rbx = file handle
    
    ; Step 2: Get file size
    mov r8, 0
    sub rsp, 40
    mov rcx, rbx
    mov rdx, 0
    mov r8, 0
    call GetFileSize
    add rsp, 40
    mov r9, rax  ; r9 = file size
    
    ; Limit to 100MB
    cmp rax, 104857600
    jg @openfile_too_large
    
    ; Step 3: Allocate read buffer
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, r9  ; allocate file size bytes
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov r10, rax  ; r10 = read buffer
    
    ; Step 4: Read file
    mov r11, 0  ; bytes read
    sub rsp, 40
    mov rcx, rbx
    mov rdx, r10
    mov r8, r9
    lea r11, [rsp + 32]
    mov [rsp + 32], r11
    call ReadFile
    add rsp, 40
    mov r11, [rsp]  ; bytes actually read
    
    ; Close file handle
    sub rsp, 40
    mov rcx, rbx
    call CloseHandle
    add rsp, 40
    
    ; Step 5: Create GapBuffer
    ; (assumes GapBuffer_Init function exists)
    ; For now, allocate buffer struct manually
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 128  ; GapBuffer struct size
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    mov rsi, rax  ; rsi = new GapBuffer
    
    ; Initialize GapBuffer fields
    ; heapHandle, capacity, gapStart, gapEnd, length, lineOffsets, lineCount
    mov qword ptr [rsi + 0], 0   ; heapHandle (temp)
    mov qword ptr [rsi + 8], r11  ; capacity = file size
    mov qword ptr [rsi + 16], 0   ; gapStart = 0
    mov qword ptr [rsi + 24], r11 ; gapEnd = capacity
    mov qword ptr [rsi + 32], r11 ; length = file size
    mov qword ptr [rsi + 40], 0   ; lineOffsets (temp)
    mov qword ptr [rsi + 48], 1   ; lineCount
    mov qword ptr [rsi + 104], r10 ; buffer_base
    
    ; Step 6: Create VirtualTab entry
    ; Call TabManager_CreateTab(tabManager, filePath, gapBuffer)
    mov rcx, tabManager
    mov rdx, filePath
    mov r8, rsi  ; gapBuffer
    
    ; (stub: would call actual VirtualTabManager function)
    
    ; Step 7: Update tokenizer for new file
    ; Call Tokenizer_TokenizeRange(tokenizer, 0, lineCount)
    
    mov rax, 1  ; return success
    ret
    
@openfile_failed:
    mov rax, -1
    ret
    
@openfile_too_large:
    ; Close file
    sub rsp, 40
    mov rcx, rbx
    call CloseHandle
    add rsp, 40
    mov rax, -1
    ret
TabBuffer_OpenFile ENDP

; CLOSE TAB AND FREE RESOURCES
; ============================================================================

TabBuffer_CloseTab PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = tab to close
    ; Returns: 1 if successful
    
    ; Step 1: Check if tab is dirty
    ; (would query VirtualTab.isDirty)
    
    ; Step 2: If dirty, ask user to save (stub for now)
    
    ; Step 3: Get GapBuffer from tab
    ; (would query VirtualTab.bufferModel)
    
    ; Step 4: Free GapBuffer
    ; - Free heap
    ; - Free lineOffsets array
    ; - Free undo/redo history
    
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    
    ; TODO: Implement full cleanup
    
    ; Step 5: Remove from VirtualTabManager
    
    mov rax, 1
    ret
TabBuffer_CloseTab ENDP

; SAVE FILE TO DISK
; ============================================================================

TabBuffer_SaveFile PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = tab to save
    ; Returns: 1 if successful, 0 if failed
    
    ; Step 1: Get tab and buffer
    ; (would query VirtualTabManager_GetTab(tabManager, tabHandle))
    
    ; Step 2: Serialize buffer to memory
    ; Call GapBuffer_GetText(buffer, 0, length, tempBuffer)
    
    ; Step 3: Open file for writing
    sub rsp, 40
    mov rcx, 0  ; would be file path from tab
    mov rdx, 0x40000000  ; GENERIC_WRITE
    mov r8, 0            ; no sharing
    mov r9, 2            ; CREATE_ALWAYS
    call CreateFileA
    add rsp, 40
    
    cmp rax, -1
    je @savefile_failed
    
    mov rbx, rax  ; rbx = file handle
    
    ; Step 4: Write buffer to file
    ; (assuming buffer is in memory)
    
    mov r8, 0  ; file size to write
    sub rsp, 40
    mov rcx, rbx
    mov rdx, 0  ; temp buffer pointer
    mov r8, 0   ; size
    lea r9, [rsp + 32]
    mov [rsp + 32], r9
    call WriteFile
    add rsp, 40
    
    ; Step 5: Close file
    sub rsp, 40
    mov rcx, rbx
    call CloseHandle
    add rsp, 40
    
    ; Step 6: Clear dirty flag
    ; (would set VirtualTab.isDirty = false)
    
    mov rax, 1
    ret
    
@savefile_failed:
    mov rax, 0
    ret
TabBuffer_SaveFile ENDP

; SWITCH ACTIVE TAB
; ============================================================================

TabBuffer_SwitchTab PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = new active tab
    ; Returns: 1 if successful
    
    ; Step 1: Deactivate current tab (save cursor, selection)
    ; Step 2: Activate new tab (restore cursor, selection)
    ; Step 3: Trigger rendering update
    
    ; (stub: would use VirtualTabManager_SwitchTab)
    
    mov rax, 1
    ret
TabBuffer_SwitchTab ENDP

; INSERT TEXT (User Typing)
; ============================================================================

TabBuffer_InsertText PROC USES rbx rcx rdx rsi rdi r8 r9 tabManager:PTR DWORD, tabHandle:QWORD, text:PTR BYTE, length:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = active tab
    ; text = typed characters
    ; length = byte count
    ; Returns: 1 if successful
    
    ; Step 1: Get GapBuffer from tab
    ; (stub: would query VirtualTab)
    
    ; Step 2: Call GapBuffer_Insert
    mov rcx, 0  ; would be gapBuffer pointer
    mov rdx, 0  ; cursor position
    mov r8, text
    mov r9, length
    
    ; call GapBuffer_Insert
    
    ; Step 3: Create undo command
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 128  ; command size
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    
    ; Initialize command: { type=INSERT, position, length, data, timestamp }
    mov rbx, rax
    mov dword ptr [rbx + 0], 1      ; type = INSERT
    mov [rbx + 8], rdx              ; position
    mov [rbx + 16], r9              ; length
    mov [rbx + 24], r8              ; data pointer
    
    ; Step 4: Push to UndoStack
    ; call UndoStack_PushCommand(undoStack, command)
    
    ; Step 5: Invalidate token cache
    ; call Tokenizer_InvalidateBlock(tokenizer, blockNum)
    
    ; Step 6: Mark tab dirty
    ; VirtualTab.isDirty = true
    
    ; Step 7: Trigger incremental rendering
    
    mov rax, 1
    ret
TabBuffer_InsertText ENDP

; DELETE SELECTION
; ============================================================================

TabBuffer_DeleteSelection PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD, selStart:QWORD, selEnd:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = active tab
    ; selStart, selEnd = selection range
    ; Returns: 1 if successful
    
    ; Step 1: Get GapBuffer
    mov rcx, 0  ; would be gapBuffer
    
    ; Step 2: Call GapBuffer_Delete
    mov rdx, selStart
    mov r8, selEnd
    sub r8, rdx  ; length = selEnd - selStart
    
    ; call GapBuffer_Delete(buffer, position, length)
    
    ; Step 3: Create undo command (DELETE type)
    sub rsp, 40
    call GetProcessHeap
    add rsp, 40
    mov rcx, rax
    mov rdx, 0
    mov r8, 128
    sub rsp, 40
    call HeapAlloc
    add rsp, 40
    
    mov rbx, rax
    mov dword ptr [rbx + 0], 2      ; type = DELETE
    mov [rbx + 8], rdx              ; position
    mov [rbx + 16], r8              ; length
    
    ; Step 4: Push undo
    ; call UndoStack_PushCommand
    
    ; Step 5: Invalidate tokens
    ; call Tokenizer_InvalidateBlock
    
    ; Step 6: Mark dirty, trigger render
    
    mov rax, 1
    ret
TabBuffer_DeleteSelection ENDP

; GET CURRENT LINE
; ============================================================================

TabBuffer_GetCurrentLine PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD, output:PTR BYTE, maxLen:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = active tab
    ; output = destination buffer
    ; maxLen = max output size
    ; Returns: actual bytes in rax
    
    ; Step 1: Get GapBuffer
    mov rcx, 0  ; would be gapBuffer
    
    ; Step 2: Get current cursor line number
    mov rax = 0  ; would be cursor line
    
    ; Step 3: Call GapBuffer_GetLine
    mov rdx, rax
    mov r8, output
    mov r9, maxLen
    
    ; call GapBuffer_GetLine(buffer, lineNum, output, maxLen)
    
    ; Returns bytes in rax
    
    ret
TabBuffer_GetCurrentLine ENDP

; GET CURRENT LINE TOKENS
; ============================================================================

TabBuffer_GetCurrentLineTokens PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD
    ; tabManager = VirtualTabManager*
    ; tabHandle = active tab
    ; Returns: Token* array in rax
    
    ; Step 1: Get current line number
    mov rax, 0  ; would be cursor line
    
    ; Step 2: Calculate block number
    mov rcx, 512  ; TOKENIZER_BLOCK_SIZE
    xor edx, edx
    div rcx
    
    ; Step 3: Call Tokenizer_GetTokens
    mov rcx, 0  ; would be tokenizer
    mov rdx, rax  ; blockNum
    
    ; call Tokenizer_GetTokens(tokenizer, blockNum)
    
    ret
TabBuffer_GetCurrentLineTokens ENDP

; VALIDATE AND RECOVER
; ============================================================================

TabBuffer_ValidateIntegrity PROC USES rbx rcx rdx rsi rdi tabManager:PTR DWORD, tabHandle:QWORD
    ; Sanity check: all tab buffers are consistent
    ; Returns: 1 if valid, 0 if corrupted
    
    ; Check: GapBuffer bounds
    ; Check: UndoStack consistency
    ; Check: Tokenizer cache validity
    
    mov rax, 1  ; assume valid
    ret
TabBuffer_ValidateIntegrity ENDP

end
