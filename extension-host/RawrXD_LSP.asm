; RawrXD_LSP.asm - Native LSP bridge (replaces rawrxd-lsp-client)
; JSON-RPC over stdio via pipes. CreatePipe -> CreateProcess -> Content-Length framing.
; Exports: ExtensionInit, ExtensionActivate, ExtensionExecuteCommand, ExtensionCleanup, ExtensionHandleChat

OPTION CASEMAP:NONE

INCLUDELIB kernel32.lib

; Win32 constants
STD_INPUT_HANDLE     EQU -10
STD_OUTPUT_HANDLE    EQU -11
STARTF_USESTDHANDLES EQU 100h
HANDLE_FLAG_INHERIT  EQU 1
CREATE_NO_WINDOW     EQU 08000000h
WAIT_TIMEOUT_MS      EQU 200
INFINITE             EQU 0FFFFFFFFh
LSP_POLL_SLEEP_MS    EQU 2
LSP_POLL_MAX_ITERS   EQU 1000
COMPLETION_MIN_INTERVAL_NORMAL_MS EQU 50
COMPLETION_MIN_INTERVAL_FALLBACK_MS EQU 150
SYNC_PEER_HEARTBEAT_TIMEOUT_MS EQU 30000
SYNC_PEER_TABLE_SIZE     EQU 4
SYNC_PEER_ID_SLOT_SIZE   EQU 128
SYNC_PEER_INFO_STRIDE    EQU 32
SYNC_VRAM_SHED_THRESHOLD_PCT EQU 5
OFFLOAD_ACK_TIMEOUT_MS   EQU 1500
OFFLOAD_ACK_OK           EQU 0
OFFLOAD_ACK_REJECTED     EQU 1
OFFLOAD_ACK_PEER_TIMEOUT EQU 2
OFFLOAD_ACK_STALE_EPOCH  EQU 3
SPEC_HIVE_INFERENCE_ENABLED EQU 1
SPEC_RACE_TIMEOUT_MS     EQU 600
SPEC_RACE_WON            EQU 0
SPEC_RACE_LOST           EQU 1
SPEC_RACE_BOTH_TIMEOUT   EQU 2
SPEC_RATE_LIMITED        EQU 3
SPEC_BURST_MAX_PER_WINDOW EQU 4
SPEC_BURST_WINDOW_MS     EQU 100
LSP_FRAME_BUFFER_SIZE    EQU 8192
LSP_RESPONSE_BUFFER_SIZE EQU 32768
DYNAMIC_REQ_BUFFER_SIZE  EQU 4096
URI_BUFFER_SIZE          EQU 1024
TRIGGER_MASK_BYTES       EQU 32
CONTENT_LENGTH_PREFIX_LEN EQU 16
CONTENT_LENGTH_TOKEN_LEN  EQU 15

LSP_ERR_NONE              EQU 0
LSP_ERR_IO                EQU 1
LSP_ERR_TIMEOUT           EQU 2
LSP_ERR_HEADER_MISSING    EQU 3
LSP_ERR_HEADER_BAD_LENGTH EQU 4
LSP_ERR_FRAME_OVERSIZE    EQU 5
LSP_ERR_OUTPUT_TOO_SMALL  EQU 6
LSP_ERR_ID_MISMATCH       EQU 7
LSP_ERR_RESULT_MISSING    EQU 8

SYNC_PEER_STATE_UNKNOWN     EQU 0
SYNC_PEER_STATE_HEALTHY     EQU 1
SYNC_PEER_STATE_DEGRADED    EQU 2
SYNC_PEER_STATE_UNREACHABLE EQU 3

LSP_COMMAND_ENTRY STRUCT
    pszCommand QWORD ?
    pszPayload QWORD ?
LSP_COMMAND_ENTRY ENDS

SYNC_PEER_INFO STRUCT
    peerId            QWORD ?
    state             DWORD ?
    latencyMs         DWORD ?
    remoteVramFreeMb  DWORD ?
    remoteVramTotalMb DWORD ?
    lastSeenTick      QWORD ?
SYNC_PEER_INFO ENDS

SECURITY_ATTRIBUTES STRUCT
    nLength              DWORD ?
    lpSecurityDescriptor QWORD ?
    bInheritHandle       DWORD ?
SECURITY_ATTRIBUTES ENDS

STARTUPINFOA STRUCT
    cb              DWORD ?
    lpReserved      QWORD ?
    lpDesktop       QWORD ?
    lpTitle         QWORD ?
    dwX             DWORD ?
    dwY             DWORD ?
    dwXSize         DWORD ?
    dwYSize         DWORD ?
    dwXCountChars   DWORD ?
    dwYCountChars   DWORD ?
    dwFillAttribute DWORD ?
    dwFlags         DWORD ?
    wShowWindow     WORD ?
    cbReserved2     WORD ?
    lpReserved2     QWORD ?
    hStdInput       QWORD ?
    hStdOutput      QWORD ?
    hStdError       QWORD ?
STARTUPINFOA ENDS

PROCESS_INFORMATION STRUCT
    hProcess    QWORD ?
    hThread     QWORD ?
    dwProcessId DWORD ?
    dwThreadId  DWORD ?
PROCESS_INFORMATION ENDS

EXTERN GetStdHandle:PROC
EXTERN CreatePipe:PROC
EXTERN SetHandleInformation:PROC
EXTERN CreateProcessA:PROC
EXTERN WriteFile:PROC
EXTERN ReadFile:PROC
EXTERN CloseHandle:PROC
EXTERN TerminateProcess:PROC
EXTERN WaitForSingleObject:PROC
EXTERN PeekNamedPipe:PROC
EXTERN Sleep:PROC
EXTERN GetTickCount64:PROC

.DATA
ALIGN 8
g_hParentWnd   DQ 0
g_hChildStdinRd   DQ 0
g_hChildStdinWr   DQ 0
g_hChildStdoutRd  DQ 0
g_hChildStdoutWr  DQ 0
g_hChildStderrRd  DQ 0
g_hChildStderrWr  DQ 0
g_hProcess     DQ 0
g_hThread      DQ 0
g_bSpawned     DWORD 0
g_dwWritten    DWORD 0
g_dwRead       DWORD 0
g_dwAvail      DWORD 0
g_lspLastError DWORD 0
g_fallbackActive DWORD 0
g_lastCompletionTick QWORD 0
g_syncPeerCount DWORD 0
g_syncEpoch DWORD 0
g_syncLastAppliedEpoch DWORD 0
g_syncGlobalCapacityPct DWORD 0
g_hiveOffloadActive DWORD 0
g_shedActive        DWORD 0
g_pendingOffloadToken QWORD 0
g_pendingOffloadTick  QWORD 0
g_lastOffloadAckToken QWORD 0
g_statsAckOk          DWORD 0
g_statsAckReject      DWORD 0
g_statsAckTimeout     DWORD 0
g_statsAckStale       DWORD 0
g_specPendingToken    QWORD 0
g_specPendingTick     QWORD 0
g_specWinnerLane      DWORD 0
g_statsSpecRaceWon    DWORD 0
g_statsSpecRaceLost   DWORD 0
g_statsSpecRaceTimeout DWORD 0
g_specBurstCount      DWORD 0
g_specBurstTick       QWORD 0
g_statsSpecThrottled  DWORD 0
g_syncPeerTable SYNC_PEER_INFO SYNC_PEER_TABLE_SIZE DUP(<0, SYNC_PEER_STATE_UNKNOWN, 0, 0, 0, 0>)

szClangdCmd    BYTE "clangd --log=error",0
szCmdInit      BYTE "initialize",0
szCmdCompletion BYTE "completion",0
szCmdHover     BYTE "hover",0
szCmdDidOpen   BYTE "didOpen",0
szCmdShutdown  BYTE "shutdown",0
szCmdRuntimeSignal BYTE "runtime.signal",0
szCmdSyncUpdate BYTE "sync.update",0
szCmdHiveOffload   BYTE "hive.offload",0
szCmdHiveOffloadAck BYTE "hive.offload.ack",0
szCmdHiveSpeculative BYTE "hive.speculative",0
szCmdHiveSpeculativeAck BYTE "hive.speculative.ack",0
szCmdShedCheck     BYTE "shed.check",0
szSignalLowVram BYTE "SIG_LOW_VRAM",0
szSignalVramNormal BYTE "SIG_VRAM_NORMAL",0
szActionRouteNanoquant BYTE "ROUTE_TO_NANOQUANT",0
szActionRoutePrimary BYTE "ROUTE_TO_PRIMARY",0
szContentLen   BYTE "Content-Length: ",0
szContentLenToken BYTE "content-length:",0
szHeaderEnd    BYTE 13,10,13,10,0
szInitRequest  BYTE '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":0,',\
               '"capabilities":{"textDocument":{"completion":{"completionItem":{"snippetSupport":true}}}}',\
               ',"rootUri":"file:///D:/rawrxd"}}',0
szInitialized  BYTE '{"jsonrpc":"2.0","method":"initialized","params":{}}',0
szCompletionReq BYTE '{"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{}}',0
szHoverReq     BYTE '{"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{}}',0
szDidOpenReq   BYTE '{"jsonrpc":"2.0","method":"textDocument/didOpen","params":{}}',0
szShutdownReq  BYTE '{"jsonrpc":"2.0","id":4,"method":"shutdown","params":{}}',0

szKeyLine      BYTE 22h,"line",22h,0
szKeyCharacter BYTE 22h,"character",22h,0
szKeyUri       BYTE 22h,"uri",22h,0
szKeyId        BYTE 22h,"id",22h,0
szKeyResult    BYTE 22h,"result",22h,0
szKeySignal    BYTE 22h,"signal",22h,0
szKeyAction    BYTE 22h,"action",22h,0
szKeyRequestId BYTE 22h,"requestId",22h,0
szKeyOutcomeCode BYTE 22h,"outcomeCode",22h,0
szKeyPeerId    BYTE 22h,"peerId",22h,0
szKeySyncEpoch BYTE 22h,"syncEpoch",22h,0
szKeyPeerState BYTE 22h,"peerState",22h,0
szKeyPeerLatencyMs BYTE 22h,"peerLatencyMs",22h,0
szKeyPeerVramFreeMb BYTE 22h,"peerVramFreeMb",22h,0
szKeyPeerVramTotalMb BYTE 22h,"peerVramTotalMb",22h,0
szKeyThroughputCapPct BYTE 22h,"throughputCapPct",22h,0
szKeyCompletionProvider BYTE 22h,"completionProvider",22h,0
szKeyTriggerCharacters BYTE 22h,"triggerCharacters",22h,0
szDefaultUri   BYTE "file:///D:/rawrxd",0

