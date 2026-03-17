; -----------------------------------------------------------------------------
; pe_generator_example.asm
; Comprehensive usage demos for RawrXD PE Generator & Encoder (MASM x64)
; Demos:
;   1) Minimal PE creation
;   2) XOR-encoded payload
;   3) RC4-encoded payload
;   4) AES-128 encrypted payload
;   5) ChaCha20 encrypted payload
;   6) Polymorphic stub generation
; -----------------------------------------------------------------------------

option casemap:none

; -----------------------------------------------------------------------------
; Imports from rawrxd_pe_generator_encoder.obj
; -----------------------------------------------------------------------------
EXTERN PeGenInitialize:PROC
EXTERN PeGenCreateHeaders:PROC
EXTERN PeGenAddSection:PROC
EXTERN PeGenWriteToFile:PROC
EXTERN PeGenCleanup:PROC
EXTERN EncoderInitialize:PROC
EXTERN EncoderXOR:PROC
EXTERN EncoderRC4:PROC
EXTERN EncoderAES:PROC
EXTERN EncoderChaCha20:PROC
EXTERN EncoderPolymorphic:PROC
EXTERN HashStringFNV1a:PROC
EXTERN GenerateRandomBytes:PROC

; -----------------------------------------------------------------------------
; Basic constants and structs (mirrors generator header)
; -----------------------------------------------------------------------------
SUBSYSTEM_CONSOLE equ 3
SEC_CODE        equ 200000000h
SEC_READ        equ 400000000h
SEC_WRITE       equ 800000000h
SEC_EXECUTE     equ 200000000h

ENC_XOR         equ 0
ENC_RC4         equ 1
ENC_AES128      equ 2
ENC_CHACHA20    equ 4
ENC_POLYMORPHIC equ 5

PE_GEN_CONTEXT STRUCT
    pOutputBuffer   QWORD ?
    dwOutputSize    DWORD ?
    dwMaxSize       DWORD ?
    pDosHeader      QWORD ?
    pNtHeaders      QWORD ?
    pFileHeader     QWORD ?
    pOptionalHeader QWORD ?
    pSectionHeaders QWORD ?
    dwNumSections   DWORD ?
    dwImageBase     QWORD ?
    dwEntryPointRVA DWORD ?
    dwSectionAlign  DWORD ?
    dwFileAlign     DWORD ?
    dwHeadersSize   DWORD ?
    dwImageSize     DWORD ?
    bEncodeSections BYTE ?
    bEncodeImports  BYTE ?
    bPolyStub       BYTE ?
    bEncryptRes     BYTE ?
    dwEncodingType  DWORD ?
    bEncryptionKey  BYTE 32 dup(?)
    dwSubsystem     DWORD ?
    dwCharacteristics WORD ?
    bIsDLL          BYTE ?
PE_GEN_CONTEXT ENDS

SECTION_ENTRY STRUCT
    szName        BYTE 8 dup(?)
    dwVirtualSize DWORD ?
    dwVirtualAddr DWORD ?
    dwRawSize     DWORD ?
    dwRawAddr     DWORD ?
    dwRelocAddr   DWORD ?
    dwLineNums    DWORD ?
    dwRelocCount  WORD ?
    dwLineCount   WORD ?
    dwChars       DWORD ?
    pRawData      QWORD ?
    bEncoded      BYTE ?
    bEncrypted    BYTE ?
SECTION_ENTRY ENDS

ENCODER_STATE STRUCT
    pInputBuffer  QWORD ?
    pOutputBuffer QWORD ?
    dwBufferSize  DWORD ?
    dwKeySchedule QWORD ?
    bKey          BYTE 32 dup(?)
    bIV           BYTE 16 dup(?)
    dwRounds      DWORD ?
    dwAlgorithm   DWORD ?
ENCODER_STATE ENDS

; -----------------------------------------------------------------------------
; Data
; -----------------------------------------------------------------------------
.data
align 16
BasicPayload BYTE 0B8h,1,0,0,0,0C3h            ; mov eax,1 / ret
PayloadXOR   BYTE 090h,090h,090h,090h,090h,090h,090h,090h
PayloadRC4   BYTE 01h,02h,03h,04h,05h,06h,07h,08h
PayloadAES   BYTE 16 dup(0C3h)                 ; simple pattern
PayloadChaCha BYTE 32 dup(055h)

