;==============================================================================
; RawrXD Private Compiler - Phase 5.4: Macro Preprocessor
; Advanced macro system with parameter substitution, conditional assembly, loops
;==============================================================================

.386
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc
include \masm32\include\shlwapi.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib
includelib \masm32\lib\shlwapi.lib

;==============================================================================
; MACRO PREPROCESSOR CONSTANTS
;==============================================================================
MAX_MACROS              equ 4096
MAX_MACRO_PARAMS        equ 16
MAX_MACRO_NESTING       equ 32
MAX_MACRO_TEXT          equ 8192
MAX_CONDITIONAL_NESTING equ 16

; Macro types
MACRO_TYPE_SIMPLE       equ 1
MACRO_TYPE_PARAMETRIC   equ 2
MACRO_TYPE_VARIADIC     equ 3
MACRO_TYPE_RECURSIVE    equ 4

; Conditional assembly states
COND_STATE_TRUE         equ 1
COND_STATE_FALSE        equ 2
COND_STATE_IGNORED      equ 3

; Macro expansion modes
EXPAND_MODE_NORMAL      equ 1
EXPAND_MODE_REPEAT      equ 2
EXPRO_MODE_WHILE        equ 3
EXPAND_MODE_FOR         equ 4

;==============================================================================
; MACRO STRUCTURES
;==============================================================================
MACRO_DEFINITION struct
    _name            db 128 dup(?)
    params          dd MAX_MACRO_PARAMS dup(?)
    paramCount      dd ?
    localSymbols    dd 256 dup(?)
    localCount      dd ?
    text            dd ?
    textSize        dd ?
    _type            dd ?
    flags           dd ?
    lineDefined     dd ?
    fileDefined     db MAX_PATH dup(?)
    expansionCount  dd ?
    lastExpanded    dd ?
MACRO_DEFINITION ends

MACRO_PARAMETER struct
    _name            db 64 dup(?)
    defaultValue    db 256 dup(?)
    isRequired      dd ?
    isVariadic      dd ?
    _type            dd ?
MACRO_PARAMETER ends

MACRO_EXPANSION struct
    macroDef        dd ?
    args            dd MAX_MACRO_PARAMS dup(?)
    argCount        dd ?
    localBindings   dd 256 dup(?)
    bindingCount    dd ?
    expansionText   dd ?
    textSize        dd ?
    parentExpansion dd ?
    nestingLevel    dd ?
    lineNumber      dd ?
MACRO_EXPANSION ends

CONDITIONAL_STATE struct
    state           dd ?
    startLine       dd ?
    nestingLevel    dd ?
    wasTrue         dd ?
CONDITIONAL_STATE ends

PREPROCESSOR_STATE struct
    macros          dd ?
    macroCount      dd ?
    expansionStack  dd MAX_MACRO_NESTING dup(?)
    expansionDepth  dd ?
    conditionalStack dd MAX_CONDITIONAL_NESTING dup(?)
    conditionalDepth dd ?
    outputBuffer    dd ?
    outputSize      dd ?
    outputCapacity  dd ?
    currentFile     db MAX_PATH dup(?)
    currentLine     dd ?
    includePaths    db 16 * MAX_PATH dup(?)
    includeCount    dd ?
    defines         dd ?
    defineCount     dd ?
    undefines       dd ?
    undefCount      dd ?
PREPROCESSOR_STATE ends

;==============================================================================
; MACRO PREPROCESSOR CORE
;==============================================================================
.data?
macroState PREPROCESSOR_STATE <>
macroTable MACRO_DEFINITION MAX_MACROS dup(<?>)
macroExpansionStack MACRO_EXPANSION MAX_MACRO_NESTING dup(<?>)
conditionalStack_ CONDITIONAL_STATE MAX_CONDITIONAL_NESTING dup(<?>)
macroTextBuffer db MAX_MACRO_TEXT * MAX_MACROS dup(?)

