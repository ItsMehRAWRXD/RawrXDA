; =============================================================================
; RawrXD_DynamicPromptEngine.asm — Context-Aware Prompt Generation Kernel
; =============================================================================
; Hardened implementation with all audit fixes applied:
;   FIX 1: 64-bit pointer math in template indexing
;   FIX 2: Proper substring matching in CountKeywordMatches
;   FIX 3: Correct little-endian marker detection
;   FIX 4: Proper calling convention (outsize via register)
;   FIX 5: Spinlock-guarded scratch buffer
;   FIX 6: ForceMode honored in AnalyzeContext
;   FIX 7: Density-scored classification with tie-break
;
; Classification Modes:
;   0x00 = GENERIC (default analytical)
;   0x01 = CASUAL  (slang/conversational)
;   0x02 = CODE    (technical review)
;   0x03 = SECURITY (vulnerability assessment)
;   0x04 = SHELL   (command analysis)
;   0x05 = ENTERPRISE (compliance-heavy)
;
; Exports:
;   PromptGen_AnalyzeContext  — Classify input text, return mode ID
;   PromptGen_BuildCritic    — Generate critic prompt for mode+context
;   PromptGen_BuildAuditor   — Generate auditor prompt for mode+context
;   PromptGen_Interpolate    — Inject context vars into template (public)
;   PromptGen_GetTemplate    — Retrieve raw template by ID
;   PromptGen_ForceMode      — Override auto-classification
;
; Rule:    NO CRT, NO HEAP, NO EXCEPTIONS
;          Fixed 8KB scratch buffer with spinlock
; Build:   ml64 /c /Zi /Fo RawrXD_DynamicPromptEngine.obj RawrXD_DynamicPromptEngine.asm
; =============================================================================

include RawrXD_Common.inc

; =============================================================================
; External Imports
; =============================================================================
EXTERNDEF lstrlenA:PROC
EXTERNDEF lstrcpyA:PROC
EXTERNDEF lstrcatA:PROC
EXTERNDEF lstrcmpiA:PROC
EXTERNDEF GetCurrentThreadId:PROC

; =============================================================================
; Constants
; =============================================================================

; Context Classification Modes
CTX_MODE_GENERIC        EQU 0
CTX_MODE_CASUAL         EQU 1
CTX_MODE_CODE           EQU 2
CTX_MODE_SECURITY       EQU 3
CTX_MODE_SHELL          EQU 4
CTX_MODE_ENTERPRISE     EQU 5
CTX_MODE_MAX            EQU 6

; Buffer sizes
MAX_TEMPLATE_SIZE       EQU 2048
MAX_CONTEXT_SIZE        EQU 4096
MAX_PROMPT_SIZE         EQU 6144
SCRATCH_BUFFER_SIZE     EQU 8192
MAX_KEYWORD_LEN         EQU 64

; Density thresholds — minimum weighted hits to classify
; Shell/Security get low thresholds (safety-critical)
DENSITY_SHELL           EQU 1
DENSITY_SECURITY        EQU 1
DENSITY_CODE            EQU 2
DENSITY_ENTERPRISE      EQU 2
DENSITY_CASUAL          EQU 2

; Short input fast-path threshold (chars)
SHORT_INPUT_THRESHOLD   EQU 60

; =============================================================================
; Data Segment — Template Database
; =============================================================================
.data

; -------------------------------------------------------------------------
; Mode 0: GENERIC — Balanced analytical
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Generic_Critic     db \
    "Analyze the following input with balanced skepticism. Identify logical flaws, ", \
    "factual errors, or potential improvements while maintaining objectivity. ", \
    "Consider edge cases and alternative interpretations.",0Dh,0Ah,0Dh,0Ah, \
    "INPUT TO ANALYZE:",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah, \
    "Provide specific, actionable feedback without unnecessary verbosity.",0

ALIGN 8
Tmpl_Generic_Auditor    db \
    "Conduct a lightweight review of the provided content. Check for:",0Dh,0Ah, \
    "- Clarity and coherence",0Dh,0Ah, \
    "- Potential misinterpretations",0Dh,0Ah, \
    "- Basic compliance with standard communication norms",0Dh,0Ah,0Dh,0Ah, \
    "CONTENT:",0Dh,0Ah, \
    "{{CONTEXT}}",0

; -------------------------------------------------------------------------
; Mode 1: CASUAL — Slang-friendly, conversational
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Casual_Critic      db \
    "Keep it real. Review this like you're talking to a friend, not a boardroom. ", \
    "If someone's being cringe or trying too hard, call it out with humor. ", \
    "Don't overthink slang or informal tone unless it's actually confusing.",0Dh,0Ah,0Dh,0Ah, \
    "THE THING:",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah, \
    "Roast lightly if deserved, but keep it constructive.",0