szCompReqP1    BYTE '{"jsonrpc":"2.0","id":2,"method":"textDocument/completion","params":{"textDocument":{"uri":"',0
szHoverReqP1   BYTE '{"jsonrpc":"2.0","id":3,"method":"textDocument/hover","params":{"textDocument":{"uri":"',0
szReqMidUri    BYTE '"},"position":{"line":',0
szReqMidLine   BYTE ',"character":',0
szReqSuffix    BYTE '}}}',0
szReqSuffixFallback BYTE ',"xRawrxdRoute":"nanoquant"}}}',0
szReqSuffixShed     BYTE ',"xRawrxdRoute":"shed"}}}',0
szOffloadSuffixP1   BYTE ',"xRawrxdOffloadPeer":"',0
szOffloadSuffixP2   BYTE '"}}}',0

g_cmdTable LSP_COMMAND_ENTRY <OFFSET szCmdInit, OFFSET szInitRequest>, \
                           <OFFSET szCmdCompletion, OFFSET szCompletionReq>, \
                           <OFFSET szCmdHover, OFFSET szHoverReq>, \
                           <OFFSET szCmdDidOpen, OFFSET szDidOpenReq>, \
                           <OFFSET szCmdShutdown, OFFSET szShutdownReq>
g_cmdCount DWORD 5

.DATA?
ALIGN 16
g_frameBuf     BYTE LSP_FRAME_BUFFER_SIZE DUP(?)
g_responseBuf  BYTE LSP_RESPONSE_BUFFER_SIZE DUP(?)
g_numBuf       BYTE 64 DUP(?)
g_uriBuf       BYTE URI_BUFFER_SIZE DUP(?)
g_syncPeerIdPool       BYTE SYNC_PEER_TABLE_SIZE * SYNC_PEER_ID_SLOT_SIZE DUP(?)
g_hiveOffloadPeerIdBuf BYTE 128 DUP(?)
g_dynamicReq   BYTE DYNAMIC_REQ_BUFFER_SIZE DUP(?)
g_triggerMask  BYTE TRIGGER_MASK_BYTES DUP(?)
g_secAttr      SECURITY_ATTRIBUTES <>
g_startInfo    STARTUPINFOA <>
g_procInfo     PROCESS_INFORMATION <>

.CODE

;----------------------------------------------------------------------
; _strlen - RCX=ptr, returns RAX=len
;----------------------------------------------------------------------
_strlen PROC
    xor eax, eax
    test rcx, rcx
    jz @F
@@: cmp byte ptr [rcx+rax], 0
    je @F
    inc rax
    jmp @B
@@: ret
_strlen ENDP

;----------------------------------------------------------------------
; _itoa - RAX=val, RCX=dest, returns RDX=digit count
;----------------------------------------------------------------------
_itoa PROC
    push rbx
    push rsi
    push rdi
    mov rdi, rcx
    mov rbx, rax
    xor ecx, ecx
    mov rsi, 10
@@: xor edx, edx
    mov rax, rbx
    div rsi
    add dl, '0'
    push rdx
    inc ecx
    mov rbx, rax
    test rax, rax
    jnz @B
    mov edx, ecx
@@: pop rax
    mov byte ptr [rdi], al
    inc rdi
    dec ecx
    jnz @B
    mov byte ptr [rdi], 0
    pop rdi
    pop rsi
    pop rbx
    ret
_itoa ENDP

;----------------------------------------------------------------------
; _streq - RCX=strA, RDX=strB, returns EAX=1 if equal else 0
;----------------------------------------------------------------------
_streq PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    test rsi, rsi
    jz @@no
    test rdi, rdi
    jz @@no
@@cmp:
    mov al, byte ptr [rsi]
    mov dl, byte ptr [rdi]
    cmp al, dl
    jne @@no
    test al, al
    je @@yes
    inc rsi
    inc rdi
    jmp @@cmp
@@yes:
    mov eax, 1
    jmp @@out
@@no:
    xor eax, eax
@@out:
    pop rdi
    pop rsi
    ret
_streq ENDP

;----------------------------------------------------------------------
; LSP_ResolveCommandPayload - RCX=command, returns RAX=payload ptr or 0
;----------------------------------------------------------------------
LSP_ResolveCommandPayload PROC
    push rbx
    push rsi
    mov rbx, rcx
    lea rsi, g_cmdTable
    mov ecx, g_cmdCount
@@loop:
    test ecx, ecx
    jz @@fail
    mov rdx, [rsi]
    mov rcx, rbx
    call _streq
    test eax, eax
    jnz @@found
    add rsi, SIZEOF LSP_COMMAND_ENTRY
    dec ecx
    jmp @@loop
@@found:
    mov rax, [rsi+8]
    jmp @@out
@@fail:
    xor rax, rax
@@out:
    pop rsi
    pop rbx
    ret
LSP_ResolveCommandPayload ENDP

;----------------------------------------------------------------------
; _match_content_length_ci - RCX=ptr, returns EAX=1 if token matches
;   token: "content-length:" (case-insensitive)
;----------------------------------------------------------------------
_match_content_length_ci PROC
    push rsi
    push rdi
    mov rsi, rcx
    lea rdi, szContentLenToken
    mov ecx, CONTENT_LENGTH_TOKEN_LEN
@@loop:
    mov al, byte ptr [rsi]
    mov dl, byte ptr [rdi]
    ; fold ASCII upper to lower for AL
    cmp al, 'A'
    jb @F
    cmp al, 'Z'
    ja @F
    add al, 20h
@@:
    ; fold ASCII upper to lower for DL
    cmp dl, 'A'
    jb @F
    cmp dl, 'Z'
    ja @F
    add dl, 20h
@@:
    cmp al, dl
    jne @@no
    inc rsi
    inc rdi
    dec ecx
    jnz @@loop
    mov eax, 1
    jmp @@out
@@no:
    xor eax, eax
@@out:
    pop rdi
    pop rsi
    ret
_match_content_length_ci ENDP

;----------------------------------------------------------------------
; _str_match_prefix - RCX=haystack, RDX=needle, returns EAX=1 if needle matches prefix
;----------------------------------------------------------------------
_str_match_prefix PROC
    push rsi
    push rdi
    mov rsi, rcx
    mov rdi, rdx
    test rsi, rsi
    jz @@no
    test rdi, rdi
    jz @@no
@@loop:
    mov dl, byte ptr [rdi]
    test dl, dl
    jz @@yes
    mov al, byte ptr [rsi]
    cmp al, dl
    jne @@no
    inc rsi
    inc rdi
    jmp @@loop
@@yes:
    mov eax, 1
    jmp @@out
@@no:
    xor eax, eax
@@out:
    pop rdi
    pop rsi
    ret
_str_match_prefix ENDP

;----------------------------------------------------------------------
; _json_find_key - RCX=json ptr, RDX=key ptr, returns RAX=value ptr or 0
;----------------------------------------------------------------------
_json_find_key PROC
    push rbx
    push rsi
    push rdi
    mov rsi, rcx
    mov rbx, rdx
    test rsi, rsi
    jz @@fail
    test rbx, rbx
    jz @@fail
@@scan:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    mov rcx, rsi
    mov rdx, rbx
    call _str_match_prefix
    test eax, eax
    jz @@next
    mov rcx, rbx
    call _strlen
    lea rdi, [rsi+rax]
@@seek_colon:
    mov al, byte ptr [rdi]
    test al, al
    jz @@fail
    cmp al, ':'
    je @@after_colon
    inc rdi
    jmp @@seek_colon
@@after_colon:
    inc rdi
@@skip_ws:
    mov al, byte ptr [rdi]
    cmp al, ' '
    je @@skip_ws_advance
    cmp al, 9
    je @@skip_ws_advance
    cmp al, 13
    je @@skip_ws_advance
    cmp al, 10
    je @@skip_ws_advance
    mov rax, rdi
    jmp @@out
@@skip_ws_advance:
    inc rdi
    jmp @@skip_ws
@@next:
    inc rsi
    jmp @@scan
@@fail:
    xor rax, rax
@@out:
    pop rdi
    pop rsi
    pop rbx
    ret
_json_find_key ENDP

;----------------------------------------------------------------------
; _json_extract_uint - RCX=json, RDX=key, returns EAX=value, EDX=1 success/0 fail
;----------------------------------------------------------------------
_json_extract_uint PROC
    push rsi
    call _json_find_key
    test rax, rax
    jz @@fail
    mov rsi, rax
    xor eax, eax
    xor ecx, ecx
@@digits:
    movzx edx, byte ptr [rsi]
    cmp dl, '0'
    jb @@done
    cmp dl, '9'
    ja @@done
    imul eax, eax, 10
    sub edx, '0'
    add eax, edx
    inc ecx
    inc rsi
    jmp @@digits
@@done:
    test ecx, ecx
    jz @@fail
    mov edx, 1
    jmp @@out
@@fail:
    xor eax, eax
    xor edx, edx
@@out:
    pop rsi
    ret
_json_extract_uint ENDP

;----------------------------------------------------------------------
; _json_extract_string_copy - RCX=json, RDX=key, R8=dest, R9=cap
; returns EAX=1 success / 0 fail
;----------------------------------------------------------------------
_json_extract_string_copy PROC
    push rsi
    push rdi
    test r8, r8
    jz @@fail
    cmp r9, 2
    jb @@fail
    call _json_find_key
    test rax, rax
    jz @@fail
    mov rsi, rax
    mov al, byte ptr [rsi]
    cmp al, '"'
    jne @@fail
    inc rsi
    mov rdi, r8
    mov rcx, r9
    dec rcx
@@copy:
    test rcx, rcx
    jz @@fail
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, '"'
    je @@done
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    dec rcx
    jmp @@copy
@@done:
    mov byte ptr [rdi], 0
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    pop rdi
    pop rsi
    ret
_json_extract_string_copy ENDP

;----------------------------------------------------------------------
; _append_cstr_bounded - RCX=dest, RDX=src, R8=endptr(last writable)
; returns RAX=new dest on success, 0 on fail
;----------------------------------------------------------------------
_append_cstr_bounded PROC
    test rcx, rcx
    jz @@fail
    test rdx, rdx
    jz @@fail
    test r8, r8
    jz @@fail
@@loop:
    mov al, byte ptr [rdx]
    test al, al
    jz @@done
    cmp rcx, r8
    jae @@fail
    mov byte ptr [rcx], al
    inc rcx
    inc rdx
    jmp @@loop
@@done:
    mov byte ptr [rcx], 0
    mov rax, rcx
    ret
@@fail:
    xor rax, rax
    ret
_append_cstr_bounded ENDP

;----------------------------------------------------------------------
; _json_escape_copy - RCX=dest, RDX=src, R8=endptr(last writable)
; Escapes: \ -> \\ and " -> \"
; Returns RAX=new dest on success, 0 on fail
;----------------------------------------------------------------------
_json_escape_copy PROC
    test rcx, rcx
    jz @@fail
    test rdx, rdx
    jz @@fail
    test r8, r8
    jz @@fail