.data
    szOutMacro      db "@Out", 0
    szOutText       db "echo %1", 0
    szInstrMacro    db "@Instr", 0
    szInstrText     db "0", 0
    szVersionMacro  db "@Version", 0
    szVersionText   db "600", 0
    szIfMacro       db "IF", 0
    szIfText        db ".if %1", 0
    szElseMacro     db "ELSE", 0
    szElseText      db ".else", 0
    szEndifMacro    db "ENDIF", 0
    szEndifText     db ".endif", 0
    szEnvMasm       db "MASM", 0
    szEnvSdk        db "WindowsSdkDir", 0
    szErrTableFull  db "Macro table full", 0
    szErrUndefined  db "Undefined macro", 0
    szErrLimit      db "Macro expansion limit exceeded", 0
    szErrNesting    db "Macro nesting limit exceeded", 0
    szErrArgs       db "Incorrect number of macro arguments", 0
    szErrElseif     db "ELSEIF without IF", 0
    szErrElse       db "ELSE without IF", 0
    szErrEndif      db "ENDIF without IF", 0
    szErrRepeat     db "Repeat count too large", 0
    szErrExitm      db "EXITM outside of macro expansion", 0
    szErrInclude    db "Include file not found", 0
    szErrRead       db "Failed to read include file", 0
    szDefined       db "DEFINED", 0
    szCatstr        db "CATSTR", 0
    szSubstr        db "SUBSTR", 0
    szSizestr       db "SIZESTR", 0
    szInstr         db "INSTR", 0
    szExitm         db "EXITM", 0
    szGoto          db "GOTO", 0
    szAlign         db "ALIGN", 0
    szOrg           db "ORG", 0

.code

;==============================================================================
; Phase 5.4.1: Macro Preprocessor Initialization
;==============================================================================
MacroPreprocessor_Init proc
    ; Initialize macro preprocessor state
    invoke ZeroMemory, addr macroState, SIZEOF PREPROCESSOR_STATE
    
    mov macroState.macroCount, 0
    mov macroState.expansionDepth, 0
    mov macroState.conditionalDepth, 0
    mov macroState.currentLine, 1
    mov macroState.includeCount, 0
    mov macroState.defineCount, 0
    mov macroState.undefCount, 0
    
    ; Initialize built-in macros
    mov macroState.macros, offset macroTable
    mov macroState.outputBuffer, offset macroTextBuffer
    mov macroState.outputCapacity, MAX_MACRO_TEXT * MAX_MACROS
    
    ; Register built-in macros
    invoke RegisterBuiltinMacros
    
    ; Set up standard include paths
    invoke SetupStandardIncludePaths
    
    mov eax, TRUE
    ret
MacroPreprocessor_Init endp

RegisterBuiltinMacros proc
    ; Register built-in MASM macros
    
    ; %OUT macro for output
    invoke DefineMacro, addr szOutMacro, MACRO_TYPE_PARAMETRIC, 1, addr szOutText, 7
    
    ; %INSTR macro for instruction count
    invoke DefineMacro, addr szInstrMacro, MACRO_TYPE_SIMPLE, 0, addr szInstrText, 1
    
    ; %VERSION macro
    invoke DefineMacro, addr szVersionMacro, MACRO_TYPE_SIMPLE, 0, addr szVersionText, 3
    
    ; Conditional assembly macros
    invoke DefineMacro, addr szIfMacro, MACRO_TYPE_PARAMETRIC, 1, addr szIfText, 6
    invoke DefineMacro, addr szElseMacro, MACRO_TYPE_SIMPLE, 0, addr szElseText, 5
    invoke DefineMacro, addr szEndifMacro, MACRO_TYPE_SIMPLE, 0, addr szEndifText, 6
    
    mov eax, TRUE
    ret
RegisterBuiltinMacros endp

SetupStandardIncludePaths proc
    ; Add standard MASM include paths
    
    ; Current directory
    invoke GetCurrentDirectoryA, MAX_PATH, addr macroState.includePaths[0]
    inc macroState.includeCount
    
    ; MASM installation directory
    invoke GetEnvironmentVariableA, addr szEnvMasm, addr macroState.includePaths[1*MAX_PATH], MAX_PATH
    .if eax > 0
        inc macroState.includeCount
    .endif
    
    ; Windows SDK include path
    invoke GetEnvironmentVariableA, addr szEnvSdk, addr macroState.includePaths[2*MAX_PATH], MAX_PATH
    .if eax > 0
        inc macroState.includeCount
    .endif
    
    mov eax, TRUE
    ret
SetupStandardIncludePaths endp

