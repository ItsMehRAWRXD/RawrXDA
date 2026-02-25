;==============================================================================
; RawrXD_LlamaBridge.asm
; Local LLM inference via dynamic llama.dll/ggml.dll loading (llama.cpp C API)
; No HTTP, no services. Ship `llama.dll` (+ optionally `ggml.dll`) beside the EXE.
;
; Build (from a VS2022 x64 tools environment):
;   ml64.exe /nologo /c /Fo RawrXD_LlamaBridge.obj /I . RawrXD_LlamaBridge.asm
;
; Notes:
; - This module targets the "modern" llama.cpp C API commonly shipped in llama.dll
;   (model-based tokenize, decode via llama_batch, token_to_piece).
; - If your llama.dll is from a different API era, symbol resolution will fail
;   and the CLI will print which export is missing.
;==============================================================================

OPTION CASEMAP:NONE

include rawrxd_win64.inc

;------------------------------------------------------------------------------
; Constants
;------------------------------------------------------------------------------
LLAMA_MAX_PROMPT_TOKENS     EQU 4096
LLAMA_PIECE_BUF_BYTES       EQU 512
LLAMA_MAX_GENERATE_TOKENS   EQU 512

;------------------------------------------------------------------------------
; llama.cpp minimal structs (ABI must match the llama.dll you ship)
;------------------------------------------------------------------------------
; We do not rely on these being complete; we only write fields we use.

llama_batch STRUCT
    n_tokens        SDWORD ?
    token           QWORD ?     ; llama_token*
    embd            QWORD ?     ; float* (unused)
    pos             QWORD ?     ; llama_pos*
    n_seq_id        QWORD ?     ; int32_t*
    seq_id          QWORD ?     ; llama_seq_id** (per-token)
    logits          QWORD ?     ; int8_t* (per-token)
    all_logits      BYTE  ?
    _pad            BYTE 7 DUP(?)
llama_batch ENDS

;------------------------------------------------------------------------------
; Bridge state
;------------------------------------------------------------------------------
LlamaBridge STRUCT
    ; DLL handles
    hGgml           QWORD ?
    hLlama          QWORD ?

    ; Function pointers (resolved via GetProcAddress)
    fp_backend_init         QWORD ?
    fp_backend_free         QWORD ?
    fp_load_model_from_file QWORD ?
    fp_free_model           QWORD ?
    fp_new_context_with_model QWORD ?
    fp_free_ctx             QWORD ? ; typically: llama_free
    fp_tokenize             QWORD ? ; model-based tokenize
    fp_decode               QWORD ?
    fp_get_logits           QWORD ?
    fp_n_vocab              QWORD ?
    fp_token_eos            QWORD ?
    fp_token_to_piece       QWORD ?
    fp_token_to_str         QWORD ? ; optional fallback: returns const char*
    fp_model_default_params QWORD ? ; optional
    fp_context_default_params QWORD ? ; optional
    fp_ggml_backend_load_all  QWORD ? ; from ggml.dll (required for some builds)
    fp_model_get_vocab      QWORD ?
    fp_vocab_n_tokens       QWORD ?
    fp_vocab_eos            QWORD ?
    fp_batch_get_one        QWORD ?

    ; Session objects
    pModel          QWORD ?
    pCtx            QWORD ?
    pVocab          QWORD ?         ; llama_vocab*
    nVocab          SDWORD ?
    eosToken        SDWORD ?
    stopRequested   SDWORD ?    ; set by LlamaBridge_RequestStop
    _padStop        SDWORD ?

    ; Parameter blobs (struct-by-value arguments are passed indirectly in common builds)
    pModelParams    QWORD ?     ; points to model params blob
    pCtxParams      QWORD ?     ; points to context params blob

    ; Buffers / batch backing memory
    pTokens         QWORD ?     ; int32[LLAMA_MAX_PROMPT_TOKENS]
    pPos            QWORD ?     ; int32[LLAMA_MAX_PROMPT_TOKENS]
    pNSeqId         QWORD ?     ; int32[LLAMA_MAX_PROMPT_TOKENS]
    pSeqIdPtrs      QWORD ?     ; QWORD[LLAMA_MAX_PROMPT_TOKENS] -> &seqId0
    pLogitsFlags    QWORD ?     ; int8[LLAMA_MAX_PROMPT_TOKENS]
    seqId0          SDWORD ?    ; 0
    _pad2           SDWORD ?

    batch           llama_batch <>

    ; Scratch
    pieceBuf        BYTE LLAMA_PIECE_BUF_BYTES DUP(?)
LlamaBridge ENDS

;------------------------------------------------------------------------------
; Exports
;------------------------------------------------------------------------------
PUBLIC LlamaBridge_CreateA
PUBLIC LlamaBridge_Destroy
PUBLIC LlamaBridge_GenerateStreamA
PUBLIC LlamaBridge_RequestStop

;------------------------------------------------------------------------------
; Imports (rawrxd_win64.inc already declares most; add missing)
;------------------------------------------------------------------------------
EXTERN GetCommandLineA:PROC
EXTERN GetModuleFileNameA:PROC

;------------------------------------------------------------------------------
; Data
;------------------------------------------------------------------------------
.data
align 16

szGgmlDll           BYTE "ggml.dll", 0
szGgmlBaseDll       BYTE "ggml-base.dll", 0
szLlamaDll          BYTE "llama.dll", 0
szGgmlBackendLoadAll BYTE "ggml_backend_load_all", 0