XorKey       BYTE 0DEh,0ADh,0BEh,0EFh
RC4Key       BYTE "SampleRC4Key!"
AESKey128    BYTE 16 dup(0A5h)
ChaChaKey    BYTE 32 dup(077h)
ChaChaNonce  BYTE 0AAh,0BBh,0CCh,0DDh,0EEh,0FFh,11h,22h,33h

OutFile1 BYTE "demo_basic.exe",0
OutFile2 BYTE "demo_xor.exe",0
OutFile3 BYTE "demo_rc4.exe",0
OutFile4 BYTE "demo_aes.exe",0
OutFile5 BYTE "demo_chacha.exe",0
OutFile6 BYTE "demo_poly.exe",0

align 8
ctx     PE_GEN_CONTEXT <>
sect    SECTION_ENTRY <>
enc     ENCODER_STATE <>

; -----------------------------------------------------------------------------
; Code
; -----------------------------------------------------------------------------
.code

Main PROC
    sub rsp, 40h                     ; shadow space + alignment
    call DemoBasic
    call DemoXor
    call DemoRc4
    call DemoAes
    call DemoChaCha
    call DemoPoly
    add rsp, 40h
    mov eax, 0
    ret
Main ENDP

; -----------------------------------------------------------------------------
DemoBasic PROC
    ; Init context
    lea rcx, ctx
    mov edx, 200000h
    xor r8d, r8d
    call PeGenInitialize
    test rax, rax
    jz @exit

    ; Create headers
    lea rcx, ctx
    mov rdx, 140000000h
    mov r8d, SUBSYSTEM_CONSOLE
    call PeGenCreateHeaders

    ; Section setup
    lea rdi, sect
    mov rcx, SIZEOF SECTION_ENTRY
    xor rax, rax
    rep stosb
    mov DWORD PTR sect.szName, 'xet.'
    mov sect.dwVirtualSize, 1000h
    mov sect.dwRawSize, SIZEOF BasicPayload
    mov sect.dwChars, SEC_CODE or SEC_EXECUTE or SEC_READ
    lea rax, BasicPayload
    mov sect.pRawData, rax

    ; Add + write
    lea rcx, ctx
    lea rdx, sect
    call PeGenAddSection
    lea rcx, ctx
    lea rdx, OutFile1
    call PeGenWriteToFile

@exit:
    lea rcx, ctx
    call PeGenCleanup
    ret
DemoBasic ENDP

; -----------------------------------------------------------------------------
DemoXor PROC
    lea rcx, ctx
    mov edx, 200000h
    xor r8d, r8d
    call PeGenInitialize
    test rax, rax
    jz @exit

    lea rcx, ctx
    mov rdx, 140000000h
    mov r8d, SUBSYSTEM_CONSOLE
    call PeGenCreateHeaders

    ; Encode payload in-place
    lea rcx, PayloadXOR
    mov edx, LENGTHOF PayloadXOR
    lea r8, XorKey
    mov r9d, LENGTHOF XorKey
    call EncoderXOR

    lea rdi, sect
    mov rcx, SIZEOF SECTION_ENTRY
    xor rax, rax
    rep stosb
    mov DWORD PTR sect.szName, 'xet.'
    mov sect.dwVirtualSize, 1000h
    mov sect.dwRawSize, LENGTHOF PayloadXOR
    mov sect.dwChars, SEC_CODE or SEC_EXECUTE or SEC_READ
    lea rax, PayloadXOR
    mov sect.pRawData, rax

    lea rcx, ctx
    lea rdx, sect
    call PeGenAddSection
    lea rcx, ctx
    lea rdx, OutFile2
    call PeGenWriteToFile

@exit:
    lea rcx, ctx
    call PeGenCleanup
    ret
DemoXor ENDP

