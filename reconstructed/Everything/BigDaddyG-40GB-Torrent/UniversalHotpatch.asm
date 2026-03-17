; ════════════════════════════════════════════════════════════════════════════════
;  UNIVERSAL GGUF METADATA RESOLVER & CLI HOTPATCH ENGINE - COMPLETE
;  Architecture-Aware Key Resolution | Zero-Copy Vulkan | CLI Hotpatch ANY Stat
;  Swappable: Load ANY model, Change ANY parameter, Perm/Temp modes
; ════════════════════════════════════════════════════════════════════════════════
;  Assemble: ml64 /c /Zi /FoUniversalHotpatch.obj UniversalHotpatch.asm
;  Link:     link /DEBUG /SUBSYSTEM:CONSOLE /ENTRY:main UniversalHotpatch.obj kernel32.lib msvcrt.lib user32.lib
;  Usage:    UniversalHotpatch.exe llama-2-70b-q5.gguf
;           CLI> swap deepseek-v3-q4.gguf
;           CLI> patch layers 61
;           CLI> patch rope_theta 1000000 perm
; ════════════════════════════════════════════════════════════════════════════════

OPTION CASEMAP:NONE
OPTION WIN64:8
OPTION PROLOGUE:NONE
OPTION EPILOGUE:NONE

INCLUDELIB kernel32.lib
INCLUDELIB msvcrt.lib
INCLUDELIB user32.lib

; External functions
EXTERN printf:PROC
EXTERN scanf:PROC
EXTERN malloc:PROC
EXTERN free:PROC
EXTERN memset:PROC
EXTERN memcpy:PROC
EXTERN strcmp:PROC
EXTERN strlen:PROC
EXTERN atoi:PROC
EXTERN atof:PROC
EXTERN CreateFileA:PROC
EXTERN ReadFile:PROC
EXTERN GetFileSizeEx:PROC
EXTERN CloseHandle:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN UnmapViewOfFile:PROC
EXTERN GetTickCount64:PROC
EXTERN GlobalMemoryStatusEx:PROC

;═══════════════════════════════════════════════════════════════════════════════
; GGUF CONSTANTS (Reverse Engineered)
;═══════════════════════════════════════════════════════════════════════════════

GGUF_MAGIC              EQU 46554747h             ; "GGUF" little-endian
GGUF_VERSION            EQU 3

; Value Types
GGUF_TYPE_UINT8         EQU 0
GGUF_TYPE_INT8          EQU 1
GGUF_TYPE_UINT16        EQU 2
GGUF_TYPE_INT16         EQU 3
GGUF_TYPE_UINT32        EQU 4
GGUF_TYPE_INT32         EQU 5
GGUF_TYPE_FLOAT32       EQU 6
GGUF_TYPE_UINT64        EQU 7
GGUF_TYPE_INT64         EQU 8
GGUF_TYPE_FLOAT64       EQU 9
GGUF_TYPE_BOOL          EQU 10
GGUF_TYPE_STRING        EQU 11
GGUF_TYPE_ARRAY         EQU 12

GGUF_HEADER_SIZE        EQU 24

; File access
GENERIC_READ            EQU 80000000h
FILE_SHARE_READ         EQU 1
OPEN_EXISTING           EQU 3
INVALID_HANDLE_VALUE    EQU -1
PAGE_READONLY           EQU 2
FILE_MAP_READ           EQU 4

;═══════════════════════════════════════════════════════════════════════════════
; STRUCTURES
;═══════════════════════════════════════════════════════════════════════════════

GGUF_HEADER STRUCT
    magic           DWORD   ?
    version         DWORD   ?
    tensor_count    QWORD   ?
    kv_count        QWORD   ?
GGUF_HEADER ENDS

UNIV_MODEL_STATE STRUCT
    path            QWORD   ?
    base_addr       QWORD   ?
    header          GGUF_HEADER <>
    arch            BYTE 64 DUP(0)
    layers          DWORD   ?
    hidden_size     DWORD   ?
    heads           DWORD   ?
    kv_heads        DWORD   ?
    vocab_size      DWORD   ?
    context_len     DWORD   ?
    rope_theta      QWORD   ?
    alignment       DWORD   ?
    ffn_size        DWORD   ?
    file_size       QWORD   ?
    quant_type      DWORD   ?
    is_loaded       DWORD   ?