sz_backend_init     BYTE "llama_backend_init", 0
sz_backend_free     BYTE "llama_backend_free", 0
sz_load_model       BYTE "llama_load_model_from_file", 0
sz_free_model       BYTE "llama_free_model", 0
sz_new_ctx          BYTE "llama_new_context_with_model", 0
sz_init_from_model  BYTE "llama_init_from_model", 0
sz_free_ctx         BYTE "llama_free", 0
sz_free_ctx2        BYTE "llama_free_context", 0
sz_tokenize         BYTE "llama_tokenize", 0
sz_decode           BYTE "llama_decode", 0
sz_get_logits       BYTE "llama_get_logits", 0
sz_n_vocab          BYTE "llama_n_vocab", 0
sz_token_eos        BYTE "llama_token_eos", 0
sz_token_to_piece   BYTE "llama_token_to_piece", 0
sz_token_to_str     BYTE "llama_token_to_str", 0
sz_model_default    BYTE "llama_model_default_params", 0
sz_ctx_default      BYTE "llama_context_default_params", 0
sz_model_load       BYTE "llama_model_load_from_file", 0
sz_model_free       BYTE "llama_model_free", 0
sz_model_get_vocab  BYTE "llama_model_get_vocab", 0
sz_vocab_n_tokens   BYTE "llama_vocab_n_tokens", 0
sz_vocab_eos        BYTE "llama_vocab_eos", 0
sz_batch_get_one    BYTE "llama_batch_get_one", 0

; Prefix for error messages
szErrPrefix         BYTE "[llama] ", 0
szErrLoadDll        BYTE "failed to load llama.dll", 13, 10, 0
szErrMissingExport  BYTE "missing export: ", 0
szErrModelLoad      BYTE "model load failed", 13, 10, 0
szErrCtxCreate      BYTE "context create failed", 13, 10, 0
szErrTokenize       BYTE "tokenize failed", 13, 10, 0
szErrDecode         BYTE "decode failed", 13, 10, 0

; Debug stage markers (stdout)
szStageAllocOk      BYTE "[bridge] alloc ok", 13, 10, 0
szStageEnter        BYTE "[bridge] enter create", 13, 10, 0
szStageGotModule    BYTE "[bridge] got module path", 13, 10, 0
szStageLoadBase     BYTE "[bridge] load ggml-base attempted", 13, 10, 0
szStageLoadGgml     BYTE "[bridge] load ggml attempted", 13, 10, 0
szStageLoadLlama    BYTE "[bridge] load llama attempted", 13, 10, 0
szStageResolve      BYTE "[bridge] resolving exports", 13, 10, 0
szStageLlamaPath    BYTE "[bridge] llama path: ", 0
szDbgNL             BYTE 13, 10, 0
szStageBackendOk    BYTE "[bridge] backend_init ok", 13, 10, 0
szStageBackendMiss  BYTE "[bridge] backend_init missing", 13, 10, 0
szStageModelCall    BYTE "[bridge] calling load_model", 13, 10, 0
szStageModelOk      BYTE "[bridge] model ok", 13, 10, 0
szStageCtxCall      BYTE "[bridge] calling new_context", 13, 10, 0
szStageCtxOk        BYTE "[bridge] context ok", 13, 10, 0
szStageQueryOk      BYTE "[bridge] queried vocab/eos", 13, 10, 0
szStageCallNVocab   BYTE "[bridge] calling n_vocab", 13, 10, 0
szStageNVocabOk     BYTE "[bridge] n_vocab ok", 13, 10, 0
szStageCallEos      BYTE "[bridge] calling token_eos", 13, 10, 0
szStageEosOk        BYTE "[bridge] token_eos ok", 13, 10, 0
szStageGenEnter     BYTE "[bridge] gen enter", 13, 10, 0
szStageGenTokOk     BYTE "[bridge] gen tokenized", 13, 10, 0
szStageGenDecOk     BYTE "[bridge] gen decoded prompt", 13, 10, 0
szStageTokUseVocab  BYTE "[bridge] gen tokenize arg=vocab", 13, 10, 0
szStageTokUseModel  BYTE "[bridge] gen tokenize arg=model", 13, 10, 0

;------------------------------------------------------------------------------
; Code
;------------------------------------------------------------------------------
.code

;------------------------------------------------------------------------------
; Internal: Overwrite the filename component of a full path buffer in-place.
; RCX = char* full_path_buf (NUL-terminated)
; RDX = char* new_filename  (NUL-terminated)
; Returns RAX = full_path_buf
;------------------------------------------------------------------------------
OverwriteFilenameInPath PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    .endprolog

    mov rsi, rcx                    ; path
    mov rdi, rdx                    ; filename
    mov rbx, rsi                    ; last_sep_ptr (defaults to start)

@@scan:
    mov al, byte ptr [rsi]
    test al, al
    je @@have_tail
    cmp al, '\'
    je @@mark
    cmp al, '/'
    jne @@next
@@mark:
    lea rbx, [rsi+1]                ; tail starts after separator
@@next:
    inc rsi
    jmp @@scan

@@have_tail:
    ; Copy filename into tail
    mov rsi, rdi
    mov rdi, rbx
@@cpy:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    test al, al
    jne @@cpy

    mov rax, rcx
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
OverwriteFilenameInPath ENDP

