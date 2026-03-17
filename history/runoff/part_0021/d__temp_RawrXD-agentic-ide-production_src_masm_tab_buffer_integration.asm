;==============================================================================
; File 27: tab_buffer_integration.asm - Connect Tabs to Text Storage
;==============================================================================
; Critical integration: VirtualTabManager ↔ GapBuffer
; Allows 1000 tabs with efficient memory management
;==============================================================================

include windows.inc

.code

;==============================================================================
; Open File in New Tab
;==============================================================================
TabBuffer_OpenFile PROC lpFilePath:QWORD
    LOCAL fileSize:QWORD
    LOCAL hFile:HANDLE
    LOCAL hMapping:HANDLE
    LOCAL lpFileData:QWORD
    LOCAL gbState:QWORD
    LOCAL tab:QWORD
    LOCAL bytesRead:DWORD
    
    ; Open file
    invoke CreateFileA, lpFilePath, 0x80000000, 1, NULL, 3, 0, NULL
    mov hFile, rax
    .if rax == INVALID_HANDLE_VALUE
        LOG_ERROR "Failed to open file: %s", lpFilePath
        mov rax, NULL
        ret
    .endif
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov fileSize, rax
    
    .if fileSize == 0
        LOG_ERROR "File is empty: %s", lpFilePath
        invoke CloseHandle, hFile
        mov rax, NULL
        ret
    .endif
    
    .if fileSize > 268435456  ; 256MB limit
        LOG_ERROR "File too large: %lld bytes", fileSize
        invoke CloseHandle, hFile
        mov rax, NULL
        ret
    .endif
    
    ; Allocate GapBuffer
    call GapBuffer_Init, fileSize + 1048576  ; Add 1MB headroom
    mov gbState, rax
    .if rax == NULL
        invoke CloseHandle, hFile
        ret
    .endif
    
    ; Read file into buffer
    invoke HeapAlloc, [gbHeap], 0, fileSize
    mov lpFileData, rax
    
    invoke ReadFile, hFile, lpFileData, fileSize, 
        ADDR bytesRead, NULL
    
    .if eax == FALSE || bytesRead != fileSize
        LOG_ERROR "Failed to read file completely"
        invoke CloseHandle, hFile
        mov rax, NULL
        ret
    .endif
    
    invoke CloseHandle, hFile
    
    ; Insert file content into GapBuffer
    call GapBuffer_Insert, gbState, 0, lpFileData, fileSize
    
    ; Create tab entry
    invoke HeapAlloc, [tabHeap], 0, SIZEOF VirtualTab
    mov tab, rax
    
    mov [tab].VirtualTab.filePath, lpFilePath
    mov [tab].VirtualTab.bufferModel, gbState
    mov [tab].VirtualTab.cursorPos, 0
    mov [tab].VirtualTab.selectionStart, 0
    mov [tab].VirtualTab.selectionEnd, 0
    mov [tab].VirtualTab.isDirty, 0
    mov [tab].VirtualTab.isActive, 1
    
    ; Add to tab manager
    call TabManager_AddTab, tab
    
    ; Tokenize buffer
    call Tokenizer_Init
    call Tokenizer_TokenizeRange, gbState, 0, 1000
    
    LOG_INFO "Opened file in tab: %s (size=%lld)", lpFilePath, fileSize
    
    mov rax, tab
    ret
TabBuffer_OpenFile ENDP

;==============================================================================
; Close Tab and Save if Dirty
;==============================================================================
TabBuffer_CloseTab PROC tab:QWORD
    LOCAL gbState:QWORD
    
    mov gbState, [tab].VirtualTab.bufferModel
    
    ; Check if dirty
    .if [tab].VirtualTab.isDirty
        call TabBuffer_SaveFile, tab
    .endif
    
    ; Free GapBuffer
    invoke HeapFree, [gbHeap], 0, gbState
    
    ; Free tab entry
    invoke HeapFree, [tabHeap], 0, tab
    
    LOG_INFO "Closed tab"
    
    ret