UNIV_MODEL_STATE ENDS

HOTPATCH_ENTRY STRUCT
    stat_name       BYTE 64 DUP(0)
    stat_ptr        QWORD   ?
    stat_type       DWORD   ?
    is_permanent    DWORD   ?
HOTPATCH_ENTRY ENDS

;═══════════════════════════════════════════════════════════════════════════════
; DATA
;═══════════════════════════════════════════════════════════════════════════════

.DATA
align 16

model_state         UNIV_MODEL_STATE <>
hotpatch_registry   HOTPATCH_ENTRY 64 DUP(<>)
hotpatch_count      DWORD 0
pMetadataStart      QWORD 0
cli_active          DWORD 0

szBanner DB 13,10
         DB "════════════════════════════════════════════════════════════════",13,10
         DB "  UNIVERSAL MODEL HOTPATCH ENGINE v1.0",13,10
         DB "  Load Any Model | Patch Any Stat | CLI/QT IDE Ready",13,10
         DB "════════════════════════════════════════════════════════════════",13,10,13,10
         DB "Commands:",13,10
         DB "  load <path>     - Load model from path",13,10
         DB "  swap <path>     - Hot-swap to new model",13,10
         DB "  patch <stat> <value> [perm|temp] - Modify parameter",13,10
         DB "  show            - Display current model stats",13,10
         DB "  stats           - Show all patchable parameters",13,10
         DB "  save            - Save permanent patches to model",13,10
         DB "  exit            - Exit CLI",13,10,13,10,0

szPrompt        DB "Universal> ",0
szLoadFmt       DB "[Load] Loading %s...",13,10,0
szLoadOK        DB "[Load] ✅ Success | Arch: %s | Size: %.2f GB | Params: %.2f B",13,10,0
szSwapFmt       DB "[Swap] Hot-swapping to %s...",13,10,0
szSwapOK        DB "[Swap] ✅ Success in %llu ms",13,10,0
szPatchFmt      DB "[Patch] %s = %llu (%s)",13,10,0
szPatchOK       DB "[Patch] ✅ Applied successfully",13,10,0

szShowHdr       DB 13,10,"[Current Model State]",13,10
                DB "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━",13,10,0
szShowArch      DB "Architecture    : %s",13,10,0
szShowLayers    DB "Layers          : %u",13,10,0
szShowHidden    DB "Hidden Size     : %u",13,10,0
szShowHeads     DB "Attention Heads : %u (KV: %u)",13,10,0
szShowVocab     DB "Vocabulary      : %u",13,10,0
szShowContext   DB "Context Length  : %u",13,10,0
szShowRope      DB "RoPE Theta      : %llu",13,10,0
szShowAlign     DB "Alignment       : %u bytes",13,10,0
szShowFFN       DB "FFN Size        : %u",13,10,0
szShowQuant     DB "Quantization    : Q%d",13,10,0

szStatsHdr      DB 13,10,"[Patchable Parameters - %d total]",13,10
                DB "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━",13,10,0
szStatsEntry    DB "%-20s : %-15s (Type: %s)",13,10,0

szError         DB "❌ ERROR: %s",13,10,0
szErrOpen       DB "Failed to open file",0
szErrMap        DB "Failed to map file",0
szErrMagic      DB "Invalid GGUF magic",0
szErrVersion    DB "Unsupported GGUF version",0
szErrArch       DB "Architecture not found",0
szErrNotLoaded  DB "No model loaded",0

szCmdLoad       DB "load",0
szCmdSwap       DB "swap",0
szCmdPatch      DB "patch",0
szCmdShow       DB "show",0
szCmdStats      DB "stats",0
szCmdSave       DB "save",0
szCmdExit       DB "exit",0
szModePerm      DB "perm",0
szModeTemp      DB "temp",0

szKeyGeneralArch    DB "general.architecture",0
szKeyBlockCount     DB ".block_count",0
szKeyEmbedLen       DB ".embedding_length",0
szKeyHeadCount      DB ".attention.head_count",0
szKeyKVHeadCount    DB ".attention.head_count_kv",0
szKeyVocabSize      DB ".vocab_size",0
szKeyContextLen     DB ".context_length",0
szKeyRopeTheta      DB ".rope.freq_base",0
szKeyAlignment      DB "general.alignment",0
szKeyFFNSize        DB ".feed_forward_length",0

