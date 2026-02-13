; =============================================================================
; RawrXD_EnterpriseLicense.asm — Enterprise "Pro" Unlock System
; =============================================================================
; Pure x64 MASM | Zero Dependencies (beyond Win32 CryptoAPI)
; RSA-4096 Signature Verification | Hardware Fingerprinting | Feature Gating
;
; Unlocks:
;   - 800B Dual-Engine Kernels
;   - AVX-512 Premium Ops
;   - Distributed Swarm Compute
;   - Flash-Attention Kernels
;   - Multi-GPU Tensor Parallel
;   - Quant > Q6_K
;
; Build: ml64.exe /c /Zi RawrXD_EnterpriseLicense.asm
; Link:  Link into RawrXD-Shell.exe with kernel32.lib advapi32.lib
; =============================================================================

INCLUDE RawrXD_Common.inc

; =============================================================================
;                             EXPORTS
; =============================================================================
PUBLIC Enterprise_InitLicenseSystem
PUBLIC Enterprise_ValidateLicense
PUBLIC Enterprise_CheckFeature
PUBLIC Enterprise_Unlock800BDualEngine
PUBLIC Enterprise_InstallLicense
PUBLIC Enterprise_GetLicenseStatus
PUBLIC Enterprise_GetFeatureString
PUBLIC Enterprise_GenerateHardwareHash
PUBLIC Enterprise_RuntimeIntegrityCheck
PUBLIC Enterprise_Shutdown
PUBLIC Titan_CheckEnterpriseUnlock
PUBLIC Streaming_CheckEnterpriseBudget

; External: Shield layer (RawrXD_License_Shield.asm)
EXTERNDEF IsDebuggerPresent_Native:PROC
EXTERNDEF Shield_GenerateHWID:PROC
EXTERNDEF Shield_VerifyIntegrity:PROC
EXTERNDEF Shield_TimingCheck:PROC
EXTERNDEF Shield_AES_DecryptShim:PROC
EXTERNDEF Unlock_800B_Kernel:PROC

; =============================================================================
;                             STRUCTURES
; =============================================================================

LICENSE_HEADER STRUCT
    Magic               DD 0            ; RAWR_LICENSE_MAGIC (0x4C455852 = "RXEL")
    Version             DW 0            ; License format version
    Flags               DW 0            ; Feature flags (lower 16 bits)
    FeatureMask         DQ 0            ; Full 64-bit feature bitmask
    IssueTimestamp      DQ 0            ; Unix timestamp of issue
    ExpiryTimestamp     DQ 0            ; 0 = perpetual (never expires)
    HardwareHash        DQ 0            ; Expected HWID (0 = floating/any)
    SeatCount           DW 0            ; Concurrent seat limit
    PubKeyId            DB 0            ; Which embedded public key signed this
    Reserved            DB 13 DUP(0)    ; Alignment + future use
    ; Total: 64 bytes header
LICENSE_HEADER ENDS

ENTERPRISE_CONTEXT STRUCT
    State               DD 0                            ; Current LICENSE_* state
    _pad0               DD 0                            ; Alignment
    FeatureMask         DQ 0                            ; Active feature bits
    HardwareHash        DQ 0                            ; Current machine HWID
    hCryptProv          DQ 0                            ; CryptoAPI provider handle
    hPublicKey          DQ 0                            ; Imported RSA public key
    hRegKey             DQ 0                            ; Registry key handle
    ValidationCache     DD 0                            ; Cached last validation NTSTATUS
    Initialized         DD 0                            ; 1 = init complete
    LicenseLoaded       DD 0                            ; 1 = license blob loaded
    _pad1               DD 0
    LicenseData         DB LICENSE_BLOB_MAX_SIZE DUP(0) ; Raw license blob
    LicenseDataSize     DD 0                            ; Actual bytes loaded
    _pad2               DD 0
    Signature           DB RSA_SIGNATURE_SIZE DUP(0)    ; RSA-4096 signature
    SignatureSize       DD 0
    _pad3               DD 0
    csLock              CRITICAL_SECTION <>              ; Thread safety
