; =========================
; FILE: rawr_main.asm  (PHASE 1 VALIDATION: all 6 systems wired)
; =========================
option casemap:none
include rawr_rt.inc
include rawr_imports.inc

extrn rawr_panic_w            : proc

; memory
extrn rawr_heap_init          : proc
extrn rawr_heap_destroy       : proc
extrn rawr_heap_alloc         : proc
extrn rawr_heap_free          : proc
extrn rawr_aligned_alloc      : proc
extrn rawr_aligned_free       : proc
extrn rawr_arena_init         : proc
extrn rawr_arena_alloc        : proc
extrn rawr_arena_destroy      : proc

; strings/log/time/ipc/cfg
extrn rawr_wcslen             : proc
extrn rawr_u64_to_hex16_w     : proc
extrn rawr_log_open           : proc
extrn rawr_log_close          : proc
extrn rawr_log_write_w        : proc
extrn rawr_time_init          : proc
extrn rawr_qpc                : proc
extrn rawr_pipe_server_create : proc
extrn rawr_pipe_server_accept : proc
extrn rawr_pipe_send_frame    : proc
extrn rawr_cfg_load_file_w    : proc

; ABI canary
extrn abi_canary_call0        : proc

; Phase 2 integration
extrn RawrSpine_Init          : proc
extrn SelfTest_All            : proc

RAWR_ARENA struct
    base        qword ?
    reserve_sz  qword ?
    commit_sz   qword ?
    bump_offset qword ?
    pagesz      dword ?
    _pad        dword ?
RAWR_ARENA ends

.data
msg_ok      dw 'R','A','W','R',' ','I','N','F','R','A',' ','O','K',0
msg_fail    dw 'R','A','W','R',' ','I','N','F','R','A',' ','F','A','I','L',0
log_path    dw 'r','a','w','r','_','i','n','f','r','a','.','l','o','g',0
pipe_name   dw '\\','\','.\','\','p','i','p','e','\','r','a','w','r','x','d','_','i','n','f','r','a',0

.data?
g_arena     RAWR_ARENA <>
tmp_hex     dw 40 dup(?)
cfg_buf     dq ?
cfg_sz      dq ?

.code

; Simple leaf function for ABI test
public rawr_test_leaf
rawr_test_leaf proc
    ; does nothing, preserves all
    ret
rawr_test_leaf endp

public rawr_main
rawr_main proc
    RAWR_PROLOGUE 0

    ; init heap
    xor     rcx, rcx
    xor     rdx, rdx
    call    rawr_heap_init
    test    eax, eax
    jz      _panic

    ; init spine
    call    RawrSpine_Init
    test    eax, eax
    jz      _panic

    ; init time
    call    rawr_time_init
    test    eax, eax
    jz      _panic

    ; open log
    lea     rcx, log_path
    call    rawr_log_open
    test    eax, eax
    jz      _panic

    ; log qpc hex
    call    rawr_qpc
    mov     rcx, rax
    lea     rdx, tmp_hex
    mov     r8d, 1
    call    rawr_u64_to_hex16_w
    lea     rcx, tmp_hex
    call    rawr_log_write_w

    ; aligned alloc check
    mov     rcx, 256
    mov     rdx, 64
    mov     r8d, 1
    call    rawr_aligned_alloc
    test    rax, rax
    jz      _panic
    mov     r9, rax
    test    r9, 63
    jne     _panic
    mov     rcx, rax
    call    rawr_aligned_free

    ; arena reserve 1MB; alloc 8x64KB aligned 4096
    lea     rcx, g_arena
    mov     rdx, 100000h
    call    rawr_arena_init
    test    eax, eax
    jz      _panic

    xor     r11d, r11d
_a:
    lea     rcx, g_arena
    mov     rdx, 10000h
    mov     r8, 1000h
    call    rawr_arena_alloc
    test    rax, rax
    jz      _panic
    test    rax, 0FFFh
    jne     _panic
    inc     r11d
    cmp     r11d, 8
    jl      _a

    ; run selftests
    call    SelfTest_All
    test    eax, eax
    jnz     _panic

    ; cfg load (optional file "rawr.cfg" if present; ignore failure)
    ; path "rawr.cfg"
    ; (build UTF-16 literal in tmp_hex buffer to reuse)
    mov     word ptr [tmp_hex+0], 'r'
    mov     word ptr [tmp_hex+2], 'a'
    mov     word ptr [tmp_hex+4], 'w'
    mov     word ptr [tmp_hex+6], 'r'
    mov     word ptr [tmp_hex+8], '.'
    mov     word ptr [tmp_hex+10],'c'
    mov     word ptr [tmp_hex+12],'f'
    mov     word ptr [tmp_hex+14],'g'
    mov     word ptr [tmp_hex+16],0

    lea     rcx, tmp_hex
    lea     rdx, cfg_buf
    lea     r8,  cfg_sz
    call    rawr_cfg_load_file_w
    ; if ok, free it
    test    eax, eax
    jz      _skip_cfg
    mov     rcx, cfg_buf
    mov     rcx, [rcx]
    call    rawr_heap_free
_skip_cfg:

    ; pipe server create+accept (will block if no client; skip by default)
    ; NOTE: comment these 3 lines in if you want pipe test active.
    ; lea     rcx, pipe_name
    ; mov     edx, 4096
    ; mov     r8d, 4096
    ; call    rawr_pipe_server_create
    ; (if you enable, you must then accept + send frame)

    ; ABI canary test on leaf function
    lea     rcx, rawr_test_leaf
    call    abi_canary_call0
    test    eax, eax
    jne     _panic

    lea     rcx, msg_ok
    call    rawr_log_write_w
    call    rawr_log_close

    lea     rcx, g_arena
    call    rawr_arena_destroy
    call    rawr_heap_destroy

    xor     eax, eax
    RAWR_EPILOGUE 0

_panic:
    lea     rcx, msg_fail
    mov     edx, 0C0DEh
    call    rawr_panic_w
    xor     eax, eax
    RAWR_EPILOGUE 0
rawr_main endp

end