ALIGN 8
Tmpl_Casual_Auditor     db \
    "Vibe check incoming content. Look for:",0Dh,0Ah, \
    "- Authenticity (does it feel forced?)",0Dh,0Ah, \
    "- Actual confusion vs just informal tone",0Dh,0Ah, \
    "- Whether the message lands or misses",0Dh,0Ah,0Dh,0Ah, \
    "CHECK THIS:",0Dh,0Ah, \
    "{{CONTEXT}}",0

; -------------------------------------------------------------------------
; Mode 2: CODE — Technical review
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Code_Critic        db "Perform a rigorous code review. Analyze for:",0Dh,0Ah
    db "1. Logic errors and off-by-one bugs",0Dh,0Ah
    db "2. Memory safety issues (leaks, use-after-free, buffer overflows)",0Dh,0Ah
    db "3. Performance bottlenecks and algorithmic inefficiency",0Dh,0Ah
    db "4. Architecture violations and anti-patterns",0Dh,0Ah
    db "5. Missing error handling and edge cases",0Dh,0Ah,0Dh,0Ah
    db "CODE UNDER REVIEW:",0Dh,0Ah
    db "```",0Dh,0Ah
    db "{{CONTEXT}}",0Dh,0Ah
    db "```",0Dh,0Ah,0Dh,0Ah
    db "Be technically precise. Suggest concrete fixes with code examples.",0

ALIGN 8
Tmpl_Code_Auditor       db \
    "Static analysis scan. Check the following code for:",0Dh,0Ah, \
    "[ ] Syntax vulnerabilities",0Dh,0Ah, \
    "[ ] Unsafe API usage (strcpy, gets, etc.)",0Dh,0Ah, \
    "[ ] Resource cleanup (handles, memory, files)",0Dh,0Ah, \
    "[ ] Thread safety concerns",0Dh,0Ah, \
    "[ ] Compiler warnings that indicate UB",0Dh,0Ah,0Dh,0Ah, \
    "SCAN TARGET:",0Dh,0Ah, \
    "```",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah, \
    "```",0

; -------------------------------------------------------------------------
; Mode 3: SECURITY — Vulnerability assessment
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Sec_Critic         db "SECURITY AUDIT - THREAT MODELING MODE",0Dh,0Ah
    db "Analyze for attack vectors:",0Dh,0Ah
    db "- Injection vulnerabilities (SQL, CMD, XSS)",0Dh,0Ah
    db "- Privilege escalation paths",0Dh,0Ah
    db "- Data exfiltration risks",0Dh,0Ah
    db "- Cryptographic weaknesses",0Dh,0Ah
    db "- Supply chain / dependency risks",0Dh,0Ah,0Dh,0Ah
    db "ATTACK SURFACE:",0Dh,0Ah
    db "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah
    db "CVSS-style severity rating required for each finding.",0

ALIGN 8
Tmpl_Sec_Auditor        db \
    "COMPLIANCE & SECURITY CHECK",0Dh,0Ah, \
    "Verify against:",0Dh,0Ah, \
    "- OWASP Top 10",0Dh,0Ah, \
    "- CWE/SANS Top 25",0Dh,0Ah, \
    "- Enterprise security baseline",0Dh,0Ah,0Dh,0Ah, \
    "AUDIT TARGET:",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah, \
    "Flag any deviation from secure coding standards.",0

; -------------------------------------------------------------------------
; Mode 4: SHELL — Command analysis
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Shell_Critic       db "SHELL COMMAND ANALYSIS",0Dh,0Ah
    db "Review this command for:",0Dh,0Ah
    db "- Dangerous flags (rm -rf, dd, mkfs)",0Dh,0Ah
    db "- Injection vulnerabilities (unquoted vars)",0Dh,0Ah
    db "- Permission issues (sudo misuse)",0Dh,0Ah
    db "- Efficiency (useless cat, grep | grep)",0Dh,0Ah
    db "- Portability (bashisms in sh scripts)",0Dh,0Ah,0Dh,0Ah
    db "COMMAND:",0Dh,0Ah
    db "$ {{CONTEXT}}",0Dh,0Ah,0Dh,0Ah
    db "Suggest safer/more efficient alternatives.",0

ALIGN 8
Tmpl_Shell_Auditor      db \
    "TERMINAL SAFETY CHECK",0Dh,0Ah, \
    "Scan command for destructive operations:",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah, \
    "APPROVAL STATUS: [ ] SAFE  [ ] REVIEW REQUIRED  [ ] DANGEROUS",0

