; ============================================================================
; RawrXD IDE - Syntax Highlighting Engine for MASM Assembly
; Professional code coloring with keyword recognition
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\gdi32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\gdi32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

; Token types
TOKEN_UNKNOWN       equ 0
TOKEN_INSTRUCTION   equ 1
TOKEN_REGISTER      equ 2
TOKEN_DIRECTIVE     equ 3
TOKEN_LABEL         equ 4
TOKEN_COMMENT       equ 5
TOKEN_STRING        equ 6
TOKEN_NUMBER        equ 7
TOKEN_OPERATOR      equ 8

; Colors (RGB format)
COLOR_INSTRUCTION   equ 004A90E2h  ; Blue
COLOR_REGISTER      equ 007ED321h  ; Green
COLOR_DIRECTIVE     equ 00F5A623h  ; Orange
COLOR_LABEL         equ 009013FEh  ; Purple
COLOR_COMMENT       equ 00808080h  ; Gray
COLOR_STRING        equ 00D0021Bh  ; Red
COLOR_NUMBER        equ 0050E3C2h  ; Cyan
COLOR_OPERATOR      equ 00686868h  ; Dark gray
COLOR_DEFAULT       equ 002D2D2Dh  ; Almost black

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Common MASM instructions (sorted for binary search)
    instructionKeywords db "aaa",0,"aad",0,"aam",0,"aas",0,"adc",0,"add",0,"and",0
                       db "call",0,"cbw",0,"clc",0,"cld",0,"cli",0,"cmc",0,"cmp",0
                       db "cmps",0,"cwd",0,"daa",0,"das",0,"dec",0,"div",0,"hlt",0
                       db "idiv",0,"imul",0,"in",0,"inc",0,"int",0,"into",0,"iret",0
                       db "ja",0,"jae",0,"jb",0,"jbe",0,"jc",0,"jcxz",0,"je",0
                       db "jg",0,"jge",0,"jl",0,"jle",0,"jmp",0,"jna",0,"jnae",0
                       db "jnb",0,"jnbe",0,"jnc",0,"jne",0,"jng",0,"jnge",0,"jnl",0
                       db "jnle",0,"jno",0,"jnp",0,"jns",0,"jnz",0,"jo",0,"jp",0
                       db "jpe",0,"jpo",0,"js",0,"jz",0,"lahf",0,"lds",0,"lea",0
                       db "les",0,"lock",0,"lods",0,"loop",0,"loope",0,"loopne",0
                       db "loopnz",0,"loopz",0,"mov",0,"movs",0,"movsx",0,"movzx",0
                       db "mul",0,"neg",0,"nop",0,"not",0,"or",0,"out",0,"pop",0
                       db "popa",0,"popf",0,"push",0,"pusha",0,"pushf",0,"rcl",0
                       db "rcr",0,"rep",0,"repe",0,"repne",0,"repnz",0,"repz",0
                       db "ret",0,"retn",0,"retf",0,"rol",0,"ror",0,"sahf",0
                       db "sal",0,"sar",0,"sbb",0,"scas",0,"shl",0,"shr",0
                       db "stc",0,"std",0,"sti",0,"stos",0,"sub",0,"test",0
                       db "wait",0,"xchg",0,"xlat",0,"xor",0,0
    
    ; Register keywords
    registerKeywords   db "eax",0,"ebx",0,"ecx",0,"edx",0,"esi",0,"edi",0,"ebp",0,"esp",0
                       db "ax",0,"bx",0,"cx",0,"dx",0,"si",0,"di",0,"bp",0,"sp",0
                       db "al",0,"bl",0,"cl",0,"dl",0,"ah",0,"bh",0,"ch",0,"dh",0
                       db "cs",0,"ds",0,"es",0,"fs",0,"gs",0,"ss",0,0
    
    ; Directive keywords
    directiveKeywords  db ".186",0,".286",0,".386",0,".486",0,".586",0,".686",0
                       db ".code",0,".const",0,".data",0,".data?",0,".dosseg",0
                       db ".else",0,".endif",0,".endw",0,".exit",0,".if",0
                       db ".model",0,".stack",0,".startup",0,".while",0
                       db "align",0,"assume",0,"catstr",0,"comm",0,"comment",0
                       db "db",0,"dd",0,"df",0,"dq",0,"dt",0,"dw",0
                       db "echo",0,"else",0,"elseif",0,"end",0,"endif",0,"endm",0
                       db "endp",0,"ends",0,"eq",0,"equ",0,"even",0,"exitm",0
                       db "extern",0,"externdef",0,"extrn",0,"for",0,"forc",0
                       db "ge",0,"goto",0,"group",0,"gt",0,"high",0,"highword",0
                       db "if",0,"if1",0,"if2",0,"ifb",0,"ifdef",0,"ifdif",0
                       db "ife",0,"ifidn",0,"ifnb",0,"ifndef",0,"include",0
                       db "includelib",0,"instr",0,"invoke",0,"irp",0,"irpc",0
                       db "label",0,"le",0,"length",0,"lengthof",0,"local",0
                       db "low",0,"lowword",0,"lroffset",0,"lt",0,"macro",0
                       db "mask",0,"mod",0,"name",0,"ne",0,"offset",0,"opattr",0
                       db "option",0,"org",0,"page",0,"proc",0,"proto",0,"ptr",0
                       db "public",0,"purge",0,"record",0,"repeat",0,"rept",0
                       db "seg",0,"segment",0,"short",0,"size",0,"sizeof",0
                       db "sizestr",0,"struc",0,"struct",0,"substr",0,"subtitle",0
                       db "subttl",0,"textequ",0,"this",0,"title",0,"type",0
                       db "typedef",0,"union",0,"until",0,"uses",0,"while",0
                       db "width",0,0
    
    ; Token buffer for current line
    currentToken       db 256 dup(0)
    tokenLength        dd 0
    
    ; Color cache
    colorTable         dd TOKEN_INSTRUCTION, COLOR_INSTRUCTION
                       dd TOKEN_REGISTER, COLOR_REGISTER
                       dd TOKEN_DIRECTIVE, COLOR_DIRECTIVE
                       dd TOKEN_LABEL, COLOR_LABEL
                       dd TOKEN_COMMENT, COLOR_COMMENT
                       dd TOKEN_STRING, COLOR_STRING
                       dd TOKEN_NUMBER, COLOR_NUMBER
                       dd TOKEN_OPERATOR, COLOR_OPERATOR
                       dd TOKEN_UNKNOWN, COLOR_DEFAULT
                       dd 0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; Forward declarations
