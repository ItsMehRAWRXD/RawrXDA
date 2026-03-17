# Pure MASM Implementation Quick Reference
**Fast lookup for all newly implemented features**  
**Updated**: December 28, 2025

---

## 📂 File Locations

| Feature Set | File | LOC | Functions |
|-------------|------|-----|-----------|
| **Phase 1** | `src/masm/final-ide/ui_phase1_implementations.asm` | 700 | 6 |
| **Phase 2** | `src/masm/final-ide/chat_persistence_phase2.asm` | 900 | 11 |
| **Phase 3** | `src/masm/final-ide/agentic_nlp_phase3.asm` | 900 | 9 |

---

## 🎯 Phase 1: UI Convenience Features

### Function: command_palette_execute
```asm
PUBLIC command_palette_execute PROC
    ; Parameters: RCX = command string (e.g., "File: Open")
    ; Returns: EAX = 0 (success), non-zero (error)
    ; Supported commands: 13+ (File, Edit, Search, Run categories)
```
**Usage Example**:
```asm
lea rcx, [rip + szCommandString]  ; "File: Save"
call command_palette_execute
test eax, eax
jz .success
```

### Function: file_search_recursive
```asm
PUBLIC file_search_recursive PROC
    ; Parameters:
    ;   RCX = directory path (e.g., "C:\project")
    ;   RDX = search pattern (e.g., "*.cpp")
    ;   R8D = max recursion depth (e.g., 10)
    ;   R9D = current depth (typically 0 to start)
    ; Returns: EAX = number of matches found
```
**Usage Example**:
```asm
lea rcx, [rip + szDir]        ; "."
lea rdx, [rip + szPattern]    ; "*.asm"
mov r8d, 10                   ; max depth
xor r9d, r9d                  ; start at depth 0
call file_search_recursive
; EAX = number of .asm files found
```

### Function: problem_navigate_to_error
```asm
PUBLIC problem_navigate_to_error PROC
    ; Parameters: RCX = error string
    ;   Format: "file.cpp(10,5): undefined symbol"
    ; Returns: EAX = 0 (success), 1 (parse error)
```
**Usage Example**:
```asm
lea rcx, [rip + szErrorStr]  ; "main.cpp(42,10): compilation error"
call problem_navigate_to_error
; Jumps editor to line 42, column 10 in main.cpp
```

### Function: debug_handle_command
```asm
PUBLIC debug_handle_command PROC
    ; Parameters:
    ;   RCX = command string ("break", "continue", "step", etc.)
    ;   RDX = optional parameter (e.g., line number for breakpoint)
    ; Returns: EAX = 0 (success)
```
**Usage Example**:
```asm
lea rcx, [rip + szCmd]  ; "break"
mov rdx, 42             ; line number
call debug_handle_command
; Sets breakpoint at line 42
```

---

## 💾 Phase 2: Data Persistence Features

### Function: chat_serialize_to_json
```asm
PUBLIC chat_serialize_to_json PROC
    ; Parameters:
    ;   RCX = CHAT_MESSAGE array pointer
    ;   RDX = message count (e.g., 10)
    ;   R8 = output JSON buffer pointer
    ;   R9 = max buffer size (e.g., 65536)
    ; Returns: EAX = bytes written to buffer
```
**Usage Example**:
```asm
mov rcx, [rip + g_messages]   ; Message array
mov edx, [rip + g_msg_count]  ; 10 messages
lea r8, [rip + g_json_buffer]
mov r9, MAX_JSON_BUFFER
call chat_serialize_to_json
; EAX = JSON string length (bytes)
```

### Function: chat_deserialize_from_json
```asm
PUBLIC chat_deserialize_from_json PROC
    ; Parameters:
    ;   RCX = JSON buffer pointer
    ;   RDX = buffer size
    ;   R8 = output CHAT_MESSAGE array
    ;   R9 = max message count (e.g., 256)
    ; Returns: EAX = messages reconstructed
```
**Usage Example**:
```asm
lea rcx, [rip + g_json_buffer]
mov edx, [rip + g_json_size]
lea r8, [rip + g_messages]
mov r9, 256
call chat_deserialize_from_json
; EAX = number of messages loaded
```

### Function: chat_save_to_file
```asm
PUBLIC chat_save_to_file PROC
    ; Parameters:
    ;   RCX = CHAT_MESSAGE array pointer
    ;   RDX = message count
    ;   R8 = filename pointer (or NULL for default "chat_history.json")
    ; Returns: EAX = 0 (success), 1 (error)
```
**Usage Example**:
```asm
mov rcx, [rip + g_messages]
mov edx, [rip + g_msg_count]
lea r8, [rip + szFilename]  ; "chat.json"
call chat_save_to_file
test eax, eax
jnz .error
```

