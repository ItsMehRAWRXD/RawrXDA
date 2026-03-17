# 🎯 RawrXD ML64 HTTP Inference Bindings — Quick Reference

## API Contract

All functions use x64 MASM ABI (fastcall on Windows):
- rcx, rdx, r8, r9 = integer/pointer args
- rax = return value
- Caller preserves rbx, rsi, rdi, rbp, r12-r15
- Callee preserves rax, rcx, rdx, r8-r11

---

## Function Signatures

### `RawrXD_Tokenizer_Init`
```asm
; Initialize tokenizer (vocabulary)
; Input:  rcx = pointer to vocab path string
; Output: rax = tokenizer handle (or 0 if failed)
; Notes:  Currently no-op (server tokenizes), returns 1
call RawrXD_Tokenizer_Init
test rax, rax
jz init_failed
mov [g_hTokenizer], rax
```

### `RawrXD_Inference_Init`
```asm
; Initialize inference engine (HTTP session)
; Input:  rcx = model path
;         rdx = tokenizer handle
; Output: rax = inference handle (or 0 if failed)
; Notes:  Opens WinHTTP session to 127.0.0.1:11434
call RawrXD_Inference_Init
test rax, rax
jz init_failed
mov [g_hInference], rax
```

### `RawrXD_Inference_Generate`
```asm
; Generate tokens from prompt
; Input:  rcx = inference handle
;         rdx = prompt string (null-terminated)
;         r8  = output buffer size (bytes)
;         r9  = pointer to output buffer
; Output: rax = bytes written to buffer
;         (tokens written as ASCII: "token1" + "token2" + ...)
; Notes:  Streams NDJSON from HTTP, parses per-line
call RawrXD_Inference_Generate
mov [g_BytesGenerated], rax
```

---

## HTTP Protocol Details

### Endpoint
```
POST http://127.0.0.1:11434/api/generate
```

### Request Format (JSON)
```json
{
  "model": "phi-3-mini",
  "prompt": "USER TEXT HERE (with \\\" for quotes)",
  "stream": true,
  "num_predict": 512
}
```

### Response Format (NDJSON)
Each line is a JSON object:
```json
{"response":"word ","done":false}
{"response":"another ","done":false}
{"response":"token","done":false}
{"response":"","done":true}
```

Parser extracts `"response":"XXXX"` field and appends to buffer.

---

## Usage Example (MASM)

```asm
; Global storage
g_hTokenizer    qword 0
g_hInference    qword 0
g_szPrompt      db "Explain x86-64 MASM", 0
g_pTokenBuffer  qword 0  ; allocated somewhere
g_BufferSize    qword 4096

; Initialize
mov rcx, 1
call RawrXD_Tokenizer_Init
mov [g_hTokenizer], rax

mov rcx, 1
mov rdx, [g_hTokenizer]
call RawrXD_Inference_Init
mov [g_hInference], rax

; Generate
mov rcx, [g_hInference]      ; inference handle
lea rdx, [g_szPrompt]        ; prompt
mov r8, [g_BufferSize]       ; buffer size
mov r9, [g_pTokenBuffer]     ; output buffer
call RawrXD_Inference_Generate
mov [g_BytesGenerated], rax

; Now [g_pTokenBuffer] contains tokens
; Can display, append to UI, etc.
```

---

## Integration Points (Core2 Calls)

Core2 automatically calls these from `RunAutonomousCycle_ml64`:

```asm
; In RawrXD_Amphibious_Core2_ml64.asm:

; During initialization:
InitializeAmphibiousCore:
  mov rcx, [g_szModelPath]      ; "D:\rawrxd\70b_simulation.gguf"
  call RawrXD_Tokenizer_Init
  mov [g_hTokenizer], rax
  
  mov rcx, [g_szModelPath]
  mov rdx, [g_hTokenizer]
  call RawrXD_Inference_Init
  mov [g_hInference], rax

; During each cycle:
RunAutonomousCycle_ml64:
  mov rcx, [g_hInference]
  mov rdx, [g_szCurrentPrompt]
  mov r8, TOKEN_BUFFER_BYTES
  mov r9, [g_pTokenBuffer]
  call RawrXD_Inference_Generate
  
  ; Process tokens returned in g_pTokenBuffer
  ; Update telemetry
  ; Render output (UI / console)
```

---

## Windows API Details

InferenceAPI uses these WinHTTP functions:

### WinHttpOpen
```asm
; Create HTTP session
extern WinHttpOpen : proc
; Input:  rcx = user agent string
;         rdx = NULL (auto-detect proxy)
;         r8d = 0 (no explicit proxy)
;         r9d = 0 (no bypass list)
;         [rsp+32] = flags (0)
; Output: rax = session handle
; Usage:
mov rcx, offset g_szUserAgent      ; "RawrXD-Inference/1.0"
xor rdx, rdx
xor r8d, r8d
xor r9d, r9d
call WinHttpOpen
mov [g_hSession], rax
```

### WinHttpConnect
```asm
; Connect to specific host:port
extern WinHttpConnect : proc
; Input:  rcx = session handle
;         rdx = host string ("127.0.0.1")
;         r8w = port (11434)
;         r9d = 0 (reserved)
; Output: rax = connection handle
; Usage:
mov rcx, [g_hSession]
lea rdx, [g_szHost]                ; "127.0.0.1"
mov r8w, 11434
xor r9d, r9d
call WinHttpConnect
mov [g_hConnect], rax
```

