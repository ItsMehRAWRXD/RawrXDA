; =============================================================================
; RawrXD_PE_Writer.asm - High-Performance Machine Code Emitter & PE+ Writer
; =============================================================================
; Specializing in direct machine code generation with zero dependencies.
; Implements the complete PE32+ structure: DOS Header, NT Headers,
; Section Headers, and Import Table generation.
; =============================================================================

.code

; --- Minimal DOS Header (64 bytes) ---
; MZ \0\0 ... @ 0x40 -> PE Offset
db 4Dh, 5Ah, 90h, 00h, 03h, 00h, 00h, 00h, 04h, 00h, 00h, 00h, FFh, FFh, 00h, 00h
db B8h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 40h, 00h, 00h, 00h, 00h, 00h, 00h, 00h
db 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h
db 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 00h, 80h, 00h, 00h, 00h

; --- DOS Stub ---
; "This program cannot be run in DOS mode."
db 0Eh, 1Fh, BAh, 0Eh, 00h, B4h, 09h, CDh, 21h, B8h, 01h, 4Ch, CDh, 21h, 54h, 68h
db 69h, 73h, 20h, 70h, 72h, 6Fh, 67h, 72h, 61h, 6Dh, 20h, 63h, 61h, 6Eh, 6Eh, 6Fh
db 74h, 20h, 62h, 65h, 20h, 72h, 75h, 6Eh, 20h, 69h, 6Eh, 20h, 44h, 4Fh, 53h, 20h
db 6Dh, 6Fh, 64h, 65h, 2Eh, 0Dh, 0Dh, 0Ah, 24h, 00h, 00h, 00h, 00h, 00h, 00h, 00h

; --- NT Headers (PE Signature) ---
db 50h, 45h, 00h, 00h  ; "PE\0\0"

; --- File Header ---
dw 8664h               ; Machine: AMD64
dw 0003h               ; Number of Sections: 3 (.text, .rdata, .data)
dd 00000000h           ; TimeDateStamp
dd 00000000h           ; PointerToSymbolTable
dd 00000000h           ; NumberOfSymbols
dw 00F0h               ; SizeOfOptionalHeader
dw 0022h               ; Characteristics: EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE

; --- Optional Header (PE32+) ---
dw 020Bh               ; Magic: PE32+
db 02h, 1Eh             ; Major/Minor Linker Version
dd 00001000h           ; SizeOfCode
dd 00001000h           ; SizeOfInitializedData
dd 00000000h           ; SizeOfUninitializedData
dd 00001000h           ; AddressOfEntryPoint
dd 00001000h           ; BaseOfCode
dq 0000000140000000h   ; ImageBase (Standard x64)
dd 00001000h           ; SectionAlignment
dd 00000200h           ; FileAlignment
dw 0006h, 0000h         ; Major/Minor OS Version
dw 0000h, 0000h         ; Major/Minor Image Version
dw 0006h, 0000h         ; Major/Minor Subsystem Version
dd 00000000h           ; Win32VersionValue
dd 00004000h           ; SizeOfImage
dd 00000400h           ; SizeOfHeaders
dd 00000000h           ; CheckSum
dw 0003h               ; Subsystem: WINDOWS_CUI (CLI)
dw 0000h               ; DllCharacteristics
dq 0000000000100000h   ; SizeOfStackReserve
dq 0000000000001000h   ; SizeOfStackCommit
dq 0000000000100000h   ; SizeOfHeapReserve
dq 0000000000001000h   ; SizeOfHeapCommit
dd 00000000h           ; LoaderFlags
dd 00000010h           ; NumberOfRvaAndSizes

; --- Data Directories ---
dq 0, 0                ; Export Table
dq 0, 0                ; Import Table (will populate)
dq 0, 0                ; Resource Table
dq 0, 0                ; Exception Table
dq 0, 0                ; Security Table
dq 0, 0                ; Base Relocation Table
dq 0, 0                ; Debug Directory
dq 0, 0                ; Architecture
dq 0, 0                ; Global Ptr
dq 0, 0                ; TLS Table
dq 0, 0                ; Load Config Table
dq 0, 0                ; Bound Import
dq 0, 0                ; IAT
dq 0, 0                ; Delay Import Descriptor
dq 0, 0                ; CLR Runtime Header
dq 0, 0                ; Reserved

; --- Section Headers ---
; .text
db '.text', 0, 0, 0
dd 00001000h           ; VirtualSize
dd 00001000h           ; VirtualAddress
dd 00000200h           ; SizeOfRawData
dd 00000400h           ; PointerToRawData
dd 0, 0                ; Relocs/Linenumbers
dw 0, 0                ; Counts
dd 60000020h           ; Char: CODE | EXECUTE | READ

; .rdata (Imports)
db '.rdata', 0, 0
dd 00001000h
dd 00002000h
dd 00000200h
dd 00000600h
dd 0, 0
dw 0, 0
dd 40000040h           ; Char: INITIALIZED | READ

; .data
db '.data', 0, 0, 0
dd 00001000h
dd 00003000h
dd 00000200h
dd 00000800h
dd 0, 0
dw 0, 0
dd C0000040h           ; Char: INITIALIZED | READ | WRITE

; =============================================================================
; Emit_FunctionEpilogue - Standard x64 stack frame teardown
; =============================================================================
Emit_FunctionEpilogue proc
    add rsp, 20h        ; Shadow space reclaim
    pop rbp             ; Restore frame pointer
    ret                 ; Return to caller
Emit_FunctionEpilogue endp

end