;------------------------------------------------------------------------------
; Internal: Write a zero-terminated string to stderr
; RCX = char* (NUL-terminated)
;------------------------------------------------------------------------------
WriteStderrA PROC FRAME
    push rbx
    ; Win64 ABI: reserve 32 bytes shadow + 8 bytes for 5th arg, keep 16-byte align.
    sub rsp, 30h
    .endprolog

    mov rbx, rcx
    invoke lstrlenA, rbx
    mov r8, rax                       ; len
    ; Many hosts run with stderr disconnected/redirected; stdout is reliably visible.
    invoke GetStdHandle, STD_OUTPUT_HANDLE
    mov rcx, rax                      ; h
    mov rdx, rbx                      ; buf
    lea r9, [rsp+28h]                 ; written
    mov qword ptr [rsp+20h], 0        ; lpOverlapped
    call WriteFile

    add rsp, 30h
    pop rbx
    ret
WriteStderrA ENDP

;------------------------------------------------------------------------------
; Internal: Resolve export or return 0
; RCX = hModule, RDX = char* name
; Returns RAX = proc or 0
;------------------------------------------------------------------------------
Resolve PROC FRAME
    ; Win64 ABI: ensure 16-byte alignment at the CALL site.
    ; On entry RSP is typically 8 mod 16, so reserve 0x28 (shadow + align).
    sub rsp, 28h
    .endprolog
    call GetProcAddress
    add rsp, 28h
    ret
Resolve ENDP

;------------------------------------------------------------------------------
; LlamaBridge_CreateA
; RCX = model_path (char*)
; Returns RAX = LlamaBridge* or 0
;------------------------------------------------------------------------------
LlamaBridge_CreateA PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    ; 4 pushes => RSP is 8 mod 16 here; reserve 0x68 to restore 16B alignment.
    sub rsp, 68h
    .endprolog

    mov rsi, rcx                      ; model_path
    invoke WriteStderrA, OFFSET szStageEnter


    ; Allocate bridge state (VirtualAlloc so we can VirtualFree cleanly)
    invoke VirtualAlloc, 0, sizeof LlamaBridge, MEM_COMMIT or MEM_RESERVE, PAGE_READWRITE
    test rax, rax
    jz  @@fail
    mov rdi, rax                      ; bridge*
    invoke WriteStderrA, OFFSET szStageAllocOk

    ; -------------------------------------------------------------------------
    ; Load dependencies using full paths relative to the EXE directory.
    ; Some hosts call SetDefaultDllDirectories which removes the current dir
    ; from the search path; absolute paths avoid that class of failure.
    ; -------------------------------------------------------------------------
    sub rsp, 140h                     ; scratch buffer for module path (>= 260)
    lea rbx, [rsp]                    ; rbx = scratch
    ; GetModuleFileNameA(NULL, buf, 260)
    invoke GetModuleFileNameA, 0, rbx, 260
    invoke WriteStderrA, OFFSET szStageGotModule

    ; Try load ggml-base.dll (best-effort)
    invoke OverwriteFilenameInPath, rbx, OFFSET szGgmlBaseDll
    invoke LoadLibraryA, rbx
    ; ignore handle (dependency only)
    invoke WriteStderrA, OFFSET szStageLoadBase

    ; Load ggml.dll
    invoke OverwriteFilenameInPath, rbx, OFFSET szGgmlDll
    invoke LoadLibraryA, rbx
    mov [rdi].LlamaBridge.hGgml, rax
    invoke WriteStderrA, OFFSET szStageLoadGgml
    
    ; Some llama.cpp builds require explicit ggml backend loading before model load.
    ; Note: WriteStderrA clobbers volatile regs; use the saved handle.
    mov rcx, [rdi].LlamaBridge.hGgml
    test rcx, rcx
    jz @@skip_ggml_backends
    sub rsp, 20h
    lea rdx, szGgmlBackendLoadAll
    call GetProcAddress
    add rsp, 20h
    mov [rdi].LlamaBridge.fp_ggml_backend_load_all, rax
    test rax, rax
    jz @@skip_ggml_backends
    sub rsp, 20h
    call rax
    add rsp, 20h
@@skip_ggml_backends:

    ; Load llama.dll
    invoke OverwriteFilenameInPath, rbx, OFFSET szLlamaDll
    invoke WriteStderrA, OFFSET szStageLoadLlama
    invoke LoadLibraryA, rbx
    test rax, rax
    jnz @@llama_loaded
    invoke WriteStderrA, OFFSET szErrLoadDll
    add rsp, 140h
    jmp @@fail_free_bridge

@@llama_loaded:
    mov r12, rax                      ; preserve hModule across calls (non-volatile)
    ; Print actual loaded module path (diagnostic)
    invoke WriteStderrA, OFFSET szStageLlamaPath
    invoke GetModuleFileNameA, r12, rbx, 260
    invoke WriteStderrA, rbx
    invoke WriteStderrA, OFFSET szDbgNL
    mov rax, r12
    mov [rdi].LlamaBridge.hLlama, rax
    mov rbx, rax                      ; hLlama
    add rsp, 140h

    ; Resolve required exports
    ; Optional backend init/free
    invoke WriteStderrA, OFFSET szStageResolve
    ; Resolve backend_init directly (aligned shadow)
    sub rsp, 20h
    mov rcx, rbx
    lea rdx, sz_backend_init
    call GetProcAddress
    add rsp, 20h
    test rax, rax
    jz  @@backend_missing
    mov [rdi].LlamaBridge.fp_backend_init, rax
    invoke WriteStderrA, OFFSET szStageBackendOk
    jmp @@backend_done