@@loop:
    mov al, byte ptr [rdx]
    test al, al
    jz @@done

    cmp al, '\\'
    je @@escape_two
    cmp al, '"'
    je @@escape_two

    cmp rcx, r8
    jae @@fail
    mov byte ptr [rcx], al
    inc rcx
    inc rdx
    jmp @@loop

@@escape_two:
    mov r9, r8
    dec r9
    cmp rcx, r9
    jae @@fail
    mov byte ptr [rcx], '\\'
    mov byte ptr [rcx+1], al
    add rcx, 2
    inc rdx
    jmp @@loop

@@done:
    mov byte ptr [rcx], 0
    mov rax, rcx
    ret
@@fail:
    xor rax, rax
    ret
_json_escape_copy ENDP

;----------------------------------------------------------------------
; _json_extract_result_slice - RCX=json, RDX=dest, R8=cap
; Copies only the JSON value behind "result": into dest, null-terminated.
; Returns EAX=1 success / 0 fail
;----------------------------------------------------------------------
_json_extract_result_slice PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    push r15
    test rcx, rcx
    jz @@fail
    test rdx, rdx
    jz @@fail
    cmp r8, 2
    jb @@fail

    mov r12, rdx            ; dest start
    mov r13, r8             ; capacity
    dec r13                 ; leave room for null

    mov rdx, OFFSET szKeyResult
    call _json_find_key
    test rax, rax
    jz @@fail
    mov rsi, rax            ; value start candidate

@@skip_ws:
    mov al, byte ptr [rsi]
    cmp al, ' '
    je @@skip_ws_adv
    cmp al, 9
    je @@skip_ws_adv
    cmp al, 13
    je @@skip_ws_adv
    cmp al, 10
    je @@skip_ws_adv
    jmp @@value_kind
@@skip_ws_adv:
    inc rsi
    jmp @@skip_ws

@@value_kind:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, '{'
    je @@copy_balanced_obj
    cmp al, '['
    je @@copy_balanced_arr
    cmp al, '"'
    je @@copy_quoted
    jmp @@copy_scalar

@@copy_balanced_obj:
    mov bl, '{'
    mov bh, '}'
    jmp @@copy_balanced
@@copy_balanced_arr:
    mov bl, '['
    mov bh, ']'

@@copy_balanced:
    mov rdi, r12
    xor r14d, r14d          ; depth
    xor r15d, r15d          ; inString
    xor ecx, ecx            ; escapeNext (cl used)
@@bal_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    test r13, r13
    jz @@fail
    mov byte ptr [rdi], al
    inc rdi
    dec r13

    cmp r15d, 0
    jne @@bal_in_string

    cmp al, '"'
    je @@bal_enter_string
    cmp al, bl
    je @@bal_open
    cmp al, bh
    je @@bal_close
    jmp @@bal_advance

@@bal_enter_string:
    mov r15d, 1
    jmp @@bal_advance
@@bal_open:
    inc r14d
    jmp @@bal_advance
@@bal_close:
    dec r14d
    cmp r14d, 0
    je @@success
    jmp @@bal_advance

@@bal_in_string:
    cmp cl, 0
    jne @@bal_escape_consumed
    cmp al, '\\'
    je @@bal_escape_set
    cmp al, '"'
    je @@bal_leave_string
    jmp @@bal_advance
@@bal_escape_set:
    mov cl, 1
    jmp @@bal_advance
@@bal_escape_consumed:
    xor cl, cl
    jmp @@bal_advance
@@bal_leave_string:
    xor r15d, r15d

@@bal_advance:
    inc rsi
    jmp @@bal_loop

@@copy_quoted:
    mov rdi, r12
    xor ecx, ecx            ; escapeNext
@@q_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    test r13, r13
    jz @@fail
    mov byte ptr [rdi], al
    inc rdi
    dec r13
    cmp cl, 0
    jne @@q_escape_consumed
    cmp al, '\\'
    je @@q_escape_set
    cmp al, '"'
    je @@success
    inc rsi
    jmp @@q_loop
@@q_escape_set:
    mov cl, 1
    inc rsi
    jmp @@q_loop
@@q_escape_consumed:
    xor cl, cl
    inc rsi
    jmp @@q_loop

@@copy_scalar:
    mov rdi, r12
@@s_loop:
    mov al, byte ptr [rsi]
    test al, al
    jz @@success
    cmp al, ','
    je @@success
    cmp al, '}'
    je @@success
    cmp al, ']'
    je @@success
    cmp al, 13
    je @@success
    cmp al, 10
    je @@success
    test r13, r13
    jz @@fail
    mov byte ptr [rdi], al
    inc rdi
    dec r13
    inc rsi
    jmp @@s_loop

@@success:
    mov byte ptr [rdi], 0
    mov eax, 1
    jmp @@out

@@fail:
    xor eax, eax
@@out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_json_extract_result_slice ENDP

;----------------------------------------------------------------------
; _clear_trigger_mask - zero trigger character bitset
;----------------------------------------------------------------------
_clear_trigger_mask PROC
    push rdi
    lea rdi, g_triggerMask
    xor eax, eax
    mov ecx, TRIGGER_MASK_BYTES
    rep stosb
    pop rdi
    ret
_clear_trigger_mask ENDP

;----------------------------------------------------------------------
; _set_trigger_char - RCX=ASCII char code
;----------------------------------------------------------------------
_set_trigger_char PROC
    push rbx
    push rdi
    mov eax, ecx
    and eax, 0FFh
    mov ebx, eax
    shr eax, 3
    cmp eax, TRIGGER_MASK_BYTES
    jae @@out
    and ebx, 7
    mov cl, bl
    mov dl, 1
    shl dl, cl
    lea rdi, g_triggerMask
    add rdi, rax
    or byte ptr [rdi], dl
@@out:
    pop rdi
    pop rbx
    ret
_set_trigger_char ENDP

;----------------------------------------------------------------------
; _is_trigger_char - RCX=ASCII char code, returns EAX=1/0
;----------------------------------------------------------------------
_is_trigger_char PROC
    push rbx
    push rdi
    mov eax, ecx
    and eax, 0FFh
    mov ebx, eax
    shr eax, 3
    cmp eax, TRIGGER_MASK_BYTES
    jae @@no
    and ebx, 7
    mov cl, bl
    mov dl, 1
    shl dl, cl
    lea rdi, g_triggerMask
    add rdi, rax
    test byte ptr [rdi], dl
    jz @@no
    mov eax, 1
    jmp @@out
@@no:
    xor eax, eax
@@out:
    pop rdi
    pop rbx
    ret
_is_trigger_char ENDP

;----------------------------------------------------------------------
; _seed_default_trigger_mask - fallback trigger set when server omits list.
;----------------------------------------------------------------------
_seed_default_trigger_mask PROC
    call _clear_trigger_mask
    mov ecx, '.'
    call _set_trigger_char
    mov ecx, ':'
    call _set_trigger_char
    mov ecx, '>'
    call _set_trigger_char
    ret
_seed_default_trigger_mask ENDP

;----------------------------------------------------------------------
; _verify_completion_provider_from_init_response - RCX=json response body
; Returns EAX=1 when completionProvider capability exists, else 0.
;----------------------------------------------------------------------
_verify_completion_provider_from_init_response PROC
    push r12
    mov r12, rcx
    test r12, r12
    jz @@fail
    mov rcx, r12
    lea rdx, szKeyCompletionProvider
    call _json_find_key
    test rax, rax
    jz @@fail
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    pop r12
    ret
_verify_completion_provider_from_init_response ENDP

;----------------------------------------------------------------------
; _cache_trigger_chars_from_init_response - RCX=json response body
; Parses "triggerCharacters" array and stores 8-bit chars in g_triggerMask.
; Returns EAX=1 success parsed / 0 missing or malformed.
;----------------------------------------------------------------------
_cache_trigger_chars_from_init_response PROC
    push rbx
    push rsi
    push rdi
    push r12
    call _clear_trigger_mask
    mov r12, rcx
    test r12, r12
    jz @@fail

    mov rcx, r12
    lea rdx, szKeyTriggerCharacters
    call _json_find_key
    test rax, rax
    jz @@fail
    mov rsi, rax

@@seek_array:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, '['
    je @@array_start
    inc rsi
    jmp @@seek_array

@@array_start:
    inc rsi
@@scan_items:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, ']'
    je @@ok
    cmp al, '"'
    je @@string_start
    inc rsi
    jmp @@scan_items

@@string_start:
    inc rsi
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, '\\'
    jne @@set_char
    inc rsi
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
@@set_char:
    movzx ecx, al
    call _set_trigger_char

@@seek_end_quote:
    mov al, byte ptr [rsi]
    test al, al
    jz @@fail
    cmp al, '"'
    je @@after_quote
    inc rsi
    jmp @@seek_end_quote

@@after_quote:
    inc rsi
    jmp @@scan_items

@@ok:
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_cache_trigger_chars_from_init_response ENDP

;----------------------------------------------------------------------
; _handle_runtime_signal - RCX=pvData JSON
; Supports:
;   signal=SIG_LOW_VRAM + action=ROUTE_TO_NANOQUANT => fallback active
;   signal=SIG_VRAM_NORMAL + action=ROUTE_TO_PRIMARY => fallback inactive
; Returns EAX=1 on accepted signal, 0 on malformed/unsupported payload.
;----------------------------------------------------------------------
_handle_runtime_signal PROC
    push rbx
    push rsi
    mov rbx, rcx
    test rbx, rbx
    jz @@fail

    mov rcx, rbx
    lea rdx, szKeySignal
    lea r8, g_uriBuf
    mov r9d, URI_BUFFER_SIZE
    call _json_extract_string_copy
    test eax, eax
    jz @@fail

    mov rcx, rbx
    lea rdx, szKeyAction
    lea r8, g_numBuf
    mov r9d, 64
    call _json_extract_string_copy
    test eax, eax
    jz @@fail

    lea rsi, g_uriBuf
    mov rcx, rsi
    lea rdx, szSignalLowVram
    call _streq
    test eax, eax
    jz @@check_normal

    lea rcx, g_numBuf
    lea rdx, szActionRouteNanoquant
    call _streq
    test eax, eax
    jz @@fail
    mov dword ptr g_fallbackActive, 1
    mov eax, 1
    jmp @@out

@@check_normal:
    mov rcx, rsi
    lea rdx, szSignalVramNormal
    call _streq
    test eax, eax
    jz @@fail

    lea rcx, g_numBuf
    lea rdx, szActionRoutePrimary
    call _streq
    test eax, eax
    jz @@fail
    mov dword ptr g_fallbackActive, 0
    mov dword ptr g_shedActive, 0     ; fallback cleared -> shed cannot be active
    mov eax, 1
    jmp @@out

@@fail:
    xor eax, eax
@@out:
    pop rsi
    pop rbx
    ret
_handle_runtime_signal ENDP

