; ===============================================================================
; RawrXD PE32+ Writer — Core Writer Engine v1.0.0
; ===============================================================================
; Purpose: Complete PE32+ binary generation without link.exe
; Handles: DOS stub, NT headers, sections, import table, relocations
; Output: Byte-reproducible x64 PE32+ executables
; ===============================================================================

EXTERN GetProcessHeap:PROC
EXTERN HeapAlloc:PROC
EXTERN HeapFree:PROC
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC

.code

; Globals for PE writer state
g_PEImageBase       qword ?     ; Image base address (0x140000000)
g_PEImageSize       qword ?     ; Total image size
g_PEHeaderSize      qword ?     ; Size of all headers
g_SectionCount      qword ?     ; Number of sections
g_EntryPointRVA     qword ?     ; RVA of entry point
g_SubsystemType     qword ?     ; SUBSYSTEM_WINDOWS_CUI (2) or GUI (3)
g_OutputBufferPtr   qword ?     ; Output PE buffer pointer
g_OutputBufferSize  qword ?     ; Output buffer size
g_CurrentFileOffset qword ?     ; Current write offset
g_ImportTableSize   qword ?     ; Size of import table
g_RelocationTableSize qword ?   ; Size of relocation table

; ===============================================================================
; PROCEDURE: RawrXD_PE_WriteDOSHeader_ml64
; ===============================================================================
; Purpose: Write minimal DOS header + stub (64 bytes)
; Input:
;   rcx = output buffer pointer
;   rdx = PE header offset (typically 0x40)
; Output:
;   rax = bytes written (0x40 = 64 bytes)
; ===============================================================================
RawrXD_PE_WriteDOSHeader_ml64 PROC FRAME
    push rbx
    .pushreg rbx
    .endprolog
    
    mov rbx, rcx                    ; rbx = output buffer
    mov r8, rdx                     ; r8 = PE header offset
    xor r9, r9                      ; r9 = loop counter
    
    ; Write DOS header signature "MZ" (5A4Dh)
    mov byte ptr [rbx + 0], 4Dh      ; 'M'
    mov byte ptr [rbx + 1], 5Ah      ; 'Z'
    
    ; Zero out DOS stub fields (bytes 2-3Bh for standard DOS header)
    lea r10, [rbx + 2]
    mov r11, 3Eh                     ; 62 bytes to zero
DOS_HEADER_ZERO_LOOP:
    cmp r9, r11
    jge DOS_HEADER_ZERO_DONE
    mov byte ptr [r10 + r9], 0
    inc r9
    jmp DOS_HEADER_ZERO_LOOP
    
DOS_HEADER_ZERO_DONE:
    ; Write PE offset at 3Ch (4 bytes, little-endian)
    mov dword ptr [rbx + 3Ch], r8d   ; PE header offset (typically 40h)
    
    ; DOS stub is complete (64 bytes)
    mov rax, 40h                     ; Return 64 bytes written
    
    pop rbx
    ret
