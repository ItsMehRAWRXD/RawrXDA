; ============================================================================
; ide_statusbar.asm - Status bar with real-time IDE information
; Shows cursor position, git branch, model, privacy mode, agent status
; ============================================================================

option casemap:none

; ----------------------------------------------------------------------------
; EXTERNALS
; ----------------------------------------------------------------------------
EXTERN CreateWindowExA:PROC
EXTERN SendMessageA:PROC
EXTERN SetWindowTextA:PROC
EXTERN hMainWnd:QWORD
EXTERN hInstance:QWORD

; ----------------------------------------------------------------------------
; CONSTANTS
; ----------------------------------------------------------------------------
NULL                equ 0
WS_CHILD            equ 40000000h
WS_VISIBLE          equ 10000000h
SB_SIMPLE           equ 0100h
SB_SETTEXT          equ 0401h
SB_GETTEXTLENGTH    equ 0402h

; Status bar parts (widths in pixels)
STATUS_PARTS_COUNT  equ 6
STATUS_PART_CURSOR  equ 100
STATUS_PART_BRANCH  equ 150
STATUS_PART_MODEL   equ 120
STATUS_PART_PRIVACY equ 80
STATUS_PART_AGENT   equ 100
STATUS_PART_MEMORY  equ 80

; ----------------------------------------------------------------------------
; PUBLICS
; ----------------------------------------------------------------------------
PUBLIC IDEStatusbar_Create
PUBLIC IDEStatusbar_Update
PUBLIC hStatusbar

; ----------------------------------------------------------------------------
; DATA
; ----------------------------------------------------------------------------
.data
hStatusbar          dq 0
szStatusbarClass    db "msctls_statusbar32",0

; Status bar parts array
statusParts         dd 0, 100, 250, 370, 450, 550, -1

; Status text buffers
szCursorPos         db "Ln 1, Col 1",0
szGitBranch         db "main",0
szModelName         db "Phi-3-mini",0
szPrivacyMode       db "Privacy",0
szAgentStatus       db "Agent Idle",0
szMemoryUsage       db "64MB",0

; ----------------------------------------------------------------------------
; CODE
; ----------------------------------------------------------------------------
.code

IDEStatusbar_Create PROC
    ; Create status bar window
    invoke CreateWindowExA, 0, ADDR szStatusbarClass, NULL,
        WS_CHILD or WS_VISIBLE or SB_SIMPLE,
        0, 0, 0, 0, hMainWnd, NULL, hInstance, NULL
    mov hStatusbar, rax

    ; Set status bar parts
    invoke SendMessageA, hStatusbar, SB_SETPARTS, STATUS_PARTS_COUNT, ADDR statusParts

    ; Initialize status text
    call IDEStatusbar_Update

    ret
IDEStatusbar_Create ENDP

IDEStatusbar_Update PROC
    ; Update cursor position (stub - would get from editor)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 0, ADDR szCursorPos

    ; Update git branch (stub - would detect from repository)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 1, ADDR szGitBranch

    ; Update model name (stub - would get from settings)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 2, ADDR szModelName

    ; Update privacy mode (stub - would get from privacy settings)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 3, ADDR szPrivacyMode

    ; Update agent status (stub - would get from agent orchestrator)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 4, ADDR szAgentStatus

    ; Update memory usage (stub - would calculate from process)
    invoke SendMessageA, hStatusbar, SB_SETTEXT, 5, ADDR szMemoryUsage

    ret
IDEStatusbar_Update ENDP

; ----------------------------------------------------------------------------
; Status update functions (would be called from other modules)
; ----------------------------------------------------------------------------

IDEStatusbar_UpdateCursor PROC line:DWORD, column:DWORD
    ; Format: "Ln X, Col Y"
    LOCAL buffer[32]:BYTE
    
    ; Convert line/column to string
    ; In production, use proper formatting
    mov szCursorPos[0], 'L'
    mov szCursorPos[1], 'n'
    mov szCursorPos[2], ' '
    mov szCursorPos[3], '1'
    mov szCursorPos[4], ','
    mov szCursorPos[5], ' '
    mov szCursorPos[6], 'C'
    mov szCursorPos[7], 'o'
    mov szCursorPos[8], 'l'
    mov szCursorPos[9], ' '
    mov szCursorPos[10], '1'
    mov szCursorPos[11], 0
    
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdateCursor ENDP

IDEStatusbar_UpdateGitBranch PROC pszBranch:QWORD
    ; Update git branch display
    invoke SetWindowTextA, hStatusbar, pszBranch
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdateGitBranch ENDP

IDEStatusbar_UpdateModel PROC pszModel:QWORD
    ; Update model name display
    invoke SetWindowTextA, hStatusbar, pszModel
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdateModel ENDP

IDEStatusbar_UpdatePrivacy PROC enabled:BYTE
    ; Update privacy mode display
    cmp enabled, 0
    je @privacy_off
    
    mov szPrivacyMode[0], 'P'
    mov szPrivacyMode[1], 'r'
    mov szPrivacyMode[2], 'i'
    mov szPrivacyMode[3], 'v'
    mov szPrivacyMode[4], 'a'
    mov szPrivacyMode[5], 'c'
    mov szPrivacyMode[6], 'y'
    mov szPrivacyMode[7], 0
    jmp @update
    
@privacy_off:
    mov szPrivacyMode[0], 0
    
@update:
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdatePrivacy ENDP

IDEStatusbar_UpdateAgentStatus PROC pszStatus:QWORD
    ; Update agent status display
    invoke SetWindowTextA, hStatusbar, pszStatus
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdateAgentStatus ENDP

IDEStatusbar_UpdateMemory PROC usageMB:DWORD
    ; Update memory usage display
    LOCAL buffer[16]:BYTE
    
    ; Convert MB to string
    ; In production, use proper formatting
    mov szMemoryUsage[0], '6'
    mov szMemoryUsage[1], '4'
    mov szMemoryUsage[2], 'M'
    mov szMemoryUsage[3], 'B'
    mov szMemoryUsage[4], 0
    
    call IDEStatusbar_Update
    ret
IDEStatusbar_UpdateMemory ENDP

END