;----------------------------------------------------------------------
; _handle_sync_update - RCX=pvData JSON
; Parses manager peer telemetry and updates g_syncPeerTable (4 slots).
;
; Slot-aliasing strategy:
;   1. Scan table: if peerId string matches existing slot -> update in place.
;   2. Else find first slot with state==UNKNOWN (empty) -> fill it.
;   3. Else evict the slot with the oldest lastSeenTick (LRU replacement).
;
; Required: peerId, peerState(0..3). Optional: latency/vram/capacity fields.
; Returns EAX=1 on success, 0 on malformed or epoch-stale values.
;----------------------------------------------------------------------
_handle_sync_update PROC
    push rbx
    push rdi
    push rsi
    push r12
    push r13
    push r14
    push r15

    mov rbx, rcx
    test rbx, rbx
    jz @@fail

    ; --- Parse required + optional fields into registers ---

    ; peerId -> g_syncPeerIdPool slot 0 (scratch; we'll copy to chosen slot later)
    ; We parse into a local scratch area first.  Use g_hiveOffloadPeerIdBuf as a
    ; parse temp — it is a 128-byte uninitialised .DATA? buffer safe to reuse here
    ; because we always overwrite it before reads in _handle_hive_offload.
    mov rcx, rbx
    lea rdx, szKeyPeerId
    lea r8, g_hiveOffloadPeerIdBuf
    mov r9d, SYNC_PEER_ID_SLOT_SIZE
    call _json_extract_string_copy
    test eax, eax
    jz @@fail

    mov rcx, rbx
    lea rdx, szKeyPeerState
    call _json_extract_uint
    test edx, edx
    jz @@fail
    cmp eax, SYNC_PEER_STATE_UNREACHABLE
    ja @@fail
    mov r12d, eax          ; r12d = peerState

    xor r13d, r13d
    mov rcx, rbx
    lea rdx, szKeyPeerLatencyMs
    call _json_extract_uint
    test edx, edx
    jz @F
    mov r13d, eax
@@:

    xor r14d, r14d
    mov rcx, rbx
    lea rdx, szKeyPeerVramFreeMb
    call _json_extract_uint
    test edx, edx
    jz @F
    mov r14d, eax
@@:

    xor r15d, r15d
    mov rcx, rbx
    lea rdx, szKeyPeerVramTotalMb
    call _json_extract_uint
    test edx, edx
    jz @F
    mov r15d, eax
@@:

    xor r11d, r11d
    mov rcx, rbx
    lea rdx, szKeyThroughputCapPct
    call _json_extract_uint
    test edx, edx
    jz @F
    cmp eax, 100
    ja @@throughput_invalid
    mov r11d, eax
    jmp @F
@@throughput_invalid:
    xor r11d, r11d
@@:
    ; r11d = throughputCapPct (0 = absent/clamped)

    ; --- Epoch validation ---

    xor esi, esi
    xor r9d, r9d
    mov rcx, rbx
    lea rdx, szKeySyncEpoch
    call _json_extract_uint
    test edx, edx
    jz @@epoch_decide
    mov esi, eax
    mov r9d, 1
    mov eax, dword ptr g_syncLastAppliedEpoch
    cmp esi, eax
    jb @@fail
@@epoch_decide:
    test r9d, r9d
    jnz @@epoch_ready
    mov eax, dword ptr g_syncLastAppliedEpoch
    inc eax
    mov esi, eax
@@epoch_ready:
    ; esi = validated epoch to apply

    ; --- Slot selection ---
    ; Pass 1: find matching peerId or first empty slot.
    ; rdi = chosen slot ptr (or 0 if none yet)
    ; r10 = oldest-tick slot ptr for LRU fallback
    ; rbx saved: restore rcx below as needed; rbx now free for slot walk.

    call GetTickCount64      ; rax = currentTick (also written as lastSeenTick)
    push rax                 ; [rsp] = currentTick  (shadow space not needed for push)

    lea rdi, g_syncPeerTable
    xor ebx, ebx             ; ebx = slot index counter
    xor r10d, r10d           ; r10 = chosen slot (0 = not found yet)
    mov r8, -1               ; r8 = oldest tick seen (max QWORD = newest, we want min)
                             ; init to all-ones so first slot always wins LRU
    ; Note: r8 = 0xFFFFFFFF_FFFFFFFF as "most recent" sentinel for LRU comparator.
    ; We want the slot whose lastSeenTick is smallest (oldest).  Use r8 as running
    ; minimum; when lastSeenTick < r8 that slot is the new LRU candidate.

@@slot_scan:
    cmp ebx, SYNC_PEER_TABLE_SIZE
    jae @@slot_scan_done

    ; Compare peerId string of this slot against the parsed ID in g_hiveOffloadPeerIdBuf.
    ; Each slot's peerId field (offset 0) holds a pointer to its ID string in the pool.
    mov rdx, qword ptr [rdi]     ; rdx = peerId pointer for this slot (0 if unused)
    test rdx, rdx
    jz @@slot_check_empty

    ; Non-null: compare string
    push rcx
    lea rcx, g_hiveOffloadPeerIdBuf
    call _streq
    pop rcx
    test eax, eax
    jnz @@slot_found_match

@@slot_check_empty:
    ; If state == UNKNOWN this is a candidate empty slot (take first found).
    cmp dword ptr [rdi+8], SYNC_PEER_STATE_UNKNOWN
    jne @@slot_lru_check
    test r10, r10
    jnz @@slot_next          ; already have an empty-slot candidate
    mov r10, rdi             ; first empty slot found
    jmp @@slot_next

@@slot_lru_check:
    ; Not a match, not empty: check if this is the oldest for LRU eviction.
    mov rax, qword ptr [rdi+24]  ; lastSeenTick of this slot
    cmp rax, r8
    jae @@slot_next
    mov r8, rax
    ; Only use as LRU candidate if we have no empty slot yet
    ; (we still record it unconditionally; precedence is resolved after scan)
    push r10
    mov r10, rdi
    pop r10
    ; We need a separate LRU register.  Use the stack-top slot.
    ; Re-use r8 as the LRU tick and a new var for ptr.
    ; Simplest: store LRU-ptr in shadow on stack.
    ; Restructure: use g_numBuf stack frame is not available here cleanly.
    ; Instead we check after scan: if r10=0 (no empty slot) we need the LRU slot.
    ; We'll capture LRU ptr in a pushed register abuse: push/pop is too expensive mid-loop.
    ; Use a free caller-saved reg that we haven't committed: r9 (was used for epoch flag, done).
    mov r9, rdi              ; r9 = current LRU candidate ptr
@@slot_next:
    add rdi, SYNC_PEER_INFO_STRIDE
    inc ebx
    jmp @@slot_scan

@@slot_found_match:
    ; rdi points to matching slot — use it directly.
    mov r10, rdi
    jmp @@slot_write

@@slot_scan_done:
    ; r10 = first-empty slot, or 0.  r9 = LRU slot ptr.
    test r10, r10
    jnz @@slot_write         ; have an empty slot to fill
    ; No empty slot: evict LRU.
    test r9, r9
    jz @@cleanup_fail        ; all slots are UNKNOWN?  table corrupt — fail safe
    mov r10, r9

@@slot_write:
    ; r10 = target slot ptr in g_syncPeerTable.
    ; Compute slot index = (r10 - &g_syncPeerTable) / SYNC_PEER_INFO_STRIDE
    lea rax, g_syncPeerTable
    mov rcx, r10
    sub rcx, rax
    ; rcx / SYNC_PEER_INFO_STRIDE
    xor edx, edx
    mov eax, ecx
    mov ecx, SYNC_PEER_INFO_STRIDE
    div ecx               ; eax = slot index (0..3)
    ; Compute pool pointer for this slot: g_syncPeerIdPool + eax * SYNC_PEER_ID_SLOT_SIZE
    mov ecx, SYNC_PEER_ID_SLOT_SIZE
    mul ecx               ; eax = byte offset into pool
    lea rdi, g_syncPeerIdPool
    add rdi, rax          ; rdi = destination pool slot

    ; Copy parsed peerId from scratch buffer into assigned pool slot.
    lea rsi, g_hiveOffloadPeerIdBuf
    push rcx
    mov ecx, SYNC_PEER_ID_SLOT_SIZE
@@copy_peer_id:
    mov al, byte ptr [rsi]
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz @@copy_peer_id
    pop rcx

    ; Restore rdi to the target slot pointer.
    mov rdi, r10

    ; Store peerId: point to its pool slot (rdi is the pool slot we just wrote).
    ; We need the pool-slot address again.
    lea rax, g_syncPeerTable
    mov rcx, rdi
    sub rcx, rax
    xor edx, edx
    mov eax, ecx
    mov ecx, SYNC_PEER_INFO_STRIDE
    div ecx
    mov ecx, SYNC_PEER_ID_SLOT_SIZE
    mul ecx
    lea rsi, g_syncPeerIdPool
    add rsi, rax          ; rsi = pool slot address

    ; Write slot fields.
    mov qword ptr [rdi], rsi     ; peerId pointer -> pool slot
    mov dword ptr [rdi+8], r12d  ; state
    mov dword ptr [rdi+12], r13d ; latencyMs
    mov dword ptr [rdi+16], r14d ; remoteVramFreeMb
    mov dword ptr [rdi+20], r15d ; remoteVramTotalMb
    pop rax                      ; currentTick (from earlier push)
    mov qword ptr [rdi+24], rax  ; lastSeenTick

    ; --- Update global state ---
    mov dword ptr g_syncEpoch, esi
    mov dword ptr g_syncLastAppliedEpoch, esi

    ; Cancel active offload if this peer is no longer healthy.
    cmp r12d, SYNC_PEER_STATE_HEALTHY
    je @@recount_peers

    ; Check if the slot we just wrote is the same one g_hiveOffloadPeerIdBuf was pointing at.
    ; Compare: g_hiveOffloadPeerIdBuf content matches the offload peer name?
    ; Simpler: always clear offload if any non-healthy update arrives (conservative).
    mov dword ptr g_hiveOffloadActive, 0

@@recount_peers:
    ; Recount active (non-UNKNOWN) peers and aggregate global capacity.
    lea rdi, g_syncPeerTable
    xor ebx, ebx             ; ebx = valid peer count
    xor r10d, r10d           ; r10d = aggregate global capacity pct accumulator
    xor r9d, r9d             ; r9d = number of healthy peers contributing to capacity

    call GetTickCount64
    mov r8, rax              ; r8 = currentTick for freshness check

@@cap_scan:
    cmp ebx, SYNC_PEER_TABLE_SIZE  ; reusing ebx as loop var — save/restore? No: ebx counts slots walked
    ; Use separate counter: replace ebx with a different slot counter here.
    ; Restructure: walk by pointer instead.
    ; Actually ebx was reused.  Use ecx as slot walker here.
    ; This function is already pushing r12-r15.  Use r11 which we preserved above as throughputCapPct context.
    ; r11d is throughputCapPct of *current* update only — not usable for table scan.
    ; Safe to clobber r11 now (we've saved the update to the slot).  Use r11 as slot counter.
    jmp @@cap_scan_start

@@cap_scan_start:
    lea rdi, g_syncPeerTable
    xor r11d, r11d           ; r11d = slot counter for capacity scan

@@cap_slot:
    cmp r11d, SYNC_PEER_TABLE_SIZE
    jae @@cap_scan_done

    cmp dword ptr [rdi+8], SYNC_PEER_STATE_UNKNOWN
    je @@cap_next
    inc ebx                  ; count non-empty slots as g_syncPeerCount

    ; Only healthy + fresh slots contribute to capacity.
    cmp dword ptr [rdi+8], SYNC_PEER_STATE_HEALTHY
    jne @@cap_next

    mov rax, qword ptr [rdi+24]  ; lastSeenTick
    mov rcx, r8                   ; currentTick
    sub rcx, rax
    cmp rcx, SYNC_PEER_HEARTBEAT_TIMEOUT_MS
    jae @@cap_next               ; stale: skip

    ; Compute VRAM percentage for this slot.
    mov eax, dword ptr [rdi+20]  ; remoteVramTotalMb
    test eax, eax
    jz @@cap_next
    mov ecx, dword ptr [rdi+16]  ; remoteVramFreeMb
    cmp ecx, eax
    ja @@cap_next                ; free > total: corrupted telemetry, skip
    push rdx
    mov edx, 0
    mov eax, ecx
    mov ecx, 100
    mul ecx                      ; eax = freeMb * 100
    mov ecx, dword ptr [rdi+20]  ; remoteVramTotalMb again (mul clobbered ecx)
    div ecx                      ; eax = vramPct
    pop rdx
    add r10d, eax                ; accumulate
    inc r9d                      ; healthy-fresh count

@@cap_next:
    add rdi, SYNC_PEER_INFO_STRIDE
    inc r11d
    jmp @@cap_slot

@@cap_scan_done:
    mov dword ptr g_syncPeerCount, ebx

    ; Average capacity across healthy fresh peers.
    test r9d, r9d
    jz @@store_zero_cap
    mov eax, r10d
    xor edx, edx
    div r9d                  ; eax = average VRAM pct
    jmp @@store_cap
@@store_zero_cap:
    xor eax, eax
@@store_cap:
    mov dword ptr g_syncGlobalCapacityPct, eax
    cmp eax, 20
    jae @@eval_shed
    mov dword ptr g_fallbackActive, 1
@@eval_shed:
    ; Derive shed: active when local-fallback AND hive capacity below threshold.
    cmp dword ptr g_fallbackActive, 0
    je @@shed_clear
    cmp dword ptr g_syncGlobalCapacityPct, SYNC_VRAM_SHED_THRESHOLD_PCT
    ja @@shed_clear
    mov dword ptr g_shedActive, 1
    jmp @@ok
@@shed_clear:
    mov dword ptr g_shedActive, 0

@@ok:
    mov eax, 1
    jmp @@out

@@cleanup_fail:
    pop rax                  ; balance currentTick push
@@fail:
    xor eax, eax
@@out:
    pop r15
    pop r14
    pop r13
    pop r12
    pop rsi
    pop rdi
    pop rbx
    ret
_handle_sync_update ENDP

;----------------------------------------------------------------------
; _handle_hive_offload - RCX=pvData JSON (optional, reserved)
; Selects the best eligible peer from the 4-slot g_syncPeerTable:
;   - state == SYNC_PEER_STATE_HEALTHY
;   - lastSeenTick within SYNC_PEER_HEARTBEAT_TIMEOUT_MS (30 s)
;   - g_syncGlobalCapacityPct >= 20 (aggregate hive capacity)
;   - among all eligible: lowest peerLatencyMs wins
; On accept: sets g_hiveOffloadActive=1, copies winning peer ID to
;            g_hiveOffloadPeerIdBuf for completion/hover route annotation.
; On stale (no fresh healthy peer): clears g_hiveOffloadActive.
; Returns EAX=1 accepted, 0 rejected.
;----------------------------------------------------------------------
_handle_hive_offload PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov r14, rcx

    ; Require at least one registered peer.
    cmp dword ptr g_syncPeerCount, 0
    je @@fail

    ; Require aggregate hive capacity >= 20%.
    cmp dword ptr g_syncGlobalCapacityPct, 20
    jb @@fail

    ; Get current tick for freshness check.
    call GetTickCount64
    mov r12, rax             ; r12 = currentTick

    ; Scan table: find best candidate (HEALTHY + fresh + lowest latency).
    lea rdi, g_syncPeerTable
    xor ebx, ebx            ; ebx = slot index
    xor r13d, r13d          ; r13 = best-candidate slot ptr (0 = none yet)
    mov esi, 0FFFFFFFFh     ; esi = best latency (start at max)

@@scan:
    cmp ebx, SYNC_PEER_TABLE_SIZE
    jae @@scan_done

    cmp dword ptr [rdi+8], SYNC_PEER_STATE_HEALTHY
    jne @@next

    ; Freshness check.
    mov rax, qword ptr [rdi+24]  ; lastSeenTick
    mov rcx, r12
    sub rcx, rax
    cmp rcx, SYNC_PEER_HEARTBEAT_TIMEOUT_MS
    jae @@next              ; stale

    ; Latency comparison: lower is better.
    mov eax, dword ptr [rdi+12]  ; latencyMs
    cmp eax, esi
    jae @@next              ; not better than current best
    mov esi, eax            ; new best latency
    mov r13, rdi            ; new best slot ptr

@@next:
    add rdi, SYNC_PEER_INFO_STRIDE
    inc ebx
    jmp @@scan

@@scan_done:
    test r13, r13
    jz @@stale              ; no eligible peer found

    ; Copy winning peer's ID string to g_hiveOffloadPeerIdBuf.
    mov rsi, qword ptr [r13]         ; peerId pointer (into g_syncPeerIdPool)
    test rsi, rsi
    jz @@stale
    lea rdi, g_hiveOffloadPeerIdBuf
    mov ecx, SYNC_PEER_ID_SLOT_SIZE - 1
@@copy_best_id:
    mov al, byte ptr [rsi]
    test al, al
    jz @@copy_best_id_done
    mov byte ptr [rdi], al
    inc rsi
    inc rdi
    dec ecx
    jnz @@copy_best_id
@@copy_best_id_done:
    mov byte ptr [rdi], 0

    mov dword ptr g_hiveOffloadActive, 1
    ; Track pending token for manager ACK closure.
    mov rax, r12
    test r14, r14
    jz @@set_pending_token
    mov rcx, r14
    lea rdx, szKeyRequestId
    call _json_extract_uint
    test edx, edx
    jz @@set_pending_token
@@set_pending_token:
    mov qword ptr g_pendingOffloadToken, rax
    mov qword ptr g_pendingOffloadTick, r12
    mov eax, 1
    jmp @@out

@@stale:
    mov dword ptr g_hiveOffloadActive, 0
    xor eax, eax
    jmp @@out

@@fail:
    xor eax, eax
@@out:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_handle_hive_offload ENDP

;----------------------------------------------------------------------
; _handle_offload_ack - RCX=pvData JSON
; Expects requestId + outcomeCode from manager ACK callback.
; Returns EAX=1 handled/ignored safely, 0 malformed payload.
;----------------------------------------------------------------------
_handle_offload_ack PROC
    push rbx
    push rsi
    push r12

    test rcx, rcx
    jz @@fail
    mov rsi, rcx

    mov rcx, rsi
    lea rdx, szKeyRequestId
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r8d, eax                         ; r8 = requestId (zero-extended)

    mov rcx, rsi
    lea rdx, szKeyOutcomeCode
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov ebx, eax                         ; ebx = outcomeCode

    ; Idempotency: exact duplicate ACKs are accepted as no-ops.
    cmp r8, qword ptr g_lastOffloadAckToken
    je @@ok

    ; Ignore stale or unrelated ACKs safely.
    cmp r8, qword ptr g_pendingOffloadToken
    jne @@stale

    ; Pending ACK timeout check.
    call GetTickCount64
    mov r12, qword ptr g_pendingOffloadTick
    test r12, r12
    jz @@timeout
    sub rax, r12
    cmp rax, OFFLOAD_ACK_TIMEOUT_MS
    jae @@timeout

    cmp ebx, OFFLOAD_ACK_OK
    je @@ack_ok
    cmp ebx, OFFLOAD_ACK_REJECTED
    je @@ack_reject
    cmp ebx, OFFLOAD_ACK_PEER_TIMEOUT
    je @@ack_timeout
    cmp ebx, OFFLOAD_ACK_STALE_EPOCH
    je @@ack_stale
    jmp @@stale

@@ack_ok:
    inc dword ptr g_statsAckOk
    mov qword ptr g_lastOffloadAckToken, r8
    mov qword ptr g_pendingOffloadToken, 0
    mov qword ptr g_pendingOffloadTick, 0
    jmp @@ok

@@ack_reject:
    mov dword ptr g_hiveOffloadActive, 0
    inc dword ptr g_statsAckReject
    mov qword ptr g_lastOffloadAckToken, r8
    mov qword ptr g_pendingOffloadToken, 0
    mov qword ptr g_pendingOffloadTick, 0
    jmp @@ok

@@ack_timeout:
@@timeout:
    mov dword ptr g_hiveOffloadActive, 0
    inc dword ptr g_statsAckTimeout
    mov qword ptr g_lastOffloadAckToken, r8
    mov qword ptr g_pendingOffloadToken, 0
    mov qword ptr g_pendingOffloadTick, 0
    jmp @@ok

@@ack_stale:
@@stale:
    inc dword ptr g_statsAckStale
    mov qword ptr g_lastOffloadAckToken, r8
    ; stale ACKs do not modify the active offload lane
    jmp @@ok

@@ok:
    mov eax, 1
    jmp @@out

@@fail:
    xor eax, eax
@@out:
    pop r12
    pop rsi
    pop rbx
    ret
_handle_offload_ack ENDP

;----------------------------------------------------------------------
; _handle_speculative_ack - processes ACK from speculative hive race.
; Inputs: RCX = pointer to JSON payload (hive.speculative.ack)
; Returns: EAX = 1 if processed, 0 if failed/ignored
; Logic:
;   - Parse requestId + outcomeCode (SPEC_RACE_WON/LOST/TIMEOUT)
;   - If RACE_WON: record winner in g_specWinnerLane, increment g_statsSpecRaceWon
;   - If RACE_LOST: no-op (second completer), increment g_statsSpecRaceLost
;   - If TIMEOUT: both completers timed out, increment g_statsSpecRaceTimeout
;   - All paths clear g_specPendingToken and g_specPendingTick
;----------------------------------------------------------------------
_handle_speculative_ack PROC
    push rbx
    push rsi
    push r12

    test rcx, rcx
    jz @@fail
    mov rsi, rcx

    mov rcx, rsi
    lea rdx, szKeyRequestId
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r8d, eax                         ; r8 = requestId

    mov rcx, rsi
    lea rdx, szKeyOutcomeCode
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov ebx, eax                         ; ebx = outcomeCode

    ; Verify speculative race is still pending (token match)
    cmp r8, qword ptr g_specPendingToken
    jne @@stale_spec

    ; Timeout gate: abort if pending tick is stale
    call GetTickCount64
    mov r12, qword ptr g_specPendingTick
    test r12, r12
    jz @@timeout_spec
    sub rax, r12
    cmp rax, SPEC_RACE_TIMEOUT_MS
    jae @@timeout_spec

    ; Throttle gate: enforce burst limit per window
    call GetTickCount64
    mov r12, qword ptr g_specBurstTick
    test r12, r12
    jz @@throttle_init
    
    ; Check if window expired
    sub rax, r12
    cmp rax, SPEC_BURST_WINDOW_MS
    jae @@throttle_reset
    
    ; Window still active; check burst count
    cmp dword ptr g_specBurstCount, SPEC_BURST_MAX_PER_WINDOW
    jge @@throttle_limit
    
    ; Burst limit not reached; increment counter and continue
    inc dword ptr g_specBurstCount
    jmp @@throttle_ok

@@throttle_init:
    ; Initialize new burst window
    call GetTickCount64
    mov qword ptr g_specBurstTick, rax
    mov dword ptr g_specBurstCount, 1
    jmp @@throttle_ok

@@throttle_reset:
    ; Window expired; start new window
    call GetTickCount64
    mov qword ptr g_specBurstTick, rax
    mov dword ptr g_specBurstCount, 1
    jmp @@throttle_ok

@@throttle_limit:
    ; Burst limit reached; reject with SPEC_RATE_LIMITED
    inc dword ptr g_statsSpecThrottled
    mov ebx, SPEC_RATE_LIMITED
    jmp @@throttle_route

@@throttle_route:
    cmp ebx, SPEC_RATE_LIMITED
    je @@spec_rate_limited
    
@@throttle_ok:
    cmp ebx, SPEC_RACE_WON
    je @@spec_won
    cmp ebx, SPEC_RACE_LOST
    je @@spec_lost
    cmp ebx, SPEC_RACE_BOTH_TIMEOUT
    je @@spec_both_timeout
    jmp @@stale_spec

@@spec_won:
    inc dword ptr g_statsSpecRaceWon
    ; Extract peerId if available to store winner lane
    mov rcx, rsi
    lea rdx, szKeyPeerId
    call _json_extract_uint
    mov dword ptr g_specWinnerLane, eax
    mov qword ptr g_specPendingToken, 0
    mov qword ptr g_specPendingTick, 0
    jmp @@ok

@@spec_lost:
    inc dword ptr g_statsSpecRaceLost
    mov qword ptr g_specPendingToken, 0
    mov qword ptr g_specPendingTick, 0
    jmp @@ok

@@spec_both_timeout:
@@timeout_spec:
    inc dword ptr g_statsSpecRaceTimeout
    mov qword ptr g_specPendingToken, 0
    mov qword ptr g_specPendingTick, 0
    jmp @@ok

@@spec_rate_limited:
    ; Rate limit reached; reject without modifying pending state
    ; Manager will backoff and retry after window resets
    inc dword ptr g_statsSpecThrottled
    jmp @@ok

@@stale_spec:
    ; Stale speculative ACK—no-op, do not modify state
    jmp @@ok

@@ok:
    mov eax, 1
    jmp @@out

@@fail:
    xor eax, eax
@@out:
    pop r12
    pop rsi
    pop rbx
    ret
_handle_speculative_ack ENDP

;----------------------------------------------------------------------
; _handle_shed_check - re-evaluates g_shedActive from current globals.
; No arguments. Returns EAX=1 if shed is now active, 0 if cleared.
; Shed is active when: g_fallbackActive=1 AND
;   g_syncGlobalCapacityPct <= SYNC_VRAM_SHED_THRESHOLD_PCT.
;----------------------------------------------------------------------
_handle_shed_check PROC
    cmp dword ptr g_fallbackActive, 0
    je @@shed_clear
    cmp dword ptr g_syncGlobalCapacityPct, SYNC_VRAM_SHED_THRESHOLD_PCT
    ja @@shed_clear
    mov dword ptr g_shedActive, 1
    mov eax, 1
    ret
@@shed_clear:
    mov dword ptr g_shedActive, 0
    xor eax, eax
    ret
_handle_shed_check ENDP

;----------------------------------------------------------------------
; _build_completion_request_from_pvdata - RCX=pvData, returns RAX=reqPtr or 0
;----------------------------------------------------------------------
_build_completion_request_from_pvdata PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov rbx, rcx
    test rbx, rbx
    jz @@fail

    mov rcx, rbx
    lea rdx, szKeyLine
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r12d, eax

    mov rcx, rbx
    lea rdx, szKeyCharacter
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r13d, eax

    ; URI optional - fallback to default when missing.
    mov rcx, rbx
    lea rdx, szKeyUri
    lea r8, g_uriBuf
    mov r9d, URI_BUFFER_SIZE
    call _json_extract_string_copy
    test eax, eax
    jz @@uri_default
    lea r14, g_uriBuf
    jmp @@uri_ready
@@uri_default:
    lea r14, szDefaultUri
@@uri_ready:
    lea rdi, g_dynamicReq
    lea rsi, [g_dynamicReq + DYNAMIC_REQ_BUFFER_SIZE - 1]

    mov rcx, rdi
    lea rdx, szCompReqP1
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    mov rcx, rax
    mov rdx, r14
    mov r8, rsi
    call _json_escape_copy
    test rax, rax
    jz @@fail

    mov rcx, rax
    lea rdx, szReqMidUri
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    lea rcx, g_numBuf
    mov eax, r12d
    call _itoa
    mov rcx, rax
    lea rdx, g_numBuf
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    mov rcx, rax
    lea rdx, szReqMidLine
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    lea rcx, g_numBuf
    mov eax, r13d
    call _itoa
    mov rcx, rax
    lea rdx, g_numBuf
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    ; Route selection: shed > fallback > hive-offload > normal
    cmp dword ptr g_shedActive, 0
    jne @@use_shed_sfx
    cmp dword ptr g_fallbackActive, 0
    jne @@use_fallback_sfx
    cmp dword ptr g_hiveOffloadActive, 0
    jne @@use_offload_sfx
@@use_normal_sfx:
    mov rcx, rax
    lea rdx, szReqSuffix
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_shed_sfx:
    mov rcx, rax
    lea rdx, szReqSuffixShed
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_fallback_sfx:
    mov rcx, rax
    lea rdx, szReqSuffixFallback
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_offload_sfx:
    mov rcx, rax
    lea rdx, szOffloadSuffixP1
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    mov rcx, rax
    lea rdx, g_hiveOffloadPeerIdBuf
    mov r8, rsi
    call _json_escape_copy
    test rax, rax
    jz @@fail
    mov rcx, rax
    lea rdx, szOffloadSuffixP2
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
@@sfx_done:
    lea rax, g_dynamicReq
    jmp @@out
@@fail:
    xor rax, rax
@@out:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_build_completion_request_from_pvdata ENDP

;----------------------------------------------------------------------
; _build_hover_request_from_pvdata - RCX=pvData, returns RAX=reqPtr or 0
;----------------------------------------------------------------------
_build_hover_request_from_pvdata PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    mov rbx, rcx
    test rbx, rbx
    jz @@fail

    mov rcx, rbx
    lea rdx, szKeyLine
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r12d, eax

    mov rcx, rbx
    lea rdx, szKeyCharacter
    call _json_extract_uint
    test edx, edx
    jz @@fail
    mov r13d, eax

    mov rcx, rbx
    lea rdx, szKeyUri
    lea r8, g_uriBuf
    mov r9d, URI_BUFFER_SIZE
    call _json_extract_string_copy
    test eax, eax
    jz @@uri_default
    lea r14, g_uriBuf
    jmp @@uri_ready
@@uri_default:
    lea r14, szDefaultUri
@@uri_ready:
    lea rdi, g_dynamicReq
    lea rsi, [g_dynamicReq + DYNAMIC_REQ_BUFFER_SIZE - 1]

    mov rcx, rdi
    lea rdx, szHoverReqP1
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    mov rcx, rax
    mov rdx, r14
    mov r8, rsi
    call _json_escape_copy
    test rax, rax
    jz @@fail

    mov rcx, rax
    lea rdx, szReqMidUri
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    lea rcx, g_numBuf
    mov eax, r12d
    call _itoa
    mov rcx, rax
    lea rdx, g_numBuf
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    mov rcx, rax
    lea rdx, szReqMidLine
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    lea rcx, g_numBuf
    mov eax, r13d
    call _itoa
    mov rcx, rax
    lea rdx, g_numBuf
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail

    ; Route selection: shed > fallback > hive-offload > normal
    cmp dword ptr g_shedActive, 0
    jne @@use_shed_sfx
    cmp dword ptr g_fallbackActive, 0
    jne @@use_fallback_sfx
    cmp dword ptr g_hiveOffloadActive, 0
    jne @@use_offload_sfx
@@use_normal_sfx:
    mov rcx, rax
    lea rdx, szReqSuffix
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_shed_sfx:
    mov rcx, rax
    lea rdx, szReqSuffixShed
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_fallback_sfx:
    mov rcx, rax
    lea rdx, szReqSuffixFallback
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    jmp @@sfx_done
@@use_offload_sfx:
    mov rcx, rax
    lea rdx, szOffloadSuffixP1
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
    mov rcx, rax
    lea rdx, g_hiveOffloadPeerIdBuf
    mov r8, rsi
    call _json_escape_copy
    test rax, rax
    jz @@fail
    mov rcx, rax
    lea rdx, szOffloadSuffixP2
    mov r8, rsi
    call _append_cstr_bounded
    test rax, rax
    jz @@fail
@@sfx_done:
    lea rax, g_dynamicReq
    jmp @@out
@@fail:
    xor rax, rax
@@out:
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
_build_hover_request_from_pvdata ENDP

;----------------------------------------------------------------------
; LSP_SendRequest - send JSON-RPC request to LSP
; RCX=JSON body (null-term), returns 1=ok 0=fail
;----------------------------------------------------------------------
LSP_SendRequest PROC
    push rbx
    push rsi
    push rdi
    sub rsp, 48h
    mov rbx, rcx
    cmp g_bSpawned, 0
    je @@fail
    ; strlen(body)
    mov rcx, rbx
    call _strlen
    mov r8, rax
    ; Build "Content-Length: N\r\n\r\n" + body in g_frameBuf
    lea rdi, g_frameBuf
    lea rsi, szContentLen
    mov ecx, 16
    rep movsb
    mov rcx, rdi
    mov rax, r8
    call _itoa
    add rdi, rdx
    mov dword ptr [rdi], 0A0D0A0Dh
    add rdi, 4
    mov r9, rdi
    lea rax, g_frameBuf
    sub r9, rax
    add r9, r8
    cmp r9, LSP_FRAME_BUFFER_SIZE
    ja @@fail
    ; copy body
    mov rcx, r8
    mov rsi, rbx
    rep movsb
    lea rax, g_frameBuf
    sub rdi, rax
    mov ebx, edi
    ; WriteFile(g_hChildStdinWr, frameBuf, len, &written, 0)
    mov rcx, g_hChildStdinWr
    lea rdx, g_frameBuf
    mov r8d, ebx
    lea r9, g_dwWritten
    mov qword ptr [rsp+28h], 0
    call WriteFile
    test eax, eax
    jz @@fail
    mov eax, 1
    jmp @@out
@@fail:
    xor eax, eax
@@out:
    add rsp, 48h
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_SendRequest ENDP

;----------------------------------------------------------------------
; LSP_ReadResponse - read Content-Length framed response
; RCX=output buffer, RDX=capacity, returns RAX=bytes read or 0
;----------------------------------------------------------------------
LSP_ReadResponse PROC
    push rbx
    push rsi
    push rdi
    push r12
    push r13
    push r14
    sub rsp, 48h
    mov dword ptr g_lspLastError, LSP_ERR_NONE
    mov rbx, rcx
    mov r12d, edx
    cmp g_bSpawned, 0
    je @@fail
    test rbx, rbx
    jz @@fail
    test r12d, r12d
    jz @@fail
    xor esi, esi
    mov r13d, LSP_POLL_MAX_ITERS
@@wait_initial:
    mov rcx, g_hChildStdoutRd
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+28h], 0
    lea rax, g_dwAvail
    mov qword ptr [rsp+30h], rax
    call PeekNamedPipe
    test eax, eax
    jz @@fail
    mov eax, g_dwAvail
    test eax, eax
    jnz @@have_initial_bytes
    dec r13d
    jz @@fail_timeout
    mov ecx, LSP_POLL_SLEEP_MS
    call Sleep
    jmp @@wait_initial
@@have_initial_bytes:
    mov ecx, eax
    cmp ecx, LSP_RESPONSE_BUFFER_SIZE
    jbe @F
    mov ecx, LSP_RESPONSE_BUFFER_SIZE
@@:
    mov rcx, g_hChildStdoutRd
    lea rdx, g_responseBuf
    mov r8d, ecx
    lea r9, g_dwRead
    mov qword ptr [rsp+28h], 0
    call ReadFile
    test eax, eax
    jz @@fail
    mov esi, g_dwRead
    test esi, esi
    jz @@fail
    xor edx, edx
@@find_header_end:
    mov eax, esi
    sub eax, 4
    cmp edx, eax
    ja @@fail_header_missing
    cmp dword ptr [g_responseBuf+rdx], 0A0D0A0Dh
    je @@header_found
    inc edx
    jmp @@find_header_end
@@header_found:
    mov r10d, edx
    add r10d, 4
    ; Scan header block for "content-length:" case-insensitively.
    xor r14d, r14d
@@find_len_hdr:
    mov eax, edx
    sub eax, CONTENT_LENGTH_TOKEN_LEN
    cmp r14d, eax
    ja @@fail_header_missing
    lea rcx, [g_responseBuf+r14]
    call _match_content_length_ci
    test eax, eax
    jnz @@len_hdr_found
    inc r14d
    jmp @@find_len_hdr
@@len_hdr_found:
    lea rsi, [g_responseBuf+r14+CONTENT_LENGTH_TOKEN_LEN]
@@skip_len_ws:
    movzx eax, byte ptr [rsi]
    cmp al, ' '
    je @@skip_len_ws_advance
    cmp al, 9
    jne @@parse_len_begin
@@skip_len_ws_advance:
    inc rsi
    jmp @@skip_len_ws
@@parse_len_begin:
    xor edi, edi
    xor r14d, r14d
@@parse_length:
    movzx eax, byte ptr [rsi]
    cmp al, 13
    je @@length_parsed
    cmp al, '0'
    jb @@fail_header_badlen
    cmp al, '9'
    ja @@fail_header_badlen
    imul edi, edi, 10
    sub eax, '0'
    add edi, eax
    inc r14d
    inc rsi
    jmp @@parse_length
@@length_parsed:
    test r14d, r14d
    jz @@fail_header_badlen
    cmp byte ptr [rsi+1], 10
    jne @@fail_header_badlen
    mov r11d, edi
    add r11d, r10d
    cmp r11d, LSP_RESPONSE_BUFFER_SIZE
    ja @@fail_frame_oversize
@@read_more:
    cmp esi, r11d
    jae @@have_frame
    mov r13d, LSP_POLL_MAX_ITERS
@@wait_more:
    mov rcx, g_hChildStdoutRd
    xor rdx, rdx
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+28h], 0
    lea rax, g_dwAvail
    mov qword ptr [rsp+30h], rax
    call PeekNamedPipe
    test eax, eax
    jz @@fail
    mov eax, g_dwAvail
    test eax, eax
    jnz @@have_more_bytes
    dec r13d
    jz @@fail_timeout
    mov ecx, LSP_POLL_SLEEP_MS
    call Sleep
    jmp @@wait_more