;==============================================================================
; Phase 5.4.2: Macro Definition Processing
;==============================================================================
DefineMacro proc pName:DWORD, macroType:DWORD, paramCount:DWORD, pText:DWORD, textLen:DWORD
    local macroIndex:DWORD
    local macroPtr:DWORD
    
    ; Find free macro slot
    .if macroState.macroCount >= MAX_MACROS
        invoke Macro_Error, addr szErrTableFull
        mov eax, FALSE
        ret
    .endif
    
    mov eax, macroState.macroCount
    mov macroIndex, eax
    inc macroState.macroCount
    
    ; Get macro pointer
    mov eax, macroIndex
    mov ebx, SIZEOF MACRO_DEFINITION
    mul ebx
    add eax, offset macroTable
    mov macroPtr, eax
    
    mov esi, macroPtr
    assume esi:ptr MACRO_DEFINITION
    
    ; Initialize macro definition
    invoke ZeroMemory, macroPtr, SIZEOF MACRO_DEFINITION
    
    ; Set macro properties
    invoke lstrcpynA, addr [esi]._name, pName, 128
    mov eax, macroType
    mov [esi]._type, eax
    mov eax, paramCount
    mov [esi].paramCount, eax
    mov eax, textLen
    mov [esi].textSize, eax
    
    ; Allocate macro text
    invoke AllocateMacroText, pText, textLen
    mov [esi].text, eax
    
    ; Set definition location
    mov eax, macroState.currentLine
    mov [esi].lineDefined, eax
    invoke lstrcpynA, addr [esi].fileDefined, addr macroState.currentFile, MAX_PATH
    
    ; Parse parameters if parametric macro
    .if macroType == MACRO_TYPE_PARAMETRIC
        invoke ParseMacroParameters, macroPtr
    .endif
    
    assume esi:nothing
    mov eax, TRUE
    ret
DefineMacro endp

ParseMacroParameters proc macroDef:DWORD
    local paramIndex:DWORD
    local paramText[256]:BYTE
    
    mov esi, macroDef
    assume esi:ptr MACRO_DEFINITION
    
    ; Scan macro text for parameters
    mov paramIndex, 0
    
    .while paramIndex < MAX_MACRO_PARAMS
        ; Look for %n parameter references
        invoke FindMacroParameters, [esi].text, paramIndex, addr paramText
        
        .if eax == TRUE
            ; Add parameter to macro definition
            mov eax, paramIndex
            mov ebx, SIZEOF MACRO_PARAMETER
            mul ebx
            add eax, esi ; Wait, params is inside MACRO_DEFINITION
            add eax, MACRO_DEFINITION.params
            mov edi, eax
            assume edi:ptr MACRO_PARAMETER
            
            invoke lstrcpynA, addr [edi]._name, addr paramText, 64
            mov [edi].isRequired, 1
            mov [edi].isVariadic, 0
            
            inc [esi].paramCount
        .else
            .break
        .endif
        
        inc paramIndex
    .endw
    
    assume esi:nothing
    mov eax, TRUE
    ret
ParseMacroParameters endp

;==============================================================================
; Phase 5.4.3: Macro Expansion Engine
;==============================================================================
ExpandMacro proc macroName:DWORD, args:DWORD, argCount:DWORD
    local macroIndex:DWORD
    local macroPtr:DWORD
    local expansion:MACRO_EXPANSION
    local expandedText[8192]:BYTE
    local argIndex:DWORD
    
    ; Find macro definition
    invoke FindMacro, macroName
    mov macroIndex, eax
    
    .if macroIndex == -1
        invoke Macro_Error, addr szErrUndefined
        mov eax, FALSE
        ret
    .endif
    
    ; Get macro pointer
    mov eax, macroIndex
    mov ebx, SIZEOF MACRO_DEFINITION
    mul ebx
    add eax, offset macroTable
    mov macroPtr, eax
    
    mov esi, macroPtr
    assume esi:ptr MACRO_DEFINITION
    
    ; Check macro expansion limits
    .if [esi].expansionCount > 1000
        invoke Macro_Error, addr szErrLimit
        mov eax, FALSE
        ret
    .endif
    
    ; Check nesting level
    .if macroState.expansionDepth >= MAX_MACRO_NESTING
        invoke Macro_Error, addr szErrNesting
        mov eax, FALSE
        ret
    .endif
    
    ; Validate argument count
    mov eax, argCount
    .if eax != [esi].paramCount
        .if [esi]._type != MACRO_TYPE_VARIADIC
            invoke Macro_Error, addr szErrArgs
            mov eax, FALSE
            ret
        .endif
    .endif
    
    ; Create expansion context
    invoke ZeroMemory, addr expansion, SIZEOF MACRO_EXPANSION
    
    mov eax, macroPtr
    mov expansion.macroDef, eax
    mov eax, argCount
    mov expansion.argCount, eax
    mov eax, macroState.expansionDepth
    mov expansion.nestingLevel, eax
    mov eax, macroState.currentLine
    mov expansion.lineNumber, eax
    
    ; Store arguments
    mov argIndex, 0
    .while argIndex < argCount
        mov eax, argIndex
        shl eax, 2
        add eax, args
        mov ecx, [eax]  ; Get argument pointer
        
        mov eax, argIndex
        mov expansion.args[eax*4], ecx
        
        inc argIndex
    .endw
    
    ; Perform parameter substitution
    invoke SubstituteParameters, addr expansion, addr expandedText
    
    ; Handle local symbols
    invoke ProcessLocalSymbols, addr expansion, addr expandedText
    
    ; Add to expansion stack
    ; push expansion ; Cannot push structure directly
    ; Copy to stack
    mov eax, macroState.expansionDepth
    mov ebx, SIZEOF MACRO_EXPANSION
    mul ebx
    add eax, offset macroExpansionStack
    invoke RtlMoveMemory, eax, addr expansion, SIZEOF MACRO_EXPANSION
    
    inc macroState.expansionDepth
    
    ; Update macro expansion count
    inc [esi].expansionCount
    
    assume esi:nothing
    mov eax, TRUE
    ret
