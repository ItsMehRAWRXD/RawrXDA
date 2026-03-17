; ============================================================================
; RawrXD Agentic IDE - Model Invoker Implementation
; Pure MASM - Full HTTP Communication via WinHTTP
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include ..\include\winapi_min.inc

 .data
include constants.inc
include structures.inc
include macros.inc

; External declarations for compression
extern Compression_Compress:proc
extern Compression_FreeResult:proc

; ============================================================================
; Constants
; ============================================================================

WINHTTP_FLAG_ESCAPE_PERCENT     equ 00000040h

; ============================================================================
; DATA SECTION
; ============================================================================

.data
    ; Global handles
    g_hSession          dd 0
    g_hConnect          dd 0
    
    ; Configuration
    g_Backend           dd 0  ; 0=Ollama, 1=Claude, 2=OpenAI, 3=LocalGGUF
    g_szEndpoint        db "http://localhost:11434", 0
    g_szModel           db "llama2", 0
    g_fTemperature      REAL4 0.7
    g_dwMaxTokens       dd 1024
    g_dwTimeout         dd 30000  ; 30 seconds
    g_bCacheEnabled     dd 1
    
    ; HTTP strings
    szUserAgent         db "RawrXD-Agentic/1.0", 0
    szContentType       db "application/json", 0
    szAccept            db "*/*", 0
    szConnection        db "Keep-Alive", 0
    
    ; Ollama API strings
    szOllamaAPI         db "/api/generate", 0
    szOllamaModel       db "model", 0
    szOllamaPrompt      db "prompt", 0
    szOllamaTemp        db "temperature", 0
    szOllamaTokens      db "num_predict", 0
    
    ; JSON response parsing
    szResponse          db "response", 0
    szError             db "error", 0
    
    ; Cache structure
    g_CacheSize         dd 0
    g_MaxCacheSize      dd 1048576  ; 1MB cache
    g_pCacheBuffer      dd 0
    
    ; Result buffer
    g_szLastResponse    db MAX_BUFFER_SIZE dup(0)
    g_dwLastResponseLen dd 0

.data?
    g_hMutex            dd ?

; ============================================================================
; PROCEDURES
; ============================================================================

; ============================================================================
; ModelInvoker_Init - Initialize HTTP session
; Returns: Handle in eax (non-zero = success)
; ============================================================================
ModelInvoker_Init proc
    LOCAL userAgentLen:DWORD
    
    ; Create global session handle
    szLen addr szUserAgent
    mov userAgentLen, eax
    
    invoke WinHttpOpen, addr szUserAgent, WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, WINHTTP_FLAG_ASYNC
    
    mov g_hSession, eax
    
    .if eax == 0
        xor eax, eax
        ret
    .endif
    
    ; Set timeout
    invoke WinHttpSetOption, g_hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, 
        addr g_dwTimeout, sizeof DWORD
    
    ; Create mutex for thread safety
    invoke CreateMutex, NULL, FALSE, NULL
    mov g_hMutex, eax
    
    ; Allocate cache buffer
    MemAlloc g_MaxCacheSize
    mov g_pCacheBuffer, eax
    
    mov eax, g_hSession
    ret
ModelInvoker_Init endp

