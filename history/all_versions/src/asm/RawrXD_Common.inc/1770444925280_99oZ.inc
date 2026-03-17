; =============================================================================
; RawrXD_Common.inc — Shared Constants, Macros, and Structures for MASM64
; =============================================================================
; Common include for all RawrXD ASM modules.
; Build: Included by .asm files, not compiled directly.
; =============================================================================

option casemap:none

; =============================================================================
;                         Win32 API Constants
; =============================================================================

; Memory allocation
MEM_COMMIT              EQU     1000h
MEM_RESERVE             EQU     2000h
MEM_RELEASE             EQU     8000h
PAGE_READWRITE          EQU     04h
PAGE_READONLY           EQU     02h
PAGE_EXECUTE_READWRITE  EQU     40h

; File access
GENERIC_READ            EQU     80000000h
GENERIC_WRITE           EQU     40000000h
FILE_SHARE_READ         EQU     00000001h
OPEN_EXISTING           EQU     3
CREATE_ALWAYS           EQU     2
INVALID_HANDLE_VALUE    EQU     -1

; Registry
KEY_READ                EQU     00020019h
KEY_WRITE               EQU     00020006h
KEY_ALL_ACCESS          EQU     000F003Fh
REG_BINARY              EQU     3
REG_DWORD               EQU     4
REG_SZ                  EQU     1
HKEY_LOCAL_MACHINE      EQU     80000002h
HKEY_CURRENT_USER       EQU     80000001h

; Process info
PROCESS_QUERY_INFORMATION EQU   0400h

; Crypto API (CryptoAPI / CNG)
CALG_SHA_256            EQU     0000800Ch
CALG_SHA_512            EQU     0000800Eh
CALG_RSA_KEYX           EQU     0000A400h
CRYPT_VERIFYCONTEXT     EQU     0F0000000h
PROV_RSA_AES            EQU     24

; =============================================================================
;                         Shield Anti-Debug Constants
; =============================================================================

SHIELD_PEB_OFFSET             EQU 60h       ; gs:[60h] = PEB on x64
SHIELD_PEB_BEINGDEBUGGED      EQU 02h       ; PEB offset to BeingDebugged byte
SHIELD_HEAP_FLAGS_OFFSET      EQU 70h       ; ProcessHeap.Flags
SHIELD_HEAP_FORCEFLAGS_OFFSET EQU 74h       ; ProcessHeap.ForceFlags
SHIELD_NT_GLOBAL_FLAG         EQU 0BCh      ; PEB.NtGlobalFlag
SHIELD_RDTSC_THRESHOLD        EQU 0C350h    ; 50,000 cycles = debugger step

; Murmur3 hash constants (for HWID fingerprinting)
SHIELD_MURMUR_C1              EQU 0CC9E2D51h
SHIELD_MURMUR_C2              EQU 01B873593h
SHIELD_MURMUR_R1              EQU 15
SHIELD_MURMUR_R2              EQU 13
SHIELD_MURMUR_M               EQU 5
SHIELD_MURMUR_N               EQU 0E6546B64h
SHIELD_MURMUR_FMIX1           EQU 0FF51AFD7ED558CCDh
SHIELD_MURMUR_FMIX2           EQU 0C4CEB9FE1A85EC53h

; SystemKernelDebuggerInformation class ID for NtQuerySystemInformation
SystemKernelDebuggerInformation EQU 23h

; =============================================================================
;                         RawrXD Engine Constants
; =============================================================================

; Engine capability flags (match streaming_engine_registry.h EngineCapability)
ENGINE_CAP_STREAMING        EQU     0001h
ENGINE_CAP_QUADBUFFER       EQU     0002h
ENGINE_CAP_AVX512_DMA       EQU     0004h
ENGINE_CAP_LRU_EVICTION     EQU     0008h
ENGINE_CAP_GGUF             EQU     0010h
ENGINE_CAP_SAFETENSORS      EQU     0020h
ENGINE_CAP_BLOB             EQU     0040h
ENGINE_CAP_VRAM_PAGING      EQU     0080h
ENGINE_CAP_NUMA_AWARE       EQU     0100h
ENGINE_CAP_FLASHATTENTION   EQU     0200h
ENGINE_CAP_QUANTIZATION     EQU     0400h
ENGINE_CAP_DUALENGINE       EQU     0800h
ENGINE_CAP_HOTPATCHING      EQU     1000h
ENGINE_CAP_GPU_COMPUTE      EQU     2000h
ENGINE_CAP_ENCRYPTION       EQU     4000h
ENGINE_CAP_MULTIDRIVE       EQU     8000h