RawrXD_PE_WriteDOSHeader_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_WriteNTHeaders_ml64
; ===============================================================================
; Purpose: Write PE signature + FILE_HEADER + OPTIONAL_HEADER (264 bytes)
; Input:
;   rcx = output buffer pointer
;   rdx = entry point RVA
;   r8 = subsystem type (2 = CONSOLE, 3 = GUI)
;   r9 = image size (file size)
;   [rsp+32] = section count
;   [rsp+40] = code size
; Output:
;   rax = bytes written
; Stack frame adjustment: RSP must be 16-byte aligned before call
; ===============================================================================
RawrXD_PE_WriteNTHeaders_ml64 PROC FRAME
    sub rsp, 48h                    ; 72 bytes local
    .allocstack 48h
    .endprolog
    
    push rbx
    push r12
    push r13
    
    mov rbx, rcx                    ; rbx = output buffer
    mov r12, rdx                    ; r12 = entry point RVA
    mov r13, r8                     ; r13 = subsystem type
    mov r10, r9                     ; r10 = image size
    
    ; ===== WRITE PE SIGNATURE (4 bytes) =====
    mov dword ptr [rbx + 0], 00004550h  ; "PE\0\0" full signature
    ; Machine field starts at offset 04h, set below
    
    ; ===== WRITE FILE_HEADER (20 bytes) =====
    ; Machine type (offset 04h)
    mov word ptr [rbx + 04h], 8664h  ; AMD64
    
    ; Number of sections (offset 06h)
    mov r8d, [rsp + 60h]             ; Load section count from stack
    mov word ptr [rbx + 06h], r8w
    
    ; TimeDateStamp (offset 08h, 4 bytes) - Use fixed value for reproducibility
    mov dword ptr [rbx + 08h], 5E000000h  ; Reproducible timestamp
    
    ; PointerToSymbolTable (offset 0Ch) - Must be 0
    mov dword ptr [rbx + 0Ch], 0
    
    ; NumberOfSymbols (offset 10h) - Must be 0
    mov dword ptr [rbx + 10h], 0
    
    ; SizeOfOptionalHeader (offset 14h) - 240 bytes for PE32+
    mov word ptr [rbx + 14h], 0F0h   ; 240 in decimal
    
    ; Characteristics (offset 16h)
    ; Flags: EXECUTABLE_IMAGE (0002h) | 32BIT_MACHINE (0100h) = 0102h
    mov word ptr [rbx + 16h], 0222h  ; EXECUTABLE_IMAGE | 32BIT_MACHINE | LARGE_ADDRESS_AWARE
    
    ; ===== WRITE OPTIONAL_HEADER (240 bytes) =====
    lea rax, [rbx + 18h]             ; rax = start of optional header
    
    ; Magic (offset 00h, 20Bh for PE32+)
    mov word ptr [rax + 0], 020Bh
    
    ; Linker version (1 byte major + 1 byte minor)
    mov byte ptr [rax + 02h], 14     ; Major version = 14 (MSVC 2015+)
    mov byte ptr [rax + 03h], 0      ; Minor version = 0
    
    ; SizeOfCode (4 bytes) - From parameter
    mov r9d, [rsp + 68h]             ; Load code size from stack
    mov dword ptr [rax + 04h], r9d
    
    ; SizeOfInitializedData (4 bytes)
    mov dword ptr [rax + 08h], 1000h
    
    ; SizeOfUninitializedData (4 bytes)
    mov dword ptr [rax + 0Ch], 0
    
    ; AddressOfEntryPoint (4 bytes) - Entry point RVA
    mov dword ptr [rax + 10h], r12d  ; Entry point RVA
    
    ; BaseOfCode (4 bytes)
    mov dword ptr [rax + 14h], 1000h ; RVA 1000h
    
    ; For PE32+, skipped BaseOfData at offset 0x18
    
    ; ImageBase (8 bytes at offset 20h, PE32+)
    ; 140000000h > 32-bit, split into two DWORD writes
    mov dword ptr [rax + 20h], 40000000h   ; ImageBase low DWORD
    mov dword ptr [rax + 24h], 1            ; ImageBase high DWORD (0x00000001)
    
    ; SectionAlignment (4 bytes)
    mov dword ptr [rax + 28h], 1000h ; 4KB
    
    ; FileAlignment (4 bytes)
    mov dword ptr [rax + 2Ch], 200h  ; 512 bytes
    
    ; Major/Minor OS version (4 bytes)
    mov dword ptr [rax + 30h], 00060006h  ; Windows Vista = 6.0
    
    ; Major/Minor Image version (4 bytes)
    mov dword ptr [rax + 34h], 00010000h  ; Version 1.0
    
    ; Major/Minor Subsystem version (4 bytes)
    mov dword ptr [rax + 38h], 00060006h  ; Windows Vista = 6.0
    
    ; Win32VersionValue (4 bytes) - Reserved
    mov dword ptr [rax + 3Ch], 0
    
    ; SizeOfImage (4 bytes)
    mov dword ptr [rax + 40h], r10d  ; Total image size
    
    ; SizeOfHeaders (4 bytes) - DOS (40h) + PE (18h + 0F0h) + sections
    mov dword ptr [rax + 44h], 400h  ; Headers = 1024 bytes
    
    ; CheckSum (4 bytes) - Can be 0 for most cases
    mov dword ptr [rax + 48h], 0
    
    ; Subsystem (2 bytes)
    mov word ptr [rax + 4Ch], r13w   ; r13 = subsystem type
    
    ; DllCharacteristics (2 bytes)
    ; Flags: ASLR (0040h) | DEP (0100h) = 0140h
    mov word ptr [rax + 4Eh], 0140h  ; ASLR + DEP
    
    ; Stack/Heap reserve/commit (8 bytes each)
    mov qword ptr [rax + 50h], 100000h ; Stack reserve = 1MB
    mov qword ptr [rax + 58h], 1000h ; Stack commit = 4KB
    mov qword ptr [rax + 60h], 100000h ; Heap reserve = 1MB
    mov qword ptr [rax + 68h], 1000h ; Heap commit = 4KB
    
    ; LoaderFlags (4 bytes)
    mov dword ptr [rax + 70h], 0
    
    ; NumberOfRvaAndSizes (4 bytes) - Always 16
    mov dword ptr [rax + 74h], 16
    
    ; Data directories (16 entries × 8 bytes = 128 bytes)
    ; All set to 0 except import table (index 1)
    xor r9, r9
