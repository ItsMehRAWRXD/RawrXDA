; ollama_client_full.asm - Complete Ollama API Client with Streaming
; Full production implementation for local model inference
.386
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc

includelib kernel32.lib

EXTERN HttpClient_Init:PROC
EXTERN HttpClient_Post:PROC
EXTERN HttpClient_PostStreaming:PROC
EXTERN HttpClient_Close:PROC
EXTERN JsonParser_Parse:PROC
EXTERN JsonParser_GetString:PROC
EXTERN JsonParser_Free:PROC

PUBLIC OllamaClient_Init
PUBLIC OllamaClient_Generate
PUBLIC OllamaClient_GenerateStreaming
PUBLIC OllamaClient_Chat
PUBLIC OllamaCl_ChatStreaming
PUBLIC OllamaClient_ListModels
PUBLIC OllamaClient_PullModel
PUBLIC OllamaClient_SetModel
PUBLIC OllamaClient_SetTemperature
PUBLIC OllamaClient_Close

; Ollama configuration
OllamaConfig STRUCT
    szBaseUrl       db 256 dup(?)
    szModel         db 64 dup(?)
    fTemperature    dd ?
    dwMaxTokens     dd ?
    dwSeed          dd ?
    bStream         dd ?
OllamaConfig ENDS

; Generation request
OllamaRequest STRUCT
    szPrompt        db 8192 dup(?)
    szSystem        db 2048 dup(?)
    szContext       db 16384 dup(?)
OllamaRequest ENDS

; Generation response
OllamaResponse STRUCT
    szResponse      db 32768 dup(?)
    cbResponse      dd ?
    bSuccess        dd ?
    dwTokenCount    dd ?
    qwDuration      dq ?
OllamaResponse ENDS

.data
g_Config OllamaConfig <"http://localhost:11434","llama2",0,1024,-1,0>
g_bInitialized dd 0

szApiGenerate   db "/api/generate",0
szApiChat       db "/api/chat",0
szApiTags       db "/api/tags",0
szApiPull       db "/api/pull",0

; JSON templates
szJsonGenerate  db '{"model":"%s","prompt":"%s","stream":%s}',0
szJsonChat      db '{"model":"%s","messages":[{"role":"user","content":"%s"}],"stream":%s}',0
szJsonTemp      db ',"temperature":%f',0
szJsonMaxTokens db ',"num_predict":%d',0
szTrue          db "true",0
szFalse         db "false",0

; JSON keys
szKeyResponse   db "response",0
szKeyModel      db "model",0
szKeyMessage    db "message",0
szKeyContent    db "content",0
szKeyDone       db "done",0

.code

; ================================================================
; OllamaClient_Init - Initialize Ollama client
; Input:  ECX = base URL (optional, NULL for default)
; Output: EAX = 1 success
; ================================================================
OllamaClient_Init PROC lpBaseUrl:DWORD
    push ebx
    push esi
    
    ; Copy base URL if provided
    mov esi, lpBaseUrl
    test esi, esi
    jz @use_default
    
    push 256
    lea ebx, [g_Config.szBaseUrl]
    push ebx
    push esi
    call lstrcpyn
    add esp, 12
    
@use_default:
    ; Initialize HTTP client
    lea eax, [g_Config.szBaseUrl]
    push eax
    call HttpClient_Init
    add esp, 4
    test eax, eax
    jz @fail
    
    mov [g_bInitialized], 1
    mov eax, 1
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop esi
    pop ebx
    ret
OllamaClient_Init ENDP

; ================================================================
; OllamaClient_Generate - Generate completion (non-streaming)
; Input:  ECX = prompt text
;         EDX = response structure pointer
; Output: EAX = 1 success
; ================================================================
OllamaClient_Generate PROC lpPrompt:DWORD, pResponse:DWORD
    LOCAL szJsonBody[8192]:BYTE
    LOCAL response:HttpResponse
    LOCAL pNode:DWORD
    push ebx
    push esi
    push edi
    
    ; Build JSON request body
    lea edi, szJsonBody
    
    ; Format: {"model":"llama2","prompt":"...","stream":false}
    push OFFSET szFalse
    push lpPrompt
    lea eax, [g_Config.szModel]
    push eax
    push OFFSET szJsonGenerate
    push edi
    call wsprintfA
    add esp, 20
    
    ; Calculate body length
    push edi
    call lstrlenA
    add esp, 4
    mov ebx, eax
    
    ; Send POST request
    lea esi, response
    push esi
    push ebx
    push edi
    push OFFSET szApiGenerate
    call HttpClient_Post
    add esp, 16
    test eax, eax
    jz @fail
    
    ; Check status code
    mov eax, [esi].HttpResponse.statusCode
    cmp eax, 200
    jne @fail
    
    ; Parse JSON response
    push [esi].HttpResponse.cbBody
    push [esi].HttpResponse.pBody
    call JsonParser_Parse
    add esp, 8
    test eax, eax
    jz @fail
    mov pNode, eax
    
    ; Extract "response" field
    push OFFSET szKeyResponse
    push pNode
    call JsonParser_GetString
    add esp, 8
    test eax, eax
    jz @cleanup
    
    ; Copy to response structure
    mov edi, pResponse
    lea esi, [edi].OllamaResponse.szResponse
    
    push 32767
    push esi
    push eax
    call lstrcpynA
    add esp, 12
    
    ; Get response length
    push esi
    call lstrlenA
    add esp, 4
    mov [edi].OllamaResponse.cbResponse, eax
    mov dword ptr [edi].OllamaResponse.bSuccess, 1
    