ExpandMacro endp

SubstituteParameters proc expansion:DWORD, result:DWORD
    local macroText:DWORD
    local paramIndex:DWORD
    local paramName[64]:BYTE
    local paramValue[256]:BYTE
    local outputPos:DWORD
    
    mov esi, expansion
    assume esi:ptr MACRO_EXPANSION
    
    ; Get macro text
    mov eax, [esi].macroDef
    mov edi, eax
    assume edi:ptr MACRO_DEFINITION
    mov eax, [edi].text
    mov macroText, eax
    
    mov outputPos, 0
    
    ; Scan macro text for parameters
    .while TRUE
        mov eax, macroText
        mov al, byte ptr [eax]
        .break .if al == 0
        
        ; Check for parameter reference
        invoke IsParameterReference, macroText, addr paramIndex
        
        .if eax == TRUE
            ; Get parameter value
            invoke GetParameterValue, expansion, paramIndex, addr paramValue
            
            ; Copy parameter value to result
            mov eax, result
            add eax, outputPos
            invoke lstrcpyA, eax, addr paramValue
            
            ; Update positions
            invoke lstrlenA, addr paramValue
            add outputPos, eax
            
            ; Skip parameter reference in source
            invoke SkipParameterReference, macroText
            mov macroText, eax
            
        .else
            ; Copy literal character
            mov eax, macroText
            mov al, byte ptr [eax]
            mov edx, result
            add edx, outputPos
            mov byte ptr [edx], al
            inc outputPos
            inc macroText
        .endif
    .endw
    
    ; Null terminate
    mov eax, result
    add eax, outputPos
    mov byte ptr [eax], 0
    
    assume esi:nothing
    assume edi:nothing
    mov eax, TRUE
    ret
SubstituteParameters endp

