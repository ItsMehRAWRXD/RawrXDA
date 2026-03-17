; =============================================================================
; BARE-METAL PE32+ WRITER & x64 MACHINE CODE EMITTER
; =============================================================================
; Zero structures. Zero abstractions. Raw byte layout only.
; Produces a valid, runnable PE32+ console executable from scratch.
;
; Build:
;   ml64 /c BareMetal_PE_Writer.asm
;   link /SUBSYSTEM:CONSOLE /ENTRY:main BareMetal_PE_Writer.obj kernel32.lib
;
; Run:
;   BareMetal_PE_Writer.exe   -> creates "generated.exe"
;   generated.exe             -> exits with code 42
;
; =============================================================================
; FILE LAYOUT (0x600 = 1536 bytes total):
;   0x000-0x1FF  Headers (DOS + PE + Section Headers + padding)
;   0x200-0x3FF  .text section (machine code, RVA 0x1000)
;   0x400-0x5FF  .idata section (imports, RVA 0x2000)
;
; .idata INTERNAL LAYOUT (section offset -> RVA):
;   0x00 (0x2000): IMAGE_IMPORT_DESCRIPTOR #1  (20 bytes)
;   0x14 (0x2014): Null descriptor              (20 bytes)
;   0x28 (0x2028): ILT entry + null             (16 bytes)
;   0x38 (0x2038): IMAGE_IMPORT_BY_NAME         (14 bytes)
;   0x46 (0x2046): 2 bytes padding
;   0x48 (0x2048): IAT entry + null             (16 bytes)
;   0x58 (0x2058): DLL name string              (13 bytes)
;   0x65-0x1FF:   Zero padding
; =============================================================================

extrn ExitProcess: PROC
extrn CreateFileA: PROC
extrn WriteFile: PROC
extrn CloseHandle: PROC

GENERIC_WRITE           EQU 40000000h
CREATE_ALWAYS           EQU 2
FILE_ATTRIBUTE_NORMAL   EQU 80h
TOTAL_FILE_SIZE         EQU 600h