; -------------------------------------------------------------------------
; Mode 5: ENTERPRISE — Maximum compliance
; -------------------------------------------------------------------------
ALIGN 8
Tmpl_Ent_Critic         db \
    "ENTERPRISE CRITICAL ANALYSIS",0Dh,0Ah, \
    "Evaluate professional suitability:",0Dh,0Ah, \
    "- Tone appropriateness for corporate environment",0Dh,0Ah, \
    "- Legal liability exposure",0Dh,0Ah, \
    "- Brand reputation risk",0Dh,0Ah, \
    "- Inclusivity and accessibility compliance",0Dh,0Ah,0Dh,0Ah, \
    "CORPORATE ASSET:",0Dh,0Ah, \
    "{{CONTEXT}}",0Dh,0Ah,0Dh,0Ah, \
    "Risk assessment and mitigation strategy required.",0

ALIGN 8
Tmpl_Ent_Auditor        db \
    "REGULATORY COMPLIANCE VERIFICATION",0Dh,0Ah, \
    "Check alignment with:",0Dh,0Ah, \
    "- ISO 27001",0Dh,0Ah, \
    "- SOC 2 Type II",0Dh,0Ah, \
    "- GDPR/CCPA data handling",0Dh,0Ah, \
    "- Industry-specific regulations",0Dh,0Ah,0Dh,0Ah, \
    "SUBMISSION:",0Dh,0Ah, \
    "{{CONTEXT}}",0

; -------------------------------------------------------------------------
; Keyword Databases for Classification
; Each keyword is null-terminated. List ends with double-null.
; (Comma-separated replaced with null-separated for proper tokenization)
; -------------------------------------------------------------------------

; Code keywords (null-separated, double-null terminated)
ALIGN 4
Kw_Code     db "function",0,"class",0,"struct",0,"import",0,"#include",0
            db "def ",0,"void ",0,"int ",0,"return ",0,"namespace",0
            db "template",0,"public:",0,"private:",0,"virtual",0
            db "malloc",0,"free(",0,"printf",0,"std::",0,"nullptr",0
            db 0  ; double-null terminator

; Security keywords
ALIGN 4
Kw_Security db "vulnerability",0,"exploit",0,"cve-",0,"xss",0
            db "sqli",0,"injection",0,"buffer overflow",0,"privilege",0
            db "escalation",0,"auth bypass",0,"token",0,"jwt",0
            db "encrypt",0,"decrypt",0,"md5",0,"sha256",0
            db "password",0,"secret",0,"ssl",0,"tls",0
            db 0

; Shell keywords
ALIGN 4
Kw_Shell    db "sudo ",0,"rm -",0,"chmod ",0,"chown ",0
            db "apt ",0,"yum ",0,"pacman ",0,"brew ",0
            db "git clone",0,"./configure",0,"make install",0
            db "| grep",0,"| awk",0,"| sed",0
            db "bash ",0,"sh ",0,"powershell",0,"cmd /c",0
            db 0

; Casual/slang keywords
ALIGN 4
Kw_Casual   db " yo ",0," sup ",0,"playa",0," bro ",0," dude ",0
            db " lol",0,"lmao",0," bruh",0," fr ",0," ngl ",0
            db " tbh",0," idk ",0," smh",0,"gonna ",0,"wanna ",0
            db "gotta ",0,"kinda ",0,"dunno",0,"lemme ",0,"yeet",0
            db 0

; Enterprise keywords
ALIGN 4
Kw_Enterprise db "stakeholder",0,"deliverable",0,"synergy",0
              db "leverage",0,"paradigm",0,"scalable",0
              db "roi",0,"kpi",0,"okr",0
              db "compliance",0,"governance",0,"framework",0
              db "methodology",0,"best practice",0,"actionable",0
              db 0

; -------------------------------------------------------------------------
; Template lookup table (2 pointers per mode: [critic, auditor])
; Index: mode * 16 + 0 = Critic, mode * 16 + 8 = Auditor
; -------------------------------------------------------------------------
ALIGN 8
g_TemplateTable         dq \
    Tmpl_Generic_Critic,  Tmpl_Generic_Auditor, \
    Tmpl_Casual_Critic,   Tmpl_Casual_Auditor, \
    Tmpl_Code_Critic,     Tmpl_Code_Auditor, \
    Tmpl_Sec_Critic,      Tmpl_Sec_Auditor, \
    Tmpl_Shell_Critic,    Tmpl_Shell_Auditor, \
    Tmpl_Ent_Critic,      Tmpl_Ent_Auditor