@@backend_missing:
    invoke WriteStderrA, OFFSET szStageBackendMiss
@@backend_done:
    invoke Resolve, rbx, OFFSET sz_backend_free
    mov [rdi].LlamaBridge.fp_backend_free, rax

    ; Resolve load-model export using RIP-relative LEA (avoids any immediate truncation issues)
    sub rsp, 20h
    mov rcx, rbx
    lea rdx, sz_load_model
    call GetProcAddress
    add rsp, 20h
    test rax, rax
    jnz @@have_load_model

    ; Fallback (newer API name)
    sub rsp, 20h
    mov rcx, rbx
    lea rdx, sz_model_load
    call GetProcAddress
    add rsp, 20h
    test rax, rax
    jz @@missing_load_model

@@have_load_model:
    mov [rdi].LlamaBridge.fp_load_model_from_file, rax

    invoke Resolve, rbx, OFFSET sz_free_model
    test rax, rax
    jnz @@have_free_model

    ; Fallback (newer API name)
    invoke Resolve, rbx, OFFSET sz_model_free
    test rax, rax
    jz @@missing_free_model

@@have_free_model:
    mov [rdi].LlamaBridge.fp_free_model, rax

    invoke Resolve, rbx, OFFSET sz_new_ctx
    test rax, rax
    jnz @@have_new_ctx
    ; Fallback name in some releases
    invoke Resolve, rbx, OFFSET sz_init_from_model
    test rax, rax
    jz @@missing_new_ctx
@@have_new_ctx:
    mov [rdi].LlamaBridge.fp_new_context_with_model, rax
    
    invoke Resolve, rbx, OFFSET sz_free_ctx
    test rax, rax
    jnz @@have_free_ctx
    ; Fallback name in some releases
    invoke Resolve, rbx, OFFSET sz_free_ctx2
    test rax, rax
    jz @@missing_free_ctx
@@have_free_ctx:
    mov [rdi].LlamaBridge.fp_free_ctx, rax

    invoke Resolve, rbx, OFFSET sz_tokenize
    test rax, rax
    jz @@missing_tokenize
    mov [rdi].LlamaBridge.fp_tokenize, rax

    invoke Resolve, rbx, OFFSET sz_decode
    test rax, rax
    jz @@missing_decode
    mov [rdi].LlamaBridge.fp_decode, rax

    invoke Resolve, rbx, OFFSET sz_get_logits
    test rax, rax
    jz @@missing_get_logits
    mov [rdi].LlamaBridge.fp_get_logits, rax

    invoke Resolve, rbx, OFFSET sz_n_vocab
    test rax, rax
    jz @@missing_n_vocab
    mov [rdi].LlamaBridge.fp_n_vocab, rax

    invoke Resolve, rbx, OFFSET sz_token_eos
    test rax, rax
    jz @@missing_token_eos
    mov [rdi].LlamaBridge.fp_token_eos, rax

    invoke Resolve, rbx, OFFSET sz_token_to_piece
    test rax, rax
    jnz @@have_token_to_piece
    ; Fallback: token_to_str (const char*) in some releases
    invoke Resolve, rbx, OFFSET sz_token_to_str
    mov [rdi].LlamaBridge.fp_token_to_str, rax
    test rax, rax
    jz @@missing_token_to_piece
    xor eax, eax
@@have_token_to_piece:
    mov [rdi].LlamaBridge.fp_token_to_piece, rax

    ; Optional: default params helpers (best-effort)
    invoke Resolve, rbx, OFFSET sz_model_default
    mov [rdi].LlamaBridge.fp_model_default_params, rax
    invoke Resolve, rbx, OFFSET sz_ctx_default
    mov [rdi].LlamaBridge.fp_context_default_params, rax

    ; Optional (but preferred): vocab-based API surface (newer llama.cpp)
    invoke Resolve, rbx, OFFSET sz_model_get_vocab
    mov [rdi].LlamaBridge.fp_model_get_vocab, rax
    invoke Resolve, rbx, OFFSET sz_vocab_n_tokens
    mov [rdi].LlamaBridge.fp_vocab_n_tokens, rax
    invoke Resolve, rbx, OFFSET sz_vocab_eos
    mov [rdi].LlamaBridge.fp_vocab_eos, rax
    invoke Resolve, rbx, OFFSET sz_batch_get_one
    mov [rdi].LlamaBridge.fp_batch_get_one, rax

    ; Allocate token/batch backing buffers
    invoke GetProcessHeap
    mov rbx, rax

    ; Allocate parameter blobs (oversized, to tolerate minor ABI drift)
    ; Model params blob (1024 bytes)
    mov rcx, rbx
    xor edx, edx
    mov r8d, 1024
    call HeapAlloc
    mov [rdi].LlamaBridge.pModelParams, rax
    test rax, rax
    jz @@fail_destroy

    ; Ctx params blob (1024 bytes)
    mov rcx, rbx
    xor edx, edx
    mov r8d, 1024
    call HeapAlloc
    mov [rdi].LlamaBridge.pCtxParams, rax
    test rax, rax
    jz @@fail_destroy

    ; Zero both blobs
    xor eax, eax
    mov rcx, [rdi].LlamaBridge.pModelParams
    mov r8d, 1024/8