### WinHttpOpenRequest
```asm
; Prepare HTTP request
extern WinHttpOpenRequest : proc
; Input:  rcx = connection handle
;         rdx = method ("POST")
;         r8 = path ("/api/generate")
;         r9 = version (NULL default)
;         [rsp+32] = referrer (NULL)
;         [rsp+40] = accept types (NULL = default)
;         [rsp+48] = flags (0)
; Output: rax = request handle
; Usage:
mov rcx, [g_hConnect]
lea rdx, [g_szMethod]              ; "POST"
lea r8, [g_szPath]                 ; "/api/generate"
xor r9, r9
call WinHttpOpenRequest
mov [g_hRequest], rax
```

### WinHttpSendRequest
```asm
; Send HTTP request with body
extern WinHttpSendRequest : proc
; Input:  rcx = request handle
;         rdx = headers (NULL = default)
;         r8d = header length (-1 = auto)
;         r9 = optional body
;         [rsp+32] = body length
;         [rsp+40] = total length (0 for streaming)
; Usage:
mov rcx, [g_hRequest]
xor rdx, rdx                       ; no extra headers
mov r8d, -1
lea r9, [g_JsonBuffer]             ; JSON request body
mov qword [rsp+32], JSON_LENGTH
xor qword [rsp+40], qword [rsp+40] ; 0 for streaming
call WinHttpSendRequest
```

### WinHttpReceiveResponse
```asm
; Wait for response headers
extern WinHttpReceiveResponse : proc
; Input:  rcx = request handle
;         rdx = NULL (reserved)
; Output: rax = 1 if success, 0 if failed
; Usage:
mov rcx, [g_hRequest]
xor rdx, rdx
call WinHttpReceiveResponse
test rax, rax
jz recv_failed
```

### WinHttpReadData
```asm
; Read response body chunk
extern WinHttpReadData : proc
; Input:  rcx = request handle
;         rdx = buffer for data
;         r8d = buffer size
;         r9d = pointer to bytes read (output)
; Output: rax = 1 if success, 0 if failed
; Usage (loop):
mov rcx, [g_hRequest]
lea rdx, [g_ReadBuffer]            ; 4KB buffer
mov r8d, 4096
lea r9d, [g_BytesRead]
call WinHttpReadData
mov ebx, [g_BytesRead]
cmp ebx, 0
je read_done                       ; EOF
; Process ReadBuffer[0..ebx-1]
jmp read_loop
```

---

## Configuration Defines

In **RawrXD_InferenceAPI.asm**:

```asm
; Model parameters
szModel             DB "phi-3-mini",0              ; Model name
szHost              DB "127.0.0.1",0              ; Server IP
wPort               EQU 11434                     ; Server port
szPath              DB "/api/generate",0          ; HTTP endpoint

; Buffers
TOKEN_BUFFER_BYTES  EQU 4096                      ; Output token buffer size
JSON_BUFFER_BYTES   EQU 2048                      ; JSON request buffer
READ_BUFFER_BYTES   EQU 4096                      ; HTTP read chunk size

; HTTP parameters
NUM_PREDICT         EQU 512                       ; Max tokens to generate
STREAM              EQU 1                         ; Enable streaming
```

---

## Error Codes

Core2 returns:
- `0` = Success (all stages complete)
- `1` = Initialization failed
- `2` = Inference failed
- `4` = Telemetry write failed
- `8` = DMA validation failed
- `16` = Healing failed

Inspect `stage_mask` in telemetry JSON:
```json
"stage_mask": 63  // Binary: 111111 = all 6 bits set = success
```

---

## Performance Tips

1. **Reuse HTTP Session** — Initialize once, call generate multiple times
2. **Buffer Tokens** — Accumulate tokens before UI updates
3. **Async Read** — Don't block on WinHttpReadData
4. **Pool Buffers** — Pre-allocate JSON and read buffers at init
5. **Limit Tokens** — num_predict=512 balances latency vs quality

---

## Testing HTTP Directly

Before running .exe, test the endpoint:

```powershell
# Test API availability
Invoke-WebRequest http://127.0.0.1:11434/api/status

# Generate tokens (streaming)
$body = @{
    model = "phi-3-mini"
    prompt = "Explain x86"
    stream = $true
    num_predict = 50
} | ConvertTo-Json

Invoke-WebRequest `
    -Uri "http://127.0.0.1:11434/api/generate" `
    -Method POST `
    -Body $body `
    -ContentType "application/json" `
    -SkipHttpErrorCheck | Select-Object -ExpandProperty Content
```

Should output NDJSON:
```
{"response":"assistant","done":false}
{"response":" I","done":false}
{"response":"'ll explain","done":false}
...
```

---

## Debugging

Add these breakpoints in debugger:

1. After `RawrXD_Inference_Init`
   - Verify `rax` != 0 (handle created)
   - Check WinHTTP session in Task Manager

2. After `RawrXD_Inference_Generate`
   - Verify `rax` > 0 (bytes returned)
   - Dump `g_pTokenBuffer` to see tokens

3. In CLI output loop
   - Print each token immediately after `Inference_Generate`
   - Measure latency per token

---

## Production Checklist

- [ ] Ollama running on localhost:11434
- [ ] Model loaded: `ollama run phi-3-mini`
- [ ] Firewall allows 127.0.0.1:11434 (usually auto on same machine)
- [ ] Build completes without errors
- [ ] CLI generates telemetry JSON (success=true)
- [ ] GUI streams tokens visibly in real-time
- [ ] Token count > 0 in telemetry
- [ ] Cycle count = 6 (or expected value)

---

**All functions are pure x64 MASM. No C++ required. Pure assembly inference pipeline.**

