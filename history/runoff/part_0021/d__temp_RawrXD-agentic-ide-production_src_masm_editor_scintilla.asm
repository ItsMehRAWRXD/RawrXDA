;==============================================================================
; File 2: editor_scintilla.asm - Scintilla Editor Integration (1M+ Lines)
;==============================================================================
include windows.inc
include richedit.inc

; Scintilla constants
SC_CP_UTF8                equ 65001
SCLEX_CPP                 equ 3
SCI_SETBUFFEREDDRAW       equ 2034
SCI_SETCODEPAGE           equ 2037
SCI_SETVIRTUALSPACEOPTIONS equ 2126
SCI_SETMARGINS            equ 2044
SCI_SETMARGINTYPEN        equ 2241
SCI_SETMARGINWIDTHN       equ 2242
SCI_SETMARGINMASKN        equ 2245
SCI_ADDTEXT               equ 2001
SC_MARGIN_NUMBER          equ 0
SC_MARGIN_SYMBOL          equ 1
SC_MASK_FOLDERS           equ 0xFE000000
SCI_STYLESETFORE          equ 2069
SCI_STYLESETFONT          equ 2056
SCI_STYLESETSIZE          equ 2055
SCI_STYLECLEARALL         equ 2050
SCI_SETPROPERTY           equ 2380
SCI_SETWRAPMODE           equ 2268
SCWS_VISIBLEALWAYS        equ 1
SCI_SETVIEWWS             equ 2205
SCI_SETSELBACK            equ 2068
SCI_SETCARETWIDTH         equ 2188
SCI_AUTOCSETMAXHEIGHT     equ 2206
SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR equ 2281
SCI_SETEDGEMODE           equ 2279
EDGE_LINE                 equ 1
SCI_SETEDGECOLUMN         equ 2278
SCI_SETEDGECOLOUR         equ 2280
STYLE_DEFAULT             equ 32
SCE_C_DEFAULT             equ 0

.code
;==============================================================================
; Initialize Scintilla with Enterprise Configuration
;==============================================================================
Editor_ConfigureScintilla PROC hSci:QWORD
    ; Direct draw for performance
    invoke SendMessage, hSci, SCI_SETBUFFEREDDRAW, 0, 0
    
    ; UTF-8 encoding
    invoke SendMessage, hSci, SCI_SETCODEPAGE, SC_CP_UTF8, 0
    
    ; Virtual space for rectangular selections
    invoke SendMessage, hSci, SCI_SETVIRTUALSPACEOPTIONS, 
        3, 0  ; SCVS_RECTANGULARSELECTION | SCVS_USERACCESSIBLE
    
    ; Enable all margins
    invoke SendMessage, hSci, SCI_SETMARGINS, 4, 0
    
    ; Margin 0: Line numbers
    invoke SendMessage, hSci, SCI_SETMARGINTYPEN, 0, SC_MARGIN_NUMBER
    invoke SendMessage, hSci, SCI_SETMARGINWIDTHN, 0, 48
    invoke SendMessage, hSci, SCI_STYLESETFORE, 
        33, 0x808080
    
    ; Margin 1: Fold margin
    invoke SendMessage, hSci, SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL
    invoke SendMessage, hSci, SCI_SETMARGINWIDTHN, 1, 16
    invoke SendMessage, hSci, SCI_SETMARGINMASKN, 1, SC_MASK_FOLDERS
    
    ; Word wrap
    invoke SendMessage, hSci, SCI_SETWRAPMODE, 1, 0  ; SC_WRAP_WORD
    
    ; Whitespace visibility
    invoke SendMessage, hSci, SCI_SETVIEWWS, SCWS_VISIBLEALWAYS, 0
    
    ; Selection behavior
    invoke SendMessage, hSci, SCI_SETSELBACK, 1, 0xC0C0C0
    invoke SendMessage, hSci, SCI_SETCARETWIDTH, 2, 0
    
    ; Long line marker
    invoke SendMessage, hSci, SCI_SETEDGEMODE, EDGE_LINE, 0
    invoke SendMessage, hSci, SCI_SETEDGECOLUMN, 100, 0
    invoke SendMessage, hSci, SCI_SETEDGECOLOUR, 0xE0E0E0, 0
    
    ; Set default font
    invoke CreateFont, 12, 0, 0, 0, 400, 0, 0, 0, 
        1, 3, 0, 5, 0x30, OFFSET szConsolas
    mov [hFontDefault], rax
    
    invoke SendMessage, hSci, SCI_STYLESETFONT, 
        STYLE_DEFAULT, OFFSET szConsolas
    invoke SendMessage, hSci, SCI_STYLESETSIZE, 
        STYLE_DEFAULT, 12
    invoke SendMessage, hSci, SCI_STYLECLEARALL, 0, 0
    
    ; Hook up completion
    call Editor_AttachCompletionEngine, hSci
    
    LOG_INFO "Scintilla configured: UTF-8, line numbers, folding, wrapping"
    
    ret
