;======================================================================
; RawrXD IDE - Utility Functions Module
; String utilities, encoding conversion, time functions, helpers
;======================================================================
INCLUDE rawrxd_includes.inc

.DATA
; String conversions
g_numBuffer[32]         DB 32 DUP(0)
g_hexBuffer[16]         DB 16 DUP(0)

; Time constants
SECS_PER_DAY            EQU 86400
SECS_PER_HOUR           EQU 3600
SECS_PER_MINUTE         EQU 60

.CODE

;----------------------------------------------------------------------
; RawrXD_Util_IntToStr - Convert integer to string
;----------------------------------------------------------------------
RawrXD_Util_IntToStr PROC intVal:QWORD, pszBuffer:QWORD
    LOCAL numChars:QWORD
    LOCAL negative:QWORD
    LOCAL temp:QWORD
    
    mov negative, 0
    mov numChars, 0
    mov rax, intVal
    
    ; Handle negative numbers
    cmp rax, 0
    jge @@not_negative
    
    mov negative, 1
    neg rax
    
@@not_negative:
    mov temp, rax
    
    ; Count digits
    mov rax, temp
    mov numChars, 0
    
@@count_loop:
    xor rdx, rdx
    mov rcx, 10
    div rcx
    inc numChars
    test rax, rax
    jnz @@count_loop
    
    ; Convert to string
    mov rax, temp
    mov rcx, pszBuffer
    add rcx, numChars
    dec rcx
    
@@convert_loop:
    xor rdx, rdx
    mov r8, 10
    div r8
    
    add dl, '0'
    mov [rcx], dl
    
    dec rcx
    test rax, rax
    jnz @@convert_loop
    
    ; Add negative sign if needed
    cmp negative, 1
    jne @@done
    
    mov byte [pszBuffer], '-'
    add pszBuffer, 1
    
@@done:
    mov rax, pszBuffer
    add rax, numChars
    cmp negative, 0
    je @@ret
    inc rax  ; Account for negative sign
    
@@ret:
    ret
    
RawrXD_Util_IntToStr ENDP

;----------------------------------------------------------------------
; RawrXD_Util_StrToInt - Convert string to integer
;----------------------------------------------------------------------
RawrXD_Util_StrToInt PROC pszStr:QWORD
    LOCAL result:QWORD
    LOCAL negative:QWORD
    
    mov result, 0
    mov negative, 0
    
    mov rsi, pszStr
    
    ; Check for negative sign
    mov al, [rsi]
    cmp al, '-'
    jne @@not_neg
    
    mov negative, 1
    inc rsi
    
@@not_neg:
    ; Skip leading whitespace
    mov al, [rsi]
    cmp al, ' '
    jne @@start_convert
    inc rsi
    jmp @@not_neg
    
@@start_convert:
    ; Convert digits
    mov al, [rsi]
    test al, al
    jz @@convert_done
    
    ; Check if digit
    cmp al, '0'
    jb @@invalid_char
    cmp al, '9'
    ja @@invalid_char
    
    ; Accumulate result
    sub al, '0'
    movzx eax, al
    
    mov rcx, result
    imul rcx, 10
    add rcx, rax
    mov result, rcx
    
    inc rsi
    jmp @@start_convert
    
@@invalid_char:
    ; Stop on invalid character
    
@@convert_done:
    ; Apply negative sign if needed
    mov rax, result
    cmp negative, 0
    je @@ret
    neg rax
    
@@ret:
    ret
    
RawrXD_Util_StrToInt ENDP

;----------------------------------------------------------------------
; RawrXD_Util_HexToStr - Convert hex to string
;----------------------------------------------------------------------
RawrXD_Util_HexToStr PROC hexVal:QWORD, pszBuffer:QWORD
    LOCAL i:QWORD
    LOCAL hexDigits[16]:BYTE
    
    ; Hex digit characters
    mov hexDigits, "0123456789ABCDEF"
    
    mov rax, hexVal
    mov rcx, pszBuffer
    mov i, 0
    
@@convert_loop:
    cmp i, 8
    jge @@done
    
    ; Extract nibble
    mov rax, hexVal
    mov edx, 28
    sub edx, i
    shl edx, 2
    shr rax, cl
    and rax, 0Fh
    
    ; Get character
    mov rdx, OFFSET hexDigits
    add rdx, rax
    mov al, [rdx]
    
    ; Store character
    mov [rcx], al
    inc rcx
    add i, 4
    
    jmp @@convert_loop
    