@@zp1:
    mov qword ptr [rcx], rax
    add rcx, 8
    dec r8d
    jnz @@zp1
    mov rcx, [rdi].LlamaBridge.pCtxParams
    mov r8d, 1024/8
@@zp2:
    mov qword ptr [rcx], rax
    add rcx, 8
    dec r8d
    jnz @@zp2

    ; Fill defaults if exports exist (sret: RCX = out struct)
    mov rax, [rdi].LlamaBridge.fp_model_default_params
    test rax, rax
    jz @@skip_model_default
    sub rsp, 20h
    mov rcx, [rdi].LlamaBridge.pModelParams
    call rax
    add rsp, 20h
@@skip_model_default:

    mov rax, [rdi].LlamaBridge.fp_context_default_params
    test rax, rax
    jz @@skip_ctx_default
    sub rsp, 20h
    mov rcx, [rdi].LlamaBridge.pCtxParams
    call rax
    add rsp, 20h
@@skip_ctx_default:

    ; tokens: int32[LLAMA_MAX_PROMPT_TOKENS]
    mov rcx, rbx
    xor edx, edx
    mov r8d, (LLAMA_MAX_PROMPT_TOKENS * 4)
    call HeapAlloc
    mov [rdi].LlamaBridge.pTokens, rax
    test rax, rax
    jz @@fail_destroy

    ; pos: int32[LLAMA_MAX_PROMPT_TOKENS]
    mov rcx, rbx
    xor edx, edx
    mov r8d, (LLAMA_MAX_PROMPT_TOKENS * 4)
    call HeapAlloc
    mov [rdi].LlamaBridge.pPos, rax
    test rax, rax
    jz @@fail_destroy

    ; n_seq_id: int32[LLAMA_MAX_PROMPT_TOKENS]
    mov rcx, rbx
    xor edx, edx
    mov r8d, (LLAMA_MAX_PROMPT_TOKENS * 4)
    call HeapAlloc
    mov [rdi].LlamaBridge.pNSeqId, rax
    test rax, rax
    jz @@fail_destroy

    ; seq_id ptrs: QWORD[LLAMA_MAX_PROMPT_TOKENS]
    mov rcx, rbx
    xor edx, edx
    mov r8d, (LLAMA_MAX_PROMPT_TOKENS * 8)
    call HeapAlloc
    mov [rdi].LlamaBridge.pSeqIdPtrs, rax
    test rax, rax
    jz @@fail_destroy

    ; logits flags: int8[LLAMA_MAX_PROMPT_TOKENS]
    mov rcx, rbx
    xor edx, edx
    mov r8d, LLAMA_MAX_PROMPT_TOKENS
    call HeapAlloc
    mov [rdi].LlamaBridge.pLogitsFlags, rax
    test rax, rax
    jz @@fail_destroy

    ; backend init (optional)
    mov rax, [rdi].LlamaBridge.fp_backend_init
    test rax, rax
    jz @@skip_backend_init
    sub rsp, 20h
    xor ecx, ecx
    call rax
    add rsp, 20h
@@skip_backend_init:
    
    ; Load model: llama_load_model_from_file(path, params)
    ; In common MSVC x64 builds, the struct-by-value `params` is passed indirectly
    ; as a pointer to caller-allocated storage. We always pass a valid blob here.
    invoke WriteStderrA, OFFSET szStageModelCall
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_load_model_from_file
    mov rcx, rsi                      ; path_model (char*)
    mov rdx, [rdi].LlamaBridge.pModelParams
    call rax
    add rsp, 20h
    test rax, rax
    jnz @@model_ok
    invoke WriteStderrA, OFFSET szErrModelLoad
    jmp @@fail_destroy

@@model_ok:
    mov [rdi].LlamaBridge.pModel, rax
    invoke WriteStderrA, OFFSET szStageModelOk
    
    ; Create context: llama_new_context_with_model(model, params)
    invoke WriteStderrA, OFFSET szStageCtxCall
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_new_context_with_model
    mov rcx, [rdi].LlamaBridge.pModel
    mov rdx, [rdi].LlamaBridge.pCtxParams
    call rax
    add rsp, 20h
    test rax, rax
    jnz @@ctx_ok
    invoke WriteStderrA, OFFSET szErrCtxCreate
    jmp @@fail_destroy

@@ctx_ok:
    mov [rdi].LlamaBridge.pCtx, rax
    invoke WriteStderrA, OFFSET szStageCtxOk

    ; Cache vocab/eos. Prefer the explicit vocab API if present; it matches
    ; the export surface of the shipped llama.dll and avoids signature drift
    ; on llama_n_vocab/llama_token_eos across releases.
    mov rax, [rdi].LlamaBridge.fp_model_get_vocab
    test rax, rax
    jz @@fallback_vocab
    mov rdx, [rdi].LlamaBridge.fp_vocab_n_tokens
    test rdx, rdx
    jz @@fallback_vocab
    mov r8,  [rdi].LlamaBridge.fp_vocab_eos
    test r8, r8
    jz @@fallback_vocab

    ; pVocab = llama_model_get_vocab(model)
    sub rsp, 20h
    mov rcx, [rdi].LlamaBridge.pModel
    call rax
    add rsp, 20h
    mov [rdi].LlamaBridge.pVocab, rax
    test rax, rax
    jz @@fallback_vocab

    ; nVocab = llama_vocab_n_tokens(vocab)
    invoke WriteStderrA, OFFSET szStageCallNVocab
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_vocab_n_tokens
    mov rcx, [rdi].LlamaBridge.pVocab
    call rax
    add rsp, 20h
    mov [rdi].LlamaBridge.nVocab, eax
    invoke WriteStderrA, OFFSET szStageNVocabOk

    ; eosToken = llama_vocab_eos(vocab)
    invoke WriteStderrA, OFFSET szStageCallEos
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_vocab_eos
    mov rcx, [rdi].LlamaBridge.pVocab
    call rax
    add rsp, 20h
    mov [rdi].LlamaBridge.eosToken, eax
    invoke WriteStderrA, OFFSET szStageEosOk
    invoke WriteStderrA, OFFSET szStageQueryOk
    jmp @@vocab_done

