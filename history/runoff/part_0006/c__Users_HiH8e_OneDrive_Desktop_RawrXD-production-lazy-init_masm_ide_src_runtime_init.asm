; ============================================================================
; runtime_init.asm - Production-ready initialization shims
; Implements SpecDecode_Init, Zstd_Init, LogManager_Init
; ============================================================================

.686
.model flat, stdcall
option casemap:none

include \masm32\include\windows.inc
include \masm32\include\kernel32.inc
include \masm32\include\user32.inc

includelib \masm32\lib\kernel32.lib
includelib \masm32\lib\user32.lib

EXTERN LogSystem_Initialize:PROC

.data
    public g_bSpecDecodeReady
    public g_bZstdReady
    public g_hZstdModule

    g_bSpecDecodeReady dd 0
    g_bZstdReady       dd 0
    g_hZstdModule      dd 0

    szZstdDll          db "zstd.dll", 0
    szLogInitFail      db "LogSystem_Initialize failed", 0

.code

; ----------------------------------------------------------------------------
; SpecDecode_Init - Initialize speculative decoder subsystem (placeholder)
; Returns: EAX = TRUE on success
; ----------------------------------------------------------------------------
public SpecDecode_Init
SpecDecode_Init proc
    mov g_bSpecDecodeReady, 1
    mov eax, TRUE
    ret
SpecDecode_Init endp

; ----------------------------------------------------------------------------
; Zstd_Init - Best-effort load of Zstandard runtime
; Returns: EAX = TRUE if initialized (soft-fail OK)
; ----------------------------------------------------------------------------
public Zstd_Init
Zstd_Init proc
    ; If already loaded, succeed
    cmp g_bZstdReady, 0
    jne @done

    ; Try to load zstd.dll (optional dependency)
    invoke LoadLibraryA, addr szZstdDll
    mov g_hZstdModule, eax
    test eax, eax
    jz @soft_ok

    mov g_bZstdReady, 1
    jmp @done

@soft_ok:
    ; Optional dependency missing; keep running
    xor eax, eax
    mov g_bZstdReady, eax
    jmp @done

@done:
    mov eax, TRUE
    ret
Zstd_Init endp

; ----------------------------------------------------------------------------
; LogManager_Init - Initialize enterprise logging (wraps LogSystem_Initialize)
; Returns: EAX = TRUE on success, FALSE on failure
; ----------------------------------------------------------------------------
public LogManager_Init
LogManager_Init proc
    call LogSystem_Initialize
    test eax, eax
    jnz @ok

    ; Log init failed; emit debug string but continue
    invoke OutputDebugStringA, addr szLogInitFail
    xor eax, eax
    ret

@ok:
    mov eax, TRUE
    ret
LogManager_Init endp

end
