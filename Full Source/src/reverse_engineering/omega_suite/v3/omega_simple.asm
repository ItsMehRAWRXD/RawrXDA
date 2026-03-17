; OMEGA-POLYGLOT SIMPLE v1.0
; Basic PE Analysis Tool
.386
.model flat, stdcall
option casemap:none

; Include MASM32 libraries
include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

; Constants
MAX_SIZE equ 10485760  ; 10MB max file size
MZ_SIG equ 5A4Dh       ; "MZ" signature
PE_SIG equ 00004550h   ; "PE\0\0" signature

.data
szBanner db "OMEGA-POLYGLOT SIMPLE v1.0", 0Dh, 0Ah
         db "Basic PE Analysis Tool", 0Dh, 0Ah, 0
szMenu db "[1] Load File", 0Dh, 0Ah
       db "[2] Show PE Info", 0Dh, 0Ah
       db "[3] Exit", 0Dh, 0Ah
       db "Choice: ", 0
szFilePrompt db "File path: ", 0
szError db "Error", 0
szSuccess db "Success", 0
szNewLine db 0Dh, 0Ah, 0

.data?
hFile dd ?
hStdIn dd ?
hStdOut dd ?
fileBuffer db MAX_SIZE dup(?)
fileSize dd ?
choiceBuffer db 256 dup(?)

.code

; Print string to console
PrintString proc uses ebx ecx edx, lpString:DWORD
    LOCAL bytesWritten:DWORD
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    invoke lstrlen, lpString
    mov ecx, eax
    invoke WriteConsole, hStdOut, lpString, ecx, addr bytesWritten, NULL
    ret
PrintString endp

; Read string from console
ReadString proc uses ebx ecx edx
    LOCAL bytesRead:DWORD
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    invoke ReadConsole, hStdIn, addr choiceBuffer, 256, addr bytesRead, NULL
    mov eax, bytesRead
    dec eax
    mov byte ptr [choiceBuffer+eax], 0
    ret
ReadString endp

; Load file into memory
LoadFile proc uses ebx ecx edx, lpFileName:DWORD
    LOCAL fileSizeHigh:DWORD
    
    ; Open file
    invoke CreateFile, lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, 
                     OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je @@error
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, addr fileSizeHigh
    cmp eax, INVALID_FILE_SIZE
    je @@error
    mov fileSize, eax
    
    ; Check if file is too large
    cmp eax, MAX_SIZE
    ja @@too_large
    
    ; Read file
    invoke ReadFile, hFile, addr fileBuffer, fileSize, addr fileSizeHigh, NULL
    test eax, eax
    jz @@error
    
    ; Close file
    invoke CloseHandle, hFile
    
    ; Check MZ signature
    mov ax, word ptr [fileBuffer]
    cmp ax, MZ_SIG
    jne @@not_pe
    
    invoke PrintString, addr szSuccess
    mov eax, 1
    ret
    
@@too_large:
    invoke CloseHandle, hFile
    invoke PrintString, addr szError
    xor eax, eax
    ret
    
@@not_pe:
    invoke CloseHandle, hFile
    invoke PrintString, addr szError
    xor eax, eax
    ret
    
@@error:
    invoke PrintString, addr szError
    xor eax, eax
    ret
LoadFile endp

; Show basic PE information
ShowPEInfo proc uses ebx ecx edx
    LOCAL peOffset:DWORD
    
    ; Check if file is loaded
    cmp fileSize, 0
    je @@no_file
    
    ; Get PE header offset
    mov eax, dword ptr [fileBuffer+3Ch]
    mov peOffset, eax
    
    ; Check PE signature
    mov eax, peOffset
    add eax, offset fileBuffer
    mov eax, dword ptr [eax]
    cmp eax, PE_SIG
    jne @@not_pe
    
    invoke PrintString, addr szSuccess
    ret
    
@@no_file:
    invoke PrintString, addr szError
    ret
    
@@not_pe:
    invoke PrintString, addr szError
    ret
ShowPEInfo endp

; Main menu
MainMenu proc
    LOCAL choice:DWORD
    
@@menu:
    invoke PrintString, addr szBanner
    invoke PrintString, addr szMenu
    call ReadString
    
    ; Convert choice to number
    movzx eax, byte ptr [choiceBuffer]
    sub eax, '0'
    mov choice, eax
    
    cmp choice, 1
    je @@load_file
    cmp choice, 2
    je @@show_info
    cmp choice, 3
    je @@exit
    jmp @@menu
    
@@load_file:
    invoke PrintString, addr szFilePrompt
    call ReadString
    invoke LoadFile, addr choiceBuffer
    jmp @@menu
    
@@show_info:
    call ShowPEInfo
    jmp @@menu
    
@@exit:
    ret
MainMenu endp

; Entry point
start:
    call MainMenu
    invoke ExitProcess, 0
end start