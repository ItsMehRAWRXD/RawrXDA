;==============================================================================
; File 19: semantic_highlighting.asm - Scintilla Lexer Integration
;==============================================================================
include windows.inc

.code
;==============================================================================
; Initialize Semantic Highlighter
;==============================================================================
SemanticHighlight_Init PROC hSci:QWORD
    ; Register for document changes
    invoke SendMessage, hSci, 2386, 0x00000100 or 0x00000200, 0
    
    ; Set C++ lexer
    invoke SendMessage, hSci, 4001, 3, 0
    
    ; Setup keywords
    call SemanticHighlight_SetupKeywords, hSci
    
    LOG_INFO "Semantic highlighting initialized (C++)"
    
    ret
SemanticHighlight_Init ENDP

;==============================================================================
; Set Up C++ Keywords
;==============================================================================
SemanticHighlight_SetupKeywords PROC hSci:QWORD
    invoke SendMessage, hSci, 4010, 0,
        OFFSET szCppKeywords
    invoke SendMessage, hSci, 4010, 1,
        OFFSET szCppTypes
    invoke SendMessage, hSci, 4010, 2,
        OFFSET szCppDirectives
    
    ret
SemanticHighlight_SetupKeywords ENDP

;==============================================================================
; Data
;==============================================================================
.data
szCppKeywords    db 'if else while for do switch case break return',0
szCppTypes       db 'int float double char bool void class struct',0
szCppDirectives  db 'include define ifdef endif pragma',0

END
