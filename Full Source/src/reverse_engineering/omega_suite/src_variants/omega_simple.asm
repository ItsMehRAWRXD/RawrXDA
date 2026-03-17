; OMEGA-POLYGLOT MAXIMUM v3.0 PRO - Minimal Professional Version
.386
.model flat, stdcall
option casemap:none

include C:\masm32\include\windows.inc
include C:\masm32\include\kernel32.inc
include C:\masm32\include\user32.inc
include C:\masm32\macros\macros.asm

includelib C:\masm32\lib\kernel32.lib
includelib C:\masm32\lib\user32.lib

.data
szTitle     db "OMEGA-POLYGLOT MAXIMUM v3.0 PRO", 13, 10
            db "Professional Reverse Engineering Suite", 13, 10
            db "Claude | Moonshot | DeepSeek | Codex", 13, 10
            db "========================================", 13, 10, 0

szMenu      db "[1] PE Analysis [2] Import Scan [3] Section Analysis", 13, 10
            db "[4] String Extraction [0] Exit", 13, 10
            db "> ", 0

szPrompt    db "Target File: ", 0
szError     db "[-] Error", 13, 10, 0
szSuccess   db "[+] Success", 13, 10, 0
szNewline   db 13, 10, 0

szPEInfo    db "Machine: %04X  Sections: %d  Entry: %08X", 13, 10, 0
szSecInfo   db "Section: %.8s  VA: %08X  Size: %08X", 13, 10, 0
szImpInfo   db "Import: %s", 13, 10, 0
szStrInfo   db "String[%08X]: %s", 13, 10, 0

.data?
hConsole    dd ?
buffer      db 1024 dup(?)
filename    db 260 dup(?)
filebuffer  db 10485760 dup(?)  ; 10MB max
filesize    dd ?

; PE structure pointers
pe_base     dd ?
dos_header  dd ?
nt_header   dd ?
file_header dd ?
opt_header  dd ?
sections    dd ?
section_count dd ?

.code

;=============================================================================
; Console output helper
;=============================================================================
print proc text:DWORD
    invoke WriteConsole, hConsole, text, lengthof, NULL, NULL
    ret
print endp

;=============================================================================
; Get console input
;=============================================================================
getinput proc
    LOCAL chars_read:DWORD
    invoke ReadConsole, hConsole, addr filename, 260, addr chars_read, NULL
    mov eax, chars_read
    dec eax
    mov filename[eax], 0
    ret
getinput endp

;=============================================================================
; Load PE file
;=============================================================================
load_pe proc fname:DWORD
    LOCAL hFile:DWORD, bytes_read:DWORD
    
    ; Open file
    invoke CreateFile, fname, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL
    cmp eax, INVALID_HANDLE_VALUE
    je error_exit
    mov hFile, eax
    
    ; Get file size
    invoke GetFileSize, hFile, NULL
    mov filesize, eax
    cmp eax, 10485760  ; 10MB limit
    jg error_close
    
    ; Read file
    invoke ReadFile, hFile, addr filebuffer, filesize, addr bytes_read, NULL
    test eax, eax
    jz error_close
    
    invoke CloseHandle, hFile
    
    ; Setup PE pointers
    lea eax, filebuffer
    mov pe_base, eax
    mov dos_header, eax
    
    ; Validate DOS header
    cmp word ptr [eax], 'ZM'
    jne error_exit
    
    ; Get NT header
    mov eax, [eax+3Ch]
    add eax, pe_base
    mov nt_header, eax
    
    ; Validate PE signature
    cmp dword ptr [eax], 'EP'
    jne error_exit
    
    ; Setup file header
    add eax, 4
    mov file_header, eax
    
    ; Get section count
    movzx eax, word ptr [eax+2]
    mov section_count, eax
    
    ; Setup optional header
    mov eax, file_header
    add eax, 20
    mov opt_header, eax
    
    ; Setup sections (skip optional header)
    add eax, 224  ; Standard PE32 optional header size
    mov sections, eax
    
    mov eax, 1  ; Success
    ret
    
error_close:
    invoke CloseHandle, hFile
error_exit:
    xor eax, eax
    ret
load_pe endp

