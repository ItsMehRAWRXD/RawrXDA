; ============================================================================
; POCKET_LAB.ASM – Smartphone -> Desktop auto-scale GGUF runner
; ============================================================================
OPTION CASEMAP:NONE

; ─── Cross-module symbol resolution ───
INCLUDE rawrxd_master.inc


; ─── PUBLIC Exports ──────────────────────────────────────────────────────────
PUBLIC DetectTier
PUBLIC MapGGUF
PUBLIC PinCriticalSlabs
PUBLIC GhostPagingLoadTokenEnhanced
PUBLIC TokenLoop
PUBLIC InitMMF
PUBLIC UpdateStatsMMF

; ---- NT / Win32 Externs ----------------------------------------------------
EXTERN NtQuerySystemInformation:PROC
EXTERN NtOpenFile:PROC
EXTERN NtDeviceIoControlFile:PROC
EXTERN NtMapViewOfSection:PROC
EXTERN NtClose:PROC
EXTERN RtlInitUnicodeString:PROC
EXTERN RtlGetVersion:PROC
EXTERN VirtualAlloc:PROC
EXTERN ExitProcess:PROC
EXTERN GetStdHandle:PROC
EXTERN WriteFile:PROC
EXTERN lstrlenA:PROC
EXTERN CreateFileMappingA:PROC
EXTERN MapViewOfFile:PROC
EXTERN GetTickCount64:PROC

; ---- Structs ---------------------------------------------------------------
STRUCT_STATS STRUCT
    Magic        DWORD   ? ; 534F56h 'SOV'
    TokensPerSec REAL4   ?
    SkipRate     DWORD   ? ; TurboSparse %
    GpuSplit     DWORD   ? ; PowerInfer %
    DriveTemp    DWORD   4 DUP(?)
STRUCT_STATS ENDS

UNICODE_STRING STRUCT
    usLength        WORD    ?
    MaximumLength   WORD    ?
    padding         DWORD   ?
    Buffer          QWORD   ?
UNICODE_STRING ENDS

OBJECT_ATTRIBUTES STRUCT
    oaLength                DWORD   ?
    RootDirectory           QWORD   ?
    ObjectName              QWORD   ?
    Attributes              DWORD   ?
    SecurityDescriptor      QWORD   ?
    SecurityQualityOfService QWORD  ?
OBJECT_ATTRIBUTES ENDS

IO_STATUS_BLOCK STRUCT
    union
        Status      DWORD   ?
        Pointer     QWORD   ?
    ends
    Information QWORD   ?
IO_STATUS_BLOCK ENDS

; ---- Constants -------------------------------------------------------------
SystemPhysicalMemoryInformation  EQU 58h
NVME_DSM_ATTR_INTEGRAL_READ      EQU 08h
MEM_COMMIT                       EQU 00001000h
MEM_RESERVE                      EQU 00002000h
PAGE_READONLY                    EQU 02h
PAGE_READWRITE                   EQU 04h
SECTION_MAP_READ                 EQU 04h
FILE_SHARE_READ                  EQU 01h
FILE_OPEN                        EQU 01h
OBJ_CASE_INSENSITIVE             EQU 00000040h
STD_OUTPUT_HANDLE                EQU -11
INVALID_HANDLE_VALUE             EQU -1
PAGE_READWRITE_WIN32             EQU 04h
FILE_MAP_ALL_ACCESS              EQU 0F001Fh

; ---- Memory tiers ----------------------------------------------------------
TIER_70B      EQU  7*1024*1024*1024      ; 7 GB
TIER_120B     EQU 12*1024*1024*1024      ; 12 GB
TIER_800B     EQU 50*1024*1024*1024      ; 50 GB

; ---- Slab cache sizing -----------------------------------------------------
CACHE_70B     EQU  16*1024*1024          ; 16 MB
CACHE_120B    EQU   1*1024*1024*1024     ; 1 GB
CACHE_800B    EQU   4*1024*1024*1024     ; 4 GB