@@have_more_bytes:
    mov ecx, eax
    mov eax, LSP_RESPONSE_BUFFER_SIZE
    sub eax, esi
    cmp ecx, eax
    jbe @F
    mov ecx, eax
@@:
    mov rcx, g_hChildStdoutRd
    lea rdx, [g_responseBuf+rsi]
    mov r8d, ecx
    lea r9, g_dwRead
    mov qword ptr [rsp+28h], 0
    call ReadFile
    test eax, eax
    jz @@fail
    mov eax, g_dwRead
    test eax, eax
    jz @@fail
    add esi, eax
    jmp @@read_more
@@have_frame:
    cmp edi, r12d
    jae @@fail_output_too_small
    mov ecx, edi
    lea rsi, [g_responseBuf+r10]
    mov rdi, rbx
    rep movsb
    mov byte ptr [rdi], 0
    mov eax, edi
    jmp @@out
@@fail_timeout:
    mov dword ptr g_lspLastError, LSP_ERR_TIMEOUT
    jmp @@fail
@@fail_header_missing:
    mov dword ptr g_lspLastError, LSP_ERR_HEADER_MISSING
    jmp @@fail
@@fail_header_badlen:
    mov dword ptr g_lspLastError, LSP_ERR_HEADER_BAD_LENGTH
    jmp @@fail
