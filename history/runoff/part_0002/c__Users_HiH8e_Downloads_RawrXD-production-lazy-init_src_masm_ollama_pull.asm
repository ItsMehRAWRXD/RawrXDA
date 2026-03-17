;==============================================================================
; ollama_pull.asm - Pure MASM64 Ollama Model Puller
; ==========================================================================
; Handles downloading models from Ollama registry.
; Zero C++ runtime dependencies.
;==============================================================================

.686p
.xmm
.model flat, c
option casemap:none

include windows.inc
include http_client.inc
include json_parser.inc
include logging.inc

.data
    szPullEndpoint      BYTE "/api/pull",0
    szPullJsonTemplate  BYTE '{"name":"%s"}',0
    szPulling           BYTE "Pulling model: %s...",0
    szPullSuccess       BYTE "Model %s pulled successfully.",0
    szPullFailure       BYTE "Failed to pull model %s. Error: %d",0

.code

;==============================================================================
; OllamaPullModel - Pulls a model from the Ollama server
;==============================================================================
OllamaPullModel PROC uses rbx rsi rdi pModelName:QWORD
    LOCAL request:HttpRequest
    LOCAL jsonBody[256]:BYTE
    
    invoke LogInfo, addr szPulling, pModelName
    
    ; 1. Prepare JSON Body
    invoke wsprintfA, addr jsonBody, addr szPullJsonTemplate, pModelName
    
    ; 2. Setup Request
    mov request.method, HTTP_METHOD_POST
    lea rax, szPullEndpoint
    mov request.endpoint, rax
    lea rax, jsonBody
    mov request.body, rax
    invoke lstrlenA, addr jsonBody
    mov request.bodyLen, rax
    
    ; 3. Execute Request
    ; (Assuming HttpClient is already initialized and connected to Ollama)
    invoke HttpClientRequest, g_pHttpClient, addr request
    .if rax == 0
        invoke LogError, addr szPullFailure, pModelName, GetLastError()
        ret
    .endif
    
    invoke LogSuccess, addr szPullSuccess, pModelName
    mov rax, TRUE
    ret
OllamaPullModel ENDP

END