; License feature bitmasks
FEATURE_800B_DUALENGINE       EQU 00000001h
FEATURE_AVX512_PREMIUM        EQU 00000002h
FEATURE_DISTRIBUTED_SWARM     EQU 00000004h
FEATURE_GPU_QUANT_4BIT        EQU 00000008h
FEATURE_ENTERPRISE_SUPPORT    EQU 00000010h
FEATURE_UNLIMITED_CONTEXT     EQU 00000020h
FEATURE_FLASH_ATTENTION       EQU 00000040h
FEATURE_MULTI_GPU             EQU 00000080h

; License validation states
LICENSE_INVALID               EQU 0
LICENSE_VALID_TRIAL           EQU 1
LICENSE_VALID_ENTERPRISE      EQU 2
LICENSE_VALID_OEM             EQU 3
LICENSE_EXPIRED               EQU 4
LICENSE_HARDWARE_MISMATCH     EQU 5
LICENSE_TAMPERED              EQU 6

; License file magic
RAWR_LICENSE_MAGIC            EQU 4C455852h    ; "RXEL" (little-endian)
RAWR_MAGIC                    EQU 52415752h    ; "RAWR"
CURRENT_LICENSE_VERSION       EQU 00010000h

; Cryptographic sizes
RSA_KEY_SIZE                  EQU 4096
RSA_SIGNATURE_SIZE            EQU 512
SHA512_HASH_SIZE              EQU 64
ED25519_SIG_SIZE              EQU 64
ED25519_PUBKEY_SIZE           EQU 32
LICENSE_BLOB_MAX_SIZE         EQU 4096

; GGUF magic
GGUF_MAGIC                    EQU 46554747h    ; "GGUF"

; Quantization types
GGML_TYPE_F32           EQU     0
GGML_TYPE_F16           EQU     1
GGML_TYPE_Q4_0          EQU     2
GGML_TYPE_Q4_1          EQU     3
GGML_TYPE_Q5_0          EQU     6
GGML_TYPE_Q5_1          EQU     7
GGML_TYPE_Q8_0          EQU     8
GGML_TYPE_Q2_K          EQU     10
GGML_TYPE_Q3_K          EQU     11
GGML_TYPE_Q4_K          EQU     12
GGML_TYPE_Q5_K          EQU     13
GGML_TYPE_Q6_K          EQU     14
GGML_TYPE_Q8_K          EQU     15

; =============================================================================
;                         Status / Error Codes
; =============================================================================

STATUS_SUCCESS              EQU 0
STATUS_UNSUCCESSFUL         EQU 0C0000001h
STATUS_ACCESS_DENIED        EQU 0C0000002h
STATUS_INVALID_PARAMETER    EQU 0C0000003h
STATUS_VERSION_MISMATCH     EQU 0C0000004h
STATUS_LICENSE_EXPIRED      EQU 0C0000005h
STATUS_HARDWARE_MISMATCH    EQU 0C0000006h
STATUS_SIGNATURE_INVALID    EQU 0C0000007h
STATUS_CRYPTO_ERROR         EQU 0C0000008h
STATUS_INVALID_SIZE         EQU 0C0000009h
STATUS_INVALID_LICENSE      EQU 0C000000Ah

; =============================================================================
;                         PatchResult Pattern
; =============================================================================
; All RawrXD operations return structured results via RAX/RDX pair:
;   RAX = 0 on success, NTSTATUS-style code on failure
;   RDX = detail pointer (const char*) or supplemental data
;
; Callers check:
;   test rax, rax
;   jnz  @error_handler

; =============================================================================
;                    External Win32 API Declarations
; =============================================================================

EXTERNDEF VirtualAlloc:PROC
EXTERNDEF VirtualFree:PROC
EXTERNDEF VirtualProtect:PROC
EXTERNDEF CreateFileA:PROC
EXTERNDEF CreateFileW:PROC
EXTERNDEF CreateFileMappingA:PROC
EXTERNDEF MapViewOfFile:PROC
EXTERNDEF UnmapViewOfFile:PROC
EXTERNDEF CloseHandle:PROC
EXTERNDEF GetLastError:PROC
EXTERNDEF GetTickCount64:PROC
EXTERNDEF GetModuleHandleA:PROC
EXTERNDEF GetProcAddress:PROC
EXTERNDEF OutputDebugStringA:PROC
EXTERNDEF IsDebuggerPresent:PROC
EXTERNDEF GetCurrentHwProfileA:PROC
EXTERNDEF GetVolumeInformationA:PROC
EXTERNDEF GetSystemTimeAsFileTime:PROC
EXTERNDEF InitializeCriticalSection:PROC
EXTERNDEF EnterCriticalSection:PROC
EXTERNDEF LeaveCriticalSection:PROC
EXTERNDEF DeleteCriticalSection:PROC
EXTERNDEF Sleep:PROC

