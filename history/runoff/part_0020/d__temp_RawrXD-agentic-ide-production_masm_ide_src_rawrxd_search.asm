;======================================================================
; RawrXD IDE - Search Module
; Find/replace, regex support, multi-file search
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; Search state
g_searchText[256]       DB 256 DUP(0)
g_replaceText[256]      DB 256 DUP(0)
g_searchFlags           DQ 0
g_currentSearchPos      DQ 0
g_searchResultCount     DQ 0

; Search flags
SEARCH_CASE_SENSITIVE   EQU 01h
SEARCH_WHOLE_WORD       EQU 02h
SEARCH_REGEX            EQU 04h
SEARCH_BACKWARDS        EQU 08h
SEARCH_SELECTION        EQU 10h
SEARCH_ALL_FILES        EQU 20h

; Search results
MAX_RESULTS             EQU 256

SEARCH_RESULT STRUCT
    filename[260]       DB 260 DUP(0)
    line                DQ ?
    column              DQ ?
    lineText[512]       DB 512 DUP(0)
SEARCH_RESULT ENDS

g_searchResults[MAX_RESULTS * (SIZEOF SEARCH_RESULT)] DQ MAX_RESULTS DUP(0)
g_resultCount           DQ 0

.CODE

;----------------------------------------------------------------------
; RawrXD_Search_Find - Find text in current document
;----------------------------------------------------------------------
RawrXD_Search_Find PROC pszText:QWORD, flags:QWORD
    LOCAL pBuffer:QWORD
    LOCAL bufferSize:QWORD
    LOCAL pPos:QWORD
    LOCAL matchCount:QWORD
    
    ; Store search text
    INVOKE lstrcpyA, ADDR g_searchText, pszText
    mov g_searchFlags, flags
    
    ; Get editor buffer
    INVOKE RawrXD_Editor_GetBuffer, ADDR pBuffer, ADDR bufferSize
    
    ; Search from current position
    mov g_currentSearchPos, 0
    mov matchCount, 0
    mov pPos, pBuffer
    
    ; Check if whole word search
    cmp flags, SEARCH_WHOLE_WORD
    jne @@normal_search
    
    INVOKE RawrXD_Search_FindWholeWord, pBuffer, pszText
    jmp @@done
    
@@normal_search:
    ; Check if case sensitive
    cmp flags, SEARCH_CASE_SENSITIVE
    jne @@case_insensitive
    
    INVOKE RawrXD_Search_FindCaseSensitive, pBuffer, pszText
    jmp @@done
    
@@case_insensitive:
    INVOKE RawrXD_Search_FindCaseInsensitive, pBuffer, pszText
    
@@done:
    mov g_searchResultCount, rax
    
    ; Update status
    cmp rax, 0
    je @@not_found
    
    INVOKE RawrXD_StatusBar_SetPosition, 0, 0
    
    xor eax, eax
    ret
    
@@not_found:
    INVOKE RawrXD_Output_Warning, OFFSET szNotFound
    mov eax, -1
    ret
    
RawrXD_Search_Find ENDP

;----------------------------------------------------------------------
; RawrXD_Search_FindCaseSensitive - Case-sensitive search
;----------------------------------------------------------------------
RawrXD_Search_FindCaseSensitive PROC pBuffer:QWORD, pszText:QWORD
    LOCAL pPos:QWORD
    LOCAL pStart:QWORD
    LOCAL count:QWORD
    LOCAL textLen:QWORD
    
    INVOKE lstrlenA, pszText
    mov textLen, rax
    
    mov pPos, pBuffer
    xor count, count
    
@@search_loop:
    mov al, [pPos]
    test al, al
    jz @@done
    
    ; Try to match at current position
    INVOKE RawrXD_Search_CompareStrings, pPos, pszText, textLen
    test eax, eax
    jz @@next_pos
    
    ; Match found
    inc count
    
    ; If we only want first match, return now
    cmp g_searchFlags, SEARCH_WHOLE_WORD
    jne @@next_pos  ; Would apply whole word check here
    
    xor eax, eax
    dec eax  ; Return position (simplified)
    ret
    
@@next_pos:
    inc pPos
    jmp @@search_loop
    
@@done:
    mov rax, count
    ret
    
RawrXD_Search_FindCaseSensitive ENDP