@@fallback_vocab:
    ; Fallback: older APIs
    invoke WriteStderrA, OFFSET szStageCallNVocab
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_n_vocab
    mov rcx, [rdi].LlamaBridge.pCtx
    call rax
    add rsp, 20h
    mov [rdi].LlamaBridge.nVocab, eax
    invoke WriteStderrA, OFFSET szStageNVocabOk

    invoke WriteStderrA, OFFSET szStageCallEos
    sub rsp, 20h
    mov rax, [rdi].LlamaBridge.fp_token_eos
    mov rcx, [rdi].LlamaBridge.pCtx
    call rax
    add rsp, 20h
    mov [rdi].LlamaBridge.eosToken, eax
    invoke WriteStderrA, OFFSET szStageEosOk
    invoke WriteStderrA, OFFSET szStageQueryOk

@@vocab_done:

    mov rax, rdi
    jmp @@done

@@missing_load_model:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_load_model
    jmp @@fail_destroy
@@missing_free_model:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_free_model
    jmp @@fail_destroy
@@missing_new_ctx:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_new_ctx
    jmp @@fail_destroy
@@missing_free_ctx:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_free_ctx
    jmp @@fail_destroy
@@missing_tokenize:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_tokenize
    jmp @@fail_destroy
@@missing_decode:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_decode
    jmp @@fail_destroy
@@missing_get_logits:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_get_logits
    jmp @@fail_destroy
@@missing_n_vocab:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_n_vocab
    jmp @@fail_destroy
@@missing_token_eos:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_token_eos
    jmp @@fail_destroy
@@missing_token_to_piece:
    invoke WriteStderrA, OFFSET szErrPrefix
    invoke WriteStderrA, OFFSET szErrMissingExport
    invoke WriteStderrA, OFFSET sz_token_to_piece
    jmp @@fail_destroy

@@fail_destroy:
    mov rcx, rdi
    call LlamaBridge_Destroy
    xor eax, eax
    jmp @@done

@@fail_free_bridge:
    invoke VirtualFree, rdi, 0, 8000h ; MEM_RELEASE
@@fail:
    xor eax, eax
@@done:
    add rsp, 68h
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret

LlamaBridge_CreateA ENDP

;------------------------------------------------------------------------------
; LlamaBridge_Destroy
; RCX = LlamaBridge*
;------------------------------------------------------------------------------
LlamaBridge_Destroy PROC FRAME
    push rbx
    push rsi
    sub rsp, 20h
    .endprolog

    mov rbx, rcx
    test rbx, rbx
    jz @@done

    ; Free context
    mov rax, [rbx].LlamaBridge.fp_free_ctx
    test rax, rax
    jz @@skip_ctx
    mov rcx, [rbx].LlamaBridge.pCtx
    test rcx, rcx
    jz @@skip_ctx
    sub rsp, 20h
    call rax
    add rsp, 20h
    mov qword ptr [rbx].LlamaBridge.pCtx, 0
@@skip_ctx:

    ; Free model
    mov rax, [rbx].LlamaBridge.fp_free_model
    test rax, rax
    jz @@skip_model
    mov rcx, [rbx].LlamaBridge.pModel
    test rcx, rcx
    jz @@skip_model
    sub rsp, 20h
    call rax
    add rsp, 20h
    mov qword ptr [rbx].LlamaBridge.pModel, 0
@@skip_model:

    ; backend free (optional)
    mov rax, [rbx].LlamaBridge.fp_backend_free
    test rax, rax
    jz @@skip_backend_free
    sub rsp, 20h
    call rax
    add rsp, 20h
@@skip_backend_free:

    ; Free backing buffers
    invoke GetProcessHeap
    mov rsi, rax

    ; Free param blobs
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pModelParams
    test r8, r8
    jz @@skip_mparams
    call HeapFree
@@skip_mparams:
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pCtxParams
    test r8, r8
    jz @@skip_cparams
    call HeapFree
@@skip_cparams:

    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pTokens
    test r8, r8
    jz @@skip_tokens
    call HeapFree
@@skip_tokens:
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pPos
    test r8, r8
    jz @@skip_pos
    call HeapFree
@@skip_pos:
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pNSeqId
    test r8, r8
    jz @@skip_nseq
    call HeapFree
@@skip_nseq:
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pSeqIdPtrs
    test r8, r8
    jz @@skip_seqptr
    call HeapFree
@@skip_seqptr:
    mov rcx, rsi
    xor edx, edx
    mov r8, [rbx].LlamaBridge.pLogitsFlags
    test r8, r8
    jz @@skip_logitsflags
    call HeapFree
@@skip_logitsflags:

    ; Free libraries
    mov rcx, [rbx].LlamaBridge.hLlama
    test rcx, rcx
    jz @@skip_llama
    call FreeLibrary
