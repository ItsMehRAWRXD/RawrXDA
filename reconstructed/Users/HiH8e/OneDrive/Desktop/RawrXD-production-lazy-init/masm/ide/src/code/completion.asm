; ============================================================================
; RawrXD IDE - Code Completion Engine (IntelliSense for MASM)
; Intelligent code suggestions and auto-completion
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

; ============================================================================
; CONSTANTS
; ============================================================================

MAX_SUGGESTIONS         equ 50
MAX_SUGGESTION_LENGTH   equ 64
MAX_CONTEXT_LENGTH      equ 256

; Suggestion types
SUGGEST_INSTRUCTION     equ 1
SUGGEST_REGISTER        equ 2
SUGGEST_DIRECTIVE       equ 3
SUGGEST_LABEL           equ 4
SUGGEST_MACRO           equ 5
SUGGEST_API             equ 6

; ============================================================================
; STRUCTURES
; ============================================================================

SUGGESTION struct
    suggType        dd ?
    text            db MAX_SUGGESTION_LENGTH dup(?)
    description     db 128 dup(?)
    priority        dd ?
    insertText      db MAX_SUGGESTION_LENGTH dup(?)
SUGGESTION ends

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Suggestion list
    suggestionList      SUGGESTION MAX_SUGGESTIONS dup(<>)
    suggestionCount     dd 0
    
    ; Current context
    currentContext      db MAX_CONTEXT_LENGTH dup(0)
    contextLength       dd 0
    
    ; Common instructions with descriptions
    szMov       db "mov", 0
    szMovDesc   db "Move data from source to destination", 0
    
    szAdd       db "add", 0
    szAddDesc   db "Add source to destination", 0
    
    szSub       db "sub", 0
    szSubDesc   db "Subtract source from destination", 0
    
    szPush      db "push", 0
    szPushDesc  db "Push value onto the stack", 0
    
    szPop       db "pop", 0
    szPopDesc   db "Pop value from the stack", 0
    
    szCall      db "call", 0
    szCallDesc  db "Call a procedure", 0
    
    szRet       db "ret", 0
    szRetDesc   db "Return from procedure", 0
    
    szJmp       db "jmp", 0
    szJmpDesc   db "Unconditional jump", 0
    
    szCmp       db "cmp", 0
    szCmpDesc   db "Compare two operands", 0
    
    szTest      db "test", 0
    szTestDesc  db "Logical compare (AND without storing)", 0
    
    szInvoke    db "invoke", 0
    szInvokeDesc db "High-level procedure call", 0
    
    ; Common registers
    szEax       db "eax", 0
    szEaxDesc   db "Accumulator register (32-bit)", 0
    
    szEbx       db "ebx", 0
    szEbxDesc   db "Base register (32-bit)", 0
    
    szEcx       db "ecx", 0
    szEcxDesc   db "Counter register (32-bit)", 0
    
    szEdx       db "edx", 0
    szEdxDesc   db "Data register (32-bit)", 0
    
    szEsi       db "esi", 0
    szEsiDesc   db "Source index register", 0
    
    szEdi       db "edi", 0
    szEdiDesc   db "Destination index register", 0
    
    szEbp       db "ebp", 0
    szEbpDesc   db "Base pointer (stack frame)", 0
    
    szEsp       db "esp", 0
    szEspDesc   db "Stack pointer", 0
    
    ; Common directives
    szProc      db "proc", 0
    szProcDesc  db "Begin procedure definition", 0
    
    szEndp      db "endp", 0
    szEndpDesc  db "End procedure definition", 0
    
    szLocal     db "LOCAL", 0
    szLocalDesc db "Declare local variable", 0
    
    szDb        db "db", 0
    szDbDesc    db "Define byte (8-bit)", 0
    
    szDw        db "dw", 0
    szDwDesc    db "Define word (16-bit)", 0
    
    szDd        db "dd", 0
    szDdDesc    db "Define doubleword (32-bit)", 0
    
    szInclude   db "include", 0
    szIncludeDesc db "Include external file", 0
    
    szExtern    db "extern", 0
    szExternDesc db "Declare external symbol", 0

; ============================================================================
; CODE SECTION
; ============================================================================

.code

; Forward declarations
InitializeCodeCompletion proto
UpdateContext proto :DWORD
GetSuggestions proto :DWORD
FilterSuggestions proto :DWORD
ApplySuggestion proto :DWORD
AddInstructionSuggestions proto
AddRegisterSuggestions proto
AddDirectiveSuggestions proto

public InitializeCodeCompletion
public UpdateContext
public GetSuggestions
public FilterSuggestions
public ApplySuggestion