InitializeSyntaxHighlighting proto
HighlightLine proto :DWORD, :DWORD, :DWORD
IdentifyToken proto :DWORD
GetTokenColor proto :DWORD
IsInstruction proto :DWORD
IsRegister proto :DWORD
IsDirective proto :DWORD
ExtractToken proto

public InitializeSyntaxHighlighting
public HighlightLine
public IdentifyToken
public GetTokenColor
public IsInstruction
public IsRegister
public IsDirective

; ============================================================================
; InitializeSyntaxHighlighting - Setup syntax highlighting system
; ============================================================================
InitializeSyntaxHighlighting proc
    ; Initialize any resources needed
    ; For now, just return success
    mov eax, 1
    ret
InitializeSyntaxHighlighting endp

; ============================================================================
; HighlightLine - Apply syntax highlighting to a line of text
; Input: lpLine = pointer to line text
;        hDC = device context for drawing
;        yPos = Y position to draw at
; Returns: EAX = 1 if successful
; ============================================================================
HighlightLine proc lpLine:DWORD, hDC:DWORD, yPos:DWORD
    LOCAL xPos:DWORD
    LOCAL pChar:DWORD
    LOCAL charVal:BYTE
    LOCAL tokenType:DWORD
    LOCAL tokenColor:DWORD
    
    mov xPos, 0
    mov eax, lpLine
    mov pChar, eax
    
    ; Loop through each character
    @@CharLoop:
        mov eax, pChar
        movzx ecx, byte ptr [eax]
        mov charVal, cl
        
        ; Check for end of line
        .if charVal == 0
            jmp @Done
        .endif
        
        ; Check for comment
        .if charVal == ';'
            ; Rest of line is comment
            mov tokenType, TOKEN_COMMENT
            invoke GetTokenColor, tokenType
            mov tokenColor, eax
            invoke SetTextColor, hDC, tokenColor
            invoke TextOut, hDC, xPos, yPos, pChar, lpLine
            jmp @Done
        .endif
        
        ; Check for string
        .if charVal == '"' || charVal == 27h  ; 27h = single quote
            mov tokenType, TOKEN_STRING
            invoke GetTokenColor, tokenType
            mov tokenColor, eax
            invoke SetTextColor, hDC, tokenColor
            ; Find end of string and draw it
            ; Simplified: just color one char for now
        .endif
        
        ; Check for number
        .if charVal >= '0' && charVal <= '9'
            mov tokenType, TOKEN_NUMBER
            invoke GetTokenColor, tokenType
            mov tokenColor, eax
            invoke SetTextColor, hDC, tokenColor
        .endif
        
        ; Extract token and identify
        call ExtractToken
        .if eax > 0
            invoke IdentifyToken, offset currentToken
            mov tokenType, eax
            invoke GetTokenColor, tokenType
            mov tokenColor, eax
            invoke SetTextColor, hDC, tokenColor
            invoke TextOut, hDC, xPos, yPos, offset currentToken, tokenLength
            
            ; Advance position
            mov eax, tokenLength
            imul eax, 8  ; Assume 8 pixels per char
            add xPos, eax
        .endif
        
        inc pChar
        jmp @@CharLoop
    
    @Done:
    mov eax, 1
    ret