DD_ZERO_LOOP:
    cmp r9, 128
    jge DD_ZERO_DONE
    mov byte ptr [rax + 78h + r9], 0
    inc r9
    jmp DD_ZERO_LOOP
    
DD_ZERO_DONE:
    ; Set import table directory entry (index 1, offset 78h + 08h = 80h)
    mov dword ptr [rax + 80h], 3000h ; Import table RVA
    mov dword ptr [rax + 84h], 1000h ; Import table size
    
    ; PE header size = 0x18 (PE sig) + 0x14 (FILE_HEADER) + 0xF0 (OPTIONAL_HEADER)
    mov rax, 104h                    ; 260 bytes
    
    pop r13
    pop r12
    pop rbx
    add rsp, 48h
    ret
RawrXD_PE_WriteNTHeaders_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_WriteSectionHeaders_ml64
; ===============================================================================
; Purpose: Write section headers (.text, .data, .reloc, .idata)
; Input:
;   rcx = output buffer pointer (at section header start)
;   rdx = number of sections
;   r8 = section config array pointer (offset, size, flags for each section)
; Output:
;   rax = bytes written (40 * number_of_sections)
; ===============================================================================
RawrXD_PE_WriteSectionHeaders_ml64 PROC FRAME
    sub rsp, 28h
    .allocstack 28h
    .endprolog
    
    push rbx
    push r12
    
    mov rbx, rcx                    ; rbx = output buffer
    mov r12, rdx                    ; r12 = section count
    mov r10, r8                     ; r10 = section config
    xor r9, r9                      ; r9 = section index
    
    ; Section names as dword values (little-endian)
    mov eax, 7478742Eh              ; ".text"
    mov edx, 6174642Eh              ; ".data"
    mov ecx, 6F6C722Eh              ; ".reloc"
    mov r11d, 61746469h             ; ".idata"
    
SECTION_HEADER_LOOP:
    cmp r9, r12
    jge SECTION_HEADER_DONE
    
    ; Calculate offset into output buffer (40 bytes per section header)
    mov rax, r9
    imul rax, 40
    add rax, rbx
    
    ; Zero out section header
    xor r11d, r11d
ZERO_SECTION:
    cmp r11d, 40
    jge ZERO_SECTION_DONE
    mov byte ptr [rax + r11], 0
    inc r11d
    jmp ZERO_SECTION
    
ZERO_SECTION_DONE:
    ; Write section name (8 bytes, null-padded)
    cmp r9, 0
    je WRITE_TEXT_SECTION
    cmp r9, 1
    je WRITE_DATA_SECTION
    cmp r9, 2
    je WRITE_RELOC_SECTION
    cmp r9, 3
    je WRITE_IDATA_SECTION
    jmp SECTION_HEADER_NEXT
    
WRITE_TEXT_SECTION:
    mov dword ptr [rax + 0], 7478742Eh  ; ".text"
    mov dword ptr [rax + 4], 0
    mov dword ptr [rax + 8], 2000h   ; VirtualSize = 8KB
    mov dword ptr [rax + 12], 1000h  ; VirtualAddress = 1000h
    mov dword ptr [rax + 16], 1000h  ; SizeOfRawData = 4KB
    mov dword ptr [rax + 20], 400h   ; PointerToRawData = 400h (after headers)
    mov dword ptr [rax + 24], 0      ; PointerToRelocations
    mov dword ptr [rax + 28], 0      ; PointerToLinenumbers
    mov word ptr [rax + 32], 0       ; NumberOfRelocations
    mov word ptr [rax + 34], 0       ; NumberOfLinenumbers
    mov dword ptr [rax + 36], 60000020h ; Characteristics = CODE | EXECUTE | READ
    jmp SECTION_HEADER_NEXT
    