; ============================================================================
; ModelInvoker_Invoke - Synchronous wish → plan conversion via LLM
; Input: pszWish
; Output: Pointer to EXECUTION_PLAN in eax
; ============================================================================
ModelInvoker_Invoke proc pszWish:DWORD
    LOCAL hConnect:DWORD
    LOCAL hRequest:DWORD
    LOCAL dwSize:DWORD
    LOCAL dwRead:DWORD
    LOCAL szQuery db 2048 dup(0)
    LOCAL szPayload db MAX_BUFFER_SIZE dup(0)
    LOCAL bResults dd FALSE
    LOCAL pPlan:DWORD
    
    ; Lock mutex
    invoke WaitForSingleObject, g_hMutex, INFINITE
    
    ; Build JSON payload for Ollama
    .if g_Backend == 0  ; Ollama
        call BuildOllamaPayload
        szCopy addr szPayload, eax
        
        ; Connect to Ollama
        invoke WinHttpConnect, g_hSession, addr g_szEndpoint, INTERNET_DEFAULT_HTTP_PORT, 0
        mov hConnect, eax
        
        .if eax == 0
            invoke ReleaseMutex, g_hMutex
            xor eax, eax
            ret
        .endif
        
        ; Create request
        invoke WinHttpOpenRequest, hConnect, addr szPost, addr szOllamaAPI,
            NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, 0
        mov hRequest, eax
        
        .if eax == 0
            invoke WinHttpCloseHandle, hConnect
            invoke ReleaseMutex, g_hMutex
            xor eax, eax
            ret
        .endif
        
        ; Add headers
        invoke WinHttpAddRequestHeaders, hRequest, addr szContentType, -1L,
            WINHTTP_ADDREQ_FLAG_ADD or WINHTTP_ADDREQ_FLAG_REPLACE
        
        ; Send request with payload
        szLen addr szPayload
        mov dwSize, eax
        
        invoke WinHttpSendRequest, hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            addr szPayload, dwSize, dwSize, 0
        
        ; Check for send error
        .if eax == 0
            invoke WinHttpCloseHandle, hRequest
            invoke WinHttpCloseHandle, hConnect
            invoke ReleaseMutex, g_hMutex
            xor eax, eax
            ret
        .endif
        
        ; Receive response
        invoke WinHttpReceiveResponse, hRequest, NULL
        
        ; Read response body
        @@ReadLoop:
            invoke WinHttpQueryDataAvailable, hRequest, addr dwSize
            .if eax == 0 || dwSize == 0
                jmp @ReadDone
            .endif
            
            invoke WinHttpReadData, hRequest, addr g_szLastResponse, 
                sizeof g_szLastResponse, addr dwRead
            
            mov g_dwLastResponseLen, dwRead
            jmp @ReadLoop
        
        @ReadDone:
        
        ; Compress large responses for storage
        mov eax, g_dwLastResponseLen
        cmp eax, 1024  ; 1KB threshold
        jl @NoCompress
        
        ; Compress the response
        invoke ModelInvoker_CompressResponse, addr g_szLastResponse, g_dwLastResponseLen
        
        ; If compression was beneficial, update response
        mov esi, eax
        mov edi, edx
        .if esi != addr g_szLastResponse
            ; Copy compressed data back to response buffer
            mov ecx, edi
            cmp ecx, sizeof g_szLastResponse
            jg @NoCompress  ; Too big for buffer
            
            mov edx, addr g_szLastResponse
            push esi
            push edi
            push ecx
            rep movsb
            pop ecx
            pop edi
            pop esi
            
            ; Update response length
            mov g_dwLastResponseLen, ecx
            
            ; Free compressed buffer
            MemFree esi
        .endif
        
@NoCompress:
        ; Close handles
        invoke WinHttpCloseHandle, hRequest
        invoke WinHttpCloseHandle, hConnect
        
        ; Parse response into EXECUTION_PLAN
        call ParseOllamaResponse
        mov pPlan, eax
        
    .endif
    
    ; Unlock mutex
    invoke ReleaseMutex, g_hMutex
    
    mov eax, pPlan
    ret
ModelInvoker_Invoke endp

; ============================================================================
; BuildOllamaPayload - Build JSON payload for Ollama
; Returns: Pointer to payload string in eax
; ============================================================================
BuildOllamaPayload proc
    LOCAL szPayload db MAX_BUFFER_SIZE dup(0)
    LOCAL dwOffset:DWORD
    
    ; Build JSON: {"model":"llama2","prompt":"...","temperature":0.7,"num_predict":1024}
    mov dwOffset, 0
    
    lea eax, szPayload
    mov ecx, [esp+4]  ; pszWish
    
    ; Simplified JSON building (no real JSON escape - just for demo)
    invoke wsprintfA, addr szPayload, addr szJSONFormat, 
        addr g_szModel, ecx, g_fTemperature, g_dwMaxTokens
    
    lea eax, szPayload
    ret
BuildOllamaPayload endp

; ============================================================================
; ParseOllamaResponse - Parse Ollama JSON response into EXECUTION_PLAN
; Returns: Pointer to EXECUTION_PLAN in eax
; ============================================================================
ParseOllamaResponse proc
    LOCAL pPlan:DWORD
    LOCAL pszResponse:DWORD
    LOCAL pszAction:DWORD
    LOCAL pszDest:DWORD
    
    ; Allocate plan structure
    MemAlloc sizeof EXECUTION_PLAN
    mov pPlan, eax
    
    ; Extract "response" field from JSON
    lea eax, g_szLastResponse
    mov pszResponse, eax
    
    ; Look for "response" key
    invoke lstrcpy, [eax + offset EXECUTION_PLAN.szWish], pszResponse
    
    ; Initialize plan
    mov eax, pPlan
    mov EXECUTION_PLAN ptr [eax].dwActionCount, 1
    mov EXECUTION_PLAN ptr [eax].dwStatus, 2  ; READY
    
    ; Set created timestamp
    invoke GetTickCount
    mov ecx, pPlan
    mov [ecx + offset EXECUTION_PLAN.qwCreated], eax
    
    mov eax, pPlan
    ret