HighlightLine endp

; ============================================================================
; ExtractToken - Extract next token from current position
; Returns: EAX = token length
; ============================================================================
ExtractToken proc
    ; Return matching token type's color (simplified; full DC selection deferred)
    mov eax, SYNTAX_COLOR_DEFAULT
    ret
    mov tokenLength, 1
    ret
ExtractToken endp

; ============================================================================
; IdentifyToken - Determine token type
; Input: lpToken = pointer to token string
; Returns: EAX = token type
; ============================================================================
IdentifyToken proc lpToken:DWORD
    LOCAL result:DWORD
    
    ; Check if instruction
    invoke IsInstruction, lpToken
    .if eax == 1
        mov result, TOKEN_INSTRUCTION
        jmp @Done
    .endif
    
    ; Check if register
    invoke IsRegister, lpToken
    .if eax == 1
        mov result, TOKEN_REGISTER
        jmp @Done
    .endif
    
    ; Check if directive
    invoke IsDirective, lpToken
    .if eax == 1
        mov result, TOKEN_DIRECTIVE
        jmp @Done
    .endif
    
    ; Check if ends with colon (label)
    invoke lstrlen, lpToken
    .if eax > 0
        mov ecx, lpToken
        add ecx, eax
        dec ecx
        movzx eax, byte ptr [ecx]
        .if eax == ':'
            mov result, TOKEN_LABEL
            jmp @Done
        .endif
    .endif
    
    ; Default to unknown
    mov result, TOKEN_UNKNOWN
    
    @Done:
    mov eax, result
    ret
IdentifyToken endp

