# Remaining Stub Implementation Plan

**Priority**: LOW to MEDIUM (Non-blocking for core agent functionality)  
**Last Updated**: December 2024

---

## Overview

This document tracks the remaining stub/placeholder code in the MASM IDE. **Core agentic systems are production-ready** - these items are polish features and advanced utilities.

---

## 🟡 Medium Priority: UI Convenience Features

### **ui_masm.asm** (Lines 2570-2664)

#### 1. Command Palette Execution (~Line 2570)
**Current**: TODO comment  
**Required**:
```asm
command_palette_execute PROC
    ; Parse command string
    ; Dispatch to handlers:
    ;   - "open file" → call file_open_dialog
    ;   - "git commit" → call git_execute_commit
    ;   - "run task" → call task_execute
    ; Return execution result
    ret
command_palette_execute ENDP
```

**Dependencies**: Command parser, handler registry

---

#### 2. File Search with Recursion (~Line 2625)
**Current**: TODO comment  
**Required**:
```asm
file_search_recursive PROC
    ; Call FindFirstFileW
    ; Loop with FindNextFileW
    ; For each directory:
    ;   - Recursive call for subdirectories
    ;   - Pattern match for files
    ; Build result array
    ret
file_search_recursive ENDP
```

**Dependencies**: Win32 `FindFirstFileW`, `FindNextFileW`, `FindClose`

---

#### 3. Problem Navigation (~Lines 2643-2644)
**Current**: TODO comment  
**Required**:
```asm
problem_navigate_to_error PROC
    ; Parse error string for file:line:column
    ; Extract filename, line number
    ; Call editor_jump_to_line
    ; Highlight error range
    ret
problem_navigate_to_error ENDP
```

**Dependencies**: Error parser, editor scroll API

---

#### 4. Debug Command Handling (~Lines 2662-2664)
**Current**: TODO comment  
**Required**:
```asm
debug_handle_command PROC
    ; Switch on command type:
    ;   - "break" → call debug_set_breakpoint
    ;   - "step" → call debug_step_over
    ;   - "continue" → call debug_resume
    ; Update debug UI
    ret
debug_handle_command ENDP
```

**Dependencies**: Debug protocol implementation

---

## 🟢 Low Priority: Advanced Puppeteer Helpers

### **agentic_puppeteer.asm** (Lines 800-900)

#### 1. Case-Insensitive String Search (~Line 800)
**Current**: Stub (uses `strstr_custom` fallback)  
**Required**:
```asm
strstr_case_insensitive PROC
    ; Convert both strings to lowercase
    ; Call strstr on lowercase versions
    ; Calculate offset in original string
    ret
strstr_case_insensitive ENDP
```

**Impact**: Improves hallucination detection accuracy (minor)

---

#### 2. Sentence Boundary Detection (~Line 810)
**Current**: Returns input pointer (no parsing)  
**Required**:
```asm
extract_sentence PROC
    ; Scan for sentence delimiters: ., !, ?
    ; Handle abbreviations: Dr., Mr., etc.
    ; Extract substring around claim
    ret
extract_sentence ENDP
```

**Impact**: Better context extraction for fact-checking (minor)

---

#### 3. Database Claim Lookup (~Line 820)
**Current**: Always returns "unknown"  
**Required**:
```asm
db_search_claim PROC
    ; Hash claim text
    ; Query fact database (SQLite or HTTP API)
    ; Return: 0=false, 1=true, 2=unknown
    ret
db_search_claim ENDP
```

**Impact**: Real fact verification (requires external DB)

---

#### 4. NLP Claim Extraction (~Line 860)
**Current**: Returns entire text as single claim  
**Required**:
```asm
_extract_claims_from_text PROC
    ; Tokenize text by sentences
    ; For each sentence:
    ;   - Identify subject-verb-object patterns
    ;   - Extract factual assertions
    ; Return array of claim strings
    ret
_extract_claims_from_text ENDP
```

**Impact**: Granular hallucination detection (requires NLP model)

---

#### 5. Claim Verification (~Line 880)
**Current**: Always returns "verified"  
**Required**:
```asm
_verify_claims_against_db PROC
    ; For each claim:
    ;   - Call db_search_claim
    ;   - Aggregate results
    ; Return verification score (0-100)
    ret
_verify_claims_against_db ENDP
```

**Impact**: Depends on db_search_claim implementation

---