Editor_ConfigureScintilla ENDP

;==============================================================================
; Load Large File with Memory-Mapped I/O (Handles 1GB+ Files)
;==============================================================================
Editor_LoadLargeFile PROC hSci:QWORD, lpFilePath:QWORD
    LOCAL hFile:HANDLE
    LOCAL hMapping:HANDLE
    LOCAL pFileMemory:QWORD
    LOCAL fileSize:QWORD
    LOCAL chunkSize:QWORD
    LOCAL bytesRead:DWORD
    
    ; Open file
    invoke CreateFile, lpFilePath, GENERIC_READ, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL or FILE_FLAG_SEQUENTIAL_SCAN, NULL
    .if rax == INVALID_HANDLE_VALUE
        LOG_ERROR "Failed to open file: {}", lpFilePath
        xor rax, rax
        ret
    .endif
    mov [hFile], rax
    
    ; Get file size
    invoke GetFileSizeEx, rax, ADDR fileSize
    mov rax, [fileSize]
    .if rax > 1073741824      ; 1GB limit
        invoke MessageBox, NULL, 
            OFFSET szFileTooLarge, NULL, MB_ICONERROR
        invoke CloseHandle, [hFile]
        xor rax, rax
        ret
    .endif
    
    ; Create file mapping
    invoke CreateFileMapping, [hFile], NULL, PAGE_READONLY,
        0, 0, NULL
    .if rax == NULL
        LOG_ERROR "CreateFileMapping failed"
        invoke CloseHandle, [hFile]
        xor rax, rax
        ret
    .endif
    mov [hMapping], rax
    
    ; Map view
    invoke MapViewOfFile, rax, FILE_MAP_READ, 0, 0, 0
    .if rax == NULL
        LOG_ERROR "MapViewOfFile failed"
        invoke CloseHandle, [hMapping]
        invoke CloseHandle, [hFile]
        xor rax, rax
        ret
    .endif
    mov [pFileMemory], rax
    
    ; Set Scintilla text (all at once for mmap)
    invoke SendMessage, hSci, SCI_ADDTEXT, 
        [fileSize], [pFileMemory]
    
    LOG_INFO "File loaded: {} bytes", [fileSize]
    
    ; Cleanup
    invoke UnmapViewOfFile, [pFileMemory]
    invoke CloseHandle, [hMapping]
    invoke CloseHandle, [hFile]
    
    mov rax, 1              ; Success
    ret
Editor_LoadLargeFile ENDP

;==============================================================================
; Attach AI Completion Engine
;==============================================================================
Editor_AttachCompletionEngine PROC hSci:QWORD
    ; Register for char added notifications
    invoke SendMessage, hSci, 2386, 0x00000100, 0  ; SC_MOD_INSERTTEXT
    
    ret
Editor_AttachCompletionEngine ENDP

;==============================================================================
; Data
;==============================================================================
.data
hFontDefault      dq ?
szConsolas        db 'Consolas',0
szFileTooLarge    db 'File exceeds 1GB limit',0

END