TabBuffer_CloseTab ENDP

;==============================================================================
; Save Tab to Disk
;==============================================================================
TabBuffer_SaveFile PROC tab:QWORD
    LOCAL hFile:HANDLE
    LOCAL gbState:QWORD
    LOCAL fileSize:QWORD
    LOCAL fileData:QWORD
    LOCAL bytesWritten:DWORD
    
    mov gbState, [tab].VirtualTab.bufferModel
    
    ; Get buffer content
    call GapBuffer_GetSize, gbState
    mov fileSize, rax
    
    ; Allocate temp buffer
    invoke HeapAlloc, [gbHeap], 0, fileSize
    mov fileData, rax
    
    ; Copy all text to temp buffer
    call GapBuffer_GetText, gbState, 0, fileSize, fileData
    
    ; Open file for writing
    invoke CreateFileA, [tab].VirtualTab.filePath, 
        0x40000000, 0, NULL, 2, 0, NULL
    mov hFile, rax
    .if rax == INVALID_HANDLE_VALUE
        LOG_ERROR "Failed to open file for writing"
        ret
    .endif
    
    ; Write file
    invoke WriteFile, hFile, fileData, fileSize, 
        ADDR bytesWritten, NULL
    
    .if eax == FALSE
        LOG_ERROR "Failed to write file"
        invoke CloseHandle, hFile
        ret
    .endif
    
    invoke CloseHandle, hFile
    
    ; Mark as not dirty
    mov [tab].VirtualTab.isDirty, 0
    
    LOG_INFO "Saved file: %s (%lld bytes)", 
        [tab].VirtualTab.filePath, fileSize
    
    ret
TabBuffer_SaveFile ENDP

;==============================================================================
; Switch to Tab (Activate)
;==============================================================================
TabBuffer_SwitchTab PROC tab:QWORD
    ; Deactivate current tab
    .if [currentTab] != NULL
        mov [currentTab].VirtualTab.isActive, 0
    .endif
    
    ; Activate new tab
    mov [currentTab], tab
    mov [tab].VirtualTab.isActive, 1
    
    ; Restore cursor position
    ; TODO: Restore scroll position, selection, etc.
    
    LOG_DEBUG "Switched to tab"
    
    ret
TabBuffer_SwitchTab ENDP

;==============================================================================
; Insert Text at Cursor (Called from Editor)
;==============================================================================
TabBuffer_InsertText PROC lpText:QWORD, textLen:QWORD
    LOCAL tab:QWORD
    LOCAL gbState:QWORD
    LOCAL newPos:QWORD
    
    mov tab, [currentTab]
    .if tab == NULL
        LOG_ERROR "No active tab"
        ret
    .endif
    
    mov gbState, [tab].VirtualTab.bufferModel
    mov [lpText], lpText  ; Store for UndoStack_PushCommand
    
    ; Insert into buffer
    call GapBuffer_Insert, gbState, [tab].VirtualTab.cursorPos, 
        lpText, textLen
    
    ; Add to undo stack
    call UndoStack_PushCommand, 0, [tab].VirtualTab.cursorPos,
        NULL, lpText, textLen
    
    ; Update cursor
    mov rax, [tab].VirtualTab.cursorPos
    add rax, textLen
    mov [tab].VirtualTab.cursorPos, rax
    
    ; Mark dirty
    mov [tab].VirtualTab.isDirty, 1
    
    ; Invalidate tokens
    mov eax, [tab].VirtualTab.cursorPos
    xor edx, edx
    mov ecx, 512
    div ecx
    call Tokenizer_InvalidateBlock, eax
    
    LOG_DEBUG "Inserted %lld bytes at %lld", 
        textLen, [tab].VirtualTab.cursorPos - textLen
    
    ret
TabBuffer_InsertText ENDP