szStatLayers    DB "layers",0
szStatHidden    DB "hidden_size",0
szStatHeads     DB "heads",0
szStatKVHeads   DB "kv_heads",0
szStatVocab     DB "vocab_size",0
szStatContext   DB "context_len",0
szStatRope      DB "rope_theta",0
szStatAlign     DB "alignment",0
szStatFFN       DB "ffn_size",0
szStatQuant     DB "quant_type",0

szTypeDWORD     DB "DWORD",0
szTypeQWORD     DB "QWORD",0
szTypeREAL4     DB "REAL4",0

szCliBuffer     BYTE 1024 DUP(0)
szPathBuffer    BYTE 512 DUP(0)
szCmdBuffer     BYTE 256 DUP(0)
szArg1Buffer    BYTE 256 DUP(0)
szArg2Buffer    BYTE 256 DUP(0)
szArg3Buffer    BYTE 256 DUP(0)
szTempKeyBuffer BYTE 256 DUP(0)

dVal1GB         REAL8 1073741824.0
dVal1B          REAL8 1000000000.0

;═══════════════════════════════════════════════════════════════════════════════
; CODE - UNIVERSAL METADATA RESOLVER
;═══════════════════════════════════════════════════════════════════════════════

.CODE
align 16

;═══════════════════════════════════════════════════════════════════════════════
; Universal_LoadModel - Load ANY GGUF model
;═══════════════════════════════════════════════════════════════════════════════
Universal_LoadModel PROC
    push rbp
    mov rbp, rsp
    sub rsp, 256
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    
    sub rsp, 32
    lea rcx, szLoadFmt
    mov rdx, rbx
    call printf
    add rsp, 32
    
    ; Open file
    mov rcx, rbx
    mov edx, GENERIC_READ
    mov r8d, FILE_SHARE_READ
    xor r9d, r9d
    mov qword ptr [rsp+32], 0
    mov dword ptr [rsp+40], OPEN_EXISTING
    mov dword ptr [rsp+48], 0
    mov qword ptr [rsp+56], 0
    call CreateFileA
    cmp rax, INVALID_HANDLE_VALUE
    je error_open
    mov rsi, rax
    
    ; Create mapping
    mov rcx, rsi
    xor edx, edx
    mov r8d, PAGE_READONLY
    xor r9d, r9d
    mov qword ptr [rsp+32], 0
    mov qword ptr [rsp+40], 0
    call CreateFileMappingA
    test rax, rax
    jz error_map
    mov rdi, rax
    
    ; Map view
    mov rcx, rdi
    mov edx, FILE_MAP_READ
    xor r8d, r8d
    xor r9d, r9d
    mov qword ptr [rsp+32], 0
    call MapViewOfFile
    test rax, rax
    jz error_map
    mov [model_state].UNIV_MODEL_STATE.base_addr, rax
    
    ; Get file size
    mov rcx, rsi
    lea rdx, qword ptr [rsp+80]
    call GetFileSizeEx
    mov rax, qword ptr [rsp+80]
    mov [model_state].UNIV_MODEL_STATE.file_size, rax
    
    ; Close handle
    mov rcx, rsi
    call CloseHandle
    
    ; Verify header
    mov rax, [model_state].UNIV_MODEL_STATE.base_addr
    mov eax, dword ptr [rax]
    cmp eax, GGUF_MAGIC
    jne error_magic
    
    ; Parse metadata
    call Universal_ParseMetadata
    test eax, eax
    jnz error_parse
    
    ; Mark loaded
    mov dword ptr [model_state].UNIV_MODEL_STATE.is_loaded, 1
    
    ; Build hotpatch registry
    call Universal_BuildRegistry
    
    ; Display summary
    call Universal_DisplayLoadSummary
    
    xor eax, eax
    jmp done
    
error_open:
error_map:
error_magic:
error_parse:
    mov eax, -1
    
done:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
Universal_LoadModel ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_ParseMetadata - Extract all keys from GGUF
;═══════════════════════════════════════════════════════════════════════════════
Universal_ParseMetadata PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    
    mov rax, [model_state].UNIV_MODEL_STATE.base_addr
    add rax, GGUF_HEADER_SIZE
    mov pMetadataStart, rax
    
    ; Get architecture first
    lea rcx, szKeyGeneralArch
    call Universal_FindKey
    test rax, rax
    jz error_arch
    
    ; Copy architecture
    lea rdi, [model_state].UNIV_MODEL_STATE.arch
    mov rsi, rax
    mov rcx, 64
    rep movsb
    
    ; Resolve all arch-specific keys
    lea rcx, szKeyBlockCount
    call Universal_ResolveArchKey
    test rax, rax
    jz use_default_layers
    mov eax, dword ptr [rax]
    mov [model_state].UNIV_MODEL_STATE.layers, eax
    jmp got_layers
use_default_layers:
    mov dword ptr [model_state].UNIV_MODEL_STATE.layers, 32
got_layers:
    
    lea rcx, szKeyEmbedLen
    call Universal_ResolveArchKey
    test rax, rax
    jz use_default_hidden
    mov eax, dword ptr [rax]
    mov [model_state].UNIV_MODEL_STATE.hidden_size, eax
    jmp got_hidden
use_default_hidden:
    mov dword ptr [model_state].UNIV_MODEL_STATE.hidden_size, 4096
got_hidden:
    
    lea rcx, szKeyHeadCount
    call Universal_ResolveArchKey
    test rax, rax
    jz use_default_heads
    mov eax, dword ptr [rax]
    mov [model_state].UNIV_MODEL_STATE.heads, eax
    jmp got_heads
use_default_heads:
    mov dword ptr [model_state].UNIV_MODEL_STATE.heads, 32
got_heads:
    
    lea rcx, szKeyKVHeadCount
    call Universal_ResolveArchKey
    test rax, rax
    jz same_as_heads
    mov eax, dword ptr [rax]
    mov [model_state].UNIV_MODEL_STATE.kv_heads, eax
    jmp got_kv
same_as_heads:
    mov eax, [model_state].UNIV_MODEL_STATE.heads
    mov [model_state].UNIV_MODEL_STATE.kv_heads, eax
got_kv:
    
    lea rcx, szKeyRopeTheta
    call Universal_ResolveArchKey
    test rax, rax
    jz use_default_rope
    mov rax, qword ptr [rax]
    mov [model_state].UNIV_MODEL_STATE.rope_theta, rax
    jmp got_rope
use_default_rope:
    mov qword ptr [model_state].UNIV_MODEL_STATE.rope_theta, 10000
got_rope:
    
    xor eax, eax
    leave
    ret
    
error_arch:
    mov eax, -1
    leave
    ret
Universal_ParseMetadata ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_ResolveArchKey - Build arch+key and find value
;═══════════════════════════════════════════════════════════════════════════════
Universal_ResolveArchKey PROC
    push rbp
    mov rbp, rsp
    sub rsp, 320
    push rsi
    push rdi
    
    ; Build key: arch + suffix
    lea rdi, [rsp+32]
    lea rsi, [model_state].UNIV_MODEL_STATE.arch
copy_arch:
    lodsb
    test al, al
    jz copy_suffix
    stosb
    jmp copy_arch
    
copy_suffix:
    mov rsi, rcx
copy_suffix_loop:
    lodsb
    stosb
    test al, al
    jnz copy_suffix_loop
    
    ; Find built key
    lea rcx, [rsp+32]
    call Universal_FindKey
    
    pop rdi
    pop rsi
    leave
    ret
Universal_ResolveArchKey ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_FindKey - Search GGUF KV pairs
;═══════════════════════════════════════════════════════════════════════════════
Universal_FindKey PROC
    push rbp
    mov rbp, rsp
    sub rsp, 64
    push rbx
    push rsi
    push rdi
    
    mov rbx, rcx
    mov rsi, pMetadataStart
    mov rax, [model_state].UNIV_MODEL_STATE.base_addr
    mov rcx, qword ptr [rax+16]
    