;----------------------------------------------------------------------
; RawrXD_Search_FindCaseInsensitive - Case-insensitive search
;----------------------------------------------------------------------
RawrXD_Search_FindCaseInsensitive PROC pBuffer:QWORD, pszText:QWORD
    LOCAL pPos:QWORD
    LOCAL count:QWORD
    LOCAL textLen:QWORD
    LOCAL szLower[256]:BYTE
    
    ; Convert search text to lowercase
    INVOKE RawrXD_Util_ToLower, pszText, ADDR szLower
    
    INVOKE lstrlenA, ADDR szLower
    mov textLen, rax
    
    mov pPos, pBuffer
    xor count, count
    
@@search_loop:
    mov al, [pPos]
    test al, al
    jz @@done
    
    ; Convert current position to lowercase for comparison
    INVOKE RawrXD_Search_CompareStringsCaseInsensitive, pPos, ADDR szLower, textLen
    test eax, eax
    jz @@next_pos
    
    ; Match found
    inc count
    
@@next_pos:
    inc pPos
    jmp @@search_loop
    
@@done:
    mov rax, count
    ret
    
RawrXD_Search_FindCaseInsensitive ENDP

;----------------------------------------------------------------------
; RawrXD_Search_FindWholeWord - Whole word search
;----------------------------------------------------------------------
RawrXD_Search_FindWholeWord PROC pBuffer:QWORD, pszText:QWORD
    LOCAL pPos:QWORD
    LOCAL count:QWORD
    LOCAL prevChar:BYTE
    LOCAL nextChar:BYTE
    
    ; This would search for word boundaries
    ; Using character class checking
    
    xor count, count
    mov pPos, pBuffer
    
@@search_loop:
    mov al, [pPos]
    test al, al
    jz @@done
    
    ; Check if character before is word boundary
    cmp pPos, pBuffer
    je @@check_match
    
    mov al, [pPos - 1]
    INVOKE RawrXD_Search_IsWordChar, al
    test eax, eax
    jnz @@next_pos  ; Part of word, skip
    
@@check_match:
    ; Try to match
    INVOKE RawrXD_Search_CompareStrings, pPos, pszText, lstrlenA(pszText)
    test eax, eax
    jz @@next_pos
    
    ; Check character after match
    mov rax, pPos
    add rax, lstrlenA(pszText)
    mov al, [rax]
    
    INVOKE RawrXD_Search_IsWordChar, al
    test eax, eax
    jnz @@next_pos  ; Part of word, skip
    
    ; Valid whole word match
    inc count
    
@@next_pos:
    inc pPos
    jmp @@search_loop
    
@@done:
    mov rax, count
    ret
    
RawrXD_Search_FindWholeWord ENDP

;----------------------------------------------------------------------
; RawrXD_Search_Replace - Replace text
;----------------------------------------------------------------------
RawrXD_Search_Replace PROC pszText:QWORD, pszReplace:QWORD, replaceAll:QWORD
    LOCAL replaceCount:QWORD
    LOCAL pBuffer:QWORD
    LOCAL bufferSize:QWORD
    
    ; Store replacement text
    INVOKE lstrcpyA, ADDR g_replaceText, pszReplace
    
    xor replaceCount, replaceCount
    
    ; Get editor buffer
    INVOKE RawrXD_Editor_GetBuffer, ADDR pBuffer, ADDR bufferSize
    
    cmp replaceAll, 1
    je @@replace_all
    
    ; Replace single occurrence
    INVOKE RawrXD_Search_ReplaceOne, pBuffer, pszText, pszReplace
    mov replaceCount, rax
    jmp @@done
    
@@replace_all:
    ; Replace all occurrences
    INVOKE RawrXD_Search_ReplaceAll, pBuffer, pszText, pszReplace
    mov replaceCount, rax
    
@@done:
    ; Mark editor as modified
    INVOKE RawrXD_FileOps_MarkModified
    
    mov rax, replaceCount
    ret
    
RawrXD_Search_Replace ENDP

;----------------------------------------------------------------------
; RawrXD_Search_ReplaceOne - Replace single occurrence
;----------------------------------------------------------------------
RawrXD_Search_ReplaceOne PROC pBuffer:QWORD, pszText:QWORD, pszReplace:QWORD
    LOCAL newSize:QWORD
    LOCAL textLen:QWORD
    LOCAL replaceLen:QWORD
    LOCAL pos:QWORD
    
    ; Find text
    INVOKE RawrXD_Search_FindCaseSensitive, pBuffer, pszText
    test eax, eax
    jz @@not_found
    
    ; Get lengths
    INVOKE lstrlenA, pszText
    mov textLen, rax
    
    INVOKE lstrlenA, pszReplace
    mov replaceLen, rax
    
    ; Replace at editor's current selection
    INVOKE RawrXD_Editor_ReplaceSelection, pszReplace
    
    mov eax, 1
    ret
    