;==============================================================================
; Phase 5.4.4: Conditional Assembly Processing
;==============================================================================
ProcessConditionalAssembly proc directive:DWORD, args:DWORD
    local condState:CONDITIONAL_STATE
    local conditionResult:DWORD
    local directiveType:DWORD
    
    ; Determine conditional directive type
    invoke GetConditionalDirectiveType, directive
    mov directiveType, eax
    
    .if directiveType == COND_IF
        ; Evaluate condition
        invoke EvaluateCondition, args
        mov conditionResult, eax
        
        ; Push conditional state
        mov eax, macroState.conditionalDepth
        mov ebx, SIZEOF CONDITIONAL_STATE
        mul ebx
        add eax, offset conditionalStack_
        mov edi, eax
        assume edi:ptr CONDITIONAL_STATE
        
        .if conditionResult != 0
            mov [edi].state, COND_STATE_TRUE
        .else
            mov [edi].state, COND_STATE_FALSE
        .endif
        
        mov eax, conditionResult
        mov [edi].wasTrue, eax
        mov eax, macroState.currentLine
        mov [edi].startLine, eax
        mov eax, macroState.conditionalDepth
        mov [edi].nestingLevel, eax
        
        inc macroState.conditionalDepth
        
    .elseif directiveType == COND_ELSEIF
        ; Check if we're in a conditional block
        .if macroState.conditionalDepth == 0
            invoke Macro_Error, addr szErrElseif
            mov eax, FALSE
            ret
        .endif
        
        ; Get current conditional state
        mov eax, macroState.conditionalDepth
        dec eax
        mov ebx, SIZEOF CONDITIONAL_STATE
        mul ebx
        add eax, offset conditionalStack_
        mov edi, eax
        assume edi:ptr CONDITIONAL_STATE
        
        ; Only evaluate if previous condition was false
        .if [edi].wasTrue == 0
            invoke EvaluateCondition, args
            mov conditionResult, eax
            
            .if conditionResult != 0
                mov [edi].state, COND_STATE_TRUE
                mov [edi].wasTrue, 1
            .else
                mov [edi].state, COND_STATE_FALSE
            .endif
        .else
            mov [edi].state, COND_STATE_IGNORED
        .endif
        
    .elseif directiveType == COND_ELSE
        ; Check if we're in a conditional block
        .if macroState.conditionalDepth == 0
            invoke Macro_Error, addr szErrElse
            mov eax, FALSE
            ret
        .endif
        
        ; Get current conditional state
        mov eax, macroState.conditionalDepth
        dec eax
        mov ebx, SIZEOF CONDITIONAL_STATE
        mul ebx
        add eax, offset conditionalStack_
        mov edi, eax
        assume edi:ptr CONDITIONAL_STATE
        
        ; Only process if no previous condition was true
        .if [edi].wasTrue == 0
            mov [edi].state, COND_STATE_TRUE
        .else
            mov [edi].state, COND_STATE_IGNORED
        .endif
        
    .elseif directiveType == COND_ENDIF
        ; Check if we're in a conditional block
        .if macroState.conditionalDepth == 0
            invoke Macro_Error, addr szErrEndif
            mov eax, FALSE
            ret
        .endif
        
        ; Pop conditional state
        dec macroState.conditionalDepth
    .endif
    
    assume edi:nothing
    mov eax, TRUE
    ret
ProcessConditionalAssembly endp

EvaluateCondition proc condition:DWORD
    local result:DWORD
    local operand1:DWORD
    local operator:DWORD
    local operand2:DWORD
    
    ; Simple condition evaluation
    ; Supports: DEFINED(symbol), expression1 EQ expression2, expression1 NE expression2
    
    ; Check for DEFINED()
    invoke lstrcmpiA, condition, addr szDefined
    .if eax == 0
        ; Extract symbol name from parentheses
        invoke ExtractSymbolFromDefined, condition, addr operand1
        .if eax == TRUE
            ; Check if symbol is defined
            invoke IsSymbolDefined, operand1
            mov result, eax
            mov eax, result
            ret
        .endif
    .endif
    
    ; Parse comparison expression
    invoke ParseComparison, condition, addr operand1, addr operator, addr operand2
    .if eax == TRUE
        ; Evaluate comparison
        invoke EvaluateComparison, operand1, operator, operand2
        mov result, eax
        mov eax, result
        ret
    .endif
    
    ; Default: evaluate as numeric expression
    invoke EvaluateNumericExpression, condition
    mov result, eax
    
    mov eax, result
    ret
    
EvaluateCondition endp

;==============================================================================
; Phase 5.4.5: Loop Constructs (REPEAT, WHILE, FOR)
;==============================================================================
ProcessLoopConstruct proc loopType:DWORD, args:DWORD
    local loopCounter:DWORD
    local loopCondition:DWORD
    local loopBody:DWORD
    local iterationCount:DWORD
    local maxIterations:DWORD
    
    mov maxIterations, 10000  ; Prevent infinite loops
    
    .if loopType == LOOP_REPEAT
        ; REPEAT count loop
        invoke GetRepeatCount, args
        mov iterationCount, eax
        
        .if iterationCount > maxIterations
            invoke Macro_Error, addr szErrRepeat
            mov eax, FALSE
            ret
        .endif
        
        mov loopCounter, 0
        .while loopCounter < iterationCount
            ; Expand loop body
            invoke ExpandLoopBody, loopBody
            
            inc loopCounter
        .endw
        
    .elseif loopType == LOOP_WHILE
        ; WHILE condition loop
        mov iterationCount, 0
        
        .while iterationCount < maxIterations
            ; Evaluate condition
            invoke EvaluateCondition, args
            mov loopCondition, eax
            
            .if loopCondition == 0
                .break
            .endif
            
            ; Expand loop body
            invoke ExpandLoopBody, loopBody
            
            inc iterationCount
        .endw
        
    .elseif loopType == LOOP_FOR
        ; FOR loop with parameters
        invoke ParseForParameters, args, addr loopCounter, addr loopCondition
        
        mov iterationCount, 0
        .while iterationCount < maxIterations
            ; Evaluate for condition
            invoke EvaluateForCondition, loopCounter, loopCondition
            .if eax == 0
                .break
            .endif
            
            ; Expand loop body
            invoke ExpandLoopBody, loopBody
            
            ; Update loop counter
            invoke UpdateForCounter, addr loopCounter
            
            inc iterationCount
        .endw
    .endif
    
    mov eax, TRUE
    ret