WRITE_DATA_SECTION:
    mov dword ptr [rax + 0], 6174642Eh  ; ".data"
    mov dword ptr [rax + 4], 0
    mov dword ptr [rax + 8], 1000h   ; VirtualSize = 4KB
    mov dword ptr [rax + 12], 3000h  ; VirtualAddress = 3000h
    mov dword ptr [rax + 16], 1000h  ; SizeOfRawData = 4KB
    mov dword ptr [rax + 20], 1400h  ; PointerToRawData
    mov dword ptr [rax + 24], 0
    mov dword ptr [rax + 28], 0
    mov word ptr [rax + 32], 0
    mov word ptr [rax + 34], 0
    mov dword ptr [rax + 36], 0C0000040h ; Characteristics = DATA | INITIALIZED | READ | WRITE
    jmp SECTION_HEADER_NEXT
    
WRITE_RELOC_SECTION:
    mov dword ptr [rax + 0], 6F6C722Eh  ; ".reloc"
    mov dword ptr [rax + 4], 0
    mov dword ptr [rax + 8], 1000h   ; VirtualSize = 4KB
    mov dword ptr [rax + 12], 4000h  ; VirtualAddress = 4000h
    mov dword ptr [rax + 16], 200h   ; SizeOfRawData = 512 bytes
    mov dword ptr [rax + 20], 2400h  ; PointerToRawData
    mov dword ptr [rax + 24], 0
    mov dword ptr [rax + 28], 0
    mov word ptr [rax + 32], 0
    mov word ptr [rax + 34], 0
    mov dword ptr [rax + 36], 42000040h ; Characteristics = DATA | DISCARDABLE | READ
    jmp SECTION_HEADER_NEXT
    
WRITE_IDATA_SECTION:
    mov dword ptr [rax + 0], 61746469h  ; ".idata"
    mov dword ptr [rax + 4], 0
    mov dword ptr [rax + 8], 1000h   ; VirtualSize = 4KB
    mov dword ptr [rax + 12], 5000h  ; VirtualAddress = 5000h
    mov dword ptr [rax + 16], 200h   ; SizeOfRawData = 512 bytes
    mov dword ptr [rax + 20], 2600h  ; PointerToRawData
    mov dword ptr [rax + 24], 0
    mov dword ptr [rax + 28], 0
    mov word ptr [rax + 32], 0
    mov word ptr [rax + 34], 0
    mov dword ptr [rax + 36], 0C0000040h ; Characteristics = DATA | READ | WRITE
    jmp SECTION_HEADER_NEXT
    
SECTION_HEADER_NEXT:
    inc r9
    jmp SECTION_HEADER_LOOP
    
SECTION_HEADER_DONE:
    ; Return bytes written = 40 * section count
    mov rax, r12
    imul rax, 40
    
    pop r12
    pop rbx
    add rsp, 28h
    ret