### Function: chat_load_from_file
```asm
PUBLIC chat_load_from_file PROC
    ; Parameters:
    ;   RCX = filename pointer (or NULL for default)
    ;   RDX = output CHAT_MESSAGE array
    ;   R8 = max message count
    ; Returns: EAX = messages loaded
```
**Usage Example**:
```asm
xor rcx, rcx                ; NULL = use default filename
lea rdx, [rip + g_messages]
mov r8, 256
call chat_load_from_file
; EAX = number of messages loaded from file
```

---

## 🧠 Phase 3: Advanced NLP Features

### Function: strstr_case_insensitive
```asm
PUBLIC strstr_case_insensitive PROC
    ; Parameters:
    ;   RCX = haystack string
    ;   RDX = needle string
    ; Returns: RAX = pointer to match in original haystack (or 0)
```
**Usage Example**:
```asm
lea rcx, [rip + szText]     ; "The QUICK Brown Fox"
lea rdx, [rip + szFind]     ; "quick" (lowercase)
call strstr_case_insensitive
; RAX points to "QUICK" in original string
```

### Function: extract_sentence
```asm
PUBLIC extract_sentence PROC
    ; Parameters:
    ;   RCX = full text buffer
    ;   RDX = starting position in text
    ; Returns: RAX = sentence pointer, RCX = sentence length
```
**Usage Example**:
```asm
lea rcx, [rip + szFullText]
xor rdx, rdx                ; start at beginning
call extract_sentence
; RAX points to first sentence, RCX = length
```

### Function: db_search_claim
```asm
PUBLIC db_search_claim PROC
    ; Parameters: RCX = claim text (e.g., "Paris is capital of France")
    ; Returns: EAX = 0 (false), 1 (true), 2 (unknown/unverifiable)
    ; Note: Ready for HTTP API integration
```
**Usage Example**:
```asm
lea rcx, [rip + szClaim]  ; Claim string
call db_search_claim
; EAX = verification result
cmp eax, 1
je .claim_verified
```

### Function: _extract_claims_from_text
```asm
PUBLIC _extract_claims_from_text PROC
    ; Parameters:
    ;   RCX = full text buffer
    ;   RDX = output CLAIM_STRUCT array
    ;   R8D = max claims (e.g., 20)
    ; Returns: EAX = claims extracted
```
**Usage Example**:
```asm
lea rcx, [rip + szText]
lea rdx, [rip + g_claims_array]
mov r8d, 20
call _extract_claims_from_text
; EAX = number of claims found
```

### Function: _verify_claims_against_db
```asm
PUBLIC _verify_claims_against_db PROC
    ; Parameters:
    ;   RCX = CLAIM_STRUCT array
    ;   RDX = claim count
    ;   R8 = output score pointer (DWORD)
    ; Returns: EAX = overall verification score (0-100)
```
**Usage Example**:
```asm
lea rcx, [rip + g_claims_array]
mov edx, [rip + g_claim_count]
lea r8, [rsp]  ; Output score
call _verify_claims_against_db
; EAX = overall confidence (0-100)
; [R8] = detailed score
```

### Function: _append_correction_string
```asm
PUBLIC _append_correction_string PROC
    ; Parameters:
    ;   RCX = destination buffer
    ;   RDX = buffer size
    ;   R8 = correction text
    ;   R9D = type (0=general, 1=factual, 2=style)
    ; Returns: RAX = bytes written
```
**Usage Example**:
```asm
lea rcx, [rip + g_output_buf]
mov rdx, 4096
lea r8, [rip + szCorrection]
mov r9d, 1              ; Factual correction
call _append_correction_string
; RAX = bytes appended to buffer
```

---

## 🔗 External Dependencies

### Win32 API Calls (All Built-In)
```asm
FindFirstFileA, FindNextFileA, FindClose
CreateFileA, WriteFile, ReadFile, CloseHandle
SendMessageA, OutputDebugStringA
CharLowerA, CharUpperA
GetTickCount, Sleep
```

### Internal Utilities (Provided)
```asm
asm_malloc, asm_free
asm_str_length
strstr_masm, strcmp_masm
console_log
ui_add_chat_message, ui_editor_set_text, etc.
```

---

## 📐 Data Structures