;=============================================================================
; Analyze PE headers
;=============================================================================
analyze_pe proc
    ; Print basic PE info
    mov eax, file_header
    movzx ebx, word ptr [eax]      ; Machine
    mov ecx, section_count         ; Number of sections
    mov eax, opt_header
    mov edx, [eax+16]              ; Entry point
    
    invoke wsprintf, addr buffer, addr szPEInfo, ebx, ecx, edx
    invoke print, addr buffer
    ret
analyze_pe endp

;=============================================================================
; Analyze sections
;=============================================================================
analyze_sections proc
    LOCAL current_section:DWORD
    
    mov current_section, 0
    
section_loop:
    mov eax, current_section
    cmp eax, section_count
    jge done_sections
    
    ; Get section pointer (40 bytes each)
    mov ebx, 40
    mul ebx
    add eax, sections
    
    ; Extract section info
    push eax  ; Save section pointer
    
    ; Get virtual address and size
    mov ebx, [eax+12]  ; VirtualAddress
    mov ecx, [eax+8]   ; VirtualSize
    
    ; Print section info (name is first 8 bytes)
    invoke wsprintf, addr buffer, addr szSecInfo, eax, ebx, ecx
    invoke print, addr buffer
    
    pop eax  ; Restore section pointer
    
    inc current_section
    jmp section_loop
    
done_sections:
    ret
analyze_sections endp

;=============================================================================
; Extract strings
;=============================================================================
extract_strings proc
    LOCAL offset:DWORD, length:DWORD, start_offset:DWORD
    
    mov offset, 0
    
string_scan:
    mov eax, offset
    cmp eax, filesize
    jge done_strings
    
    ; Check if current byte is printable
    lea ebx, filebuffer
    add ebx, eax
    movzx ecx, byte ptr [ebx]
    cmp cl, 32
    jb next_byte
    cmp cl, 126
    ja next_byte
    
    ; Start of potential string
    mov start_offset, eax
    mov length, 0
    
string_build:
    mov eax, offset
    cmp eax, filesize
    jge check_string
    
    lea ebx, filebuffer
    add ebx, eax
    movzx ecx, byte ptr [ebx]
    cmp cl, 32
    jb check_string
    cmp cl, 126
    ja check_string
    
    ; Add to buffer
    mov ebx, length
    cmp ebx, 100  ; Max string length
    jge check_string
    mov buffer[ebx], cl
    inc length
    inc offset
    jmp string_build
    
check_string:
    cmp length, 4  ; Minimum string length
    jl next_byte
    
    ; Null terminate and print
    mov eax, length
    mov buffer[eax], 0
    
    invoke wsprintf, addr filename, addr szStrInfo, start_offset, addr buffer
    invoke print, addr filename
    
next_byte:
    inc offset
    jmp string_scan
    
done_strings:
    ret
extract_strings endp

;=============================================================================
; Main program loop
;=============================================================================
main_loop proc
    LOCAL choice:DWORD
    
menu_display:
    invoke print, addr szTitle
    invoke print, addr szMenu
    
    ; Get choice (simplified input)
    invoke ReadConsole, hConsole, addr buffer, 10, addr choice, NULL
    movzx eax, byte ptr buffer
    sub eax, '0'
    mov choice, eax
    
    cmp choice, 0
    je exit_program
    cmp choice, 1
    je do_pe_analysis
    cmp choice, 2
    je do_pe_analysis  ; Same for now
    cmp choice, 3
    je do_sections
    cmp choice, 4
    je do_strings
    jmp menu_display
    
do_pe_analysis:
    invoke print, addr szPrompt
    invoke getinput
    invoke load_pe, addr filename
    test eax, eax
    jz menu_display
    invoke analyze_pe
    jmp menu_display
    
do_sections:
    invoke print, addr szPrompt
    invoke getinput
    invoke load_pe, addr filename
    test eax, eax
    jz menu_display
    invoke analyze_sections
    jmp menu_display
    
do_strings:
    invoke print, addr szPrompt
    invoke getinput
    invoke load_pe, addr filename
    test eax, eax
    jz menu_display
    invoke extract_strings
    jmp menu_display
    
exit_program:
    ret
main_loop endp

;=============================================================================
; Program entry point
;=============================================================================
start:
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hConsole, eax
    
    invoke main_loop
    invoke ExitProcess, 0
end start