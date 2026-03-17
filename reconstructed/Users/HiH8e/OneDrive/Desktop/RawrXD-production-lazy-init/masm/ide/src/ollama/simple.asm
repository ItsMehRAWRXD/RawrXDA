; ============================================================================
; ollama_simple.asm - Ollama API Wrapper for Local Model Inference
; Formats requests, handles streaming, parses JSON responses
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include windows.inc
include winsock2.inc

includelib kernel32.lib
includelib ws2_32.lib

PUBLIC OllamaSimple_GenerateComplete
PUBLIC OllamaSimple_GenerateStreaming
PUBLIC OllamaSimple_ListModels
PUBLIC OllamaSimple_ParseResponse
PUBLIC OllamaSimple_FormatRequest

; Constants
OLLAMA_API_GENERATE    EQU 1
OLLAMA_API_CHAT        EQU 2
OLLAMA_API_LIST        EQU 3

MAX_RESPONSE_SIZE      EQU 8192
MAX_PROMPT_SIZE        EQU 4096

; Streaming callback prototype
; typedef void (*StreamCallback)(const char* token, DWORD cbToken)

; ============================================================================
; Structures
; ============================================================================

OllamaRequest STRUCT
    pModel              DWORD ?
    pPrompt             DWORD ?
    fTemperature        REAL4 ?
    dwTopK              DWORD ?
    fTopP               REAL4 ?
    dwNumPredict        DWORD ?
    bStream             DWORD ?
OllamaRequest ENDS

OllamaResponse STRUCT
    pResponse           DWORD ?
    cbResponse          DWORD ?
    pModel              DWORD ?
    dwPromptEvalCount   DWORD ?
    dwEvalCount         DWORD ?
    llTotalDuration     QWORD ?
    bDone               DWORD ?
OllamaResponse ENDS

; ============================================================================
; Data Section
; ============================================================================