@@skip_llama:
    mov rcx, [rbx].LlamaBridge.hGgml
    test rcx, rcx
    jz @@skip_ggml
    call FreeLibrary
@@skip_ggml:

    ; Free bridge struct
    invoke VirtualFree, rbx, 0, 8000h ; MEM_RELEASE

@@done:
    add rsp, 20h
    pop rsi
    pop rbx
    ret
LlamaBridge_Destroy ENDP

;------------------------------------------------------------------------------
; Internal: prepare batch arrays for [0..n_tokens)
; RCX = bridge*, EDX = n_tokens, R8D = pos_base
; Sets per-token pos, n_seq_id, seq_id_ptrs, logits_flags (only last=1)
;------------------------------------------------------------------------------
PrepareBatch PROC FRAME
    push rbx
    push rsi
    push rdi
    sub rsp, 20h
    .endprolog

    mov rbx, rcx
    mov esi, edx                    ; n_tokens
    mov edi, r8d                    ; pos_base

    ; seqId0 is stored in bridge
    lea r9, [rbx].LlamaBridge.seqId0

    xor ecx, ecx
@@loop:
    cmp ecx, esi
    jge @@done

    ; pos[i] = pos_base + i
    mov rax, [rbx].LlamaBridge.pPos
    lea edx, [edi + ecx]
    mov dword ptr [rax + rcx*4], edx

    ; n_seq_id[i] = 1
    mov rax, [rbx].LlamaBridge.pNSeqId
    mov dword ptr [rax + rcx*4], 1

    ; seq_id_ptrs[i] = &seqId0
    mov rax, [rbx].LlamaBridge.pSeqIdPtrs
    mov qword ptr [rax + rcx*8], r9

    ; logits_flags[i] = (i == n_tokens-1)
    mov rax, [rbx].LlamaBridge.pLogitsFlags
    mov byte ptr [rax + rcx], 0
    lea edx, [esi - 1]
    cmp ecx, edx
    jne @@next
    mov byte ptr [rax + rcx], 1

@@next:
    inc ecx
    jmp @@loop

@@done:
    add rsp, 20h
    pop rdi
    pop rsi
    pop rbx
    ret
PrepareBatch ENDP

;------------------------------------------------------------------------------
; LlamaBridge_GenerateStreamA
; RCX = bridge*
; RDX = prompt (char*, NUL terminated)
; R8  = callback (void (*)(const char* piece, int len, void* user))
; R9  = user ptr
;
; Returns EAX = 1 on success, 0 on failure.
;------------------------------------------------------------------------------
LlamaBridge_GenerateStreamA PROC FRAME
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    sub rsp, 60h
    .endprolog

    mov rbx, rcx                    ; bridge
    mov rsi, rdx                    ; prompt
    mov r12, r8                     ; callback
    mov r13, r9                     ; user
    mov dword ptr [rbx].LlamaBridge.stopRequested, 0
    invoke WriteStderrA, OFFSET szStageGenEnter

    ; This llama.dll's llama_tokenize asserts/crashes on negative text_len.
    ; Always pass an explicit byte length.
    sub rsp, 20h
    mov rcx, rsi
    call lstrlenA
    add rsp, 20h
    mov r15d, eax                   ; prompt_len

    ; Tokenize prompt:
    ; Verified by disassembly of this llama.dll build:
    ;   int llama_tokenize(vocab, text, text_len, tokens, n_max, add_special, parse_special)
    ; 7 args => shadow(0x20) + 3 stack args(0x18) => round up to 0x40
    sub rsp, 40h
    ; Prefer vocab handle when available; fall back to model.
    mov rdi, [rbx].LlamaBridge.pVocab
    test rdi, rdi
    jz @@tok_use_model_v7
    invoke WriteStderrA, OFFSET szStageTokUseVocab
    jmp @@tok_have_arg_v7
@@tok_use_model_v7:
    mov rdi, [rbx].LlamaBridge.pModel
    invoke WriteStderrA, OFFSET szStageTokUseModel
@@tok_have_arg_v7:
    mov rcx, rdi
    mov rdx, rsi
    mov r8d, r15d                   ; text_len
    mov r9, [rbx].LlamaBridge.pTokens
    mov dword ptr [rsp+20h], LLAMA_MAX_PROMPT_TOKENS
    mov dword ptr [rsp+28h], 1      ; add_special
    mov dword ptr [rsp+30h], 0      ; parse_special
    mov rax, [rbx].LlamaBridge.fp_tokenize
    call rax
    add rsp, 40h

    mov r14d, eax                   ; n_prompt_tokens
    cmp r14d, 1
    jge @@tok_ok
    invoke WriteStderrA, OFFSET szErrTokenize
    xor eax, eax
    jmp @@done

@@tok_ok:
    invoke WriteStderrA, OFFSET szStageGenTokOk
    ; Build a batch using llama.dll helper to avoid ABI/layout mismatches.
    lea rdi, [rbx].LlamaBridge.batch
    mov rax, [rbx].LlamaBridge.fp_batch_get_one
    test rax, rax
    jz @@fail
    sub rsp, 20h
    mov rcx, rdi                    ; out batch*
    mov rdx, [rbx].LlamaBridge.pTokens
    mov r8d, r14d                   ; n_tokens
    call rax
    add rsp, 20h

    ; Decode prompt
    sub rsp, 20h
    mov rax, [rbx].LlamaBridge.fp_decode
    mov rcx, [rbx].LlamaBridge.pCtx
    mov rdx, rdi                     ; batch*
    call rax
    add rsp, 20h
    test eax, eax
    jge @@decode_ok
    invoke WriteStderrA, OFFSET szErrDecode
    xor eax, eax
    jmp @@done