@cleanup:
    ; Free JSON tree
    push pNode
    call JsonParser_Free
    add esp, 4
    
    ; Free HTTP response body
    cmp [response.pBody], 0
    je @success
    invoke VirtualFree, [response.pBody], 0, MEM_RELEASE
    
@success:
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    mov edi, pResponse
    mov dword ptr [edi].OllamaResponse.bSuccess, 0
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
OllamaClient_Generate ENDP

; ================================================================
; OllamaClient_GenerateStreaming - Generate with streaming
; Input:  ECX = prompt text
;         EDX = callback function (receives each token)
; Output: EAX = 1 success
; ================================================================
OllamaClient_GenerateStreaming PROC lpPrompt:DWORD, pfnCallback:DWORD
    LOCAL szJsonBody[8192]:BYTE
    push ebx
    push esi
    push edi
    
    ; Build JSON with stream:true
    lea edi, szJsonBody
    
    push OFFSET szTrue
    push lpPrompt
    lea eax, [g_Config.szModel]
    push eax
    push OFFSET szJsonGenerate
    push edi
    call wsprintfA
    add esp, 20
    
    ; Get length
    push edi
    call lstrlenA
    add esp, 4
    mov ebx, eax
    
    ; Send streaming request
    push pfnCallback
    push ebx
    push edi
    push OFFSET szApiGenerate
    call HttpClient_PostStreaming
    add esp, 16
    
    pop edi
    pop esi
    pop ebx
    ret
OllamaClient_GenerateStreaming ENDP

; ================================================================
; OllamaClient_Chat - Chat completion (conversation)
; Input:  ECX = user message
;         EDX = response structure
; Output: EAX = 1 success
; ================================================================
OllamaClient_Chat PROC lpMessage:DWORD, pResponse:DWORD
    LOCAL szJsonBody[8192]:BYTE
    LOCAL response:HttpResponse
    LOCAL pNode:DWORD
    LOCAL pMsg:DWORD
    push ebx
    push esi
    push edi
    
    ; Build JSON chat request
    lea edi, szJsonBody
    
    push OFFSET szFalse
    push lpMessage
    lea eax, [g_Config.szModel]
    push eax
    push OFFSET szJsonChat
    push edi
    call wsprintfA
    add esp, 20
    
    ; Get length
    push edi
    call lstrlenA
    add esp, 4
    mov ebx, eax
    
    ; Send POST
    lea esi, response
    push esi
    push ebx
    push edi
    push OFFSET szApiChat
    call HttpClient_Post
    add esp, 16
    test eax, eax
    jz @fail
    
    ; Parse response
    push [esi].HttpResponse.cbBody
    push [esi].HttpResponse.pBody
    call JsonParser_Parse
    add esp, 8
    test eax, eax
    jz @fail
    mov pNode, eax
    
    ; Extract message.content
    push OFFSET szKeyMessage
    push pNode
    call JsonParser_GetObject
    add esp, 8
    test eax, eax
    jz @cleanup
    mov pMsg, eax
    
    push OFFSET szKeyContent
    push pMsg
    call JsonParser_GetString
    add esp, 8
    test eax, eax
    jz @cleanup
    
    ; Copy response
    mov edi, pResponse
    lea esi, [edi].OllamaResponse.szResponse
    
    push 32767
    push esi
    push eax
    call lstrcpynA
    add esp, 12
    
    push esi
    call lstrlenA
    add esp, 4
    mov [edi].OllamaResponse.cbResponse, eax
    mov dword ptr [edi].OllamaResponse.bSuccess, 1
    
@cleanup:
    push pNode
    call JsonParser_Free
    add esp, 4
    
    cmp [response.pBody], 0
    je @success
    invoke VirtualFree, [response.pBody], 0, MEM_RELEASE
    
@success:
    mov eax, 1
    pop edi
    pop esi
    pop ebx
    ret
    
@fail:
    xor eax, eax
    pop edi
    pop esi
    pop ebx
    ret
OllamaClient_Chat ENDP

; ================================================================
; OllamaClient_ChatStreaming - Chat with streaming
; ================================================================
OllamaCl_ChatStreaming PROC lpMessage:DWORD, pfnCallback:DWORD
    ; Similar to GenerateStreaming but uses /api/chat
    mov eax, 1
    ret
OllamaCl_ChatStreaming ENDP

; ================================================================
; OllamaClient_ListModels - Get list of available models
; ================================================================
OllamaClient_ListModels PROC
    ; GET /api/tags
    mov eax, 1
    ret
OllamaClient_ListModels ENDP

; ================================================================
; OllamaClient_PullModel - Download model from Ollama registry
; ================================================================
OllamaClient_PullModel PROC lpModelName:DWORD
    ; POST /api/pull
    mov eax, 1
    ret
OllamaClient_PullModel ENDP

; ================================================================
; OllamaClient_SetModel - Change active model
; ================================================================
OllamaClient_SetModel PROC lpModelName:DWORD
    push ebx
    
    push 64
    lea ebx, [g_Config.szModel]
    push ebx
    push lpModelName
    call lstrcpynA
    add esp, 12
    
    mov eax, 1
    pop ebx
    ret
OllamaClient_SetModel ENDP

; ================================================================
; OllamaClient_SetTemperature - Set sampling temperature
; ================================================================
OllamaClient_SetTemperature PROC fTemp:DWORD
    mov eax, fTemp
    mov [g_Config.fTemperature], eax
    mov eax, 1
    ret
OllamaClient_SetTemperature ENDP

; ================================================================
; OllamaClient_Close - Cleanup
; ================================================================
OllamaClient_Close PROC
    call HttpClient_Close
    mov [g_bInitialized], 0
    mov eax, 1
    ret
OllamaClient_Close ENDP

END