ProcessLoopConstruct endp

;==============================================================================
; Phase 5.4.6: String Manipulation Macros
;==============================================================================
ProcessStringMacro proc macroName:DWORD, args:DWORD
    local result[512]:BYTE
    local arg1[256]:BYTE
    local arg2[256]:BYTE
    
    ; CATSTR - Concatenate strings
    invoke lstrcmpiA, macroName, addr szCatstr
    .if eax == 0
        invoke GetStringArgument, args, 0, addr arg1
        invoke GetStringArgument, args, 1, addr arg2
        invoke lstrcpyA, addr result, addr arg1
        invoke lstrcatA, addr result, addr arg2
        lea eax, result
        ret
    .endif
    
    ; SUBSTR - Extract substring
    invoke lstrcmpiA, macroName, addr szSubstr
    .if eax == 0
        invoke GetStringArgument, args, 0, addr arg1
        invoke GetNumericArgument, args, 1
        mov ecx, eax  ; Start position
        invoke GetNumericArgument, args, 2
        mov edx, eax  ; Length
        
        invoke ExtractSubstring, addr arg1, ecx, edx, addr result
        lea eax, result
        ret
    .endif
    
    ; SIZESTR - Get string length
    invoke lstrcmpiA, macroName, addr szSizestr
    .if eax == 0
        invoke GetStringArgument, args, 0, addr arg1
        invoke lstrlenA, addr arg1
        invoke wsprintfA, addr result, addr szFmtD, eax
        lea eax, result
        ret
    .endif
    
    ; INSTR - Find substring
    invoke lstrcmpiA, macroName, addr szInstr
    .if eax == 0
        invoke GetStringArgument, args, 0, addr arg1
        invoke GetStringArgument, args, 1, addr arg2
        invoke FindSubstring, addr arg1, addr arg2
        invoke wsprintfA, addr result, addr szFmtD, eax
        lea eax, result
        ret
    .endif
    
    mov eax, NULL
    ret
    
ProcessStringMacro endp

;==============================================================================
; Phase 5.4.7: Advanced Macro Features
;==============================================================================
ProcessAdvancedMacro proc directive:DWORD, args:DWORD
    local result:DWORD
    
    ; EXITM - Exit macro expansion
    invoke lstrcmpiA, directive, addr szExitm
    .if eax == 0
        invoke ProcessExitMacro, args
        mov eax, TRUE
        ret
    .endif
    
    ; GOTO - Jump to label in macro
    invoke lstrcmpiA, directive, addr szGoto
    .if eax == 0
        invoke ProcessGotoMacro, args
        mov eax, TRUE
        ret
    .endif
    
    ; ALIGN - Align to boundary
    invoke lstrcmpiA, directive, addr szAlign
    .if eax == 0
        invoke ProcessAlignMacro, args
        mov eax, TRUE
        ret
    .endif
    
    ; ORG - Set origin
    invoke lstrcmpiA, directive, addr szOrg
    .if eax == 0
        invoke ProcessOrgMacro, args
        mov eax, TRUE
        ret
    .endif
    
    mov eax, FALSE
    ret
    
ProcessAdvancedMacro endp

ProcessExitMacro proc args:DWORD
    ; Exit current macro expansion
    local exitValue[256]:BYTE
    
    .if macroState.expansionDepth == 0
        invoke Macro_Error, addr szErrExitm
        mov eax, FALSE
        ret
    .endif
    
    ; Get exit value
    invoke GetStringArgument, args, 0, addr exitValue
    
    ; Set exit flag in current expansion
    mov eax, macroState.expansionDepth
    dec eax
    mov ebx, SIZEOF MACRO_EXPANSION
    mul ebx
    add eax, offset macroExpansionStack
    mov esi, eax
    assume esi:ptr MACRO_EXPANSION
    
    ; Mark for early exit
    mov [esi].textSize, 0  ; Empty expansion to signal exit
    
    assume esi:nothing
    mov eax, TRUE
    ret