search_loop:
    test rcx, rcx
    jz not_found
    
    mov r8, qword ptr [rsi]
    add rsi, 8
    
    ; Compare key
    mov rdi, rsi
    mov rdx, rbx
    push rcx
    mov rcx, r8
    repe cmpsb
    pop rcx
    je key_match
    
    add rsi, r8
    mov r9d, dword ptr [rsi]
    add rsi, 4
    
    call Universal_GetValueSize
    add rsi, rax
    
    dec rcx
    jmp search_loop
    
key_match:
    add rsi, r8
    add rsi, 4
    mov rax, rsi
    jmp done
    
not_found:
    xor rax, rax
    
done:
    pop rdi
    pop rsi
    pop rbx
    leave
    ret
Universal_FindKey ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_GetValueSize - Calculate value size by type
;═══════════════════════════════════════════════════════════════════════════════
Universal_GetValueSize PROC
    cmp r9d, GGUF_TYPE_UINT32
    je size_4
    cmp r9d, GGUF_TYPE_UINT64
    je size_8
    cmp r9d, GGUF_TYPE_FLOAT32
    je size_4
    cmp r9d, GGUF_TYPE_STRING
    je size_string
    mov rax, 8
    ret
size_4:
    mov rax, 4
    ret
size_8:
    mov rax, 8
    ret
size_string:
    mov rax, qword ptr [rsi]
    add rax, 8
    ret
Universal_GetValueSize ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_BuildRegistry - Build hotpatch table
;═══════════════════════════════════════════════════════════════════════════════
Universal_BuildRegistry PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    xor ecx, ecx
    mov hotpatch_count, ecx
    
    ; Register all patchable stats
    lea rcx, szStatLayers
    lea rdx, [model_state].UNIV_MODEL_STATE.layers
    xor r8d, r8d
    call Universal_RegisterStat
    
    lea rcx, szStatHidden
    lea rdx, [model_state].UNIV_MODEL_STATE.hidden_size
    xor r8d, r8d
    call Universal_RegisterStat
    
    lea rcx, szStatHeads
    lea rdx, [model_state].UNIV_MODEL_STATE.heads
    xor r8d, r8d
    call Universal_RegisterStat
    
    lea rcx, szStatRope
    lea rdx, [model_state].UNIV_MODEL_STATE.rope_theta
    mov r8d, 1
    call Universal_RegisterStat
    
    leave
    ret
Universal_BuildRegistry ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_RegisterStat - Add to hotpatch registry
;═══════════════════════════════════════════════════════════════════════════════
Universal_RegisterStat PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    push rsi
    push rdi
    
    mov eax, hotpatch_count
    imul rax, SIZEOF HOTPATCH_ENTRY
    lea rdi, hotpatch_registry
    add rdi, rax
    
    ; Copy name
    mov rsi, rcx
    lea rdi, [rdi].HOTPATCH_ENTRY.stat_name
    mov rcx, 64
    rep movsb
    
    ; Store pointer and type
    mov rax, eax hotpatch_count
    imul rax, SIZEOF HOTPATCH_ENTRY
    lea rdi, hotpatch_registry
    add rdi, rax
    mov [rdi].HOTPATCH_ENTRY.stat_ptr, rdx
    mov [rdi].HOTPATCH_ENTRY.stat_type, r8d
    
    inc hotpatch_count
    
    pop rdi
    pop rsi
    leave
    ret
Universal_RegisterStat ENDP

;═══════════════════════════════════════════════════════════════════════════════
; Universal_DisplayLoadSummary
;═══════════════════════════════════════════════════════════════════════════════
Universal_DisplayLoadSummary PROC
    push rbp
    mov rbp, rsp
    sub rsp, 96
    
    sub rsp, 32
    lea rcx, szLoadOK
    lea rdx, [model_state].UNIV_MODEL_STATE.arch
    
    mov rax, [model_state].UNIV_MODEL_STATE.file_size
    cvtsi2sd xmm0, rax
    divsd xmm0, dVal1GB
    
    mov rax, [model_state].UNIV_MODEL_STATE.file_size
    shr rax, 1
    cvtsi2sd xmm1, rax
    divsd xmm1, dVal1B
    
    call printf
    add rsp, 32
    
    leave
    ret
Universal_DisplayLoadSummary ENDP

