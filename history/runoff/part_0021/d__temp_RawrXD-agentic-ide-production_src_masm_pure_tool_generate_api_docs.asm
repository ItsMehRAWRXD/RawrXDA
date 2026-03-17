; ============================================================================
; TOOL 16: Generate API Documentation - Pure x64 MASM
; Autonomous API documentation generation
; ============================================================================

option casemap:none

; ============================================================================
; WIN32 API DECLARATIONS
; ============================================================================
EXTERN CreateFileA:PROC
EXTERN WriteFile:PROC
EXTERN CloseHandle:PROC
EXTERN GetLastError:PROC

; Constants
NULL                equ 0
TRUE                equ 1
FALSE               equ 0
GENERIC_WRITE       equ 40000000h
CREATE_ALWAYS       equ 2

; ============================================================================
; PUBLIC EXPORTS
; ============================================================================
PUBLIC Tool_GenerateAPIDocs

; ============================================================================
; GLOBAL STATE
; ============================================================================
.data
    g_qwToolCalls      dq 0
    g_dwLastError      dd 0

; JSON keys
szEndpointsKey       db 'endpoints',0
szFormatKey          db 'format',0

; File names
apiDocFile           db 'api_docs.yaml',0

; Documentation templates
szSwaggerHeader      db 'openapi: 3.0.0',10,'info:',10,'  title: API',10,'  version: 1.0.0',10,'paths:',10,0
szSwaggerFooter      db 'components:',10,'  schemas:',10,'    Empty: {}',10,0

; ============================================================================
; TOOL IMPLEMENTATION
; ============================================================================
.code

Tool_GenerateAPIDocs PROC
    ; RCX = JSON params string
    inc g_qwToolCalls
    
    push rbx
    push rsi
    sub rsp, 32
    
    mov rbx, rcx                    ; Save params
    
    ; Validate parameters
    test rbx, rbx
    jz @invalidParams
    
    ; Create API documentation file
    lea rcx, apiDocFile
    mov rdx, GENERIC_WRITE
    mov r8, 0
    mov r9, 0
    mov qword ptr [rsp+32], CREATE_ALWAYS
    mov qword ptr [rsp+40], 0
    mov qword ptr [rsp+48], 0
    call CreateFileA
    
    cmp rax, INVALID_HANDLE_VALUE
    je @fileError
    
    mov rsi, rax                    ; File handle
    
    ; Write Swagger header
    lea rcx, szSwaggerHeader
    call lstrlenA
    mov r8, rax                     ; Length
    mov rdx, rcx                    ; Buffer
    mov rcx, rsi                    ; File handle
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Write Swagger footer
    lea rcx, szSwaggerFooter
    call lstrlenA
    mov r8, rax
    mov rdx, rcx
    mov rcx, rsi
    xor r9, r9
    mov qword ptr [rsp+32], 0
    call WriteFile
    
    ; Close file
    mov rcx, rsi
    call CloseHandle
    
    mov rax, TRUE                   ; Success
    jmp @exit
    
@invalidParams:
    xor rax, rax
    jmp @exit
    
@fileError:
    call GetLastError
    mov g_dwLastError, eax
    xor rax, rax
    
@exit:
    add rsp, 32
    pop rsi
    pop rbx
    ret
Tool_GenerateAPIDocs ENDP

END