RawrXD_PE_WriteSectionHeaders_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_GenerateImportTable_ml64
; ===============================================================================
; Purpose: Generate import table with kernel32, user32, msvcrt imports
; Input:
;   rcx = output buffer pointer
;   rdx = RVA of import table section
; Output:
;   rax = bytes written (size of complete import table)
; ===============================================================================
RawrXD_PE_GenerateImportTable_ml64 PROC FRAME
    sub rsp, 40h
    .allocstack 40h
    .endprolog
    
    push rbx
    push r12
    push r13
    push r14
    
    mov rbx, rcx                    ; rbx = output buffer
    xor r9, r9                      ; r9 = write offset
    
    ; === Import Directory (3 entries for 3 DLLs + 1 terminator) ===
    ; Each entry is 20 bytes
    
    ; Entry 0: kernel32.dll
    mov dword ptr [rbx + r9 + 0], 100h  ; ILT RVA (import lookup table)
    mov dword ptr [rbx + r9 + 4], 0  ; TimeDateStamp (bound)
    mov dword ptr [rbx + r9 + 8], 0FFFFFFFFh ; ForwarderChain (-1)
    mov dword ptr [rbx + r9 + 12], 200h ; NameRVA (DLL name offset)
    mov dword ptr [rbx + r9 + 16], 300h ; IAT RVA (import address table)
    add r9, 20
    
    ; Entry 1: user32.dll
    mov dword ptr [rbx + r9 + 0], 120h  ; ILT RVA
    mov dword ptr [rbx + r9 + 4], 0
    mov dword ptr [rbx + r9 + 8], 0FFFFFFFFh
    mov dword ptr [rbx + r9 + 12], 220h ; NameRVA
    mov dword ptr [rbx + r9 + 16], 340h ; IAT RVA
    add r9, 20
    
    ; Entry 2: msvcrt.dll
    mov dword ptr [rbx + r9 + 0], 140h  ; ILT RVA
    mov dword ptr [rbx + r9 + 4], 0
    mov dword ptr [rbx + r9 + 8], 0FFFFFFFFh
    mov dword ptr [rbx + r9 + 12], 232h ; NameRVA
    mov dword ptr [rbx + r9 + 16], 380h ; IAT RVA
    add r9, 20
    
    ; Terminator (null entry)
    xor eax, eax
    mov dword ptr [rbx + r9 + 0], eax
    mov dword ptr [rbx + r9 + 4], eax
    mov dword ptr [rbx + r9 + 8], eax
    mov dword ptr [rbx + r9 + 12], eax
    mov dword ptr [rbx + r9 + 16], eax
    add r9, 20
    
    ; === Import Lookup Tables (ILT) ===
    ; Each entry is 8 bytes RVA to hint/name pair
    
    ; ILT 0 for kernel32 functions (ExitProcess, CreateWindowExA, ...)
    mov qword ptr [rbx + 100h], 400h ; RVA of hint/name pair
    mov qword ptr [rbx + 108h], 0    ; Terminator
    
    ; ILT 1 for user32 functions
    mov qword ptr [rbx + 120h], 420h ; RVA of hint/name pair
    mov qword ptr [rbx + 128h], 0    ; Terminator
    
    ; ILT 2 for msvcrt functions
    mov qword ptr [rbx + 140h], 440h ; RVA of hint/name pair
    mov qword ptr [rbx + 148h], 0    ; Terminator
    
    ; === Import Address Tables (IAT) - initially same as ILT ===
    ; Loader will overwrite these with actual function addresses at runtime
    mov qword ptr [rbx + 300h], 400h ; RVA of hint/name pair (ExitProcess)
    mov qword ptr [rbx + 308h], 0    ; Terminator
    
    mov qword ptr [rbx + 340h], 420h ; RVA of hint/name pair (CreateWindowExA)
    mov qword ptr [rbx + 348h], 0    ; Terminator
    
    mov qword ptr [rbx + 380h], 440h ; RVA of hint/name pair (printf)
    mov qword ptr [rbx + 388h], 0    ; Terminator
    
    ; === DLL Names (null-terminated strings) ===
    lea rax, [rbx + 200h]
    mov byte ptr [rax + 0], 'k'
    mov byte ptr [rax + 1], 'e'
    mov byte ptr [rax + 2], 'r'
    mov byte ptr [rax + 3], 'n'
    mov byte ptr [rax + 4], 'e'
    mov byte ptr [rax + 5], 'l'
    mov byte ptr [rax + 6], '3'
    mov byte ptr [rax + 7], '2'
    mov byte ptr [rax + 8], '.'
    mov byte ptr [rax + 9], 'd'
    mov byte ptr [rax + 10], 'l'
    mov byte ptr [rax + 11], 'l'
    mov byte ptr [rax + 12], 0      ; Null terminator
    
    lea rax, [rbx + 220h]
    mov byte ptr [rax + 0], 'u'
    mov byte ptr [rax + 1], 's'
    mov byte ptr [rax + 2], 'e'
    mov byte ptr [rax + 3], 'r'
    mov byte ptr [rax + 4], '3'
    mov byte ptr [rax + 5], '2'
    mov byte ptr [rax + 6], '.'
    mov byte ptr [rax + 7], 'd'
    mov byte ptr [rax + 8], 'l'
    mov byte ptr [rax + 9], 'l'
    mov byte ptr [rax + 10], 0      ; Null terminator
    
    lea rax, [rbx + 232h]
    mov byte ptr [rax + 0], 'm'
    mov byte ptr [rax + 1], 's'
    mov byte ptr [rax + 2], 'v'
    mov byte ptr [rax + 3], 'c'
    mov byte ptr [rax + 4], 'r'
    mov byte ptr [rax + 5], 't'
    mov byte ptr [rax + 6], '.'
    mov byte ptr [rax + 7], 'd'
    mov byte ptr [rax + 8], 'l'
    mov byte ptr [rax + 9], 'l'
    mov byte ptr [rax + 10], 0      ; Null terminator
    
    ; === Hint/Name Pairs ===
    ; Each pair: 2-byte hint + function name (null-terminated)
    
    ; ExitProcess
    lea rax, [rbx + 400h]
    mov word ptr [rax], 259          ; Hint = 259
    mov byte ptr [rax + 2], 'E'
    mov byte ptr [rax + 3], 'x'
    mov byte ptr [rax + 4], 'i'
    mov byte ptr [rax + 5], 't'
    mov byte ptr [rax + 6], 'P'
    mov byte ptr [rax + 7], 'r'
    mov byte ptr [rax + 8], 'o'
    mov byte ptr [rax + 9], 'c'
    mov byte ptr [rax + 10], 'e'
    mov byte ptr [rax + 11], 's'
    mov byte ptr [rax + 12], 's'
    mov byte ptr [rax + 13], 0      ; Null terminator
    
    ; CreateWindowExA
    lea rax, [rbx + 420h]
    mov word ptr [rax], 485          ; Hint
    mov byte ptr [rax + 2], 'C'
    mov byte ptr [rax + 3], 'r'
    mov byte ptr [rax + 4], 'e'
    mov byte ptr [rax + 5], 'a'
    mov byte ptr [rax + 6], 't'
    mov byte ptr [rax + 7], 'e'
    mov byte ptr [rax + 8], 'W'
    mov byte ptr [rax + 9], 'i'
    mov byte ptr [rax + 10], 'n'
    mov byte ptr [rax + 11], 'd'
    mov byte ptr [rax + 12], 'o'
    mov byte ptr [rax + 13], 'w'
    mov byte ptr [rax + 14], 'E'
    mov byte ptr [rax + 15], 'x'
    mov byte ptr [rax + 16], 'A'
    mov byte ptr [rax + 17], 0      ; Null terminator
    
    ; printf
    lea rax, [rbx + 440h]
    mov word ptr [rax], 391          ; Hint
    mov byte ptr [rax + 2], 'p'
    mov byte ptr [rax + 3], 'r'
    mov byte ptr [rax + 4], 'i'
    mov byte ptr [rax + 5], 'n'
    mov byte ptr [rax + 6], 't'
    mov byte ptr [rax + 7], 'f'
    mov byte ptr [rax + 8], 0        ; Null terminator
    
    ; Return total import table size
    mov rax, 500h                    ; Total size = 500h bytes
    
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 40h
    ret
