;============================================================================
; OS Explorer Memory Reader v4.0
; Process enumeration and memory analysis tool
;============================================================================

.386
.model flat, stdcall
option casemap :none

;============================================================================
; INCLUDES
;============================================================================

include windows.inc
include kernel32.inc
include user32.inc
include advapi32.inc
include psapi.inc
include tlhelp32.inc

includelib kernel32.lib
includelib user32.lib
includelib advapi32.lib
includelib psapi.lib

;============================================================================
; CONSTANTS
;============================================================================

CLI_VERSION             equ "4.0.0"
MAX_PATH                equ 260
MAX_PROCESSES           equ 1024
MEM_COMMIT              equ 00001000h
PAGE_READWRITE          equ 00000004h
PAGE_EXECUTE_READWRITE equ 00000040h
PAGE_EXECUTE_READ       equ 00000020h
PAGE_READONLY           equ 00000002h
INFINITE                equ 0FFFFFFFFh
PROCESS_ALL_ACCESS      equ 001F0FFFh
PROCESS_VM_READ         equ 00000010h
PROCESS_QUERY_INFORMATION equ 00000400h
TH32CS_SNAPPROCESS      equ 00000002h
MEM_DUMP_SIZE           equ 4096

;============================================================================
; DATA
;============================================================================

.data

szWelcome               db "OS Explorer Memory Reader v", CLI_VERSION, 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0Dh, 0Ah, 0
szMenu                  db "[1] List all processes", 0Dh, 0Ah
                        db "[2] Scan process memory", 0Dh, 0Ah
                        db "[3] Dump memory region", 0Dh, 0Ah
                        db "[4] Find pattern in memory", 0Dh, 0Ah
                        db "[5] Exit", 0Dh, 0Ah
                        db "Select option: ", 0
szPromptPid             db "Enter PID: ", 0
szPromptAddress         db "Enter address (hex): ", 0
szPromptSize            db "Enter size: ", 0
szPromptPattern         db "Enter pattern (hex): ", 0
szProcessesHeader       db 0Dh, 0Ah, "PID\t\tProcess Name", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szProcessEntry          db "%d\t\t%s", 0Dh, 0Ah, 0
szMemoryHeader          db 0Dh, 0Ah, "Address\t\tSize\t\tState\t\tProtect", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szMemoryEntry           db "%08X\t%X\t%d\t%X", 0Dh, 0Ah, 0
szDumpHeader            db 0Dh, 0Ah, "Memory dump:", 0Dh, 0Ah
                        db "========================================", 0Dh, 0Ah, 0
szPatternFound          db "[+] Pattern found at: %08X", 0Dh, 0Ah, 0
szError                 db "[-] ERROR: ", 0
szErrorOpenProcess      db "Failed to open process.", 0Dh, 0Ah, 0
szErrorReadMemory       db "Failed to read memory.", 0Dh, 0Ah, 0
szErrorInvalidPid       db "Invalid PID.", 0Dh, 0Ah, 0
szErrorNoAccess         db "No access to process.", 0Dh, 0Ah, 0
szSuccess               db "[+] Operation completed successfully.", 0Dh, 0Ah, 0
szPressAnyKey           db 0Dh, 0Ah, "Press any key to continue...", 0
szFormatHex             db "%08X", 0
szFormatDec             db "%d", 0
szFormatString          db "%s", 0
szBuffer                db 256 dup(0)
szHexBuffer             db 16 dup(0)
processIds              dd MAX_PROCESSES dup(0)
memoryBuffer            db MEM_DUMP_SIZE dup(0)
hStdIn                  dd 0
hStdOut                 dd 0

;============================================================================
; CODE
;============================================================================

.code

;----------------------------------------------------------------------------
; Display message
;----------------------------------------------------------------------------
DisplayMessage PROC lpMessage:DWORD
    LOCAL dwWritten :DWORD
    LOCAL dwLen :DWORD
    
    invoke lstrlen, lpMessage
    mov dwLen, eax
    
    invoke WriteConsole, hStdOut, lpMessage, dwLen, addr dwWritten, NULL
    
    ret
DisplayMessage ENDP

;----------------------------------------------------------------------------
; Read integer input
;----------------------------------------------------------------------------
ReadInt PROC
    LOCAL dwRead :DWORD
    LOCAL dwValue :DWORD
    
    invoke ReadConsole, hStdIn, addr szBuffer, 256, addr dwRead, NULL
    
    ; Convert string to integer
    mov eax, 0
    lea esi, szBuffer
    