### CHAT_MESSAGE Structure
```asm
CHAT_MESSAGE STRUCT
    msg_type        DWORD ?         ; 0=user, 1=agent, 2=system, etc.
    timestamp       DWORD ?         ; GetTickCount value
    agent_mode      DWORD ?         ; Which mode (ask, edit, plan, etc.)
    confidence      DWORD ?         ; 0-255 confidence score
    sender          BYTE 32 DUP (?) ; "User", "Agent", "System"
    content         BYTE 1024 DUP (?) ; Message text
    reserved        QWORD 16 DUP (?)
CHAT_MESSAGE ENDS
```

### CLAIM_STRUCT Structure
```asm
CLAIM_STRUCT STRUCT
    claim_text          BYTE 256 DUP (?)  ; The claim itself
    confidence          DWORD ?            ; 0-100 confidence
    verification_status DWORD ?            ; 0=false, 1=true, 2=unknown
    source_line         DWORD ?            ; Sentence number
CLAIM_STRUCT ENDS
```

---

## ⚡ Quick Patterns

### Calling Conventions (x64)
```asm
; Function call with 4 parameters
mov rcx, param1   ; Parameter 1
mov rdx, param2   ; Parameter 2
mov r8, param3    ; Parameter 3
mov r9d, param4   ; Parameter 4 (DWORD)
call function_name
; Result in RAX or EDX:EAX
```

### Memory Management Pattern
```asm
; Allocate
mov rcx, size
call asm_malloc
test rax, rax
jz .alloc_failed
; Use RAX...

; Free
mov rcx, rax
call asm_free
```

### String Operations Pattern
```asm
; Find substring
mov rcx, haystack
mov rdx, needle
call strstr_masm
test rax, rax
jz .not_found
; RAX = pointer to match
```

---

## 🧪 Testing Checklist

### Phase 1 Tests
- [ ] Command: File → New (clear editor)
- [ ] Command: File → Open (load file dialog)
- [ ] Command: File → Save (write to disk)
- [ ] Command: Edit → Cut/Copy/Paste
- [ ] File Search: Find all .cpp in directory
- [ ] Error Navigation: Jump to "main.cpp(10,5): error"
- [ ] Debug: Set breakpoint, continue, step

### Phase 2 Tests
- [ ] Serialize 10 messages to JSON
- [ ] Deserialize JSON back to messages
- [ ] Save messages to file (chat.json)
- [ ] Load messages from file
- [ ] Verify JSON format is valid
- [ ] Test with special characters (quotes, newlines)

### Phase 3 Tests
- [ ] Case-insensitive search: Find "HELLO" with "hello"
- [ ] Extract sentence from multi-sentence text
- [ ] Verify claim (returns 0, 1, or 2)
- [ ] Extract claims from paragraph (5+ sentences)
- [ ] Aggregate verification score
- [ ] Append correction to buffer

---

## 🚀 Build Commands

### Individual Assembly
```powershell
# Phase 1
ml64 /c /Fo ui_phase1_implementations.obj src/masm/final-ide/ui_phase1_implementations.asm

# Phase 2
ml64 /c /Fo chat_persistence_phase2.obj src/masm/final-ide/chat_persistence_phase2.asm

# Phase 3
ml64 /c /Fo agentic_nlp_phase3.obj src/masm/final-ide/agentic_nlp_phase3.asm
```

### Full Build
```powershell
cd build_masm
cmake --build . --config Release --target RawrXD-QtShell
```

### Verify Symbols
```powershell
dumpbin /symbols ui_phase1_implementations.obj | findstr "PUBLIC"
dumpbin /symbols chat_persistence_phase2.obj | findstr "PUBLIC"
dumpbin /symbols agentic_nlp_phase3.obj | findstr "PUBLIC"
```

---

## 📞 Common Issues & Solutions

### Issue: "Undefined reference to command_palette_execute"
**Solution**: Add `EXTERN command_palette_execute:PROC` to your .asm file

### Issue: "Buffer overflow in chat_serialize_to_json"
**Solution**: Ensure R9 (max buffer size) is set correctly (usually 65536)

### Issue: File search finds no files
**Solution**: Verify path exists and pattern is correct (e.g., "*.*" for all files)

### Issue: JSON deserialization returns 0 messages
**Solution**: Check JSON format is valid (proper array brackets and object structure)

---

## 📚 Further Reading

- `PURE_MASM_AUDIT_COMPREHENSIVE.md` - Full feature audit
- `COMPLETE_MASM_PHASE_IMPLEMENTATION_GUIDE.md` - Detailed integration guide
- `ui_masm.asm` - Main UI framework (4,000+ lines)
- `copilot-instructions.md` - Architecture and design patterns

---

**Quick Start**: Copy 3 ASM files → Update CMakeLists.txt → Build → Test → Deploy  
**Estimated Time**: 4-6 hours including testing