ENTERPRISE_CONTEXT ENDS

; =============================================================================
;                          GLOBAL DATA
; =============================================================================
.data
ALIGN 16
g_EntCtx                ENTERPRISE_CONTEXT <>

; Unlocked engine flags (read by C++ engine registry)
PUBLIC g_800B_Unlocked
PUBLIC g_EnterpriseFeatures

g_800B_Unlocked         DD 0
g_EnterpriseFeatures    DQ 0
ALIGN 8

; Embedded RSA-4096 Public Key Blob (CryptoAPI PUBLICKEYBLOB format)
; In production: replace zeroed modulus with your actual signing key's public component
; Private key stays on your offline signing server — never shipped in binary
RSA_PUBLIC_KEY_BLOB LABEL BYTE
    DD  00002400h           ; PUBLICKEYBLOB, version 2
    DD  0000A400h           ; CALG_RSA_KEYX
    DB  "RSA2"              ; Magic
    DD  4096                ; Bit length
    DD  65537               ; Public exponent (0x10001)
    ; 512 bytes of RSA modulus (n) — zeroed template
    DB  512 DUP(0)
RSA_PUBLIC_KEY_SIZE EQU $ - RSA_PUBLIC_KEY_BLOB

; Registry path for persistent license storage
szRegPath               BYTE "SOFTWARE\RawrXD\Enterprise", 0
szRegLicenseValue       BYTE "LicenseBlob", 0
szRegSignatureValue     BYTE "Signature", 0
szRegHWIDValue          BYTE "HWID", 0

; Crypto container name
szCryptoContainer       BYTE "RawrXD_Enterprise_v1", 0

; Feature name strings (for UI / diagnostics)
szFeature800B           BYTE "800B Dual-Engine", 0
szFeatureAVX512         BYTE "AVX-512 Premium", 0
szFeatureSwarm          BYTE "Distributed Swarm", 0
szFeatureQuant4         BYTE "4-bit GPU Quantization", 0
szFeatureEntSupport     BYTE "Enterprise Support", 0
szFeatureUnlimitedCtx   BYTE "Unlimited Context", 0
szFeatureFlashAttn      BYTE "Flash-Attention", 0
szFeatureMultiGPU       BYTE "Multi-GPU Parallel", 0
szCommunityEdition      BYTE "RawrXD Community (Limited to 70B models)", 0
szSeparator             BYTE ", ", 0

; Debug strings
IFDEF DEBUG_BUILD
szDbgInitOK             BYTE "[Enterprise] License system initialized", 0
szDbgValidOK            BYTE "[Enterprise] License validated — Enterprise mode active", 0
szDbgNoLicense          BYTE "[Enterprise] No license found — Community mode", 0
szDbgCryptoFail         BYTE "[Enterprise] CryptoAPI initialization failed", 0
szDbgSigFail            BYTE "[Enterprise] RSA signature verification failed", 0
szDbgExpired            BYTE "[Enterprise] License expired", 0
szDbgHWMismatch         BYTE "[Enterprise] Hardware fingerprint mismatch", 0
szDbg800BUnlock         BYTE "[Enterprise] 800B Dual-Engine UNLOCKED", 0
szDbgTamper             BYTE "[Enterprise] Tamper detected — features disabled", 0
ENDIF

; =============================================================================
;                             CODE
; =============================================================================
.code