@@fail_frame_oversize:
    mov dword ptr g_lspLastError, LSP_ERR_FRAME_OVERSIZE
    jmp @@fail
@@fail_output_too_small:
    mov dword ptr g_lspLastError, LSP_ERR_OUTPUT_TOO_SMALL
    jmp @@fail
@@fail:
    cmp dword ptr g_lspLastError, LSP_ERR_NONE
    jne @F
    mov dword ptr g_lspLastError, LSP_ERR_IO
@@:
    xor eax, eax
@@out:
    add rsp, 48h
    pop r14
    pop r13
    pop r12
    pop rdi
    pop rsi
    pop rbx
    ret
LSP_ReadResponse ENDP

;----------------------------------------------------------------------
; ExtensionInit(hParentWnd) - spawn LSP server, init pipes
;----------------------------------------------------------------------
ExtensionInit PROC hParentWnd:QWORD
    push rbx
    push rsi
    push rdi
    sub rsp, 168h
    mov g_hParentWnd, rcx
    lea rcx, g_secAttr
    mov dword ptr [rcx], SIZEOF SECURITY_ATTRIBUTES
    mov qword ptr [rcx+8], 0
    mov dword ptr [rcx+16], 1
    ; CreatePipe for child stdout (parent reads)
    lea rcx, g_hChildStdoutRd
    lea rdx, g_hChildStdoutWr
    lea r8, g_secAttr
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hChildStdoutRd
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; CreatePipe for child stderr (parent reads)
    lea rcx, g_hChildStderrRd
    lea rdx, g_hChildStderrWr
    lea r8, g_secAttr
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hChildStderrRd
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; CreatePipe for child stdin (parent writes)
    lea rcx, g_hChildStdinRd
    lea rdx, g_hChildStdinWr
    lea r8, g_secAttr
    xor r9d, r9d
    call CreatePipe
    test eax, eax
    jz @@fail
    mov rcx, g_hChildStdinWr
    mov edx, HANDLE_FLAG_INHERIT
    xor r8d, r8d
    call SetHandleInformation
    ; Zero STARTUPINFO
    lea rdi, g_startInfo
    xor eax, eax
    mov ecx, SIZEOF STARTUPINFOA / 8
    rep stosq
    mov dword ptr g_startInfo, SIZEOF STARTUPINFOA
    mov dword ptr g_startInfo.dwFlags, STARTF_USESTDHANDLES
    mov rax, g_hChildStdinRd
    mov g_startInfo.hStdInput, rax
    mov rax, g_hChildStdoutWr
    mov g_startInfo.hStdOutput, rax
    mov rax, g_hChildStderrWr
    mov g_startInfo.hStdError, rax
    ; CreateProcessA(0, cmdLine, 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi)
    xor rcx, rcx
    lea rdx, szClangdCmd
    xor r8, r8
    xor r9, r9
    mov qword ptr [rsp+28h], 1
    mov dword ptr [rsp+30h], CREATE_NO_WINDOW
    mov qword ptr [rsp+38h], 0
    mov qword ptr [rsp+40h], 0
    lea rax, g_startInfo
    mov qword ptr [rsp+48h], rax
    lea rax, g_procInfo
    mov qword ptr [rsp+50h], rax
    call CreateProcessA
    test eax, eax
    jz @@fail
    ; Close child-side handles in parent
    mov rcx, g_hChildStdoutWr
    call CloseHandle
    mov qword ptr g_hChildStdoutWr, 0
    mov rcx, g_hChildStdinRd
    call CloseHandle
    mov qword ptr g_hChildStdinRd, 0
    mov rcx, g_hChildStderrWr
    call CloseHandle
    mov qword ptr g_hChildStderrWr, 0
    mov rax, g_procInfo.hProcess
    mov g_hProcess, rax
    mov rax, g_procInfo.hThread
    mov g_hThread, rax
    mov g_bSpawned, 1
    ; Send initialize
    lea rcx, szInitRequest
    call LSP_SendRequest
    test eax, eax
    jz @@fail
    ; Read response (ignore for now - init completes)
    lea rcx, g_responseBuf
    mov edx, LSP_RESPONSE_BUFFER_SIZE
    call LSP_ReadResponse
    test eax, eax
    jz @@fail

    ; Enforce completion capability presence before activating command path.
    lea rcx, g_responseBuf
    call _verify_completion_provider_from_init_response
    test eax, eax
    jnz @@cache_triggers
    mov dword ptr g_lspLastError, LSP_ERR_RESULT_MISSING
    jmp @@fail

    ; Cache trigger characters from initialize result for O(1) key checks.