@@done:
    mov byte [rcx], 0
    ret
    
RawrXD_Util_HexToStr ENDP

;----------------------------------------------------------------------
; RawrXD_Util_ToUpper - Convert character to uppercase
;----------------------------------------------------------------------
RawrXD_Util_ToUpper PROC charCode:QWORD
    mov al, charCode
    
    cmp al, 'a'
    jb @@ret
    cmp al, 'z'
    ja @@ret
    
    sub al, 32
    movzx eax, al
    
@@ret:
    ret
    
RawrXD_Util_ToUpper ENDP

;----------------------------------------------------------------------
; RawrXD_Util_ToLower - Convert character to lowercase
;----------------------------------------------------------------------
RawrXD_Util_ToLower PROC pszSource:QWORD, pszDest:QWORD
    LOCAL idx:QWORD
    LOCAL charCode:QWORD
    
    xor idx, idx
    
@@loop:
    mov al, [pszSource + idx]
    test al, al
    jz @@done
    
    mov charCode, rax
    
    ; Convert uppercase to lowercase
    cmp al, 'A'
    jb @@not_upper
    cmp al, 'Z'
    ja @@not_upper
    
    add al, 32
    
@@not_upper:
    mov [pszDest + idx], al
    inc idx
    jmp @@loop
    
@@done:
    mov byte [pszDest + idx], 0
    ret
    
RawrXD_Util_ToLower ENDP

;----------------------------------------------------------------------
; RawrXD_Util_HasExtension - Check if filename has extension
;----------------------------------------------------------------------
RawrXD_Util_HasExtension PROC pszFilename:QWORD, pszExt:QWORD
    LOCAL pDot:QWORD
    LOCAL pPos:QWORD
    
    ; Find last dot
    mov pPos, pszFilename
    xor pDot, pDot
    
@@search_dot:
    mov al, [pPos]
    test al, al
    jz @@compare
    
    cmp al, '.'
    jne @@next_char
    
    mov pDot, pPos
    
@@next_char:
    inc pPos
    jmp @@search_dot
    
@@compare:
    cmp pDot, 0
    je @@no_match
    
    ; Compare extensions
    INVOKE lstrcmpA, pDot, pszExt
    test eax, eax
    jz @@match
    
@@no_match:
    xor eax, eax
    ret
    
@@match:
    mov eax, 1
    ret
    
RawrXD_Util_HasExtension ENDP

;----------------------------------------------------------------------
; RawrXD_Util_ReverseString - Reverse string in place
;----------------------------------------------------------------------
RawrXD_Util_ReverseString PROC pszString:QWORD
    LOCAL start:QWORD
    LOCAL end:QWORD
    LOCAL temp:BYTE
    
    mov start, pszString
    mov end, pszString
    
    ; Find end of string
@@find_end:
    mov al, [end]
    test al, al
    jz @@reverse
    inc end
    jmp @@find_end
    
@@reverse:
    dec end  ; Point to last character
    
@@reverse_loop:
    cmp start, end
    jge @@done
    
    ; Swap characters
    mov al, [start]
    mov cl, [end]
    mov [start], cl
    mov [end], al
    
    inc start
    dec end
    jmp @@reverse_loop
    
@@done:
    ret
    
RawrXD_Util_ReverseString ENDP

;----------------------------------------------------------------------
; RawrXD_Util_TrimWhitespace - Trim leading and trailing whitespace
;----------------------------------------------------------------------
RawrXD_Util_TrimWhitespace PROC pszString:QWORD
    LOCAL start:QWORD
    LOCAL end:QWORD
    LOCAL idx:QWORD
    
    mov start, pszString
    
    ; Skip leading whitespace
@@skip_leading:
    mov al, [start]
    cmp al, ' '
    je @@continue_leading
    cmp al, 9   ; Tab
    je @@continue_leading
    cmp al, 13  ; CR
    je @@continue_leading
    cmp al, 10  ; LF
    je @@continue_leading
    jmp @@find_end
    
@@continue_leading:
    inc start
    jmp @@skip_leading
    
@@find_end:
    ; Find end of string
    mov end, start
    
@@find_loop:
    mov al, [end]
    test al, al
    jz @@trim_trailing
    inc end
    jmp @@find_loop
    
@@trim_trailing:
    ; Remove trailing whitespace
    dec end
    
@@trim_loop:
    mov al, [end]
    cmp al, ' '
    je @@continue_trim
    cmp al, 9   ; Tab
    je @@continue_trim
    cmp al, 13  ; CR
    je @@continue_trim
    cmp al, 10  ; LF
    je @@continue_trim
    
    ; Found first non-whitespace from end
    inc end
    jmp @@copy
    