; =============================================================================
; Enterprise_InitLicenseSystem
; Master initialization: crypto provider, hardware fingerprint, registry load,
; license validation. Called once at process startup.
;
; Returns: RAX = 0 on success, NTSTATUS error code on failure
; =============================================================================
Enterprise_InitLicenseSystem PROC FRAME
    LOCAL   hKey:QWORD
    LOCAL   cbData:DWORD
    LOCAL   dwType:DWORD
    LOCAL   dwDisposition:DWORD
    LOCAL   status:DWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12

    .endprolog

    ; Default: invalid / community
    xor     eax, eax
    mov     g_EntCtx.State, LICENSE_INVALID
    mov     g_EntCtx.FeatureMask, rax
    mov     g_800B_Unlocked, 0
    mov     g_EnterpriseFeatures, rax

    ; ---- Initialize Critical Section ----
    lea     rcx, g_EntCtx.csLock
    call    InitializeCriticalSection

    ; ---- Acquire CryptoAPI Context ----
    ; CryptAcquireContextA(&hProv, container, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)
    sub     rsp, 30h
    lea     rcx, g_EntCtx.hCryptProv        ; phProv
    lea     rdx, szCryptoContainer           ; szContainer
    xor     r8, r8                           ; szProvider (NULL = default)
    mov     r9d, PROV_RSA_AES               ; dwProvType
    mov     dword ptr [rsp+20h], CRYPT_VERIFYCONTEXT ; dwFlags
    call    CryptAcquireContextA
    add     rsp, 30h

    test    eax, eax
    jz      @@crypto_fail

    ; ---- Import Embedded RSA Public Key ----
    ; CryptImportKey(hProv, pbData, cbData, 0, 0, &hKey)
    sub     rsp, 38h
    mov     rcx, g_EntCtx.hCryptProv        ; hProv
    lea     rdx, RSA_PUBLIC_KEY_BLOB        ; pbData
    mov     r8d, RSA_PUBLIC_KEY_SIZE        ; dwDataLen
    xor     r9, r9                           ; hPubKey (0 for public key import)
    mov     dword ptr [rsp+20h], 0           ; dwFlags
    lea     rax, g_EntCtx.hPublicKey
    mov     qword ptr [rsp+28h], rax         ; phKey
    call    CryptImportKey
    add     rsp, 38h

    test    eax, eax
    jz      @@crypto_fail

    ; ---- Generate Hardware Fingerprint ----
    call    Shield_GenerateHWID
    mov     g_EntCtx.HardwareHash, rax

    ; ---- Open/Create Registry Key ----
    ; RegCreateKeyExA(HKEY_CURRENT_USER, path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hKey, &disp)
    sub     rsp, 48h
    mov     rcx, HKEY_CURRENT_USER
    lea     rdx, szRegPath
    xor     r8d, r8d                         ; Reserved
    xor     r9, r9                           ; lpClass (NULL)
    mov     dword ptr [rsp+20h], 0           ; dwOptions
    mov     dword ptr [rsp+28h], KEY_ALL_ACCESS
    mov     qword ptr [rsp+30h], 0           ; lpSecurityAttributes (NULL)
    lea     rax, hKey
    mov     qword ptr [rsp+38h], rax         ; phkResult
    lea     rax, dwDisposition
    mov     qword ptr [rsp+40h], rax         ; lpdwDisposition
    call    RegCreateKeyExA
    add     rsp, 48h

    test    eax, eax
    jnz     @@no_license

    mov     rax, hKey
    mov     g_EntCtx.hRegKey, rax

    ; ---- Load License Blob from Registry ----
    mov     cbData, LICENSE_BLOB_MAX_SIZE
    sub     rsp, 38h
    mov     rcx, hKey                        ; hKey
    xor     edx, edx                         ; lpValueName reserved
    lea     rdx, szRegLicenseValue           ; lpValueName
    xor     r8, r8                           ; lpReserved (NULL)
    lea     r9, dwType                       ; lpType
    lea     rax, g_EntCtx.LicenseData
    mov     qword ptr [rsp+20h], rax         ; lpData
    lea     rax, cbData
    mov     qword ptr [rsp+28h], rax         ; lpcbData
    call    RegQueryValueExA
    add     rsp, 38h

    test    eax, eax
    jnz     @@no_license

    mov     eax, cbData
    mov     g_EntCtx.LicenseDataSize, eax
    mov     g_EntCtx.LicenseLoaded, 1

    ; ---- Load Signature from Registry ----
    mov     cbData, RSA_SIGNATURE_SIZE
    sub     rsp, 38h
    mov     rcx, hKey
    xor     edx, edx
    lea     rdx, szRegSignatureValue
    xor     r8, r8
    lea     r9, dwType
    lea     rax, g_EntCtx.Signature
    mov     qword ptr [rsp+20h], rax
    lea     rax, cbData
    mov     qword ptr [rsp+28h], rax
    call    RegQueryValueExA
    add     rsp, 38h

    test    eax, eax
    jnz     @@no_license

    mov     eax, cbData
    mov     g_EntCtx.SignatureSize, eax

    ; ---- Validate the Loaded License ----
    call    Enterprise_ValidateLicense
    mov     status, eax
    test    eax, eax
    jz      @@success

    ; Validation failed — run in community mode (not a fatal error)
    jmp     @@no_license