ParseOllamaResponse endp

; ============================================================================
; ModelInvoker_InvokeAsync - Asynchronous invocation
; Input: pszWish, pfnCallback
; ============================================================================
ModelInvoker_InvokeAsync proc pszWish:DWORD, pfnCallback:DWORD
    LOCAL hThread:DWORD
    
    ; Create thread to do async work
    invoke CreateThread, NULL, 0, offset AsyncInvokeWorker, pszWish, 0, NULL
    mov hThread, eax
    
    .if hThread != 0
        invoke CloseHandle, hThread
    .endif
    
    ret
ModelInvoker_InvokeAsync endp

; ============================================================================
; AsyncInvokeWorker - Worker thread for async invocation
; ============================================================================
AsyncInvokeWorker proc pszWish:DWORD
    LOCAL pPlan:DWORD
    
    invoke ModelInvoker_Invoke, pszWish
    mov pPlan, eax
    
    ; Call callback if provided
    ; InvokeCallback would call the callback with the plan
    
    ret
AsyncInvokeWorker endp

; ============================================================================
; ModelInvoker_SetBackend - Set LLM backend
; Input: dwBackend (0=Ollama, 1=Claude, 2=OpenAI, 3=LocalGGUF)
; ============================================================================
ModelInvoker_SetBackend proc dwBackend:DWORD
    mov eax, dwBackend
    mov g_Backend, eax
    ret
ModelInvoker_SetBackend endp

; ============================================================================
; ModelInvoker_SetEndpoint - Set LLM endpoint URL
; Input: pszEndpoint
; ============================================================================
ModelInvoker_SetEndpoint proc pszEndpoint:DWORD
    szCopy addr g_szEndpoint, pszEndpoint
    ret
ModelInvoker_SetEndpoint endp

; ============================================================================
; ModelInvoker_SetModel - Set model name
; Input: pszModel
; ============================================================================
ModelInvoker_SetModel proc pszModel:DWORD
    szCopy addr g_szModel, pszModel
    ret
ModelInvoker_SetModel endp

; ============================================================================
; ModelInvoker_SetTemperature - Set temperature for generation
; Input: fTemperature (REAL4)
; ============================================================================
ModelInvoker_SetTemperature proc fTemperature:REAL4
    mov eax, fTemperature
    mov g_fTemperature, eax
    ret
ModelInvoker_SetTemperature endp

; ============================================================================
; ModelInvoker_SetMaxTokens - Set max tokens for generation
; Input: dwMaxTokens
; ============================================================================
ModelInvoker_SetMaxTokens proc dwMaxTokens:DWORD
    mov eax, dwMaxTokens
    mov g_dwMaxTokens, eax
    ret
ModelInvoker_SetMaxTokens endp

; ============================================================================
; ModelInvoker_ClearCache - Clear response cache
; ============================================================================
ModelInvoker_ClearCache proc
    .if g_pCacheBuffer != 0
        MemZero g_pCacheBuffer, g_MaxCacheSize
        mov g_CacheSize, 0
    .endif
    ret
ModelInvoker_ClearCache endp

; ============================================================================
; ModelInvoker_CompressResponse - Compress large LLM responses
; Input: pResponse, dwResponseSize
; Returns: Pointer to compressed data in eax, size in edx
; ============================================================================
ModelInvoker_CompressResponse proc pResponse:DWORD, dwResponseSize:DWORD
    LOCAL result:COMPRESSION_RESULT
    
    ; Only compress if response is large enough
    mov eax, dwResponseSize
    cmp eax, 1024  ; 1KB threshold
    jl @NoCompression
    
    ; Compress using brutal MASM for speed
    invoke Compression_Compress, pResponse, dwResponseSize, COMPRESS_METHOD_BRUTAL, addr result
    test eax, eax
    jz @NoCompression
    
    ; Check if compression was beneficial
    mov eax, result.dwCompressedSize
    mov ecx, dwResponseSize
    .if eax < ecx
        ; Compression beneficial, return compressed data
        mov eax, result.pCompressedData
        mov edx, result.dwCompressedSize
        ret
    .endif
    
    ; Compression not beneficial, free compressed data
    invoke Compression_FreeResult, addr result
    
@NoCompression:
    ; Return original data
    mov eax, pResponse
    mov edx, dwResponseSize
    ret
ModelInvoker_CompressResponse endp