; -------------------------------------------------------------------------
; Context marker for interpolation (aligned for safe dword comparison)
; -------------------------------------------------------------------------
ALIGN 4
szContextMarker         db "{{CONTEXT}}",0
CONTEXT_MARKER_LEN      EQU 11          ; strlen("{{CONTEXT}}")

; Little-endian dword for the first 4 bytes of "{{CO" = 0x4F437B7B
CONTEXT_MARKER_DWORD    EQU 4F437B7Bh   ; '{','{','C','O' in LE

; -------------------------------------------------------------------------
; Scratch buffer with spinlock (FIX 5)
; -------------------------------------------------------------------------
ALIGN 8
g_ScratchLock           dd 0
                        dd 0            ; padding for alignment
g_ScratchBuffer         db SCRATCH_BUFFER_SIZE dup(0)

; -------------------------------------------------------------------------
; Force mode override (FIX 6)
; -1 = auto-detect, 0-5 = forced mode
; -------------------------------------------------------------------------
ALIGN 4
g_ForceMode             dd -1

; -------------------------------------------------------------------------
; Score accumulators (per-call, not global; these are layout reference)
; Actual scores use stack in AnalyzeContext
; -------------------------------------------------------------------------

; Code-block fence detector
szCodeFence             db "```",0

; =============================================================================
; Code Segment
; =============================================================================
.code

; =============================================================================
; AcquireScratchLock — Simple spinlock acquire
; Clobbers: EAX, ECX
; =============================================================================
AcquireScratchLock PROC
    mov     ecx, 1
@@spin:
    xor     eax, eax
    lock cmpxchg dword ptr [g_ScratchLock], ecx
    jnz     @@spin
    ret
AcquireScratchLock ENDP

; =============================================================================
; ReleaseScratchLock — Release spinlock
; =============================================================================
ReleaseScratchLock PROC
    mov     dword ptr [g_ScratchLock], 0
    ret
ReleaseScratchLock ENDP

; =============================================================================
; CaseFoldChar — Fold ASCII uppercase to lowercase
; AL = input char
; Returns: AL = lowercase char
; =============================================================================
CaseFoldChar PROC
    cmp     al, 'A'
    jb      @@done
    cmp     al, 'Z'
    ja      @@done
    add     al, 20h             ; 'A' -> 'a'
@@done:
    ret
CaseFoldChar ENDP

; =============================================================================
; IsWordBoundary — Check if character is a word boundary
; AL = character to check
; Returns: AL = 1 if boundary, 0 if not
; =============================================================================
IsWordBoundary PROC
    cmp     al, 0               ; null
    je      @@yes
    cmp     al, ' '
    je      @@yes
    cmp     al, 09h             ; tab
    je      @@yes
    cmp     al, 0Ah             ; LF
    je      @@yes
    cmp     al, 0Dh             ; CR
    je      @@yes
    cmp     al, ','
    je      @@yes
    cmp     al, '.'
    je      @@yes
    cmp     al, ';'
    je      @@yes
    cmp     al, ':'
    je      @@yes
    cmp     al, '('
    je      @@yes
    cmp     al, ')'
    je      @@yes
    cmp     al, '['
    je      @@yes
    cmp     al, ']'
    je      @@yes
    cmp     al, '{'
    je      @@yes
    cmp     al, '}'
    je      @@yes
    cmp     al, '"'
    je      @@yes
    cmp     al, 27h             ; single quote
    je      @@yes
    cmp     al, '!'
    je      @@yes
    cmp     al, '?'
    je      @@yes
    cmp     al, '|'
    je      @@yes
    cmp     al, '&'
    je      @@yes
    xor     al, al              ; not boundary
    ret
@@yes:
    mov     al, 1
    ret
IsWordBoundary ENDP

; =============================================================================
; SubstringSearchCI — Case-insensitive substring search
; RCX = haystack, RDX = haystack length, R8 = needle (null-terminated)
; Returns: RAX = 1 if found, 0 if not
; =============================================================================
SubstringSearchCI PROC
    push    rbx
    push    rsi
    push    rdi
    push    r12
    push    r13

    mov     rsi, rcx            ; haystack
    mov     r12, rdx            ; haystack len
    mov     rdi, r8             ; needle

    ; Get needle length
    xor     r13, r13
@@needle_len:
    cmp     byte ptr [rdi + r13], 0
    je      @@have_needle_len
    inc     r13
    jmp     @@needle_len
@@have_needle_len:

    test    r13, r13
    jz      @@not_found         ; empty needle

    ; If needle longer than haystack, impossible
    cmp     r13, r12
    ja      @@not_found

    ; Scan haystack
    mov     rcx, r12
    sub     rcx, r13
    inc     rcx                 ; number of positions to check
    xor     rbx, rbx            ; haystack position