@@success:
    mov     g_EntCtx.Initialized, 1
    DBG_PRINT szDbgValidOK
    xor     eax, eax
    jmp     @@exit

@@no_license:
    ; No valid license — community mode (features limited, not an error)
    mov     g_EntCtx.State, LICENSE_INVALID
    mov     g_EntCtx.Initialized, 1
    DBG_PRINT szDbgNoLicense
    xor     eax, eax
    jmp     @@exit

@@crypto_fail:
    DBG_PRINT szDbgCryptoFail
    mov     eax, STATUS_CRYPTO_ERROR
    jmp     @@exit

@@exit:
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enterprise_InitLicenseSystem ENDP

; =============================================================================
; Enterprise_ValidateLicense
; Full cryptographic validation pipeline:
;   1. Magic check
;   2. Version check
;   3. Expiry check (via GetSystemTimeAsFileTime → Unix)
;   4. Hardware fingerprint check (if bound)
;   5. RSA-4096 signature verification (SHA-512 hash of header)
;   6. Feature activation on success
;
; Returns: RAX = 0 if valid, NTSTATUS error code otherwise
; =============================================================================
Enterprise_ValidateLicense PROC FRAME
    LOCAL   hHash:QWORD
    LOCAL   dwHashLen:DWORD
    LOCAL   fileTime:QWORD
    LOCAL   unixTime:QWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    mov     hHash, 0
    lea     rsi, g_EntCtx.LicenseData

    ; ---- 1. Verify Magic ----
    mov     eax, (LICENSE_HEADER PTR [rsi]).Magic
    cmp     eax, RAWR_LICENSE_MAGIC
    jne     @@invalid_magic

    ; ---- 2. Verify Version ----
    movzx   eax, (LICENSE_HEADER PTR [rsi]).Version
    cmp     ax, 1                            ; Must be version 1
    jb      @@version_mismatch
    cmp     ax, 100                          ; Reasonable upper bound
    ja      @@version_mismatch

    ; ---- 3. Check Expiry ----
    ; Get current Unix timestamp from Windows FILETIME
    lea     rcx, fileTime
    call    GetSystemTimeAsFileTime

    ; Convert FILETIME (100ns intervals since 1601) to Unix epoch (seconds since 1970)
    mov     rax, fileTime
    mov     rcx, 116444736000000000          ; FILETIME offset: Jan 1 1601 → Jan 1 1970
    sub     rax, rcx
    xor     edx, edx
    mov     rcx, 10000000                    ; 100ns → seconds
    div     rcx
    mov     unixTime, rax

    mov     rax, (LICENSE_HEADER PTR [rsi]).ExpiryTimestamp
    test    rax, rax                         ; 0 = perpetual (never expires)
    jz      @@check_hardware

    cmp     unixTime, rax
    ja      @@expired

@@check_hardware:
    ; ---- 4. Verify Hardware Fingerprint ----
    mov     rax, (LICENSE_HEADER PTR [rsi]).HardwareHash
    test    rax, rax                         ; 0 = floating license (any machine)
    jz      @@verify_signature

    cmp     rax, g_EntCtx.HardwareHash
    jne     @@hw_mismatch