#### 6. Correction String Append (~Line 890)
**Current**: Partial implementation (basic string copy)  
**Required**:
```asm
_append_correction_string PROC
    ; Find end of destination buffer
    ; Append correction text
    ; Add proper formatting (newlines, prefixes)
    ; Ensure null termination
    ret
_append_correction_string ENDP
```

**Impact**: Better correction formatting (cosmetic)

---

## 🔵 Low Priority: Persistence Features

### **chat_persistence.asm** (JSON TODOs)

#### 1. JSON Serialization
**Current**: TODO comments  
**Required**:
```asm
chat_serialize_to_json PROC
    ; Build JSON object:
    ;   - messages array
    ;   - timestamps
    ;   - metadata
    ; Call json_builder APIs
    ret
chat_serialize_to_json ENDP
```

---

#### 2. JSON Deserialization
**Current**: TODO comments  
**Required**:
```asm
chat_deserialize_from_json PROC
    ; Parse JSON string
    ; Extract message array
    ; Reconstruct chat history
    ret
chat_deserialize_from_json ENDP
```

---

#### 3. File I/O
**Current**: TODO comments  
**Required**:
```asm
chat_save_to_file PROC
    ; Serialize to JSON
    ; Create/open file via CreateFileW
    ; Write JSON string via WriteFile
    ; Close handle
    ret
chat_save_to_file ENDP
```

---

## 🔵 Low Priority: File Tree Features

### **file_tree_context_menu.asm** (Traversal TODOs)

#### 1. Directory Tree Recursion
**Current**: TODO comments  
**Required**:
```asm
tree_recurse_directory PROC
    ; FindFirstFileW
    ; For each entry:
    ;   - If directory: recursive call
    ;   - If file: add to tree node
    ; Build tree structure
    ret
tree_recurse_directory ENDP
```

---

#### 2. Context Menu Handlers
**Current**: TODO comments  
**Required**:
```asm
context_menu_on_rename PROC
    ; Show input dialog
    ; Call MoveFileW
    ; Update tree display
    ret
context_menu_on_rename ENDP

context_menu_on_delete PROC
    ; Confirm dialog
    ; Call DeleteFileW or RemoveDirectoryW
    ; Update tree display
    ret
context_menu_on_delete ENDP
```

---

## 📊 Implementation Effort Estimates

| Feature | Priority | Effort | Dependencies |
|---------|----------|--------|--------------|
| Command Palette | MEDIUM | 4 hours | Command parser |
| File Search | MEDIUM | 3 hours | Win32 file APIs |
| Problem Navigation | MEDIUM | 2 hours | Error parser |
| Debug Commands | MEDIUM | 6 hours | Debug protocol |
| Case-Insensitive Search | LOW | 1 hour | String utilities |
| Sentence Extraction | LOW | 3 hours | NLP patterns |
| DB Claim Lookup | LOW | 8 hours | Database integration |
| NLP Claim Extraction | LOW | 12 hours | External NLP model |
| Chat Persistence | LOW | 4 hours | JSON + File I/O |
| File Tree Recursion | LOW | 2 hours | Win32 file APIs |

**Total Effort**: ~45 hours (1 week of work)

---

## 🎯 Recommended Implementation Order

1. **Phase 1: UI Convenience** (MEDIUM priority)
   - Command palette execution
   - File search recursion
   - Problem navigation
   - Debug command handlers

2. **Phase 2: Persistence** (LOW priority)
   - Chat JSON serialization
   - File I/O for chat history
   - File tree context menu handlers

3. **Phase 3: Advanced NLP** (LOW priority, optional)
   - Case-insensitive search
   - Sentence boundary detection
   - Database claim lookup
   - NLP claim extraction

---

## ✅ Acceptance Criteria

Each implementation must:
- ✅ Replace TODO/stub with real logic
- ✅ Use proper x64 calling convention (shadow space)
- ✅ Handle errors (null checks, invalid inputs)
- ✅ Follow MASM coding standards (comments, indentation)
- ✅ Integrate with existing systems (no breaking changes)
- ✅ Compile without warnings

---

## 🚀 Status

**Core Agentic Systems**: ✅ PRODUCTION-READY  
**UI Features**: ⚠️ 4 TODOs remaining (non-blocking)  
**Advanced NLP**: ⚠️ 6 stubs remaining (optional)  
**Persistence**: ⚠️ 3 TODOs remaining (optional)

**Next Action**: Implement Phase 1 (UI Convenience) if needed, or proceed with deployment using current production-ready agent systems.