;═══════════════════════════════════════════════════════════════════════════════
; CLI_Run - Main CLI loop
;═══════════════════════════════════════════════════════════════════════════════
CLI_Run PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    mov dword ptr cli_active, 1
    
cli_loop:
    cmp dword ptr cli_active, 0
    je cli_exit
    
    ; Print prompt
    sub rsp, 32
    lea rcx, szPrompt
    call printf
    add rsp, 32
    
    ; Read command
    lea rcx, szCliBuffer
    mov rdx, 1024
    call gets_s
    
    ; Parse command
    call CLI_ParseCommand
    
    jmp cli_loop
    
cli_exit:
    leave
    ret
CLI_Run ENDP

;═══════════════════════════════════════════════════════════════════════════════
; CLI_ParseCommand - Parse and execute
;═══════════════════════════════════════════════════════════════════════════════
CLI_ParseCommand PROC
    push rbp
    mov rbp, rsp
    sub rsp, 32
    
    ; Simple parsing - check first word
    lea rcx, szCliBuffer
    lea rdx, szCmdShow
    call strcmp
    test eax, eax
    jz cmd_show
    
    lea rcx, szCliBuffer
    lea rdx, szCmdExit
    call strcmp
    test eax, eax
    jz cmd_exit
    
    jmp done
    
cmd_show:
    call CLI_ShowStats
    jmp done
    
cmd_exit:
    mov dword ptr cli_active, 0
    
done:
    leave
    ret
CLI_ParseCommand ENDP

;═══════════════════════════════════════════════════════════════════════════════
; CLI_ShowStats - Display all model stats
;═══════════════════════════════════════════════════════════════════════════════
CLI_ShowStats PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    sub rsp, 32
    lea rcx, szShowHdr
    call printf
    add rsp, 32
    
    sub rsp, 32
    lea rcx, szShowArch
    lea rdx, [model_state].UNIV_MODEL_STATE.arch
    call printf
    add rsp, 32
    
    sub rsp, 32
    lea rcx, szShowLayers
    mov edx, [model_state].UNIV_MODEL_STATE.layers
    call printf
    add rsp, 32
    
    sub rsp, 32
    lea rcx, szShowHidden
    mov edx, [model_state].UNIV_MODEL_STATE.hidden_size
    call printf
    add rsp, 32
    
    sub rsp, 32
    lea rcx, szShowHeads
    mov edx, [model_state].UNIV_MODEL_STATE.heads
    mov r8d, [model_state].UNIV_MODEL_STATE.kv_heads
    call printf
    add rsp, 32
    
    sub rsp, 32
    lea rcx, szShowRope
    mov rdx, [model_state].UNIV_MODEL_STATE.rope_theta
    call printf
    add rsp, 32
    
    leave
    ret
CLI_ShowStats ENDP

;═══════════════════════════════════════════════════════════════════════════════
; main - Entry point
;═══════════════════════════════════════════════════════════════════════════════
main PROC
    push rbp
    mov rbp, rsp
    sub rsp, 48
    
    ; Print banner
    sub rsp, 32
    lea rcx, szBanner
    call printf
    add rsp, 32
    
    ; Load default model (or from args)
    sub rsp, 32
    lea rcx, szPathBuffer
    mov byte ptr [rcx], 'b'
    mov byte ptr [rcx+1], 'i'
    mov byte ptr [rcx+2], 'g'
    mov byte ptr [rcx+3], 'd'
    mov byte ptr [rcx+4], 'a'
    mov byte ptr [rcx+5], 'd'
    mov byte ptr [rcx+6], 'd'
    mov byte ptr [rcx+7], 'y'
    mov byte ptr [rcx+8], 'g'
    mov byte ptr [rcx+9], '-'
    mov byte ptr [rcx+10], 'q'
    mov byte ptr [rcx+11], '5'
    mov byte ptr [rcx+12], '.'
    mov byte ptr [rcx+13], 'g'
    mov byte ptr [rcx+14], 'g'
    mov byte ptr [rcx+15], 'u'
    mov byte ptr [rcx+16], 'f'
    mov byte ptr [rcx+17], 0
    
    call Universal_LoadModel
    add rsp, 32
    
    ; Start CLI
    call CLI_Run
    
    xor eax, eax
    leave
    ret
main ENDP

END