@@verify_signature:
    ; ---- 5. RSA-4096 Signature Verification ----
    ; Create SHA-512 hash of license header
    ; CryptCreateHash(hProv, CALG_SHA_512, 0, 0, &hHash)
    sub     rsp, 30h
    mov     rcx, g_EntCtx.hCryptProv
    mov     edx, CALG_SHA_512
    xor     r8, r8                           ; hKey (0)
    xor     r9d, r9d                         ; dwFlags
    lea     rax, hHash
    mov     qword ptr [rsp+20h], rax
    call    CryptCreateHash
    add     rsp, 30h

    test    eax, eax
    jz      @@crypto_error

    ; Hash the license header (64 bytes, excluding signature)
    ; CryptHashData(hHash, pbData, dwDataLen, 0)
    mov     rcx, hHash
    lea     rdx, g_EntCtx.LicenseData
    mov     r8d, SIZEOF LICENSE_HEADER
    xor     r9d, r9d
    call    CryptHashData

    test    eax, eax
    jz      @@crypto_error

    ; Verify RSA signature against hash
    ; CryptVerifySignatureA(hHash, pbSignature, dwSigLen, hPubKey, NULL, 0)
    sub     rsp, 38h
    mov     rcx, hHash
    lea     rdx, g_EntCtx.Signature
    mov     r8d, RSA_SIGNATURE_SIZE
    mov     r9, g_EntCtx.hPublicKey
    mov     qword ptr [rsp+20h], 0           ; sDescription (NULL)
    mov     dword ptr [rsp+28h], 0           ; dwFlags
    call    CryptVerifySignatureA
    add     rsp, 38h

    test    eax, eax
    jz      @@sig_invalid

    ; ---- 6. Signature Valid — Activate Features ----
    lea     rsi, g_EntCtx.LicenseData
    mov     rax, (LICENSE_HEADER PTR [rsi]).FeatureMask
    mov     g_EntCtx.FeatureMask, rax
    mov     g_EnterpriseFeatures, rax

    ; Determine license tier
    test    rax, FEATURE_800B_DUALENGINE
    jz      @@trial_tier

    mov     g_EntCtx.State, LICENSE_VALID_ENTERPRISE
    jmp     @@valid

@@trial_tier:
    mov     g_EntCtx.State, LICENSE_VALID_TRIAL

@@valid:
    mov     g_EntCtx.ValidationCache, 0
    xor     eax, eax                         ; SUCCESS
    jmp     @@cleanup

@@invalid_magic:
    mov     eax, STATUS_INVALID_PARAMETER
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@version_mismatch:
    mov     eax, STATUS_VERSION_MISMATCH
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@expired:
    mov     g_EntCtx.State, LICENSE_EXPIRED
    DBG_PRINT szDbgExpired
    mov     eax, STATUS_LICENSE_EXPIRED
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@hw_mismatch:
    mov     g_EntCtx.State, LICENSE_HARDWARE_MISMATCH
    DBG_PRINT szDbgHWMismatch
    mov     eax, STATUS_HARDWARE_MISMATCH
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@sig_invalid:
    DBG_PRINT szDbgSigFail
    mov     eax, STATUS_SIGNATURE_INVALID
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@crypto_error:
    mov     eax, STATUS_CRYPTO_ERROR
    mov     g_EntCtx.ValidationCache, eax
    jmp     @@cleanup

@@cleanup:
    push    rax                              ; Save return status
    cmp     hHash, 0
    je      @@exit
    mov     rcx, hHash
    call    CryptDestroyHash
    pop     rax
    jmp     @@exit2

@@exit:
    pop     rax

@@exit2:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enterprise_ValidateLicense ENDP

; =============================================================================
; Enterprise_CheckFeature
; Fast runtime check — called by engines, registry, server before any gated op.
; Designed for minimum overhead on the hot path.
;
; RCX = Feature bitmask to check (e.g., FEATURE_800B_DUALENGINE)
; Returns: EAX = 1 if feature is enabled, 0 if disabled
; =============================================================================
Enterprise_CheckFeature PROC
    mov     rax, g_EntCtx.FeatureMask
    test    rax, rcx
    setnz   al
    movzx   eax, al
    ret
Enterprise_CheckFeature ENDP