@@decode_ok:
    invoke WriteStderrA, OFFSET szStageGenDecOk
    ; Generation loop
    mov r15d, r14d                   ; cur_pos
    xor r14d, r14d                   ; generated

@@gen_loop:
    cmp dword ptr [rbx].LlamaBridge.stopRequested, 0
    jne @@success
    cmp r14d, LLAMA_MAX_GENERATE_TOKENS
    jae @@success

    ; logits = llama_get_logits(ctx)
    sub rsp, 20h
    mov rax, [rbx].LlamaBridge.fp_get_logits
    mov rcx, [rbx].LlamaBridge.pCtx
    call rax
    add rsp, 20h
    test rax, rax
    jz @@fail
    mov rsi, rax                     ; logits*

    ; Argmax over vocab
    xor ecx, ecx                     ; i
    mov edx, [rbx].LlamaBridge.nVocab
    mov eax, 0                       ; best_id
    movss xmm0, dword ptr [rsi]      ; best_logit
@@argmax:
    inc ecx
    cmp ecx, edx
    jae @@argmax_done
    movss xmm1, dword ptr [rsi + rcx*4]
    comiss xmm1, xmm0
    jbe @@argmax
    movss xmm0, xmm1
    mov eax, ecx
    jmp @@argmax
@@argmax_done:
    mov r11d, eax                    ; preserve token id

    ; EOS?
    cmp r11d, [rbx].LlamaBridge.eosToken
    je @@success

    ; Convert token to text and emit via callback.
    ; Preferred: llama_token_to_piece(vocab/model, token, buf, buf_sz, lstrip, special)
    ; Fallback:  llama_token_to_str(model, token) -> const char*
    lea rdi, [rbx].LlamaBridge.pieceBuf
    mov r10, [rbx].LlamaBridge.fp_token_to_piece
    test r10, r10
    jz @@emit_via_str

    ; 6 args => shadow(0x20) + 2 stack args(0x10) => 0x30
    sub rsp, 30h
    mov rcx, [rbx].LlamaBridge.pVocab
    test rcx, rcx
    jnz @F
    mov rcx, [rbx].LlamaBridge.pModel
@@:
    mov edx, r11d                    ; token
    mov r8, rdi                      ; buf
    mov r9d, LLAMA_PIECE_BUF_BYTES
    mov dword ptr [rsp+20h], 0       ; lstrip
    mov dword ptr [rsp+28h], 0       ; special
    call r10
    add rsp, 30h

    mov ecx, eax                     ; piece_len
    jmp @@emit_common

@@emit_via_str:
    mov r10, [rbx].LlamaBridge.fp_token_to_str
    test r10, r10
    jz @@skip_emit
    sub rsp, 20h
    mov rcx, [rbx].LlamaBridge.pVocab
    test rcx, rcx
    jnz @F
    mov rcx, [rbx].LlamaBridge.pModel
@@:
    mov edx, r11d
    call r10
    add rsp, 20h
    test rax, rax
    jz @@skip_emit
    mov rdi, rax                     ; piece ptr
    invoke lstrlenA, rdi
    mov ecx, eax                     ; piece_len

@@emit_common:
    test ecx, ecx
    jle @@skip_emit
    sub rsp, 20h
    mov rcx, rdi                     ; piece ptr
    mov edx, ecx                     ; piece len
    mov r8, r13                      ; user
    call r12                         ; callback
    add rsp, 20h

@@skip_emit:
    ; Decode single token at current position
    ; tokens[0] = token
    mov rdx, [rbx].LlamaBridge.pTokens
    mov eax, r11d
    mov dword ptr [rdx], eax

    ; Build batch for a single token (ABI-safe)
    lea rdi, [rbx].LlamaBridge.batch
    mov rax, [rbx].LlamaBridge.fp_batch_get_one
    sub rsp, 20h
    mov rcx, rdi
    mov rdx, [rbx].LlamaBridge.pTokens
    mov r8d, 1
    call rax
    add rsp, 20h

    sub rsp, 20h
    mov rax, [rbx].LlamaBridge.fp_decode
    mov rcx, [rbx].LlamaBridge.pCtx
    mov rdx, rdi
    call rax
    add rsp, 20h
    test eax, eax
    jl @@fail

    inc r15d                          ; cur_pos++
    inc r14d                          ; generated++
    jmp @@gen_loop

@@success:
    mov eax, 1
    jmp @@done

@@fail:
    xor eax, eax

@@done:
    add rsp, 60h
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LlamaBridge_GenerateStreamA ENDP

;------------------------------------------------------------------------------
; LlamaBridge_RequestStop
; RCX = bridge*
; Best-effort cancellation: causes GenerateStream loop to exit early.
;------------------------------------------------------------------------------
LlamaBridge_RequestStop PROC FRAME
    sub rsp, 20h
    .endprolog

    test rcx, rcx
    jz @@done
    mov dword ptr [rcx].LlamaBridge.stopRequested, 1

@@done:
    add rsp, 20h
    ret
LlamaBridge_RequestStop ENDP

END