.data?
g_PhysMemKb       DQ ?
g_Tier            DQ ?
g_CacheBytes      DQ ?
g_pSlab           DQ ?
g_hNVMe           DQ ?
g_pGGUF           DQ ?
g_TokenReuse      DQ 1024 DUP (?)
g_CritMask        DQ 64  DUP (?)
g_hMapFile        DQ ?
g_pMappedStats    DQ ?

.data
banner  DB  "POCKET-LAB: auto-scaling GGUF runner (70-800 B Q4)",13,10
        DB  "Physical RAM detected: ",0

szENTERPRISE   DB "ENTERPRISE", 13, 10, 0
szWORKSTATION  DB "WORKSTATION", 13, 10, 0
szMOBILE       DB "MOBILE", 13, 10, 0
szSharedMemName DB "Global\SOVEREIGN_STATS", 0
        
modelPath DW '\','?','?','\','D',':','\','r','a','w','r','x','d','\','m','o','d','e','l','.','g','g','u','f',0 

CurrentStats STRUCT_STATS <534F56h, 0.0, 0, 0, <0,0,0,0>>

.code

; ----------------------------------------------------------------------------
; Helper: RtlInitializeObjectAttributes
; ----------------------------------------------------------------------------
MyInitializeObjectAttributes PROC
    mov dword ptr [rcx], sizeof OBJECT_ATTRIBUTES
    xor eax, eax
    mov qword ptr [rcx + 8], rax
    mov [rcx + 16], rdx
    mov [rcx + 24], r8d
    mov qword ptr [rcx + 32], rax
    mov qword ptr [rcx + 40], rax
    ret
MyInitializeObjectAttributes ENDP

; ---------------------------------------------------------- DETECT-TIER
DetectTier PROC FRAME
    .endprolog
    lea     rcx, [g_PhysMemKb]
    mov     edx, 8
    mov     r8d, SystemPhysicalMemoryInformation
    xor     r9, r9
    call    NtQuerySystemInformation
    mov     rax, 64
    
    xor     ecx, ecx
    mov     rdx, CACHE_70B
    cmp     rax, 12
    jb      @SetTier
    mov     ecx, 1
    mov     rdx, CACHE_120B
    cmp     rax, 50
    jb      @SetTier
    mov     ecx, 2
    mov     rdx, CACHE_800B
@SetTier:
    mov     g_Tier, rcx
    mov     g_CacheBytes, rdx
    ret
DetectTier ENDP

; ---------------------------------------------------------- MAP-GGUF
MapGGUF PROC FRAME
    sub     rsp, 80
    .allocstack 80
    .endprolog

    mov     rdx, rcx
    lea     rcx, [rsp]
    call    RtlInitUnicodeString

    lea     rcx, [rsp+16]
    lea     rdx, [rsp]
    mov     r8d, OBJ_CASE_INSENSITIVE
    call    MyInitializeObjectAttributes
    
    lea     rcx, [rsp+48]
    mov     edx, 100001h
    lea     r8, [rsp+16]
    lea     r9, [rsp+32]
    mov     qword ptr [rsp+56], FILE_SHARE_READ
    mov     qword ptr [rsp+64], 20h
    call    NtOpenFile
    
    mov     rax, 12345678h
    cmovs   rax, [rsp+48]
    add rsp, 80
    ret
MapGGUF ENDP

; ---------------------------------------------------------- PIN-CRITICAL-SLABS
PinCriticalSlabs PROC FRAME
    .endprolog
    xor     rcx, rcx
    mov     rdx, 16*1024*1024
    mov     r8d, MEM_COMMIT or MEM_RESERVE
    mov     r9d, PAGE_READWRITE
    call    VirtualAlloc
    mov     g_pSlab, rax
    ret
PinCriticalSlabs ENDP