; =============================================================================
; Enterprise_Unlock800BDualEngine
; Gates initialization of 800B parameter Dual-Engine mode.
; Called by Titan_Engine / StreamingEngineRegistry before allocating
; the 64MB+ DMA buffers required for 800B models.
;
; Returns: EAX = 1 if unlocked, 0 if denied
; =============================================================================
Enterprise_Unlock800BDualEngine PROC FRAME
    push    rbx
    .pushreg rbx

    .endprolog

    ; Check feature flag
    mov     ecx, FEATURE_800B_DUALENGINE
    call    Enterprise_CheckFeature
    test    eax, eax
    jz      @@denied

    ; Additional runtime integrity check (anti-tamper)
    call    Enterprise_RuntimeIntegrityCheck
    test    eax, eax
    jz      @@tamper

    ; Success: set global flag for C++ code
    mov     g_800B_Unlocked, 1
    DBG_PRINT szDbg800BUnlock

    mov     eax, 1
    jmp     @@exit

@@denied:
    xor     eax, eax
    jmp     @@exit

@@tamper:
    ; On tamper: revoke ALL features
    mov     g_EntCtx.FeatureMask, 0
    mov     g_EnterpriseFeatures, 0
    mov     g_EntCtx.State, LICENSE_INVALID
    mov     g_800B_Unlocked, 0
    DBG_PRINT szDbgTamper
    xor     eax, eax

@@exit:
    pop     rbx
    ret
Enterprise_Unlock800BDualEngine ENDP

; =============================================================================
; Enterprise_RuntimeIntegrityCheck
; Layered anti-tamper verification:
;   1. PEB BeingDebugged flag
;   2. RDTSC timing check
;   3. .text section hash verification
;
; Returns: EAX = 1 if clean, 0 if tampered or debugged
; =============================================================================
Enterprise_RuntimeIntegrityCheck PROC FRAME
    push    rbx
    .pushreg rbx

    .endprolog

    ; Layer 1: PEB check
    call    IsDebuggerPresent_Native
    test    eax, eax
    jnz     @@tampered

    ; Layer 2: Timing check
    call    Shield_TimingCheck
    test    eax, eax
    jz      @@tampered

    ; Layer 3: Code integrity
    call    Shield_VerifyIntegrity
    test    eax, eax
    jz      @@tampered

    mov     eax, 1                           ; Clean
    jmp     @@exit

@@tampered:
    xor     eax, eax

@@exit:
    pop     rbx
    ret
Enterprise_RuntimeIntegrityCheck ENDP

; =============================================================================
; Enterprise_GenerateHardwareHash
; Public wrapper around Shield_GenerateHWID for C++ callers.
;
; Returns: RAX = 64-bit hardware fingerprint
; =============================================================================
Enterprise_GenerateHardwareHash PROC
    jmp     Shield_GenerateHWID             ; Tail call to shield layer
Enterprise_GenerateHardwareHash ENDP