; Registry
EXTERNDEF RegCreateKeyExA:PROC
EXTERNDEF RegOpenKeyExA:PROC
EXTERNDEF RegQueryValueExA:PROC
EXTERNDEF RegSetValueExA:PROC
EXTERNDEF RegCloseKey:PROC

; Crypto
EXTERNDEF CryptAcquireContextA:PROC
EXTERNDEF CryptReleaseContext:PROC
EXTERNDEF CryptImportKey:PROC
EXTERNDEF CryptDestroyKey:PROC
EXTERNDEF CryptCreateHash:PROC
EXTERNDEF CryptHashData:PROC
EXTERNDEF CryptVerifySignatureA:PROC
EXTERNDEF CryptDestroyHash:PROC

; NT internals
EXTERNDEF NtQueryInformationProcess:PROC
EXTERNDEF NtQuerySystemInformation:PROC

; CRT
EXTERNDEF memcpy:PROC
EXTERNDEF memset:PROC
EXTERNDEF memcmp:PROC
EXTERNDEF strlen:PROC
EXTERNDEF strcpy:PROC

; =============================================================================
;                        CRITICAL_SECTION struct
; =============================================================================
; Win64 CRITICAL_SECTION is 40 bytes
CRITICAL_SECTION STRUCT
    DebugInfo       DQ ?
    LockCount       DD ?
    RecursionCount  DD ?
    OwningThread    DQ ?
    LockSemaphore   DQ ?
    SpinCount       DQ ?
CRITICAL_SECTION ENDS

; =============================================================================
;                        HW_PROFILE_INFOA struct
; =============================================================================
HW_PROFILE_INFOA STRUCT
    dwDockInfo          DD ?
    szHwProfileGuid     DB 39 DUP(?)
    _pad0               DB ?
    szHwProfileName     DB 80 DUP(?)
HW_PROFILE_INFOA ENDS

; =============================================================================
;                       IMAGE_DOS_HEADER (subset)
; =============================================================================
IMAGE_DOS_HEADER STRUCT
    e_magic     DW ?
    e_cblp      DW ?
    e_cp        DW ?
    e_crlc      DW ?
    e_cparhdr   DW ?
    e_minalloc  DW ?
    e_maxalloc  DW ?
    e_ss        DW ?
    e_sp        DW ?
    e_csum      DW ?
    e_ip        DW ?
    e_cs        DW ?
    e_lfarlc    DW ?
    e_ovno      DW ?
    e_res       DW 4 DUP(?)
    e_oemid     DW ?
    e_oeminfo   DW ?
    e_res2      DW 10 DUP(?)
    e_lfanew    DD ?
IMAGE_DOS_HEADER ENDS

; =============================================================================
;                      IMAGE_SECTION_HEADER
; =============================================================================
IMAGE_SECTION_HEADER STRUCT
    SecName         DB 8 DUP(?)
    VirtualSize     DD ?
    VirtualAddress  DD ?
    SizeOfRawData   DD ?
    PointerToRawData DD ?
    PointerToRelocations DD ?
    PointerToLinenumbers DD ?
    NumberOfRelocations DW ?
    NumberOfLinenumbers  DW ?
    Characteristics DD ?
IMAGE_SECTION_HEADER ENDS

; =============================================================================
;                           Utility Macros
; =============================================================================

; Align stack to 16-byte boundary for Win64 ABI
ALIGN_STACK MACRO
    and     rsp, NOT 0Fh
ENDM

; Shadow space allocation (32 bytes minimum for Win64 calls)
SHADOW_SPACE EQU 32

; Save/restore non-volatile registers  
SAVE_NONVOL MACRO
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13
    push    r14
    push    r15
ENDM

RESTORE_NONVOL MACRO
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
ENDM

; Aliases (used by Shield layer)
SAVE_REGS MACRO
    SAVE_NONVOL
ENDM

RESTORE_REGS MACRO
    RESTORE_NONVOL
ENDM

; Shadow space allocation/restore for Win64 calls
SHADOW_ALLOC MACRO
    sub     rsp, 28h        ; 32 bytes shadow + 8 bytes alignment
ENDM

SHADOW_FREE MACRO
    add     rsp, 28h
ENDM

; Debug-only output
IFDEF DEBUG_BUILD
DBG_PRINT MACRO msg_label
    lea     rcx, msg_label
    call    OutputDebugStringA
ENDM
ELSE
DBG_PRINT MACRO msg_label
    ; stripped in release
ENDM
ENDIF

; =============================================================================
; END OF INCLUDE
; =============================================================================