@@cache_triggers:
    lea rcx, g_responseBuf
    call _cache_trigger_chars_from_init_response
    test eax, eax
    jnz @@send_initialized

    ; Missing trigger array is tolerated with a deterministic default set.
    call _seed_default_trigger_mask

@@send_initialized:
    lea rcx, szInitialized
    call LSP_SendRequest
    test eax, eax
    jz @@fail
    mov eax, 1
    jmp @@out
@@fail:
    call ExtensionCleanup
    xor eax, eax
@@out:
    add rsp, 168h
    pop rdi
    pop rsi
    pop rbx
    ret
ExtensionInit ENDP

;----------------------------------------------------------------------
; ExtensionActivate - no-op
;----------------------------------------------------------------------
ExtensionActivate PROC
    mov eax, 1
    ret
ExtensionActivate ENDP

;----------------------------------------------------------------------
; ExtensionCleanup - terminate child process and close all handles
;----------------------------------------------------------------------
ExtensionCleanup PROC
    sub rsp, 28h
    mov eax, g_bSpawned
    test eax, eax
    jz @@close_handles

    mov rcx, g_hProcess
    test rcx, rcx
    jz @@close_thread
    xor edx, edx
    call TerminateProcess
    mov rcx, g_hProcess
    mov edx, WAIT_TIMEOUT_MS
    call WaitForSingleObject