; ============================================================================
; ModelInvoker_CacheResponse - Cache response with compression
; Input: pResponse, dwResponseSize
; ============================================================================
ModelInvoker_CacheResponse proc pResponse:DWORD, dwResponseSize:DWORD
    LOCAL result:COMPRESSION_RESULT
    LOCAL pCompressed:DWORD
    LOCAL dwCompressedSize:DWORD
    
    ; Check if cache is enabled
    .if g_bCacheEnabled == 0
        ret
    .endif
    
    ; Only cache if response is significant
    mov eax, dwResponseSize
    cmp eax, 256  ; 256 byte threshold
    jl @NoCache
    
    ; Check if we have cache buffer
    .if g_pCacheBuffer == 0
        MemAlloc g_MaxCacheSize
        mov g_pCacheBuffer, eax
        .if eax == 0
            ret
        .endif
    .endif
    
    ; Compress response if large
    mov eax, dwResponseSize
    cmp eax, 1024  ; 1KB threshold
    jl @StoreUncompressed
    
    ; Compress the response
    invoke Compression_Compress, pResponse, dwResponseSize, COMPRESS_METHOD_BRUTAL, addr result
    test eax, eax
    jz @StoreUncompressed
    
    ; Use compressed data if beneficial
    mov eax, result.dwCompressedSize
    mov ecx, dwResponseSize
    .if eax < ecx
        mov pCompressed, result.pCompressedData
        mov dwCompressedSize, eax
        jmp @StoreData
    .endif
    
    ; Compression not beneficial, free compressed data
    invoke Compression_FreeResult, addr result
    
@StoreUncompressed:
    ; Store uncompressed
    mov pCompressed, pResponse
    mov dwCompressedSize, dwResponseSize
    
@StoreData:
    ; Check if we have space
    mov eax, g_CacheSize
    add eax, dwCompressedSize
    cmp eax, g_MaxCacheSize
    jg @CacheFull
    
    ; Copy to cache
    mov esi, pCompressed
    mov edi, g_pCacheBuffer
    add edi, g_CacheSize
    mov ecx, dwCompressedSize
    rep movsb
    
    ; Update cache size
    mov eax, dwCompressedSize
    add g_CacheSize, eax
    
@CacheFull:
    ; Free compressed data if we used it
    .if pCompressed != pResponse && pCompressed != 0
        invoke Compression_FreeResult, addr result
    .endif
    
@NoCache:
    ret
ModelInvoker_CacheResponse endp

; ============================================================================
; ModelInvoker_GetCachedResponse - Get cached response with decompression
; Input: pQuery, dwQuerySize
; Returns: Pointer to response in eax, size in edx, or 0 if not found
; ============================================================================
ModelInvoker_GetCachedResponse proc pQuery:DWORD, dwQuerySize:DWORD
    LOCAL pResponse:DWORD
    LOCAL dwResponseSize:DWORD
    LOCAL result:COMPRESSION_RESULT
    
    ; Check if cache is enabled and has data
    .if g_bCacheEnabled == 0 || g_pCacheBuffer == 0 || g_CacheSize == 0
        xor eax, eax
        xor edx, edx
        ret
    .endif
    
    ; Simple cache lookup (in full implementation, use hash table)
    ; For now, just return the last response if query matches
    
    ; Check if we have a cached response
    .if g_dwLastResponseLen > 0
        ; Check if response might be compressed (has gzip header)
        mov eax, g_pCacheBuffer
        movzx ecx, word ptr [eax]
        cmp cx, 08B1Fh  ; Gzip header
        jne @ReturnUncompressed
        
        ; Response is compressed, decompress it
        invoke Compression_Decompress, g_pCacheBuffer, g_CacheSize, addr result
        test eax, eax
        jz @ReturnUncompressed
        
        ; Return decompressed data
        mov eax, result.pCompressedData
        mov edx, result.dwCompressedSize
        ret
        
@ReturnUncompressed:
        ; Return uncompressed data
        mov eax, g_pCacheBuffer
        mov edx, g_CacheSize
        ret
    .endif
    
    xor eax, eax
    xor edx, edx
    ret
ModelInvoker_GetCachedResponse endp

; ============================================================================
; ModelInvoker_Cleanup - Cleanup HTTP session
; ============================================================================
ModelInvoker_Cleanup proc
    .if g_hSession != 0
        invoke WinHttpCloseHandle, g_hSession
        mov g_hSession, 0
    .endif
    
    .if g_pCacheBuffer != 0
        MemFree g_pCacheBuffer
        mov g_pCacheBuffer, 0
    .endif
    
    .if g_hMutex != 0
        invoke CloseHandle, g_hMutex
        mov g_hMutex, 0
    .endif
    
    ret
ModelInvoker_Cleanup endp

; ============================================================================
; Data for HTTP
; ============================================================================

.data
    szPost              db "POST", 0
    szGet               db "GET", 0
    
    szJSONFormat        db '{"model":"%s","prompt":"%s","temperature":%f,"num_predict":%d}', 0

end