@@convert_loop:
    movzx ecx, byte ptr [esi]
    cmp ecx, 0Dh
    je @@done
    cmp ecx, 0
    je @@done
    
    sub ecx, '0'
    cmp ecx, 9
    ja @@invalid
    
    imul eax, eax, 10
    add eax, ecx
    inc esi
    jmp @@convert_loop
    
@@invalid:
    mov eax, -1
    
@@done:
    ret
ReadInt ENDP

;----------------------------------------------------------------------------
; Read hex input
;----------------------------------------------------------------------------
ReadHex PROC
    LOCAL dwRead :DWORD
    
    invoke ReadConsole, hStdIn, addr szBuffer, 256, addr dwRead, NULL
    
    ; Convert hex string to integer
    mov eax, 0
    lea esi, szBuffer
    
@@convert_loop:
    movzx ecx, byte ptr [esi]
    cmp ecx, 0Dh
    je @@done
    cmp ecx, 0
    je @@done
    
    ; Convert hex digit
    cmp ecx, '0'
    jb @@invalid
    cmp ecx, '9'
    jbe @@is_digit
    
    cmp ecx, 'A'
    jb @@invalid
    cmp ecx, 'F'
    jbe @@is_upper
    
    cmp ecx, 'a'
    jb @@invalid
    cmp ecx, 'f'
    ja @@invalid
    
    sub ecx, 'a'
    add ecx, 10
    jmp @@continue
    
@@is_upper:
    sub ecx, 'A'
    add ecx, 10
    jmp @@continue
    
@@is_digit:
    sub ecx, '0'
    
@@continue:
    shl eax, 4
    add eax, ecx
    inc esi
    jmp @@convert_loop
    
@@invalid:
    mov eax, 0
    
@@done:
    ret
ReadHex ENDP

;----------------------------------------------------------------------------
; List all processes
;----------------------------------------------------------------------------
ListProcesses PROC
    LOCAL hSnapshot :DWORD
    LOCAL pe :PROCESSENTRY32
    LOCAL dwWritten :DWORD
    
    ; Create toolhelp snapshot
    invoke CreateToolhelp32Snapshot, TH32CS_SNAPPROCESS, 0
    cmp eax, -1
    je @@error
    mov hSnapshot, eax
    
    ; Initialize process entry structure
    mov pe.dwSize, SIZEOF PROCESSENTRY32
    
    ; Display header
    invoke DisplayMessage, addr szProcessesHeader
    
    ; Get first process
    invoke Process32First, hSnapshot, addr pe
    cmp eax, 0
    je @@cleanup
    
@@process_loop:
    ; Display process info
    invoke wsprintf, addr szBuffer, addr szProcessEntry, pe.th32ProcessID, addr pe.szExeFile
    invoke DisplayMessage, addr szBuffer
    
    ; Get next process
    invoke Process32Next, hSnapshot, addr pe
    cmp eax, 0
    jne @@process_loop
    
@@cleanup:
    invoke CloseHandle, hSnapshot
    
    mov eax, TRUE
    ret
    
@@error:
    mov eax, FALSE
    ret
ListProcesses ENDP

;----------------------------------------------------------------------------
; Scan process memory regions
;----------------------------------------------------------------------------
ScanMemory PROC dwPid:DWORD
    LOCAL hProcess :DWORD
    LOCAL mbi :MEMORY_BASIC_INFORMATION
    LOCAL lpAddress :DWORD
    LOCAL dwWritten :DWORD
    
    ; Open process
    invoke OpenProcess, PROCESS_QUERY_INFORMATION OR PROCESS_VM_READ, FALSE, dwPid
    cmp eax, NULL
    je @@error_open
    mov hProcess, eax
    
    ; Display header
    invoke DisplayMessage, addr szMemoryHeader
    
    ; Start from address 0
    mov lpAddress, 0
    
@@scan_loop:
    ; Query memory region
    invoke VirtualQueryEx, hProcess, lpAddress, addr mbi, SIZEOF MEMORY_BASIC_INFORMATION
    cmp eax, 0
    je @@cleanup
    
    ; Display memory region info
    invoke wsprintf, addr szBuffer, addr szMemoryEntry, mbi.BaseAddress, mbi.RegionSize, mbi.State, mbi.Protect
    invoke DisplayMessage, addr szBuffer
    
    ; Move to next region
    mov eax, mbi.BaseAddress
    add eax, mbi.RegionSize
    mov lpAddress, eax
    
    ; Check if we've reached the end
    cmp lpAddress, 0
    je @@cleanup
    
    jmp @@scan_loop
    