;==============================================================================
; Delete Selected Text
;==============================================================================
TabBuffer_DeleteSelection PROC
    LOCAL tab:QWORD
    LOCAL gbState:QWORD
    LOCAL length:QWORD
    
    mov tab, [currentTab]
    .if tab == NULL
        LOG_ERROR "No active tab"
        ret
    .endif
    
    mov gbState, [tab].VirtualTab.bufferModel
    
    mov rax, [tab].VirtualTab.selectionEnd
    sub rax, [tab].VirtualTab.selectionStart
    mov length, rax
    
    .if length == 0
        ret
    .endif
    
    ; Delete from buffer
    call GapBuffer_Delete, gbState, 
        [tab].VirtualTab.selectionStart, length
    
    ; Add to undo stack
    call UndoStack_PushCommand, 1, [tab].VirtualTab.selectionStart,
        NULL, NULL, length
    
    ; Update cursor
    mov [tab].VirtualTab.cursorPos, [tab].VirtualTab.selectionStart
    mov [tab].VirtualTab.selectionStart, 0
    mov [tab].VirtualTab.selectionEnd, 0
    
    ; Mark dirty
    mov [tab].VirtualTab.isDirty, 1
    
    LOG_DEBUG "Deleted %lld bytes", length
    
    ret
TabBuffer_DeleteSelection ENDP

;==============================================================================
; Get Current Line for Display
;==============================================================================
TabBuffer_GetCurrentLine PROC lpOutBuffer:QWORD, maxLen:DWORD
    LOCAL tab:QWORD
    LOCAL gbState:QWORD
    LOCAL lineNum:DWORD
    
    mov tab, [currentTab]
    .if tab == NULL
        mov rax, 0
        ret
    .endif
    
    mov gbState, [tab].VirtualTab.bufferModel
    
    ; Calculate line number from cursor position
    ; TODO: Use line index for O(1) lookup
    xor edx, edx
    mov rax, [tab].VirtualTab.cursorPos
    mov ecx, 80  ; Average line length
    div ecx
    mov lineNum, eax
    
    call GapBuffer_GetLine, gbState, lineNum, lpOutBuffer, maxLen
    
    ret
TabBuffer_GetCurrentLine ENDP

;==============================================================================
; Get Tokens for Current Line
;==============================================================================
TabBuffer_GetCurrentLineTokens PROC lpOutTokens:QWORD
    LOCAL tab:QWORD
    LOCAL lineNum:DWORD
    LOCAL blockNum:DWORD
    
    mov tab, [currentTab]
    .if tab == NULL
        mov eax, 0
        ret
    .endif
    
    ; Calculate line and block number
    xor edx, edx
    mov rax, [tab].VirtualTab.cursorPos
    mov ecx, 80
    div ecx
    mov lineNum, eax
    
    xor edx, edx
    mov rax, lineNum
    mov ecx, 512
    div ecx
    mov blockNum, eax
    
    ; Get tokens from block cache
    call Tokenizer_GetTokens, blockNum, lpOutTokens
    
    ret
TabBuffer_GetCurrentLineTokens ENDP

;==============================================================================
; Data Structures
;==============================================================================
.data

; Extended VirtualTab (from File 13, now with buffer reference)
VirtualTab STRUCT
    filePath        QWORD ?      ; Full file path
    bufferModel     QWORD ?      ; Pointer to GapBuffer (NEW)
    syntaxTokens    QWORD ?      ; Cached Token array
    tokenCount      DWORD ?      ; Token count
    undoStack       QWORD ?      ; Undo history for this tab
    cursorPos       QWORD ?      ; Cursor position
    selectionStart  QWORD ?      ; Selection bounds
    selectionEnd    QWORD ?      ; Selection end
    isDirty         BYTE ?       ; Modified?
    isActive        BYTE ?       ; Loaded in memory?
    lastAccessTime  QWORD ?      ; For LRU eviction
VirtualTab ENDS

; Global state
currentTab          dq ?         ; Currently active tab
tabHeap             dq ?         ; Heap for tab allocations
gbHeap              dq ?         ; GapBuffer heap (from File 22)

END