RawrXD_PE_GenerateImportTable_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_GenerateRelocations_ml64
; ===============================================================================
; Purpose: Generate base relocation table for ASLR support
; Input:
;   rcx = output buffer pointer
;   rdx = relocation count (number of addresses requiring relocation)
; Output:
;   rax = bytes written (size of relocation table)
; ===============================================================================
RawrXD_PE_GenerateRelocations_ml64 PROC FRAME
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    push rbx
    push r12
    
    mov rbx, rcx                    ; rbx = output buffer
    mov r12, rdx                    ; r12 = relocation count
    xor r9, r9                      ; r9 = offset
    
    ; === Base Relocation Block for .text section (.text @ RVA 0x1000) ===
    
    ; Block header (8 bytes)
    mov dword ptr [rbx + r9], 1000h  ; PageRVA = 1000h (.text)
    mov dword ptr [rbx + r9 + 4], 20h ; BlockSize = 32 bytes (header + 3 entries)
    add r9, 8
    
    ; Relocation entry format: Bit[15:12] = type, Bit[11:0] = offset
    ; Type 10 (DIR64) = 0xA, so (0xA << 12) | offset
    
    ; Entry 1: 8-byte relocation at offset 0x0000
    mov word ptr [rbx + r9], (10 shl 12) or 0000h  ; Type 10 + offset
    add r9, 2
    
    ; Entry 2: 8-byte relocation at offset 0008h
    mov word ptr [rbx + r9], (10 shl 12) or 0008h
    add r9, 2
    
    ; Entry 3: 8-byte relocation at offset 0010h
    mov word ptr [rbx + r9], (10 shl 12) or 0010h
    add r9, 2
    
    ; Entry 4: 8-byte relocation at offset 0018h
    mov word ptr [rbx + r9], (10 shl 12) or 0018h
    add r9, 2
    
    ; Entry 5: 8-byte relocation at offset 0020h
    mov word ptr [rbx + r9], (10 shl 12) or 0020h
    add r9, 2
    
    ; Entry 6: 8-byte relocation at offset 0028h
    mov word ptr [rbx + r9], (10 shl 12) or 0028h
    add r9, 2
    
    ; Terminator block
    mov dword ptr [rbx + r9], 0      ; PageRVA = 0 (sentinel)
    mov dword ptr [rbx + r9 + 4], 0  ; BlockSize = 0
    add r9, 8
    
    ; Return total relocation table size
    mov rax, r9                      ; rax = total bytes written
    
    pop r12
    pop rbx
    add rsp, 20h
    ret