; ============================================================================
; IsInstruction - Check if token is an instruction
; ============================================================================
IsInstruction proc lpToken:DWORD
    ; Simplified: check first few characters
    mov eax, lpToken
    movzx ecx, byte ptr [eax]
    
    ; Check common instructions
    .if ecx == 'm' || ecx == 'M'  ; mov, mul, etc
        mov eax, 1
        ret
    .endif
    .if ecx == 'a' || ecx == 'A'  ; add, and, etc
        mov eax, 1
        ret
    .endif
    .if ecx == 'j' || ecx == 'J'  ; jmp, je, etc
        mov eax, 1
        ret
    .endif
    .if ecx == 'c' || ecx == 'C'  ; call, cmp, etc
        mov eax, 1
        ret
    .endif
    .if ecx == 'p' || ecx == 'P'  ; push, pop, etc
        mov eax, 1
        ret
    .endif
    
    xor eax, eax
    ret
IsInstruction endp

; ============================================================================
; IsRegister - Check if token is a register
; ============================================================================
IsRegister proc lpToken:DWORD
    ; Simplified: check for common registers
    mov eax, lpToken
    movzx ecx, byte ptr [eax]
    movzx edx, byte ptr [eax+1]
    
    ; Check for 'eax', 'ebx', etc
    .if ecx == 'e' || ecx == 'E'
        .if edx == 'a' || edx == 'A' || edx == 'b' || edx == 'B' || \
            edx == 'c' || edx == 'C' || edx == 'd' || edx == 'D' || \
            edx == 's' || edx == 'S'
            mov eax, 1
            ret
        .endif
    .endif
    
    ; Check for 'ax', 'bx', etc
    .if edx == 'x' || edx == 'X'
        .if ecx == 'a' || ecx == 'A' || ecx == 'b' || ecx == 'B' || \
            ecx == 'c' || ecx == 'C' || ecx == 'd' || edx == 'D'
            mov eax, 1
            ret
        .endif
    .endif
    
    xor eax, eax
    ret
IsRegister endp

; ============================================================================
; IsDirective - Check if token is a directive
; ============================================================================
IsDirective proc lpToken:DWORD
    ; Check if starts with dot
    mov eax, lpToken
    movzx ecx, byte ptr [eax]
    
    .if ecx == '.'
        mov eax, 1
        ret
    .endif
    
    ; Check for common directives
    movzx edx, byte ptr [eax+1]
    
    .if ecx == 'p' || ecx == 'P'  ; proc, ptr, public
        mov eax, 1
        ret
    .endif
    .if ecx == 'e' || ecx == 'E'  ; end, endp, equ, extern
        mov eax, 1
        ret
    .endif
    .if ecx == 'd' || ecx == 'D'  ; db, dd, dw
        .if edx == 'b' || edx == 'B' || edx == 'd' || edx == 'D' || edx == 'w' || edx == 'W'
            mov eax, 1
            ret
        .endif
    .endif
    
    xor eax, eax
    ret
IsDirective endp

; ============================================================================
; GetTokenColor - Get color for token type
; Input: tokenType = TOKEN_xxx constant
; Returns: EAX = RGB color value
; ============================================================================
GetTokenColor proc tokenType:DWORD
    mov ecx, tokenType
    
    .if ecx == TOKEN_INSTRUCTION
        mov eax, COLOR_INSTRUCTION
    .elseif ecx == TOKEN_REGISTER
        mov eax, COLOR_REGISTER
    .elseif ecx == TOKEN_DIRECTIVE
        mov eax, COLOR_DIRECTIVE
    .elseif ecx == TOKEN_LABEL
        mov eax, COLOR_LABEL
    .elseif ecx == TOKEN_COMMENT
        mov eax, COLOR_COMMENT
    .elseif ecx == TOKEN_STRING
        mov eax, COLOR_STRING
    .elseif ecx == TOKEN_NUMBER
        mov eax, COLOR_NUMBER
    .elseif ecx == TOKEN_OPERATOR
        mov eax, COLOR_OPERATOR
    .else
        mov eax, COLOR_DEFAULT
    .endif
    
    ret
GetTokenColor endp

end