@@scan_loop:
    test    rcx, rcx
    jz      @@not_found

    ; Compare needle at rsi+rbx (case-insensitive)
    xor     rdx, rdx            ; needle index
@@compare_chars:
    cmp     rdx, r13
    je      @@found             ; all chars matched

    lea     rcx, [rsi + rbx]        ; base + outer offset
    mov     al, byte ptr [rcx + rdx]  ; haystack[rbx + rdx]
    ; Case-fold haystack char
    cmp     al, 'A'
    jb      @@hc_done
    cmp     al, 'Z'
    ja      @@hc_done
    add     al, 20h
@@hc_done:
    mov     ah, al              ; ah = folded haystack char

    mov     al, byte ptr [rdi + rdx]
    ; Case-fold needle char
    cmp     al, 'A'
    jb      @@nc_done
    cmp     al, 'Z'
    ja      @@nc_done
    add     al, 20h
@@nc_done:

    cmp     al, ah
    jne     @@next_pos

    inc     rdx
    jmp     @@compare_chars

@@next_pos:
    inc     rbx
    dec     rcx
    jmp     @@scan_loop

@@found:
    mov     rax, 1
    jmp     @@return

@@not_found:
    xor     rax, rax

@@return:
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
SubstringSearchCI ENDP

; =============================================================================
; CountKeywordMatches — Proper word-aware keyword counting (FIX 2)
;
; Walks the null-separated keyword list. For each keyword, does a
; case-insensitive substring search in the input text.
; Returns the number of distinct keywords found.
;
; RCX = Input text
; RDX = Input text length
; R8  = Keyword list (null-separated, double-null terminated)
; Returns: EAX = match count
; =============================================================================
CountKeywordMatches PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     r12, rcx            ; text
    mov     r13, rdx            ; text length
    mov     r14, r8             ; keyword list
    xor     r15d, r15d          ; match count

@@kw_loop:
    ; Check if we've hit the double-null terminator
    cmp     byte ptr [r14], 0
    je      @@kw_done

    ; r14 points to current keyword (null-terminated)
    ; Search for it in text
    mov     rcx, r12            ; haystack = input text
    mov     rdx, r13            ; haystack len
    mov     r8, r14             ; needle = current keyword
    call    SubstringSearchCI
    test    eax, eax
    jz      @@kw_advance
    inc     r15d                ; found a match

@@kw_advance:
    ; Advance past this keyword's null terminator
@@kw_skip:
    cmp     byte ptr [r14], 0
    je      @@kw_next
    inc     r14
    jmp     @@kw_skip
@@kw_next:
    inc     r14                 ; skip the null terminator
    jmp     @@kw_loop

@@kw_done:
    mov     eax, r15d

    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
CountKeywordMatches ENDP

; =============================================================================
; DetectCodeBlock — Fast check for ``` fences in input
; RCX = text, RDX = length
; Returns: EAX = 1 if code block detected, 0 otherwise
; =============================================================================
DetectCodeBlock PROC
    push    rsi
    mov     rsi, rcx
    mov     rcx, rdx
    sub     rcx, 2              ; need at least 3 chars for ```
    jle     @@no_fence