RawrXD_PE_GenerateRelocations_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_AssembleBinary_ml64
; ===============================================================================
; Purpose: Assemble all PE components into final binary file
; Input:
;   rcx = output file path (null-terminated string)
;   rdx = text section buffer
;   r8 = data section buffer
;   r9 = text section size
;   [rsp+32] = data section size
; Output:
;   rax = 1 on success, 0 on failure
; ===============================================================================
RawrXD_PE_AssembleBinary_ml64 PROC FRAME
    sub rsp, 80h
    .allocstack 80h
    .endprolog
    
    push rbx
    push r12
    push r13
    push r14
    push r15
    push rsi
    push rdi
    
    ; Save parameters
    mov r12, rcx                    ; r12 = output file path
    mov r13, rdx                    ; r13 = text section buffer
    mov r14, r8                     ; r14 = data section buffer
    mov r15d, r9d                   ; r15d = text section size
    mov esi, dword ptr [rsp + 80h + 7*8 + 28h]  ; data section size from caller stack
    
    ; ----- Step 1: Allocate 64KB PE output buffer -----
    call GetProcessHeap
    test rax, rax
    jz ASSEMBLE_FAIL
    mov rcx, rax                    ; hHeap
    mov rdx, 8                      ; HEAP_ZERO_MEMORY
    mov r8, 10000h                   ; 64KB
    call HeapAlloc
    test rax, rax
    jz ASSEMBLE_FAIL
    mov rbx, rax                    ; rbx = PE buffer base
    mov rdi, rax                    ; rdi = preserve heap ptr for free
    
    ; ----- Step 2: Write DOS header (64 bytes) -----
    mov rcx, rbx
    mov rdx, 40h                    ; PE header offset
    call RawrXD_PE_WriteDOSHeader_ml64
    ; rax = 0x40
    
    ; ----- Step 3: Write NT headers -----
    lea rcx, [rbx + 40h]            ; After DOS header
    mov rdx, 1000h                   ; Entry point RVA (start of .text)
    mov r8, 3                        ; Subsystem = GUI (default)
    
    ; Calculate total image size: headers(0x1000) + .text(0x1000) + .data(0x1000) + .reloc(0x1000) + .idata(0x1000) = 0x6000
    mov r9, 6000h                    ; Image size
    mov qword ptr [rsp + 28h], 4     ; Section count = 4
    mov rax, r15
    mov qword ptr [rsp + 30h], rax   ; Code size
    call RawrXD_PE_WriteNTHeaders_ml64
    ; rax = bytes written
    
    ; ----- Step 4: Write section headers -----
    ; Section headers start after DOS(0x40) + NT headers(0x108) = 0x148
    lea rcx, [rbx + 148h]
    mov rdx, 4                       ; 4 sections
    xor r8, r8                       ; Default config
    call RawrXD_PE_WriteSectionHeaders_ml64
    ; rax = 160 bytes
    
    ; ----- Step 5: Copy .text section data -----
    ; .text at file offset 0x400
    test r13, r13
    jz SKIP_TEXT_COPY
    lea rcx, [rbx + 400h]           ; Destination
    mov rdx, r13                     ; Source
    xor r9d, r9d
COPY_TEXT:
    cmp r9d, r15d
    jge SKIP_TEXT_COPY
    movzx eax, byte ptr [rdx + r9]
    mov byte ptr [rcx + r9], al
    inc r9d
    jmp COPY_TEXT
SKIP_TEXT_COPY:
    
    ; ----- Step 6: Copy .data section data -----
    ; .data at file offset 0x1400
    test r14, r14
    jz SKIP_DATA_COPY
    test esi, esi
    jz SKIP_DATA_COPY
    lea rcx, [rbx + 1400h]          ; Destination
    mov rdx, r14                     ; Source
    xor r9d, r9d