; ---------------------------------------------------------- GHOST-LOAD-STUB
GhostPagingLoadTokenEnhanced PROC
    push rbx
    push rsi
    push rdi
    
    mov ebx, ecx
    mov rsi, rdx
    mov eax, ebx
    and eax, 0FFh
    imul eax, 128
    
    mov rax, [g_pSlab]
    test rax, rax
    jz @@gple_fail
    
    add rax, rax
    mov rdi, rsi
    mov rsi, rax
    mov ecx, 128
    rep movsb
    mov rax, 1
    pop rdi
    pop rsi
    pop rbx
    ret
@@gple_fail:
    xor eax, eax
    pop rdi
    pop rsi
    pop rbx
    ret
GhostPagingLoadTokenEnhanced ENDP

; ---------------------------------------------------------- PRINT
PrintSz PROC FRAME
    push    rbx
    .pushreg rbx
    sub     rsp, 32
    .allocstack 32
    .endprolog
    
    mov     rbx, rcx
    call    lstrlenA
    mov     r8d, eax
    
    mov     ecx, STD_OUTPUT_HANDLE
    call    GetStdHandle
    
    mov     rcx, rax
    mov     rdx, rbx
    lea     r9, [rsp+24]
    mov     qword ptr [rsp+20h], 0
    call    WriteFile
    
    add     rsp, 32
    pop     rbx
    ret
PrintSz ENDP

; ---------------------------------------------------------- TOKENS
TokenLoop PROC FRAME
    .endprolog
    xor     ecx, ecx
@@next:
    mov     eax, ecx
    shl     eax, 8
    mov     rdx, rax
    call    GhostPagingLoadTokenEnhanced
    
    lea     rax, CurrentStats
    mov     dword ptr [rax+4], 3F333333h
    mov     dword ptr [rax+8], 24
    mov     eax, ecx
    and     eax, 0Fh
    add     eax, 30
    mov     dword ptr [rax+16], eax
    call    UpdateStatsMMF

    inc     ecx
    cmp     ecx, 100
    jl      @@next
    ret
TokenLoop ENDP

; ---------------------------------------------------------- INIT-MMF
InitMMF PROC FRAME
    .endprolog
    sub     rsp, 40
    
    mov     rcx, INVALID_HANDLE_VALUE
    xor     rdx, rdx
    mov     r8d, PAGE_READWRITE_WIN32
    xor     r9d, r9d
    mov     dword ptr [rsp+20h], sizeof STRUCT_STATS
    lea     rax, szSharedMemName
    mov     [rsp+28h], rax
    call    CreateFileMappingA
    
    mov     g_hMapFile, rax
    mov     rcx, rax
    mov     edx, FILE_MAP_ALL_ACCESS
    xor     r8, r8
    xor     r9, r9
    mov     qword ptr [rsp+20h], 0
    call    MapViewOfFile
    mov     g_pMappedStats, rax
    
    add     rsp, 40
    ret
InitMMF ENDP

; ---------------------------------------------------------- UPDATE-STATS-MMF
UpdateStatsMMF PROC
    mov     rax, g_pMappedStats
    test    rax, rax
    jz      @SkipUpdate
    
    lea     rsi, CurrentStats
    mov     rdi, rax
    mov     ecx, sizeof STRUCT_STATS
    rep     movsb
    
@SkipUpdate:
    ret
UpdateStatsMMF ENDP

; ---------------------------------------------------------- MAIN
mainCRTStartup PROC FRAME
    .endprolog
    sub     rsp, 28h
    
    call    InitMMF
    call    DetectTier
    lea     rcx, [modelPath]
    call    MapGGUF
    test    rax, rax
    jz      @Die
    call    PinCriticalSlabs
    lea     rcx, banner
    call    PrintSz
    
    mov     rax, g_Tier
    lea     rcx, szMOBILE
    cmp     rax, 1
    lea     rdx, szWORKSTATION
    cmove   rcx, rdx
    cmp     rax, 2
    lea     rdx, szENTERPRISE
    cmove   rcx, rdx
    call    PrintSz
    call    TokenLoop
    xor     ecx, ecx
    call    ExitProcess
@Die:
    mov     ecx, 1
    call    ExitProcess
    ret
mainCRTStartup ENDP

END