@@not_found:
    xor eax, eax
    ret
    
RawrXD_Search_ReplaceOne ENDP

;----------------------------------------------------------------------
; RawrXD_Search_ReplaceAll - Replace all occurrences
;----------------------------------------------------------------------
RawrXD_Search_ReplaceAll PROC pBuffer:QWORD, pszText:QWORD, pszReplace:QWORD
    LOCAL count:QWORD
    LOCAL pPos:QWORD
    LOCAL textLen:QWORD
    LOCAL replaceLen:QWORD
    
    INVOKE lstrlenA, pszText
    mov textLen, rax
    
    INVOKE lstrlenA, pszReplace
    mov replaceLen, rax
    
    xor count, count
    mov pPos, pBuffer
    
@@search_loop:
    ; Find next occurrence
    INVOKE RawrXD_Search_FindCaseSensitive, pPos, pszText
    test eax, eax
    jz @@done
    
    ; Replace
    INVOKE lstrcpyA, pPos, pszReplace
    
    ; Move position
    add pPos, replaceLen
    inc count
    
    jmp @@search_loop
    
@@done:
    mov rax, count
    ret
    
RawrXD_Search_ReplaceAll ENDP

;----------------------------------------------------------------------
; RawrXD_Search_SearchInFiles - Search across multiple files
;----------------------------------------------------------------------
RawrXD_Search_SearchInFiles PROC pszPath:QWORD, pszPattern:QWORD, flags:QWORD
    LOCAL findData:WIN32_FIND_DATAA
    LOCAL hFind:QWORD
    LOCAL szPath[260]:BYTE
    LOCAL szFile[260]:BYTE
    LOCAL hFile:QWORD
    LOCAL buffer[8192]:BYTE
    LOCAL bytesRead:QWORD
    
    mov g_resultCount, 0
    
    ; Build search pattern
    INVOKE lstrcpyA, ADDR szPath, pszPath
    INVOKE lstrcatA, ADDR szPath, "\*"
    
    ; Find first file
    INVOKE FindFirstFileA, ADDR szPath, ADDR findData
    mov hFind, rax
    
    cmp hFind, INVALID_HANDLE_VALUE
    je @@done
    
@@loop:
    ; Skip directories
    mov eax, findData.dwFileAttributes
    and eax, FILE_ATTRIBUTE_DIRECTORY
    cmp eax, 0
    jne @@next
    
    ; Check if source file
    INVOKE RawrXD_FileOps_IsSourceFile, ADDR findData.cFileName
    test eax, eax
    jz @@next
    
    ; Search in this file
    INVOKE lstrcpyA, ADDR szFile, pszPath
    INVOKE lstrcatA, ADDR szFile, ADDR findData.cFileName
    
    INVOKE RawrXD_Search_SearchFile, ADDR szFile, pszPattern
    
@@next:
    INVOKE FindNextFileA, hFind, ADDR findData
    test eax, eax
    jnz @@loop
    
@@done:
    INVOKE FindClose, hFind
    
    mov rax, g_resultCount
    ret
    
RawrXD_Search_SearchInFiles ENDP

;----------------------------------------------------------------------
; RawrXD_Search_SearchFile - Search single file
;----------------------------------------------------------------------
RawrXD_Search_SearchFile PROC pszFilename:QWORD, pszPattern:QWORD
    LOCAL hFile:QWORD
    LOCAL fileSize:QWORD
    LOCAL buffer[8192]:BYTE
    LOCAL bytesRead:QWORD
    
    ; Open file
    INVOKE CreateFileA,
        pszFilename,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    
    cmp rax, INVALID_HANDLE_VALUE
    je @@fail
    
    mov hFile, rax
    
    ; Read file
    INVOKE ReadFile, hFile, ADDR buffer, 8192, ADDR bytesRead, NULL
    test eax, eax
    jz @@close
    
    ; Search buffer
    INVOKE RawrXD_Search_SearchBuffer, ADDR buffer, bytesRead, pszPattern, pszFilename
    