.data
    szOutFile   db "generated.exe", 0
    dwWritten   dd 0
    hOutFile    dq 0

    ALIGN 16
    pe_image    LABEL BYTE

    ; =================================================================
    ; DOS HEADER  (64 bytes, file offset 0x00 - 0x3F)
    ; =================================================================
    dw 5A4Dh                        ; 00: e_magic 'MZ'
    dw 0090h                        ; 02: e_cblp
    dw 0003h                        ; 04: e_cp
    dw 0000h                        ; 06: e_crlc
    dw 0004h                        ; 08: e_cparhdr
    dw 0000h                        ; 0A: e_minalloc
    dw 0FFFFh                       ; 0C: e_maxalloc
    dw 0000h                        ; 0E: e_ss
    dw 00B8h                        ; 10: e_sp
    dw 0000h                        ; 12: e_csum
    dw 0000h                        ; 14: e_ip
    dw 0000h                        ; 16: e_cs
    dw 0040h                        ; 18: e_lfarlc
    dw 0000h                        ; 1A: e_ovno
    dq 0                            ; 1C: e_res  (8 bytes)
    dw 0000h                        ; 24: e_oemid
    dw 0000h                        ; 26: e_oeminfo
    dq 0, 0                         ; 28: e_res2 (16 bytes)
    dd 0                            ; 38: e_res2 cont (4 bytes)
    dd 00000080h                    ; 3C: e_lfanew -> PE sig at 0x80

    ; =================================================================
    ; DOS STUB  (64 bytes, file offset 0x40 - 0x7F)
    ; =================================================================
    db 0Eh, 1Fh, 0BAh, 0Eh, 00h, 0B4h, 09h, 0CDh  ; 8 bytes
    db 21h, 0B8h, 01h, 4Ch, 0CDh, 21h              ; 6 bytes
    db "This program cannot be run i"                ; 28 bytes
    db "n DOS mode.", 0Dh, 0Dh, 0Ah, 24h            ; 15 bytes
    db 7 DUP(0)                                      ; 7 bytes pad
    ; Total: 8+6+28+15+7 = 64 bytes

    ; =================================================================
    ; PE SIGNATURE  (4 bytes, file offset 0x80)
    ; =================================================================
    dd 00004550h                    ; "PE\0\0"

    ; =================================================================
    ; COFF FILE HEADER  (20 bytes, file offset 0x84 - 0x97)
    ; =================================================================
    dw 8664h                        ; 84: Machine = AMD64
    dw 0002h                        ; 86: NumberOfSections
    dd 00000000h                    ; 88: TimeDateStamp
    dd 00000000h                    ; 8C: PointerToSymbolTable
    dd 00000000h                    ; 90: NumberOfSymbols
    dw 00F0h                        ; 94: SizeOfOptionalHeader (240)
    dw 0022h                        ; 96: Characteristics

    ; =================================================================
    ; OPTIONAL HEADER PE32+  (240 bytes, file offset 0x98 - 0x187)
    ; Standard fields (24 bytes)
    ; =================================================================
    dw 020Bh                        ; 98: Magic = PE32+
    db 01h                          ; 9A: MajorLinkerVersion
    db 00h                          ; 9B: MinorLinkerVersion
    dd 00000200h                    ; 9C: SizeOfCode
    dd 00000200h                    ; A0: SizeOfInitializedData
    dd 00000000h                    ; A4: SizeOfUninitializedData
    dd 00001000h                    ; A8: AddressOfEntryPoint
    dd 00001000h                    ; AC: BaseOfCode

    ; PE32+-specific fields (88 bytes)
    dd 40000000h                    ; B0: ImageBase low  (0x140000000)
    dd 00000001h                    ; B4: ImageBase high
    dd 00001000h                    ; B8: SectionAlignment
    dd 00000200h                    ; BC: FileAlignment
    dw 0006h                        ; C0: MajorOperatingSystemVersion
    dw 0000h                        ; C2: MinorOperatingSystemVersion
    dw 0000h                        ; C4: MajorImageVersion
    dw 0000h                        ; C6: MinorImageVersion
    dw 0006h                        ; C8: MajorSubsystemVersion
    dw 0000h                        ; CA: MinorSubsystemVersion
    dd 00000000h                    ; CC: Win32VersionValue
    dd 00003000h                    ; D0: SizeOfImage
    dd 00000200h                    ; D4: SizeOfHeaders
    dd 00000000h                    ; D8: CheckSum
    dw 0003h                        ; DC: Subsystem = CONSOLE
    dw 8160h                        ; DE: DllCharacteristics
    dq 00100000h                    ; E0: SizeOfStackReserve
    dq 00001000h                    ; E8: SizeOfStackCommit
    dq 00100000h                    ; F0: SizeOfHeapReserve
    dq 00001000h                    ; F8: SizeOfHeapCommit
    dd 00000000h                    ; 100: LoaderFlags
    dd 00000010h                    ; 104: NumberOfRvaAndSizes (16)

    ; Data Directories (128 bytes, file offset 0x108 - 0x187)
    ; [0]  Export:  RVA=0, Size=0
    dq 0                            ; 108
    ; [1]  Import:  RVA=0x2000, Size=0x28
    dd 00002000h                    ; 110: Import RVA
    dd 00000028h                    ; 114: Import Size
    ; [2]-[15]: all zeros  (14 * 8 = 112 bytes)
    dq 0, 0, 0, 0, 0, 0, 0         ; 118-14F (56 bytes)
    dq 0, 0, 0, 0, 0, 0, 0         ; 150-187 (56 bytes)

    ; =================================================================
    ; SECTION HEADER #1: .text  (40 bytes, file offset 0x188 - 0x1AF)
    ; =================================================================
    db ".text", 0, 0, 0             ; 188: Name (8 bytes)
    dd 00000200h                    ; 190: VirtualSize
    dd 00001000h                    ; 194: VirtualAddress
    dd 00000200h                    ; 198: SizeOfRawData
    dd 00000200h                    ; 19C: PointerToRawData
    dd 00000000h                    ; 1A0: PointerToRelocations
    dd 00000000h                    ; 1A4: PointerToLinenumbers
    dw 0000h                        ; 1A8: NumberOfRelocations
    dw 0000h                        ; 1AA: NumberOfLinenumbers
    dd 60000020h                    ; 1AC: Characteristics (CODE|EXEC|READ)

    ; =================================================================
    ; SECTION HEADER #2: .idata  (40 bytes, file offset 0x1B0 - 0x1D7)
    ; =================================================================
    db ".idata", 0, 0               ; 1B0: Name (8 bytes)
    dd 00000200h                    ; 1B8: VirtualSize
    dd 00002000h                    ; 1BC: VirtualAddress
    dd 00000200h                    ; 1C0: SizeOfRawData
    dd 00000400h                    ; 1C4: PointerToRawData
    dd 00000000h                    ; 1C8: PointerToRelocations
    dd 00000000h                    ; 1CC: PointerToLinenumbers
    dw 0000h                        ; 1D0: NumberOfRelocations
    dw 0000h                        ; 1D2: NumberOfLinenumbers
    dd 0C0000040h                   ; 1D4: Characteristics (INIT_DATA|READ)

    ; =================================================================
    ; HEADER PADDING  (0x200 - 0x1D8 = 40 bytes)
    ; =================================================================
    db 28h DUP(0)

    ; =================================================================
    ; .text SECTION  (512 bytes, file offset 0x200 - 0x3FF)
    ;
    ; Machine code at RVA 0x1000:
    ;   sub rsp, 28h
    ;   mov ecx, 42
    ;   call [rip+disp32]   ; -> IAT at RVA 0x2048
    ;   add rsp, 28h
    ;   ret
    ;
    ; RIP-relative call calculation:
    ;   Instruction at RVA 0x1009, length 6
    ;   RIP after = 0x100F
    ;   Target    = 0x2048 (IAT entry for ExitProcess)
    ;   disp32    = 0x2048 - 0x100F = 0x1039
    ; =================================================================
    db 48h, 83h, 0ECh, 28h         ; sub rsp, 28h           (4 bytes, RVA 0x1000)
    db 0B9h                         ; mov ecx, imm32         (1 byte,  RVA 0x1004)
    dd 0000002Ah                    ;   imm32 = 42           (4 bytes, RVA 0x1005)
    db 0FFh, 15h                    ; call [rip+disp32]      (2 bytes, RVA 0x1009)
    dd 00001039h                    ;   disp32 = 0x1039      (4 bytes, RVA 0x100B)
    db 48h, 83h, 0C4h, 28h         ; add rsp, 28h           (4 bytes, RVA 0x100F)
    db 0C3h                         ; ret                    (1 byte,  RVA 0x1013)
    ; Total code: 4+5+6+4+1 = 20 bytes = 0x14
    ; Padding: 0x200 - 0x14 = 0x1EC bytes
    db 1ECh DUP(0)

    ; =================================================================
    ; .idata SECTION  (512 bytes, file offset 0x400 - 0x5FF)
    ;
    ; Layout (section offset -> RVA):
    ;   0x00 (0x2000): Import Descriptor #1     20 bytes
    ;   0x14 (0x2014): Null Descriptor           20 bytes
    ;   0x28 (0x2028): ILT (1 entry + null)      16 bytes
    ;   0x38 (0x2038): IMPORT_BY_NAME            14 bytes
    ;   0x46 (0x2046): 2-byte alignment pad       2 bytes
    ;   0x48 (0x2048): IAT (1 entry + null)      16 bytes
    ;   0x58 (0x2058): DLL name                  13 bytes
    ;   0x65-0x1FF:    Zero padding             411 bytes
    ; =================================================================

    ; --- Import Descriptor #1 (20 bytes, offset 0x00) ---
    dd 00002028h                    ; OriginalFirstThunk = ILT RVA
    dd 00000000h                    ; TimeDateStamp
    dd 00000000h                    ; ForwarderChain
    dd 00002058h                    ; Name = DLL name RVA
    dd 00002048h                    ; FirstThunk = IAT RVA

    ; --- Null Descriptor (20 bytes, offset 0x14) ---
    dd 0, 0, 0, 0, 0

    ; --- ILT: Import Lookup Table (16 bytes, offset 0x28) ---
    dq 00002038h                    ; -> IMAGE_IMPORT_BY_NAME RVA
    dq 0                            ; Null terminator

    ; --- IMAGE_IMPORT_BY_NAME (14 bytes, offset 0x38) ---
    dw 0000h                        ; Hint
    db "ExitProcess", 0             ; 12 bytes (11 chars + null)

    ; --- Alignment padding (2 bytes, offset 0x46) ---
    db 0, 0

    ; --- IAT: Import Address Table (16 bytes, offset 0x48) ---
    dq 00002038h                    ; -> IMAGE_IMPORT_BY_NAME RVA (loader overwrites)
    dq 0                            ; Null terminator

    ; --- DLL Name (13 bytes, offset 0x58) ---
    db "kernel32.dll", 0

    ; --- Zero padding to fill 512-byte section ---
    ; Current: 0x65 bytes used. Remaining: 0x200 - 0x65 = 0x19B = 411 bytes
    db 19Bh DUP(0)

    pe_image_end LABEL BYTE