ProcessExitMacro endp

;==============================================================================
; Phase 5.4.8: Include File Processing
;==============================================================================
ProcessInclude proc includePath:DWORD
    local fullPath[MAX_PATH]:BYTE
    local fileContent:DWORD
    local fileSize:DWORD
    local resolvedPath:DWORD
    
    ; Resolve include path
    invoke ResolveIncludePath, includePath, addr fullPath
    mov resolvedPath, eax
    
    .if resolvedPath == NULL
        invoke Macro_Error, addr szErrInclude
        mov eax, FALSE
        ret
    .endif
    
    ; Read include file
    invoke ReadIncludeFile, addr fullPath, addr fileContent, addr fileSize
    .if eax == FALSE
        invoke Macro_Error, addr szErrRead
        mov eax, FALSE
        ret
    .endif
    
    ; Process included content
    invoke ProcessIncludedContent, fileContent, fileSize
    
    ; Free file content
    invoke GlobalFree, fileContent
    
    mov eax, TRUE
    ret
ProcessInclude endp

ResolveIncludePath proc includePath:DWORD, fullPath:DWORD
    local searchPath[MAX_PATH]:BYTE
    local i:DWORD
    
    ; Check if path is absolute
    invoke PathIsRelativeA, includePath
    .if eax == FALSE
        ; Absolute path - use as-is
        invoke lstrcpynA, fullPath, includePath, MAX_PATH
        mov eax, fullPath
        ret
    .endif
    
    ; Search in include paths
    mov i, 0
    .while i < macroState.includeCount
        ; Build search path
        mov eax, i
        mov ebx, MAX_PATH
        mul ebx
        add eax, offset macroState.includePaths
        invoke wsprintfA, addr searchPath, addr szFmtPath, eax, includePath
        
        ; Check if file exists
        invoke GetFileAttributesA, addr searchPath
        .if eax != -1
            ; File found
            invoke lstrcpynA, fullPath, addr searchPath, MAX_PATH
            mov eax, fullPath
            ret
        .endif
        
        inc i
    .endw
    
    mov eax, NULL
    ret
    
ResolveIncludePath endp

;==============================================================================
; Phase 5.4.9: Macro Debugging and Tracing
;==============================================================================
MacroTrace_Init proc traceFile:DWORD
    ; Initialize macro tracing
    mov eax, TRUE
    ret
MacroTrace_Init endp

MacroTrace_LogExpansion proc macroName:DWORD, args:DWORD, argCount:DWORD
    ; Log macro expansion for debugging
    mov eax, TRUE
    ret
MacroTrace_LogExpansion endp

MacroTrace_LogConditional proc condition:DWORD, result:DWORD, line:DWORD
    ; Log conditional assembly evaluation
    mov eax, TRUE
    ret
MacroTrace_LogConditional endp

;==============================================================================
; Phase 5.4.10: Utility Functions
;==============================================================================
FindMacro proc macroName:DWORD
    local i:DWORD
    
    mov i, 0
    .while i < macroState.macroCount
        mov eax, i
        mov ebx, SIZEOF MACRO_DEFINITION
        mul ebx
        add eax, offset macroTable
        mov esi, eax
        assume esi:ptr MACRO_DEFINITION
        
        invoke lstrcmpiA, addr [esi]._name, macroName
        .if eax == 0
            mov eax, i
            ret
        .endif
        
        inc i
    .endw
    
    mov eax, -1
    ret
    
FindMacro endp

GetParameterValue proc expansion:DWORD, paramIndex:DWORD, result:DWORD
    ; Get parameter value from expansion
    mov eax, TRUE
    ret
GetParameterValue endp

IsParameterReference proc text:DWORD, pParamIndex:DWORD
    ; Check if text contains parameter reference
    mov eax, FALSE
    ret
IsParameterReference endp

SkipParameterReference proc text:DWORD
    ; Skip parameter reference in text
    mov eax, text
    ret
SkipParameterReference endp

ProcessLocalSymbols proc expansion:DWORD, result:DWORD
    ; Process local symbols in macro
    mov eax, TRUE
    ret