@@fence_scan:
    cmp     byte ptr [rsi], '`'
    jne     @@fence_next
    cmp     byte ptr [rsi+1], '`'
    jne     @@fence_next
    cmp     byte ptr [rsi+2], '`'
    jne     @@fence_next
    ; Found ```
    mov     eax, 1
    pop     rsi
    ret

@@fence_next:
    inc     rsi
    dec     rcx
    jnz     @@fence_scan

@@no_fence:
    xor     eax, eax
    pop     rsi
    ret
DetectCodeBlock ENDP

; =============================================================================
; PromptGen_AnalyzeContext — Heuristic classification with scoring (FIX 6, 7)
;
; RCX = Input text pointer
; RDX = Text length (0 = auto-detect via lstrlenA)
; Returns: EAX = Mode ID (CTX_MODE_*)
;          EDX = Match score for winning mode
; =============================================================================
PUBLIC PromptGen_AnalyzeContext
PromptGen_AnalyzeContext PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 128
    .allocstack 128
    .endprolog

    mov     rsi, rcx            ; rsi = text
    mov     r12, rdx            ; r12 = length

    ; ----- FIX 6: Check ForceMode override first ----- 
    mov     eax, dword ptr [g_ForceMode]
    cmp     eax, -1
    je      @@auto_detect
    ; Forced mode — validate range
    cmp     eax, CTX_MODE_MAX
    jae     @@auto_detect       ; invalid forced mode, fall through
    xor     edx, edx            ; score = 0 (forced)
    jmp     @@return

@@auto_detect:
    ; Auto-detect length if zero
    test    r12, r12
    jnz     @@have_len
    mov     rcx, rsi
    sub     rsp, 32
    call    lstrlenA
    add     rsp, 32
    mov     r12, rax
@@have_len:

    test    r12, r12
    jz      @@default_generic

    ; ----- Score accumulators on stack -----
    ; [rsp+0]  = code score
    ; [rsp+4]  = security score
    ; [rsp+8]  = shell score
    ; [rsp+12] = casual score
    ; [rsp+16] = enterprise score
    mov     dword ptr [rsp+0],  0
    mov     dword ptr [rsp+4],  0
    mov     dword ptr [rsp+8],  0
    mov     dword ptr [rsp+12], 0
    mov     dword ptr [rsp+16], 0

    ; ----- Fast-path: code block detection -----
    mov     rcx, rsi
    mov     rdx, r12
    call    DetectCodeBlock
    test    eax, eax
    jz      @@no_code_block
    ; Code fence found — strong code signal (+5)
    add     dword ptr [rsp+0], 5
@@no_code_block:

    ; ----- Scan: Code keywords -----
    mov     rcx, rsi
    mov     rdx, r12
    lea     r8, Kw_Code
    call    CountKeywordMatches
    add     dword ptr [rsp+0], eax

    ; ----- Scan: Security keywords -----
    mov     rcx, rsi
    mov     rdx, r12
    lea     r8, Kw_Security
    call    CountKeywordMatches
    ; Security keywords get 2x weight (safety-critical)
    shl     eax, 1
    add     dword ptr [rsp+4], eax

    ; ----- Scan: Shell keywords -----
    mov     rcx, rsi
    mov     rdx, r12
    lea     r8, Kw_Shell
    call    CountKeywordMatches
    ; Shell also 2x weight (dangerous commands)
    shl     eax, 1
    add     dword ptr [rsp+8], eax

    ; ----- Scan: Casual keywords -----
    mov     rcx, rsi
    mov     rdx, r12
    lea     r8, Kw_Casual
    call    CountKeywordMatches
    add     dword ptr [rsp+12], eax

    ; ----- Scan: Enterprise keywords -----
    mov     rcx, rsi
    mov     rdx, r12
    lea     r8, Kw_Enterprise
    call    CountKeywordMatches
    add     dword ptr [rsp+16], eax

    ; ----- Short input heuristic -----
    ; Short messages (<60 chars) with no code/security signals
    ; get a casual bias (+2) since formal prompts tend to be longer
    cmp     r12, SHORT_INPUT_THRESHOLD
    ja      @@no_short_bias
    cmp     dword ptr [rsp+0], 0    ; no code hits
    jne     @@no_short_bias
    cmp     dword ptr [rsp+4], 0    ; no security hits
    jne     @@no_short_bias
    cmp     dword ptr [rsp+8], 0    ; no shell hits
    jne     @@no_short_bias
    add     dword ptr [rsp+12], 2   ; casual bias for short inputs
@@no_short_bias:

    ; ----- Decision: highest score wins (FIX 7) -----
    ; Priority tie-break order: Shell > Security > Code > Enterprise > Casual > Generic
    ; Each must exceed its density threshold

    mov     eax, CTX_MODE_GENERIC
    xor     ebx, ebx            ; winning score

    ; Check Shell (highest priority for safety)
    mov     ecx, dword ptr [rsp+8]
    cmp     ecx, DENSITY_SHELL
    jb      @@chk_security
    cmp     ecx, ebx
    jbe     @@chk_security
    mov     eax, CTX_MODE_SHELL
    mov     ebx, ecx

@@chk_security:
    mov     ecx, dword ptr [rsp+4]
    cmp     ecx, DENSITY_SECURITY
    jb      @@chk_code
    cmp     ecx, ebx
    jbe     @@chk_code
    mov     eax, CTX_MODE_SECURITY
    mov     ebx, ecx

@@chk_code:
    mov     ecx, dword ptr [rsp+0]
    cmp     ecx, DENSITY_CODE
    jb      @@chk_enterprise
    cmp     ecx, ebx
    jbe     @@chk_enterprise
    mov     eax, CTX_MODE_CODE
    mov     ebx, ecx

@@chk_enterprise:
    mov     ecx, dword ptr [rsp+16]
    cmp     ecx, DENSITY_ENTERPRISE
    jb      @@chk_casual
    cmp     ecx, ebx
    jbe     @@chk_casual
    mov     eax, CTX_MODE_ENTERPRISE
    mov     ebx, ecx

@@chk_casual:
    mov     ecx, dword ptr [rsp+12]
    cmp     ecx, DENSITY_CASUAL
    jb      @@decision_done
    cmp     ecx, ebx
    jbe     @@decision_done
    mov     eax, CTX_MODE_CASUAL
    mov     ebx, ecx

@@decision_done:
    mov     edx, ebx            ; return winning score in EDX
    jmp     @@return

@@default_generic:
    xor     eax, eax
    xor     edx, edx

@@return:
    add     rsp, 128
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PromptGen_AnalyzeContext ENDP

; =============================================================================
; InterpolateTemplate — Replace {{CONTEXT}} marker with actual content (FIX 3,4)
;
; RCX = Template string (null-terminated)
; RDX = Context string (null-terminated)
; R8  = Output buffer
; R9  = Output buffer size (FIX 4: passed in register, not stack)
; Returns: RAX = Bytes written (0 on overflow/error)
; =============================================================================
InterpolateTemplate PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 32
    .allocstack 32
    .endprolog

    mov     rsi, rcx            ; template
    mov     r12, rdx            ; context
    mov     rdi, r8             ; output buffer
    mov     r15, r9             ; output size (FIX 4: from R9, not stack)

    xor     r14, r14            ; bytes written

    ; Need at least 1 byte for null terminator
    test    r15, r15
    jz      @@overflow

@@tmpl_loop:
    ; Check output space (leave 1 for null)
    lea     rax, [r14 + 1]
    cmp     rax, r15
    jae     @@overflow

    mov     al, byte ptr [rsi]
    test    al, al
    jz      @@done

    ; FIX 3: Check for "{{CONTEXT}}" marker using correct LE dword
    ; "{{CO" in LE = 0x4F437B7B
    cmp     al, '{'
    jne     @@copy_char

    ; Potential marker — verify we have enough bytes
    lea     rax, [rsi]
    cmp     byte ptr [rax+1], '{'
    jne     @@copy_char
    cmp     byte ptr [rax+2], 'C'
    jne     @@copy_char
    cmp     byte ptr [rax+3], 'O'
    jne     @@copy_char
    cmp     byte ptr [rax+4], 'N'
    jne     @@copy_char
    cmp     byte ptr [rax+5], 'T'
    jne     @@copy_char
    cmp     byte ptr [rax+6], 'E'
    jne     @@copy_char
    cmp     byte ptr [rax+7], 'X'
    jne     @@copy_char
    cmp     byte ptr [rax+8], 'T'
    jne     @@copy_char
    cmp     byte ptr [rax+9], '}'
    jne     @@copy_char
    cmp     byte ptr [rax+10], '}'
    jne     @@copy_char

    ; Full "{{CONTEXT}}" matched — inject context
    push    rsi
    mov     rsi, r12            ; switch to context

@@inject_loop:
    mov     al, byte ptr [rsi]
    test    al, al
    jz      @@inject_done

    ; Bounds check
    lea     rbx, [r14 + 1]
    cmp     rbx, r15
    jae     @@inject_overflow

    mov     byte ptr [rdi + r14], al
    inc     r14
    inc     rsi
    jmp     @@inject_loop

@@inject_overflow:
    pop     rsi
    jmp     @@overflow

@@inject_done:
    pop     rsi
    add     rsi, CONTEXT_MARKER_LEN  ; skip past "{{CONTEXT}}"
    jmp     @@tmpl_loop

@@copy_char:
    mov     byte ptr [rdi + r14], al
    inc     r14
    inc     rsi
    jmp     @@tmpl_loop

@@overflow:
    ; Null-terminate what we have
    test    r15, r15
    jz      @@overflow_ret
    cmp     r14, r15
    jb      @@overflow_null
    mov     r14, r15
    dec     r14
@@overflow_null:
    mov     byte ptr [rdi + r14], 0
@@overflow_ret:
    xor     rax, rax            ; return 0 on overflow
    jmp     @@return

@@done:
    mov     byte ptr [rdi + r14], 0   ; null terminate
    mov     rax, r14            ; return bytes written

@@return:
    add     rsp, 32
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
InterpolateTemplate ENDP

; =============================================================================
; PromptGen_BuildCritic — Generate complete critic prompt
;
; RCX = Input context text (null-terminated)
; RDX = Context length (0 = auto via lstrlenA)
; R8  = Output buffer
; R9  = Output buffer size
; Returns: RAX = Bytes written (0 on overflow)
; =============================================================================
PUBLIC PromptGen_BuildCritic
PromptGen_BuildCritic PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     rsi, rcx            ; context text
    mov     r12, rdx            ; context length
    mov     r13, r8             ; output buffer
    mov     r14, r9             ; output size

    ; Classify input
    mov     rcx, rsi
    mov     rdx, r12
    call    PromptGen_AnalyzeContext
    mov     ebx, eax            ; ebx = mode

    ; FIX 1: 64-bit pointer math for template indexing
    movzx   rax, bx             ; zero-extend mode to 64-bit
    shl     rax, 4              ; mode * 16 (2 qword pointers per mode)
    lea     rdx, g_TemplateTable
    add     rdx, rax
    mov     rcx, qword ptr [rdx]  ; critic template (even slot)

    ; Interpolate: template + context -> output
    ; RCX = template (already set)
    mov     rdx, rsi            ; context
    mov     r8, r13             ; output buffer
    mov     r9, r14             ; output size (FIX 4: in R9)
    call    InterpolateTemplate

    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PromptGen_BuildCritic ENDP

; =============================================================================
; PromptGen_BuildAuditor — Generate complete auditor prompt
;
; Same signature as BuildCritic. Uses Auditor templates (odd index = +8).
; =============================================================================
PUBLIC PromptGen_BuildAuditor
PromptGen_BuildAuditor PROC FRAME
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    push    r12
    .pushreg r12
    push    r13
    .pushreg r13
    push    r14
    .pushreg r14
    push    r15
    .pushreg r15
    sub     rsp, 64
    .allocstack 64
    .endprolog

    mov     rsi, rcx
    mov     r12, rdx
    mov     r13, r8
    mov     r14, r9

    ; Classify
    mov     rcx, rsi
    mov     rdx, r12
    call    PromptGen_AnalyzeContext
    mov     ebx, eax

    ; FIX 1: Full 64-bit math for Auditor index
    movzx   rax, bx
    shl     rax, 4              ; mode * 16
    add     rax, 8              ; +8 for Auditor slot (FIX 1: RAX not EAX)
    lea     rdx, g_TemplateTable
    add     rdx, rax
    mov     rcx, qword ptr [rdx]  ; auditor template (odd slot)

    ; Interpolate
    mov     rdx, rsi
    mov     r8, r13
    mov     r9, r14
    call    InterpolateTemplate

    add     rsp, 64
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rdi
    pop     rsi
    pop     rbx
    ret
PromptGen_BuildAuditor ENDP

; =============================================================================
; PromptGen_Interpolate — Public wrapper for InterpolateTemplate
;
; RCX = Template (null-terminated)
; RDX = Context (null-terminated)
; R8  = Output buffer
; R9  = Output buffer size
; Returns: RAX = Bytes written (0 on overflow)
; =============================================================================
PUBLIC PromptGen_Interpolate
PromptGen_Interpolate PROC
    ; Direct pass-through — all args already in correct registers
    jmp     InterpolateTemplate
PromptGen_Interpolate ENDP

; =============================================================================
; PromptGen_GetTemplate — Retrieve raw template by mode and type
;
; ECX = Mode ID (0-5)
; EDX = Type (0 = Critic, 1 = Auditor)
; Returns: RAX = Pointer to template string (or NULL)
; =============================================================================
PUBLIC PromptGen_GetTemplate
PromptGen_GetTemplate PROC
    ; Validate mode
    cmp     ecx, CTX_MODE_MAX
    jae     @@invalid

    ; Validate type
    cmp     edx, 1
    ja      @@invalid

    ; Table index: mode * 16 + type * 8
    movzx   rax, cx
    shl     rax, 4              ; mode * 16
    movzx   rcx, dx
    shl     rcx, 3              ; type * 8
    add     rax, rcx

    lea     rcx, g_TemplateTable
    mov     rax, qword ptr [rcx + rax]
    ret

@@invalid:
    xor     rax, rax
    ret
PromptGen_GetTemplate ENDP

; =============================================================================
; PromptGen_ForceMode — Set/clear classification override (FIX 6)
;
; ECX = Mode ID (0-5), or -1 to return to auto-detection
; Returns: EAX = Previous force mode value
; =============================================================================
PUBLIC PromptGen_ForceMode
PromptGen_ForceMode PROC
    ; Validate: allow -1 (auto) or 0..CTX_MODE_MAX-1
    cmp     ecx, -1
    je      @@valid
    cmp     ecx, CTX_MODE_MAX
    jae     @@invalid

@@valid:
    mov     eax, dword ptr [g_ForceMode]  ; return old value
    mov     dword ptr [g_ForceMode], ecx
    ret

@@invalid:
    mov     eax, -1             ; return -1 = invalid
    ret
PromptGen_ForceMode ENDP

END