@@continue_trim:
    dec end
    jmp @@trim_loop
    
@@copy:
    ; Copy trimmed string back
    mov rsi, start
    mov rdi, pszString
    mov rcx, end
    sub rcx, start
    rep movsb
    mov byte [rdi], 0
    
    ret
    
RawrXD_Util_TrimWhitespace ENDP

;----------------------------------------------------------------------
; RawrXD_Util_GetTicks - Get current tick count
;----------------------------------------------------------------------
RawrXD_Util_GetTicks PROC
    INVOKE GetTickCount
    ret
RawrXD_Util_GetTicks ENDP

;----------------------------------------------------------------------
; RawrXD_Util_GetTimeString - Get formatted time string
;----------------------------------------------------------------------
RawrXD_Util_GetTimeString PROC pszBuffer:QWORD
    LOCAL systemTime:SYSTEMTIME
    
    ; Get system time
    INVOKE GetLocalTime, ADDR systemTime
    
    ; Format as "HH:MM:SS"
    INVOKE wsprintf, pszBuffer, "%02d:%02d:%02d",
        systemTime.wHour,
        systemTime.wMinute,
        systemTime.wSecond
    
    ret
    
RawrXD_Util_GetTimeString ENDP

;----------------------------------------------------------------------
; RawrXD_Util_GetDateString - Get formatted date string
;----------------------------------------------------------------------
RawrXD_Util_GetDateString PROC pszBuffer:QWORD
    LOCAL systemTime:SYSTEMTIME
    
    ; Get system time
    INVOKE GetLocalTime, ADDR systemTime
    
    ; Format as "YYYY-MM-DD"
    INVOKE wsprintf, pszBuffer, "%04d-%02d-%02d",
        systemTime.wYear,
        systemTime.wMonth,
        systemTime.wDay
    
    ret
    
RawrXD_Util_GetDateString ENDP

;----------------------------------------------------------------------
; RawrXD_Util_CompareStringsCase - Case-insensitive string compare
;----------------------------------------------------------------------
RawrXD_Util_CompareStringsCase PROC pStr1:QWORD, pStr2:QWORD
    LOCAL c1:BYTE
    LOCAL c2:BYTE
    
    xor rax, rax
    
@@loop:
    mov al, [pStr1]
    mov cl, [pStr2]
    
    ; Convert to uppercase for comparison
    cmp al, 'a'
    jb @@skip_c1
    cmp al, 'z'
    ja @@skip_c1
    sub al, 32
    
@@skip_c1:
    cmp cl, 'a'
    jb @@compare
    cmp cl, 'z'
    ja @@compare
    sub cl, 32
    
@@compare:
    cmp al, cl
    jne @@ret
    
    test al, al
    jz @@equal
    
    inc pStr1
    inc pStr2
    jmp @@loop
    
@@equal:
    xor eax, eax
    ret
    
@@ret:
    sub al, cl
    movsx eax, al
    ret
    
RawrXD_Util_CompareStringsCase ENDP

;----------------------------------------------------------------------
; RawrXD_Util_FindChar - Find character in string
;----------------------------------------------------------------------
RawrXD_Util_FindChar PROC pszString:QWORD, charCode:QWORD
    LOCAL pPos:QWORD
    
    mov pPos, pszString
    mov al, charCode
    
@@search:
    mov cl, [pPos]
    test cl, cl
    jz @@not_found
    
    cmp cl, al
    je @@found
    
    inc pPos
    jmp @@search
    
@@found:
    mov rax, pPos
    ret
    
@@not_found:
    xor eax, eax
    ret
    
RawrXD_Util_FindChar ENDP

;----------------------------------------------------------------------
; RawrXD_Util_CountLines - Count lines in buffer
;----------------------------------------------------------------------
RawrXD_Util_CountLines PROC pBuffer:QWORD, bufSize:QWORD
    LOCAL count:QWORD
    LOCAL pos:QWORD
    
    mov count, 1  ; At least one line
    mov pos, 0
    
@@count_loop:
    cmp pos, bufSize
    jge @@done
    
    mov al, [pBuffer + pos]
    cmp al, 10  ; LF
    jne @@next
    
    inc count
    
@@next:
    inc pos
    jmp @@count_loop
    
@@done:
    mov rax, count
    ret
    
RawrXD_Util_CountLines ENDP

END