@@close_thread:
    mov rcx, g_hThread
    test rcx, rcx
    jz @@close_handles
    call CloseHandle
    mov qword ptr g_hThread, 0

@@close_handles:
    mov rcx, g_hProcess
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hProcess, 0
@@:
    mov rcx, g_hChildStdinRd
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStdinRd, 0
@@:
    mov rcx, g_hChildStdinWr
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStdinWr, 0
@@:
    mov rcx, g_hChildStdoutRd
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStdoutRd, 0
@@:
    mov rcx, g_hChildStdoutWr
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStdoutWr, 0
@@:
    mov rcx, g_hChildStderrRd
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStderrRd, 0
@@:
    mov rcx, g_hChildStderrWr
    test rcx, rcx
    jz @F
    call CloseHandle
    mov qword ptr g_hChildStderrWr, 0
@@:
    mov dword ptr g_bSpawned, 0
    mov dword ptr g_dwWritten, 0
    mov dword ptr g_dwRead, 0
    mov dword ptr g_fallbackActive, 0
    mov qword ptr g_lastCompletionTick, 0
    mov dword ptr g_syncPeerCount, 0
    mov dword ptr g_syncEpoch, 0
    mov dword ptr g_syncLastAppliedEpoch, 0
    mov dword ptr g_syncGlobalCapacityPct, 0
    ; Zero all 4 peer table slots.
    lea rax, g_syncPeerTable
    mov ecx, SYNC_PEER_TABLE_SIZE
@@zero_slot:
    mov qword ptr [rax], 0
    mov dword ptr [rax+8], SYNC_PEER_STATE_UNKNOWN
    mov dword ptr [rax+12], 0
    mov dword ptr [rax+16], 0
    mov dword ptr [rax+20], 0
    mov qword ptr [rax+24], 0
    add rax, SYNC_PEER_INFO_STRIDE
    dec ecx
    jnz @@zero_slot
    ; Zero peer ID pool.
    push rdi
    lea rdi, g_syncPeerIdPool
    xor eax, eax
    mov ecx, SYNC_PEER_TABLE_SIZE * SYNC_PEER_ID_SLOT_SIZE
    rep stosb
    pop rdi
    mov dword ptr g_hiveOffloadActive, 0
    mov dword ptr g_shedActive, 0
    mov qword ptr g_pendingOffloadToken, 0
    mov qword ptr g_pendingOffloadTick, 0
    mov qword ptr g_lastOffloadAckToken, 0
    mov dword ptr g_statsAckOk, 0
    mov dword ptr g_statsAckReject, 0
    mov dword ptr g_statsAckTimeout, 0
    mov dword ptr g_statsAckStale, 0
    mov qword ptr g_specPendingToken, 0
    mov qword ptr g_specPendingTick, 0
    mov dword ptr g_specWinnerLane, 0
    mov dword ptr g_statsSpecRaceWon, 0
    mov dword ptr g_statsSpecRaceLost, 0
    mov dword ptr g_statsSpecRaceTimeout, 0
    mov dword ptr g_specBurstCount, 0
    mov qword ptr g_specBurstTick, 0
    mov dword ptr g_statsSpecThrottled, 0
    mov byte ptr g_hiveOffloadPeerIdBuf, 0
    mov eax, 1
    add rsp, 28h
    ret
ExtensionCleanup ENDP

;----------------------------------------------------------------------
; ExtensionExecuteCommand(pszCommand, pvData)
; Commands: "initialize", "completion", "didOpen"
;----------------------------------------------------------------------
ExtensionExecuteCommand PROC pszCommand:QWORD, pvData:QWORD
    push rbx
    push r12
    cmp g_bSpawned, 0
    je @@fail

    mov rbx, 0
    xor r12d, r12d
    mov rcx, pszCommand
    test rcx, rcx
    jz @@fail

    ; sync.update is bridge-local telemetry update from manager.
    mov rcx, pszCommand
    lea rdx, szCmdSyncUpdate
    call _streq
    test eax, eax
    jz @@check_runtime_signal
    mov rcx, pvData
    call _handle_sync_update
    test eax, eax
    jz @@fail
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; runtime.signal is bridge-local control, not forwarded to LSP.
@@check_runtime_signal:
    mov rcx, pszCommand
    lea rdx, szCmdRuntimeSignal
    call _streq
    test eax, eax
    jz @@check_shed
    mov rcx, pvData
    call _handle_runtime_signal
    test eax, eax
    jz @@fail
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; shed.check re-evaluates shed state from current globals.
@@check_shed:
    mov rcx, pszCommand
    lea rdx, szCmdShedCheck
    call _streq
    test eax, eax
    jz @@check_hive_offload
    call _handle_shed_check
    ; Return 1 regardless - shed evaluation is always a valid operation.
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; hive.offload activates peer-side weight handoff when primary peer is eligible.
@@check_hive_offload:
    mov rcx, pszCommand
    lea rdx, szCmdHiveOffload
    call _streq
    test eax, eax
    jz @@check_hive_offload_ack
    mov rcx, pvData
    call _handle_hive_offload
    test eax, eax
    jz @@fail
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; hive.offload.ack finalizes manager-side handoff outcome.
@@check_hive_offload_ack:
    mov rcx, pszCommand
    lea rdx, szCmdHiveOffloadAck
    call _streq
    test eax, eax
    jz @@check_hive_speculative_ack
    mov rcx, pvData
    call _handle_offload_ack
    test eax, eax
    jz @@fail
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; hive.speculative.ack processes race results from dual-inference path.
@@check_hive_speculative_ack:
    mov rcx, pszCommand
    lea rdx, szCmdHiveSpeculativeAck
    call _streq
    test eax, eax
    jz @@check_dynamic
    mov rcx, pvData
    call _handle_speculative_ack
    test eax, eax
    jz @@fail
    mov eax, 1
    pop r12
    pop rbx
    ret

    ; Dynamic marshalling path for completion and hover when pvData exists.
@@check_dynamic:
    mov rcx, pszCommand
    lea rdx, szCmdCompletion
    call _streq
    test eax, eax
    jz @@check_hover

    ; Tactical throttle: adaptive debounce by routing mode.
    call GetTickCount64
    mov r10, qword ptr g_lastCompletionTick
    test r10, r10
    jz @@mark_completion_tick
    mov r11, rax
    sub r11, r10

    mov ecx, COMPLETION_MIN_INTERVAL_NORMAL_MS
    cmp dword ptr g_fallbackActive, 0
    je @@have_interval
    mov ecx, COMPLETION_MIN_INTERVAL_FALLBACK_MS
@@have_interval:

    cmp r11d, ecx
    jae @@mark_completion_tick
    sub ecx, r11d
    call Sleep
    call GetTickCount64
@@mark_completion_tick:
    mov qword ptr g_lastCompletionTick, rax

@@build_completion:
    mov rcx, pvData
    call _build_completion_request_from_pvdata
    test rax, rax
    jz @@fail
    mov rbx, rax
    mov r12d, 2
    jmp @@have_payload
@@check_hover:
    mov rcx, pszCommand
    lea rdx, szCmdHover
    call _streq
    test eax, eax
    jz @@resolve_static
    mov rcx, pvData
    call _build_hover_request_from_pvdata
    test rax, rax
    jz @@fail
    mov rbx, rax
    mov r12d, 3
    jmp @@have_payload
@@resolve_static:
    mov rcx, pszCommand
    call LSP_ResolveCommandPayload
    test rax, rax
    jz @@fail
    mov rbx, rax
@@have_payload:
    mov rcx, rbx
    call LSP_SendRequest
    test eax, eax
    jz @@fail

    ; shutdown command is write-only; cleanup immediately.
    mov rcx, pszCommand
    lea rdx, szCmdShutdown
    call _streq
    test eax, eax
    jnz @@do_shutdown

    mov rcx, pvData
    test rcx, rcx
    jz @@ok
    mov edx, 32768
    call LSP_ReadResponse
    test eax, eax
    jz @@fail

    ; For completion/hover, verify response id and forward only result slice to UI.
    test r12d, r12d
    jz @@ok

    mov rcx, pvData
    lea rdx, szKeyId
    call _json_extract_uint
    test edx, edx
    jz @@fail_id
    cmp eax, r12d
    jne @@fail_id

    mov rcx, pvData
    mov rdx, pvData
    mov r8d, 32768
    call _json_extract_result_slice
    test eax, eax
    jz @@fail_result
@@ok:
    mov eax, 1
    pop r12
    pop rbx
    ret
@@do_shutdown:
    call ExtensionCleanup
    mov eax, 1
    pop r12
    pop rbx
    ret
@@fail_id:
    mov dword ptr g_lspLastError, LSP_ERR_ID_MISMATCH
    jmp @@fail
@@fail_result:
    mov dword ptr g_lspLastError, LSP_ERR_RESULT_MISSING
    jmp @@fail
@@fail:
    xor eax, eax
    pop r12
    pop rbx
    ret
ExtensionExecuteCommand ENDP

;----------------------------------------------------------------------
; ExtensionHandleChat - NOP for LSP
;----------------------------------------------------------------------
ExtensionHandleChat PROC pszMessage:QWORD, pfnCallback:QWORD
    xor eax, eax
    ret
ExtensionHandleChat ENDP

END