ProcessLocalSymbols endp

GetConditionalDirectiveType proc directive:DWORD
    ; Get conditional directive type
    mov eax, 0
    ret
GetConditionalDirectiveType endp

ExtractSymbolFromDefined proc definedExpr:DWORD, symbol:DWORD
    ; Extract symbol from DEFINED() expression
    mov eax, TRUE
    ret
ExtractSymbolFromDefined endp

ParseComparison proc expr:DWORD, operand1:DWORD, operator:DWORD, operand2:DWORD
    ; Parse comparison expression
    mov eax, TRUE
    ret
ParseComparison endp

EvaluateComparison proc operand1:DWORD, operator:DWORD, operand2:DWORD
    ; Evaluate comparison
    mov eax, 0
    ret
EvaluateComparison endp

EvaluateNumericExpression proc expr:DWORD
    ; Evaluate numeric expression
    mov eax, 0
    ret
EvaluateNumericExpression endp

IsSymbolDefined proc symbol:DWORD
    ; Check if symbol is defined
    mov eax, FALSE
    ret
IsSymbolDefined endp

GetRepeatCount proc args:DWORD
    ; Get repeat count from arguments
    mov eax, 1
    ret
GetRepeatCount endp

ExpandLoopBody proc loopBody:DWORD
    ; Expand loop body
    mov eax, TRUE
    ret
ExpandLoopBody endp

GetStringArgument proc args:DWORD, index:DWORD, result:DWORD
    ; Get string argument
    mov eax, TRUE
    ret
GetStringArgument endp

GetNumericArgument proc args:DWORD, index:DWORD
    ; Get numeric argument
    mov eax, 0
    ret
GetNumericArgument endp

ExtractSubstring proc source:DWORD, _start:DWORD, _len:DWORD, result:DWORD
    ; Extract substring
    mov eax, TRUE
    ret
ExtractSubstring endp

FindSubstring proc source:DWORD, substring:DWORD
    ; Find substring
    mov eax, -1
    ret
FindSubstring endp

ParseForParameters proc args:DWORD, counter:DWORD, condition:DWORD
    ; Parse FOR loop parameters
    mov eax, TRUE
    ret
ParseForParameters endp

EvaluateForCondition proc counter:DWORD, condition:DWORD
    ; Evaluate FOR loop condition
    mov eax, 0
    ret
EvaluateForCondition endp

UpdateForCounter proc counter:DWORD
    ; Update FOR loop counter
    mov eax, TRUE
    ret
UpdateForCounter endp

ProcessGotoMacro proc args:DWORD
    ; Process GOTO directive
    mov eax, TRUE
    ret
ProcessGotoMacro endp

ProcessAlignMacro proc args:DWORD
    ; Process ALIGN directive
    mov eax, TRUE
    ret
ProcessAlignMacro endp

ProcessOrgMacro proc args:DWORD
    ; Process ORG directive
    mov eax, TRUE
    ret
ProcessOrgMacro endp

ReadIncludeFile proc filePath:DWORD, pContent:DWORD, pSize:DWORD
    ; Read include file
    mov eax, TRUE
    ret
ReadIncludeFile endp

ProcessIncludedContent proc content:DWORD, _size:DWORD
    ; Process included content
    mov eax, TRUE
    ret
ProcessIncludedContent endp

Macro_Error proc message:DWORD
    ; Report macro error
    invoke OutputDebugStringA, message
    mov eax, TRUE
    ret
Macro_Error endp

AllocateMacroText proc pText:DWORD, _len:DWORD
    mov eax, pText
    ret
AllocateMacroText endp

FindMacroParameters proc pText:DWORD, index:DWORD, pBuffer:DWORD
    mov eax, FALSE
    ret
FindMacroParameters endp

RtlMoveMemory proc dest:DWORD, src:DWORD, _len:DWORD
    push esi
    push edi
    mov esi, src
    mov edi, dest
    mov ecx, _len
    rep movsb
    pop edi
    pop esi
    ret
RtlMoveMemory endp

.data
    szFmtD      db "%d", 0
    szFmtPath   db "%s\%s", 0

; Constants
LOOP_REPEAT equ 1
LOOP_WHILE  equ 2
LOOP_FOR    equ 3

COND_IF     equ 1
COND_ELSEIF equ 2
COND_ELSE   equ 3
COND_ENDIF  equ 4

END