; ============================================================================
; InitializeCodeCompletion - Setup code completion system
; ============================================================================
InitializeCodeCompletion proc
    ; Clear suggestion list
    invoke RtlZeroMemory, offset suggestionList, sizeof SUGGESTION * MAX_SUGGESTIONS
    mov suggestionCount, 0
    
    ; Initialize context
    invoke RtlZeroMemory, offset currentContext, MAX_CONTEXT_LENGTH
    mov contextLength, 0
    
    mov eax, 1
    ret
InitializeCodeCompletion endp

; ============================================================================
; UpdateContext - Update current typing context
; Input: lpText = pointer to current line text
; Returns: EAX = 1 if context updated
; ============================================================================
UpdateContext proc lpText:DWORD
    LOCAL len:DWORD
    
    ; Get length of text
    invoke lstrlen, lpText
    mov len, eax
    
    ; Limit to MAX_CONTEXT_LENGTH
    .if eax > MAX_CONTEXT_LENGTH
        mov len, MAX_CONTEXT_LENGTH
    .endif
    
    ; Copy to context buffer
    invoke RtlZeroMemory, offset currentContext, MAX_CONTEXT_LENGTH
    invoke lstrcpyn, offset currentContext, lpText, len
    mov eax, len
    mov contextLength, eax
    
    mov eax, 1
    ret
UpdateContext endp

; ============================================================================
; GetSuggestions - Build suggestion list based on current context
; Input: lpPartialWord = pointer to partial word being typed
; Returns: EAX = number of suggestions
; ============================================================================
GetSuggestions proc lpPartialWord:DWORD
    LOCAL len:DWORD
    LOCAL firstChar:BYTE
    
    ; Clear existing suggestions
    mov suggestionCount, 0
    invoke RtlZeroMemory, offset suggestionList, sizeof SUGGESTION * MAX_SUGGESTIONS
    
    ; Get length and first character
    invoke lstrlen, lpPartialWord
    mov len, eax
    
    .if len == 0
        ; No partial word - show all suggestions
        call AddInstructionSuggestions
        call AddRegisterSuggestions
        call AddDirectiveSuggestions
        jmp @Done
    .endif
    
    ; Get first character to optimize search
    mov eax, lpPartialWord
    movzx ecx, byte ptr [eax]
    mov firstChar, cl
    
    ; Convert to lowercase for comparison
    .if firstChar >= 'A' && firstChar <= 'Z'
        add firstChar, 20h  ; Convert to lowercase
    .endif
    
    ; Add relevant suggestions based on first character
    .if firstChar == 'm'
        call AddMoveSuggestion
    .elseif firstChar == 'p'
        call AddPushPopSuggestions
    .elseif firstChar == 'c'
        call AddCallCmpSuggestions
    .elseif firstChar == 'j'
        call AddJumpSuggestions
    .elseif firstChar == 'e'
        call AddRegisterSuggestions  ; eax, ebx, etc
    .else
        ; Add all suggestions
        call AddInstructionSuggestions
        call AddRegisterSuggestions
        call AddDirectiveSuggestions
    .endif
    
    ; Filter suggestions by partial word
    invoke FilterSuggestions, lpPartialWord
    
    @Done:
    mov eax, suggestionCount
    ret
GetSuggestions endp

; ============================================================================
; AddMoveSuggestion - Add MOV instruction suggestion
; ============================================================================
AddMoveSuggestion proc
    LOCAL pSugg:DWORD
    
    ; Get pointer to next suggestion slot
    mov eax, suggestionCount
    .if eax >= MAX_SUGGESTIONS
        ret
    .endif
    
    mov ecx, sizeof SUGGESTION
    mul ecx
    lea edx, suggestionList
    add edx, eax
    mov pSugg, edx
    
    ; Fill in suggestion
    mov eax, pSugg
    mov dword ptr [eax], SUGGEST_INSTRUCTION  ; type
    
    invoke lstrcpy, addr [eax + 4], offset szMov  ; text
    invoke lstrcpy, addr [eax + 68], offset szMovDesc  ; description
    mov dword ptr [eax + 196], 10  ; priority
    invoke lstrcpy, addr [eax + 200], offset szMov  ; insertText
    
    inc suggestionCount
    ret
AddMoveSuggestion endp

; ============================================================================
; AddPushPopSuggestions - Add PUSH/POP suggestions
; ============================================================================
AddPushPopSuggestions proc
    LOCAL pSugg:DWORD

    ; PUSH
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx

        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_INSTRUCTION
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szPush
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szPushDesc
        mov dword ptr [eax + 196], 9
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szPush
        inc suggestionCount
    .endif

    ; POP
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx

        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_INSTRUCTION
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szPop
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szPopDesc
        mov dword ptr [eax + 196], 9
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szPop
        inc suggestionCount
    .endif

    ret