@@cleanup:
    invoke CloseHandle, hProcess
    
    mov eax, TRUE
    ret
    
@@error_open:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorOpenProcess
    mov eax, FALSE
    ret
ScanMemory ENDP

;----------------------------------------------------------------------------
; Dump memory region
;----------------------------------------------------------------------------
DumpMemory PROC dwPid:DWORD, lpAddress:DWORD, dwSize:DWORD
    LOCAL hProcess :DWORD
    LOCAL dwRead :DWORD
    LOCAL dwWritten :DWORD
    LOCAL i :DWORD
    LOCAL j :DWORD
    
    ; Open process
    invoke OpenProcess, PROCESS_QUERY_INFORMATION OR PROCESS_VM_READ, FALSE, dwPid
    cmp eax, NULL
    je @@error_open
    mov hProcess, eax
    
    ; Display header
    invoke DisplayMessage, addr szDumpHeader
    
    ; Read memory
    invoke ReadProcessMemory, hProcess, lpAddress, addr memoryBuffer, dwSize, addr dwRead
    cmp eax, 0
    je @@error_read
    
    ; Display hex dump
    mov i, 0
    
@@dump_loop:
    cmp i, dwRead
    jge @@cleanup
    
    ; Format address
    invoke wsprintf, addr szBuffer, addr szFormatHex, lpAddress
    invoke DisplayMessage, addr szBuffer
    invoke DisplayMessage, addr szHexBuffer
    
    ; Display hex values
    mov j, 0
    
@@hex_loop:
    cmp j, 16
    jge @@next_line
    
    mov eax, i
    add eax, j
    cmp eax, dwRead
    jge @@next_line
    
    ; Get byte value
    lea edx, memoryBuffer
    add edx, eax
    movzx ecx, byte ptr [edx]
    
    ; Format hex byte
    invoke wsprintf, addr szHexBuffer, addr szFormatHex, ecx
    invoke DisplayMessage, addr szHexBuffer
    invoke DisplayMessage, addr szHexBuffer
    
    inc j
    jmp @@hex_loop
    
@@next_line:
    add i, 16
    jmp @@dump_loop
    
@@cleanup:
    invoke CloseHandle, hProcess
    
    mov eax, TRUE
    ret
    
@@error_open:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorOpenProcess
    mov eax, FALSE
    ret
    
@@error_read:
    invoke CloseHandle, hProcess
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorReadMemory
    mov eax, FALSE
    ret
DumpMemory ENDP

;----------------------------------------------------------------------------
; Find pattern in memory
;----------------------------------------------------------------------------
FindPattern PROC dwPid:DWORD, lpPattern:DWORD, dwPatternLen:DWORD
    LOCAL hProcess :DWORD
    LOCAL mbi :MEMORY_BASIC_INFORMATION
    LOCAL lpAddress :DWORD
    LOCAL dwRead :DWORD
    LOCAL i :DWORD
    LOCAL j :DWORD
    LOCAL bMatch :DWORD
    
    ; Open process
    invoke OpenProcess, PROCESS_QUERY_INFORMATION OR PROCESS_VM_READ, FALSE, dwPid
    cmp eax, NULL
    je @@error_open
    mov hProcess, eax
    
    ; Start from address 0
    mov lpAddress, 0
    
@@scan_loop:
    ; Query memory region
    invoke VirtualQueryEx, hProcess, lpAddress, addr mbi, SIZEOF MEMORY_BASIC_INFORMATION
    cmp eax, 0
    je @@cleanup
    
    ; Check if region is readable
    cmp mbi.State, MEM_COMMIT
    jne @@next_region
    
    ; Read memory region
    invoke ReadProcessMemory, hProcess, lpAddress, addr memoryBuffer, MEM_DUMP_SIZE, addr dwRead
    cmp eax, 0
    je @@next_region
    
    ; Search for pattern
    mov i, 0
    
@@search_loop:
    cmp i, dwRead
    jge @@next_region
    
    ; Check if pattern fits
    mov eax, i
    add eax, dwPatternLen
    cmp eax, dwRead
    jg @@next_region
    
    ; Compare pattern
    mov bMatch, TRUE
    mov j, 0
    