; -----------------------------------------------------------------------------
DemoRc4 PROC
    lea rcx, ctx
    mov edx, 200000h
    xor r8d, r8d
    call PeGenInitialize
    test rax, rax
    jz @exit

    lea rcx, ctx
    mov rdx, 140000000h
    mov r8d, SUBSYSTEM_CONSOLE
    call PeGenCreateHeaders

    lea rcx, PayloadRC4
    mov edx, LENGTHOF PayloadRC4
    lea r8, RC4Key
    mov r9d, LENGTHOF RC4Key
    call EncoderRC4

    lea rdi, sect
    mov rcx, SIZEOF SECTION_ENTRY
    xor rax, rax
    rep stosb
    mov DWORD PTR sect.szName, 'xet.'
    mov sect.dwVirtualSize, 1000h
    mov sect.dwRawSize, LENGTHOF PayloadRC4
    mov sect.dwChars, SEC_CODE or SEC_EXECUTE or SEC_READ
    lea rax, PayloadRC4
    mov sect.pRawData, rax

    lea rcx, ctx
    lea rdx, sect
    call PeGenAddSection
    lea rcx, ctx
    lea rdx, OutFile3
    call PeGenWriteToFile

@exit:
    lea rcx, ctx
    call PeGenCleanup
    ret
DemoRc4 ENDP

; -----------------------------------------------------------------------------
DemoAes PROC
    lea rcx, ctx
    mov edx, 200000h
    xor r8d, r8d
    call PeGenInitialize
    test rax, rax
    jz @exit

    lea rcx, ctx
    mov rdx, 140000000h
    mov r8d, SUBSYSTEM_CONSOLE
    call PeGenCreateHeaders

    ; AES-encode payload in-place (encrypt flag = 1)
    lea rcx, PayloadAES
    mov edx, LENGTHOF PayloadAES
    lea r8, AESKey128
    mov r9b, 1
    call EncoderAES

    lea rdi, sect
    mov rcx, SIZEOF SECTION_ENTRY
    xor rax, rax
    rep stosb
    mov DWORD PTR sect.szName, 'xet.'
    mov sect.dwVirtualSize, 1000h
    mov sect.dwRawSize, LENGTHOF PayloadAES
    mov sect.dwChars, SEC_CODE or SEC_EXECUTE or SEC_READ
    lea rax, PayloadAES
    mov sect.pRawData, rax

    lea rcx, ctx
    lea rdx, sect
    call PeGenAddSection
    lea rcx, ctx
    lea rdx, OutFile4
    call PeGenWriteToFile

@exit:
    lea rcx, ctx
    call PeGenCleanup
    ret
DemoAes ENDP

; -----------------------------------------------------------------------------
DemoChaCha PROC
    lea rcx, ctx
    mov edx, 200000h
    xor r8d, r8d
    call PeGenInitialize
    test rax, rax
    jz @exit

    lea rcx, ctx
    mov rdx, 140000000h
    mov r8d, SUBSYSTEM_CONSOLE
    call PeGenCreateHeaders

    lea rcx, PayloadChaCha
    mov edx, LENGTHOF PayloadChaCha
    lea r8, ChaChaKey
    lea r9, ChaChaNonce
    call EncoderChaCha20

    lea rdi, sect
    mov rcx, SIZEOF SECTION_ENTRY
    xor rax, rax
    rep stosb
    mov DWORD PTR sect.szName, 'xet.'
    mov sect.dwVirtualSize, 1000h
    mov sect.dwRawSize, LENGTHOF PayloadChaCha
    mov sect.dwChars, SEC_CODE or SEC_EXECUTE or SEC_READ
    lea rax, PayloadChaCha
    mov sect.pRawData, rax

    lea rcx, ctx
    lea rdx, sect
    call PeGenAddSection
    lea rcx, ctx
    lea rdx, OutFile5
    call PeGenWriteToFile

@exit:
    lea rcx, ctx
    call PeGenCleanup
    ret
DemoChaCha ENDP

; -----------------------------------------------------------------------------
DemoPoly PROC
    ; Demonstrates polymorphic encoder usage only; PE writing is omitted
    lea rcx, PayloadXOR
    mov rdx, LENGTHOF PayloadXOR
    lea r8, XorKey
    mov r9d, LENGTHOF XorKey
    call EncoderInitialize          ; example init (algorithm not used directly)

    lea rcx, ctx                    ; reuse context as scratch
    lea rdx, PayloadXOR
    mov r8d, LENGTHOF PayloadXOR
    call EncoderPolymorphic         ; returns encoded buffer in RAX, size in RDX

    ; In a full pipeline the returned buffer would be placed into a PE section.
    ret
DemoPoly ENDP

END Main