; =============================================================================
; CODE
; =============================================================================
.code

main PROC
    sub rsp, 28h

    ; --- CreateFileA("generated.exe", GENERIC_WRITE, ...) ---
    lea rcx, szOutFile              ; lpFileName
    mov edx, GENERIC_WRITE          ; dwDesiredAccess
    xor r8d, r8d                    ; dwShareMode = 0
    xor r9d, r9d                    ; lpSecurityAttributes = NULL
    mov dword ptr [rsp+20h], CREATE_ALWAYS
    mov dword ptr [rsp+28h], FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp+30h], 0      ; hTemplateFile = NULL
    call CreateFileA
    cmp rax, -1
    je _exit
    mov hOutFile, rax

    ; --- WriteFile(hFile, pe_image, 0x600, &dwWritten, NULL) ---
    mov rcx, rax                    ; hFile
    lea rdx, pe_image               ; lpBuffer
    mov r8d, TOTAL_FILE_SIZE        ; nNumberOfBytesToWrite
    lea r9, dwWritten               ; lpNumberOfBytesWritten
    mov qword ptr [rsp+20h], 0      ; lpOverlapped = NULL
    call WriteFile

    ; --- CloseHandle ---
    mov rcx, hOutFile
    call CloseHandle

_exit:
    xor ecx, ecx
    call ExitProcess

main ENDP

END
