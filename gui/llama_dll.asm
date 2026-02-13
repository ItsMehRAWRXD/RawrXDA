.686
.model flat, stdcall
option casemap:none

include windows.inc
include kernel32.inc
includelib kernel32.lib

.data
    model_path db MAX_PATH dup(0)
    model_handle dd 0
    context_handle dd 0
    
.code

; External C functions from llama.cpp
extern llama_backend_init: proc
extern llama_model_default_params: proc
extern llama_context_default_params: proc
extern llama_load_model_from_file: proc
extern llama_new_context_with_model: proc
extern llama_tokenize: proc
extern llama_eval: proc
extern llama_token_to_piece: proc
extern llama_free_model: proc
extern llama_free: proc

DllMain proc hInstance:HINSTANCE, fdwReason:DWORD, lpvReserved:LPVOID
    mov eax, TRUE
    ret
DllMain endp

; Initialize the model
InitModel proc stdcall modelPath:DWORD
    push ebp
    mov ebp, esp
    
    ; Copy model path
    push esi
    push edi
    mov esi, modelPath
    lea edi, model_path
    mov ecx, MAX_PATH
    rep movsb
    pop edi
    pop esi
    
    ; Initialize backend
    call llama_backend_init
    
    ; Load model
    push offset model_path
    call llama_load_model_from_file
    mov model_handle, eax
    
    ; Create context
    push model_handle
    call llama_new_context_with_model
    mov context_handle, eax
    
    mov eax, 1  ; Success
    pop ebp
    ret 4
InitModel endp

; Generate response
GenerateResponse proc stdcall prompt:DWORD, response:DWORD, maxLen:DWORD
    push ebp
    mov ebp, esp
    
    ; Tokenize input
    sub esp, 1024  ; Token buffer
    lea eax, [ebp-1024]
    push 256       ; Max tokens
    push eax       ; Token buffer
    push prompt    ; Input text
    push model_handle
    call llama_tokenize
    
    ; Evaluate tokens
    push eax       ; Token count
    lea ebx, [ebp-1024]
    push ebx       ; Tokens
    push context_handle
    call llama_eval
    
    ; Generate response (simplified)
    mov edi, response
    mov ecx, maxLen
    mov al, 'O'
    stosb
    mov al, 'K'
    stosb
    mov al, 0
    stosb
    
    mov eax, 1  ; Success
    add esp, 1024
    pop ebp
    ret 12
GenerateResponse endp

; Cleanup
CleanupModel proc stdcall
    push ebp
    mov ebp, esp
    
    cmp context_handle, 0
    je skip_context
    push context_handle
    call llama_free
    mov context_handle, 0
    
skip_context:
    cmp model_handle, 0
    je skip_model
    push model_handle
    call llama_free_model
    mov model_handle, 0
    
skip_model:
    mov eax, 1
    pop ebp
    ret
CleanupModel endp

end DllMain