@@compare_loop:
    cmp j, dwPatternLen
    jge @@pattern_found
    
    ; Get byte from memory
    lea edx, memoryBuffer
    add edx, i
    add edx, j
    movzx ecx, byte ptr [edx]
    
    ; Get byte from pattern
    lea edx, lpPattern
    add edx, j
    movzx edx, byte ptr [edx]
    
    ; Compare
    cmp ecx, edx
    jne @@no_match
    
    inc j
    jmp @@compare_loop
    
@@no_match:
    mov bMatch, FALSE
    
@@pattern_found:
    cmp bMatch, TRUE
    jne @@continue_search
    
    ; Pattern found
    mov eax, lpAddress
    add eax, i
    invoke wsprintf, addr szBuffer, addr szPatternFound, eax
    invoke DisplayMessage, addr szBuffer
    
@@continue_search:
    inc i
    jmp @@search_loop
    
@@next_region:
    ; Move to next region
    mov eax, mbi.BaseAddress
    add eax, mbi.RegionSize
    mov lpAddress, eax
    
    ; Check if we've reached the end
    cmp lpAddress, 0
    je @@cleanup
    
    jmp @@scan_loop
    
@@cleanup:
    invoke CloseHandle, hProcess
    
    mov eax, TRUE
    ret
    
@@error_open:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorOpenProcess
    mov eax, FALSE
    ret
FindPattern ENDP

;----------------------------------------------------------------------------
; Main menu
;----------------------------------------------------------------------------
MainMenu PROC
    LOCAL dwChoice :DWORD
    LOCAL dwPid :DWORD
    LOCAL lpAddress :DWORD
    LOCAL dwSize :DWORD
    
@@menu_loop:
    ; Clear screen
    invoke system, addr szClearScreen
    
    ; Display welcome
    invoke DisplayMessage, addr szWelcome
    
    ; Display menu
    invoke DisplayMessage, addr szMenu
    
    ; Get choice
    call ReadInt
    mov dwChoice, eax
    
    cmp dwChoice, 1
    je @@option_list
    
    cmp dwChoice, 2
    je @@option_scan
    
    cmp dwChoice, 3
    je @@option_dump
    
    cmp dwChoice, 4
    je @@option_pattern
    
    cmp dwChoice, 5
    je @@option_exit
    
    jmp @@menu_loop
    
@@option_list:
    call ListProcesses
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
    
@@option_scan:
    invoke DisplayMessage, addr szPromptPid
    call ReadInt
    cmp eax, -1
    je @@invalid_pid
    mov dwPid, eax
    
    call ScanMemory, dwPid
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
    
@@option_dump:
    invoke DisplayMessage, addr szPromptPid
    call ReadInt
    cmp eax, -1
    je @@invalid_pid
    mov dwPid, eax
    
    invoke DisplayMessage, addr szPromptAddress
    call ReadHex
    mov lpAddress, eax
    
    invoke DisplayMessage, addr szPromptSize
    call ReadInt
    cmp eax, -1
    je @@invalid_size
    mov dwSize, eax
    
    call DumpMemory, dwPid, lpAddress, dwSize
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
    
@@option_pattern:
    invoke DisplayMessage, addr szPromptPid
    call ReadInt
    cmp eax, -1
    je @@invalid_pid
    mov dwPid, eax
    
    invoke DisplayMessage, addr szPromptPattern
    ; For now, use a simple pattern
    mov byte ptr szBuffer, 0x90
    mov byte ptr szBuffer+1, 0x90
    mov byte ptr szBuffer+2, 0x90
    
    call FindPattern, dwPid, addr szBuffer, 3
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
    
@@option_exit:
    mov eax, 0
    ret
    
@@invalid_pid:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidPid
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
    
@@invalid_size:
    invoke DisplayMessage, addr szError
    invoke DisplayMessage, addr szErrorInvalidPid
    invoke DisplayMessage, addr szPressAnyKey
    invoke ReadConsole, hStdIn, addr szBuffer, 2, addr dwSize, NULL
    jmp @@menu_loop
MainMenu ENDP

;----------------------------------------------------------------------------
; Main entry point
;----------------------------------------------------------------------------
main PROC
    LOCAL dwExitCode :DWORD
    
    ; Get standard input/output handles
    invoke GetStdHandle, STD_INPUT_HANDLE
    mov hStdIn, eax
    
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov hStdOut, eax
    
    ; Run main menu
    call MainMenu
    mov dwExitCode, eax
    
    ; Exit
    invoke ExitProcess, dwExitCode
main ENDP

END main