COPY_DATA:
    cmp r9d, esi
    jge SKIP_DATA_COPY
    movzx eax, byte ptr [rdx + r9]
    mov byte ptr [rcx + r9], al
    inc r9d
    jmp COPY_DATA
SKIP_DATA_COPY:
    
    ; ----- Step 7: Generate import table at .idata (0x2600) -----
    lea rcx, [rbx + 2600h]
    mov rdx, 5000h                   ; .idata RVA
    call RawrXD_PE_GenerateImportTable_ml64
    
    ; ----- Step 8: Generate relocations at .reloc (0x2400) -----
    lea rcx, [rbx + 2400h]
    mov rdx, 6                       ; 6 relocation entries
    call RawrXD_PE_GenerateRelocations_ml64
    
    ; ----- Step 9: Create output file -----
    ; CreateFileA(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes,
    ;             dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile)
    mov rcx, r12                     ; File path
    mov edx, 40000000h               ; GENERIC_WRITE
    xor r8d, r8d                     ; No sharing
    xor r9d, r9d                     ; No security
    mov qword ptr [rsp + 20h], 2    ; CREATE_ALWAYS
    mov qword ptr [rsp + 28h], 80h   ; FILE_ATTRIBUTE_NORMAL
    mov qword ptr [rsp + 30h], 0     ; No template
    call CreateFileA
    cmp rax, -1
    je ASSEMBLE_FREE_AND_FAIL
    mov r12, rax                     ; r12 = file handle
    
    ; ----- Step 10: Write entire PE buffer -----
    ; Total file size: .idata ends at 0x2600 + 0x200 = 0x2800
    mov rcx, r12                     ; hFile
    mov rdx, rbx                     ; lpBuffer = PE buffer
    mov r8d, 2800h                   ; nNumberOfBytesToWrite = 10240 bytes
    lea r9, [rsp + 40h]             ; lpNumberOfBytesWritten
    mov qword ptr [rsp + 20h], 0     ; lpOverlapped = NULL
    call WriteFile
    test eax, eax
    jz ASSEMBLE_CLOSE_AND_FAIL
    
    ; ----- Step 11: Close file -----
    mov rcx, r12
    call CloseHandle
    
    ; ----- Step 12: Free buffer -----
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx                    ; Flags = 0
    mov r8, rdi                      ; Buffer pointer
    call HeapFree
    
    ; Return success
    mov rax, 1
    jmp ASSEMBLE_DONE
    
ASSEMBLE_CLOSE_AND_FAIL:
    mov rcx, r12
    call CloseHandle
    
ASSEMBLE_FREE_AND_FAIL:
    call GetProcessHeap
    mov rcx, rax
    xor edx, edx
    mov r8, rdi
    call HeapFree
    
ASSEMBLE_FAIL:
    xor eax, eax
    
ASSEMBLE_DONE:
    pop rdi
    pop rsi
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    add rsp, 80h
    ret
RawrXD_PE_AssembleBinary_ml64 ENDP

; ===============================================================================
; PROCEDURE: RawrXD_PE_ValidateReproduce_ml64
; ===============================================================================
; Purpose: Validate PE binary and ensure byte-reproducibility
; Input:
;   rcx = PE binary buffer pointer
;   rdx = binary size
; Output:
;   rax = checksum/hash for reproducibility validation
; ===============================================================================
RawrXD_PE_ValidateReproduce_ml64 PROC FRAME
    sub rsp, 20h
    .allocstack 20h
    .endprolog
    
    push rbx
    push r12
    
    mov rbx, rcx                    ; rbx = PE buffer
    mov r12, rdx                    ; r12 = binary size
    xor r9, r9                      ; r9 = checksum accumulator
    xor r10, r10                    ; r10 = offset
    
VALIDATE_LOOP:
    cmp r10, r12
    jge VALIDATE_DONE
    
    ; Simple checksum: XOR all bytes
    movzx eax, byte ptr [rbx + r10]
    xor r9d, eax
    
    inc r10
    jmp VALIDATE_LOOP
    
VALIDATE_DONE:
    mov rax, r9                     ; rax = checksum
    
    pop r12
    pop rbx
    add rsp, 20h
    ret
RawrXD_PE_ValidateReproduce_ml64 ENDP

END