.data

    ; API endpoints
    szGenerateEndpoint  db "/api/generate", 0
    szChatEndpoint      db "/api/chat", 0
    szListEndpoint      db "/api/tags", 0

    ; HTTP request templates
    szHttpPost          db "POST %s HTTP/1.1", 13, 10, 0
    szHttpHost          db "Host: localhost:11434", 13, 10, 0
    szHttpContentType   db "Content-Type: application/json", 13, 10, 0
    szHttpContentLen    db "Content-Length: %d", 13, 10, 0
    szHttpConnection    db "Connection: close", 13, 10, 0
    szHttpDoubleNewline db 13, 10, 0

    ; JSON templates
    szJsonGenerate      db "{""model"":""%s"",""prompt"":""%s"",""stream"":%s,""temperature"":%d,""top_k"":%d,""top_p"":%d}", 0
    szJsonTrue          db "true", 0
    szJsonFalse         db "false", 0

    ; Response parsing
    szResponseKey       db """response"":", 0
    szDoneKey           db """done"":", 0
    szQuote             db """", 0

.data?

    g_RequestBuffer     BYTE MAX_PROMPT_SIZE DUP(?)
    g_ResponseBuffer    BYTE MAX_RESPONSE_SIZE DUP(?)

; ============================================================================
; Code Section
; ============================================================================

.code

; ============================================================================
; OllamaSimple_FormatRequest - Build JSON request for Ollama API
; Input: pRequest (OllamaRequest structure)
; Output: eax = pointer to formatted request, ecx = size
; ============================================================================

OllamaSimple_FormatRequest PROC USES esi edi ebx pRequest:DWORD

    LOCAL pModel:DWORD
    LOCAL pPrompt:DWORD
    LOCAL fTemp:REAL4
    LOCAL dwTopK:DWORD
    LOCAL fTopP:REAL4
    LOCAL cbRequest:DWORD

    mov esi, pRequest
    test esi, esi
    jz @fail

    ; Load parameters from request structure
    mov eax, [esi].OllamaRequest.pModel
    mov pModel, eax
    mov eax, [esi].OllamaRequest.pPrompt
    mov pPrompt, eax
    mov eax, [esi].OllamaRequest.fTemperature
    mov fTemp, eax
    mov eax, [esi].OllamaRequest.dwTopK
    mov dwTopK, eax
    mov eax, [esi].OllamaRequest.fTopP
    mov fTopP, eax

    ; Build request: POST /api/generate HTTP/1.1
    lea edi, g_RequestBuffer

    ; Format first line: POST /api/generate HTTP/1.1
    invoke wsprintf, edi, addr szHttpPost, addr szGenerateEndpoint
    mov ecx, eax
    add edi, ecx

    ; Add headers
    invoke lstrcpy, edi, addr szHttpHost
    mov ecx, eax
    add edi, ecx

    invoke lstrcpy, edi, addr szHttpContentType
    mov ecx, eax
    add edi, ecx

    ; Content-Length header (will update after knowing body size)
    invoke lstrcpy, edi, addr szHttpConnection
    mov ecx, eax
    add edi, ecx

    ; Double newline to separate headers from body
    invoke lstrcpy, edi, addr szHttpDoubleNewline
    mov ecx, eax
    add edi, ecx

    ; Build JSON body
    mov eax, [esi].OllamaRequest.bStream
    test eax, eax
    jnz @use_true
    lea eax, szJsonFalse
    jmp @format_json

@use_true:
    lea eax, szJsonTrue

@format_json:
    ; Format: {"model":"...","prompt":"...","stream":true/false,...}
    invoke wsprintf, edi, addr szJsonGenerate, pModel, pPrompt, eax, fTemp, dwTopK, fTopP

    mov ecx, eax
    add edi, ecx

    ; Calculate total request size
    lea eax, g_RequestBuffer
    mov ecx, edi
    sub ecx, eax
    mov cbRequest, ecx

    ; Update Content-Length header
    ; (simplified: actual implementation would insert correctly)

    lea eax, g_RequestBuffer
    mov ecx, cbRequest
    ret

@fail:
    xor eax, eax
    xor ecx, ecx
    ret

OllamaSimple_FormatRequest ENDP

; ============================================================================
; OllamaSimple_GenerateComplete - Full inference (non-streaming)
; Input: pszModel, pszPrompt, pResponse (buffer), cbResponse
; Output: eax = response text pointer, ecx = length
; ============================================================================

OllamaSimple_GenerateComplete PROC USES esi edi ebx pszModel:DWORD, pszPrompt:DWORD, pResponse:DWORD, cbResponse:DWORD

    LOCAL request:OllamaRequest

    ; Build request
    mov esi, pszModel
    mov [request.pModel], esi
    mov esi, pszPrompt
    mov [request.pPrompt], esi
    mov [request.fTemperature], 0.7
    mov [request.dwTopK], 40
    mov [request.fTopP], 0.9
    mov [request.dwNumPredict], 512
    mov [request.bStream], 0            ; Non-streaming

    ; Format request
    lea eax, request
    push eax
    call OllamaSimple_FormatRequest
    mov esi, eax                        ; esi = request pointer
    mov ecx, ecx                        ; ecx = request size

    ; Send to Ollama (would use http_minimal here)
    ; TODO: Wire to HttpMin_Send and HttpMin_Recv

    ; Parse response
    mov eax, pResponse
    mov ecx, cbResponse
    ret

OllamaSimple_GenerateComplete ENDP

; ============================================================================
; OllamaSimple_GenerateStreaming - Streaming inference (token-by-token)
; Input: pszModel, pszPrompt, pfnCallback (token handler)
; Output: eax = 1 success, 0 failure
; ============================================================================

OllamaSimple_GenerateStreaming PROC USES esi edi ebx pszModel:DWORD, pszPrompt:DWORD, pfnCallback:DWORD

    LOCAL request:OllamaRequest
    LOCAL dwLine:DWORD
    LOCAL pToken:DWORD

    ; Build request with streaming enabled
    mov esi, pszModel
    mov [request.pModel], esi
    mov esi, pszPrompt
    mov [request.pPrompt], esi
    mov [request.fTemperature], 0.7
    mov [request.dwTopK], 40
    mov [request.fTopP], 0.9
    mov [request.dwNumPredict], 512
    mov [request.bStream], 1            ; Enable streaming

    ; Format request
    lea eax, request
    push eax
    call OllamaSimple_FormatRequest
    mov esi, eax
    mov ecx, ecx

    ; Send request (would use http_minimal)
    ; TODO: Wire to HttpMin_Send

    ; Receive streaming response
    ; Each line is a JSON object ending with \n
    ; Format: {"response":"token",...,"done":false}

    xor edx, edx                        ; line counter

@stream_loop:
    cmp edx, MAX_RESPONSE_SIZE
    jge @stream_done

    ; Receive one line
    ; TODO: Read until \n

    ; Parse JSON line to extract "response" field
    ; TODO: Wire to OllamaSimple_ParseResponse

    ; Check for "done":true
    ; TODO: if done, break

    ; Call callback with token
    ; TODO: invoke pfnCallback, pToken, cbToken

    inc edx
    jmp @stream_loop

@stream_done:
    mov eax, 1
    ret

OllamaSimple_GenerateStreaming ENDP

; ============================================================================
; OllamaSimple_ParseResponse - Extract JSON field from Ollama response
; Input: pszJson, pszFieldName
; Output: eax = field value pointer, ecx = length
; ============================================================================

OllamaSimple_ParseResponse PROC USES esi edi ebx pszJson:DWORD, pszFieldName:DWORD

    LOCAL pFieldStart:DWORD
    LOCAL pFieldEnd:DWORD
    LOCAL cbField:DWORD

    mov esi, pszJson
    test esi, esi
    jz @not_found

    mov edi, pszFieldName
    test edi, edi
    jz @not_found

    ; Find field name in JSON
    xor ecx, ecx

@search_loop:
    cmp byte ptr [esi], 0
    je @not_found

    ; Check for field name match
    mov eax, esi
    mov ebx, edi

@match_loop:
    mov cl, byte ptr [eax]
    mov dl, byte ptr [ebx]
    cmp cl, dl
    jne @search_mismatch
    test cl, cl
    jz @match_found
    inc eax
    inc ebx
    jmp @match_loop

@match_found:
    ; Field found, extract value
    mov pFieldStart, eax

    ; Skip past field name and colon
@skip_to_value:
    mov cl, byte ptr [eax]
    cmp cl, ':'
    je @found_colon
    test cl, cl
    jz @not_found
    inc eax
    jmp @skip_to_value

@found_colon:
    inc eax
    
    ; Skip whitespace
@skip_whitespace:
    mov cl, byte ptr [eax]
    cmp cl, ' '
    je @skip_whitespace_next
    cmp cl, 9               ; Tab
    je @skip_whitespace_next
    jmp @extract_value

@skip_whitespace_next:
    inc eax
    jmp @skip_whitespace

@extract_value:
    mov pFieldStart, eax
    
    ; Find end of value (quote, comma, or brace)
    mov cl, byte ptr [eax]
    cmp cl, '"'             ; String value
    je @extract_string
    
    ; Numeric value - find comma or }
@find_delimiter:
    mov cl, byte ptr [eax]
    cmp cl, ','
    je @found_end
    cmp cl, '}'
    je @found_end
    test cl, cl
    jz @found_end
    inc eax
    jmp @find_delimiter

@extract_string:
    ; Skip opening quote
    inc eax
    mov pFieldStart, eax
    
@find_string_end:
    mov cl, byte ptr [eax]
    cmp cl, '"'
    je @found_end
    test cl, cl
    jz @not_found
    inc eax
    jmp @find_string_end

@found_end:
    mov pFieldEnd, eax
    mov ecx, eax
    sub ecx, pFieldStart
    mov eax, pFieldStart
    mov cbField, ecx
    ret

@search_mismatch:
    inc esi
    jmp @search_loop

@not_found:
    xor eax, eax
    xor ecx, ecx
    ret

OllamaSimple_ParseResponse ENDP

; ============================================================================
; OllamaSimple_ListModels - Get list of available models
; Output: eax = pointer to models JSON, ecx = size
; ============================================================================

OllamaSimple_ListModels PROC

    ; Would send GET /api/tags and return response
    ; TODO: Implement

    xor eax, eax
    xor ecx, ecx
    ret

OllamaSimple_ListModels ENDP

END