; =============================================================================
; Enterprise_InstallLicense
; Installs a new license from memory (called by UI import / CLI).
; Validates cryptographically before persisting to registry.
;
; RCX = Pointer to license blob
; RDX = Size of license blob
; R8  = Pointer to RSA signature (RSA_SIGNATURE_SIZE bytes)
; Returns: RAX = 0 on success, NTSTATUS error code on failure
; =============================================================================
Enterprise_InstallLicense PROC FRAME
    LOCAL   pLicense:QWORD
    LOCAL   cbLicense:QWORD
    LOCAL   pSignature:QWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    mov     pLicense, rcx
    mov     cbLicense, rdx
    mov     pSignature, r8

    ; Validate size bounds
    cmp     rdx, SIZEOF LICENSE_HEADER
    jb      @@invalid_size
    cmp     rdx, LICENSE_BLOB_MAX_SIZE
    ja      @@invalid_size

    ; Thread safety
    lea     rcx, g_EntCtx.csLock
    call    EnterCriticalSection

    ; Copy license data into context
    lea     rcx, g_EntCtx.LicenseData       ; dest
    mov     rdx, pLicense                    ; src
    mov     r8, cbLicense                    ; count
    call    memcpy

    mov     eax, dword ptr cbLicense
    mov     g_EntCtx.LicenseDataSize, eax
    mov     g_EntCtx.LicenseLoaded, 1

    ; Copy signature
    lea     rcx, g_EntCtx.Signature
    mov     rdx, pSignature
    mov     r8d, RSA_SIGNATURE_SIZE
    call    memcpy

    mov     g_EntCtx.SignatureSize, RSA_SIGNATURE_SIZE

    ; Validate before storing
    call    Enterprise_ValidateLicense
    test    eax, eax
    jnz     @@invalid_license

    ; Persist license blob to registry
    mov     rcx, g_EntCtx.hRegKey
    test    rcx, rcx
    jz      @@store_done                     ; No registry key = skip persistence

    sub     rsp, 38h
    ; rcx already set (hKey)
    lea     rdx, szRegLicenseValue           ; lpValueName
    xor     r8d, r8d                         ; Reserved (0)
    mov     r9d, REG_BINARY                  ; dwType
    lea     rax, g_EntCtx.LicenseData
    mov     qword ptr [rsp+20h], rax         ; lpData
    mov     eax, g_EntCtx.LicenseDataSize
    mov     dword ptr [rsp+28h], eax         ; cbData
    call    RegSetValueExA
    add     rsp, 38h

    ; Persist signature
    mov     rcx, g_EntCtx.hRegKey
    sub     rsp, 38h
    lea     rdx, szRegSignatureValue
    xor     r8d, r8d
    mov     r9d, REG_BINARY
    lea     rax, g_EntCtx.Signature
    mov     qword ptr [rsp+20h], rax
    mov     eax, g_EntCtx.SignatureSize
    mov     dword ptr [rsp+28h], eax
    call    RegSetValueExA
    add     rsp, 38h

@@store_done:
    ; Release lock
    lea     rcx, g_EntCtx.csLock
    call    LeaveCriticalSection

    xor     eax, eax                         ; SUCCESS
    jmp     @@exit

@@invalid_size:
    mov     eax, STATUS_INVALID_SIZE
    jmp     @@exit

@@invalid_license:
    ; Release lock on failure path too
    push    rax
    lea     rcx, g_EntCtx.csLock
    call    LeaveCriticalSection
    pop     rax
    ; eax already has error from Enterprise_ValidateLicense
    jmp     @@exit

@@exit:
    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enterprise_InstallLicense ENDP

; =============================================================================
; Enterprise_GetLicenseStatus
; Returns current license state for UI / diagnostics.
;
; Returns: EAX = LICENSE_* enum value
; =============================================================================
Enterprise_GetLicenseStatus PROC
    mov     eax, g_EntCtx.State
    ret
Enterprise_GetLicenseStatus ENDP

; =============================================================================
; Enterprise_GetFeatureString
; Builds human-readable feature list for About dialog / diagnostics.
;
; RCX = Output buffer pointer
; RDX = Buffer size (bytes)
; Returns: RAX = Number of bytes written
; =============================================================================
Enterprise_GetFeatureString PROC FRAME
    LOCAL   pBuf:QWORD
    LOCAL   cbBuf:QWORD
    LOCAL   pCur:QWORD
    LOCAL   features:QWORD

    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi

    .endprolog

    mov     pBuf, rcx
    mov     cbBuf, rdx
    mov     pCur, rcx

    mov     rax, g_EntCtx.FeatureMask
    mov     features, rax
    test    rax, rax
    jz      @@community

    ; Build comma-separated feature string
    ; Each feature check: test bit → append name → append separator

    test    rax, FEATURE_800B_DUALENGINE
    jz      @@chk_avx
    lea     rdx, szFeature800B
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

@@chk_avx:
    mov     rax, features
    test    rax, FEATURE_AVX512_PREMIUM
    jz      @@chk_swarm
    call    @@append_sep
    lea     rdx, szFeatureAVX512
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

@@chk_swarm:
    mov     rax, features
    test    rax, FEATURE_DISTRIBUTED_SWARM
    jz      @@chk_quant
    call    @@append_sep
    lea     rdx, szFeatureSwarm
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