@@close:
    INVOKE CloseHandle, hFile
    
@@fail:
    ret
    
RawrXD_Search_SearchFile ENDP

;----------------------------------------------------------------------
; RawrXD_Search_SearchBuffer - Search text buffer
;----------------------------------------------------------------------
RawrXD_Search_SearchBuffer PROC pBuffer:QWORD, bufSize:QWORD, pszPattern:QWORD, pszFilename:QWORD
    LOCAL pPos:QWORD
    LOCAL line:QWORD
    LOCAL column:QWORD
    LOCAL idx:QWORD
    
    mov pPos, pBuffer
    xor line, line
    xor column, column
    
@@scan_loop:
    cmp pPos, bufSize
    jge @@done
    
    ; Track line numbers
    mov al, [pBuffer + pPos]
    cmp al, 10  ; LF
    jne @@check_search
    inc line
    xor column, column
    
@@check_search:
    ; Try to match pattern
    INVOKE RawrXD_Search_CompareStrings, pBuffer + pPos, pszPattern, lstrlenA(pszPattern)
    test eax, eax
    jz @@next
    
    ; Match found - add to results
    cmp g_resultCount, MAX_RESULTS
    jge @@done
    
    ; Store result
    mov idx, g_resultCount
    imul idx, SIZEOF SEARCH_RESULT
    mov rax, OFFSET g_searchResults
    add rax, idx
    
    INVOKE lstrcpyA, SEARCH_RESULT.filename[rax], pszFilename
    mov SEARCH_RESULT.line[rax], line
    mov SEARCH_RESULT.column[rax], column
    
    inc g_resultCount
    
@@next:
    inc pPos
    inc column
    jmp @@scan_loop
    
@@done:
    ret
    
RawrXD_Search_SearchBuffer ENDP

;----------------------------------------------------------------------
; RawrXD_Search_CompareStrings - Compare strings
;----------------------------------------------------------------------
RawrXD_Search_CompareStrings PROC pStr1:QWORD, pStr2:QWORD, len:QWORD
    LOCAL i:QWORD
    
    mov i, 0
    
@@loop:
    cmp i, len
    jge @@match
    
    mov al, [pStr1 + i]
    mov cl, [pStr2 + i]
    cmp al, cl
    jne @@no_match
    
    inc i
    jmp @@loop
    
@@match:
    mov eax, 1
    ret
    
@@no_match:
    xor eax, eax
    ret
    
RawrXD_Search_CompareStrings ENDP

;----------------------------------------------------------------------
; RawrXD_Search_CompareStringsCaseInsensitive - Case-insensitive compare
;----------------------------------------------------------------------
RawrXD_Search_CompareStringsCaseInsensitive PROC pStr1:QWORD, pStr2:QWORD, len:QWORD
    LOCAL i:QWORD
    LOCAL c1:QWORD
    LOCAL c2:QWORD
    
    mov i, 0
    
@@loop:
    cmp i, len
    jge @@match
    
    mov al, [pStr1 + i]
    INVOKE RawrXD_Util_ToUpper, al
    mov c1, rax
    
    mov al, [pStr2 + i]
    INVOKE RawrXD_Util_ToUpper, al
    mov c2, rax
    
    cmp c1, c2
    jne @@no_match
    
    inc i
    jmp @@loop
    
@@match:
    mov eax, 1
    ret
    
@@no_match:
    xor eax, eax
    ret
    
RawrXD_Search_CompareStringsCaseInsensitive ENDP

;----------------------------------------------------------------------
; RawrXD_Search_IsWordChar - Check if character is word character
;----------------------------------------------------------------------
RawrXD_Search_IsWordChar PROC charCode:QWORD
    mov al, charCode
    
    ; Check alphanumeric or underscore
    cmp al, '_'
    je @@yes
    
    cmp al, '0'
    jb @@no
    cmp al, '9'
    ja @@check_alpha
    jmp @@yes
    
@@check_alpha:
    cmp al, 'a'
    jb @@check_upper
    cmp al, 'z'
    ja @@no
    jmp @@yes
    
@@check_upper:
    cmp al, 'A'
    jb @@no
    cmp al, 'Z'
    ja @@no
    
@@yes:
    mov eax, 1
    ret
    
@@no:
    xor eax, eax
    ret
    
RawrXD_Search_IsWordChar ENDP

; String literals
szNotFound              DB "Text not found", 0

END