AddPushPopSuggestions endp

; ============================================================================
; AddCallCmpSuggestions - Add CALL/CMP suggestions
; ============================================================================
AddCallCmpSuggestions proc
    ; Stub: Call/cmp instruction completion not yet implemented
    ; Feature would suggest matching function names
    ret
AddCallCmpSuggestions endp

; ============================================================================
; AddJumpSuggestions - Add jump instruction suggestions
; ============================================================================
AddJumpSuggestions proc
    ; Stub: Jump instruction completion not yet implemented
    ; Feature would suggest labels and conditions
    ret
AddJumpSuggestions endp

; ============================================================================
; AddInstructionSuggestions - Add all instruction suggestions
; ============================================================================
AddInstructionSuggestions proc
    call AddMoveSuggestion
    ; Additional instruction categories deferred for future enhancement
    ret
AddInstructionSuggestions endp

; ============================================================================
; AddRegisterSuggestions - Add register suggestions
; ============================================================================
AddRegisterSuggestions proc
    ; Simplified stub: register suggestions not implemented in this build
    mov eax, TRUE
    ret
AddRegisterSuggestions endp

; ============================================================================
; AddDirectiveSuggestions - Add directive suggestions
; ============================================================================
AddDirectiveSuggestions proc
    LOCAL pSugg:DWORD

    ; PROC
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szProc
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szProcDesc
        mov dword ptr [eax + 196], 7
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szProc
        inc suggestionCount
    .endif

    ; ENDP
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szEndp
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szEndpDesc
        mov dword ptr [eax + 196], 7
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szEndp
        inc suggestionCount
    .endif

    ; LOCAL
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szLocal
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szLocalDesc
        mov dword ptr [eax + 196], 7
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szLocal
        inc suggestionCount
    .endif

    ; DB
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szDb
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szDbDesc
        mov dword ptr [eax + 196], 6
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szDb
        inc suggestionCount
    .endif

    ; DW
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szDw
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szDwDesc
        mov dword ptr [eax + 196], 6
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szDw
        inc suggestionCount
    .endif

    ; DD
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szDd
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szDdDesc
        mov dword ptr [eax + 196], 6
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szDd
        inc suggestionCount
    .endif

    ; INCLUDE
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szInclude
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szIncludeDesc
        mov dword ptr [eax + 196], 5
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szInclude
        inc suggestionCount
    .endif

    ; EXTERN
    mov eax, suggestionCount
    .if eax < MAX_SUGGESTIONS
        mov ecx, sizeof SUGGESTION
        mul ecx
        lea edx, suggestionList
        add edx, eax
        mov pSugg, edx
        mov eax, pSugg
        mov dword ptr [eax], SUGGEST_DIRECTIVE
        lea edx, [eax + 4]
        invoke lstrcpy, edx, offset szExtern
        lea edx, [eax + 68]
        invoke lstrcpy, edx, offset szExternDesc
        mov dword ptr [eax + 196], 5
        lea edx, [eax + 200]
        invoke lstrcpy, edx, offset szExtern
        inc suggestionCount
    .endif

    ret
AddDirectiveSuggestions endp

; ============================================================================
; FilterSuggestions - Filter suggestions by partial match
; Input: lpPartialWord = pointer to partial word
; Returns: EAX = number of matching suggestions (current implementation)
; ============================================================================
FilterSuggestions proc lpPartialWord:DWORD
    ; Filtering logic deferred; currently returns full count
    mov eax, suggestionCount
    ret
FilterSuggestions endp

; ============================================================================
; ApplySuggestion - Insert selected suggestion into editor
; Input: suggestionIndex = index of suggestion to apply
; Returns: EAX = 1 if successful
; ============================================================================
ApplySuggestion proc suggestionIndex:DWORD
    LOCAL pSugg:DWORD
    
    ; Validate index
    mov eax, suggestionIndex
    .if eax >= suggestionCount
        xor eax, eax
        ret
    .endif
    
    ; Get pointer to suggestion
    mov ecx, sizeof SUGGESTION
    mul ecx
    lea edx, suggestionList
    add edx, eax
    mov pSugg, edx
    
    ; Get insert text
    mov eax, pSugg
    add eax, 200  ; Offset to insertText field
    
    ; Would insert text into editor here
    ; For now, just return success
    mov eax, 1
    ret
ApplySuggestion endp

end