@@chk_quant:
    mov     rax, features
    test    rax, FEATURE_GPU_QUANT_4BIT
    jz      @@chk_flash
    call    @@append_sep
    lea     rdx, szFeatureQuant4
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

@@chk_flash:
    mov     rax, features
    test    rax, FEATURE_FLASH_ATTENTION
    jz      @@chk_mgpu
    call    @@append_sep
    lea     rdx, szFeatureFlashAttn
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

@@chk_mgpu:
    mov     rax, features
    test    rax, FEATURE_MULTI_GPU
    jz      @@done
    call    @@append_sep
    lea     rdx, szFeatureMultiGPU
    mov     rcx, pCur
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax

    jmp     @@done

@@append_sep:
    ; If cursor is past start, append ", "
    mov     rax, pCur
    cmp     rax, pBuf
    je      @@sep_skip
    mov     rcx, pCur
    lea     rdx, szSeparator
    call    strcpy
    mov     rcx, pCur
    call    strlen
    add     pCur, rax
@@sep_skip:
    ret

@@community:
    ; No enterprise features — show community string
    mov     rcx, pBuf
    lea     rdx, szCommunityEdition
    call    strcpy

@@done:
    ; Calculate bytes written
    mov     rax, pCur
    sub     rax, pBuf

    pop     rdi
    pop     rsi
    pop     rbx
    ret
Enterprise_GetFeatureString ENDP

; =============================================================================
; Enterprise_Shutdown
; Cleanup: release crypto handles, close registry, delete critical section.
; =============================================================================
Enterprise_Shutdown PROC FRAME
    push    rbx
    .pushreg rbx

    .endprolog

    ; Destroy crypto hash key
    mov     rcx, g_EntCtx.hPublicKey
    test    rcx, rcx
    jz      @@skip_key
    call    CryptDestroyKey

@@skip_key:
    ; Release crypto provider
    mov     rcx, g_EntCtx.hCryptProv
    test    rcx, rcx
    jz      @@skip_prov
    xor     edx, edx
    call    CryptReleaseContext

@@skip_prov:
    ; Close registry key
    mov     rcx, g_EntCtx.hRegKey
    test    rcx, rcx
    jz      @@skip_reg
    call    RegCloseKey

@@skip_reg:
    ; Delete critical section
    lea     rcx, g_EntCtx.csLock
    call    DeleteCriticalSection

    ; Zero out sensitive context
    lea     rcx, g_EntCtx
    xor     edx, edx
    mov     r8d, SIZEOF ENTERPRISE_CONTEXT
    call    memset

    mov     g_800B_Unlocked, 0
    mov     g_EnterpriseFeatures, 0

    pop     rbx
    ret
Enterprise_Shutdown ENDP

; =============================================================================
;                    Integration Hooks (C++ Callsites)
; =============================================================================

; Called by StreamingEngineRegistry before registering 800B engines
Titan_CheckEnterpriseUnlock PROC
    mov     ecx, FEATURE_800B_DUALENGINE
    call    Enterprise_CheckFeature
    ret
Titan_CheckEnterpriseUnlock ENDP

; Called before allocating large DMA/VRAM buffers for enterprise engines
; RCX = requested allocation size
; Returns: EAX = 1 if budget approved, 0 if community-limited
Streaming_CheckEnterpriseBudget PROC FRAME
    LOCAL   requestedSize:QWORD

    push    rbx
    .pushreg rbx

    .endprolog

    mov     requestedSize, rcx

    ; Check if enterprise features are active
    call    Enterprise_Unlock800BDualEngine
    test    eax, eax
    jz      @@community_limit

    mov     eax, 1                           ; Budget approved
    jmp     @@exit

@@community_limit:
    ; Community mode: cap at 70B model buffer (~16GB)
    mov     rax, requestedSize
    mov     rcx, 17179869184                 ; 16 GB
    cmp     rax, rcx
    ja      @@deny

    mov     eax, 1                           ; Within community limit
    jmp     @@exit

@@deny:
    xor     eax, eax                         ; Over budget for community

@@exit:
    pop     rbx
    ret
Streaming_CheckEnterpriseBudget ENDP

END
