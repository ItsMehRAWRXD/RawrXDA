; ============================================================================
; MACRO SUBSTITUTION ENGINE INTEGRATION DOCUMENT
; ============================================================================
; This file documents the exact integration points and code flow for 
; incorporating the macro_substitution_engine.asm into masm_nasm_universal.asm

.code

; ============================================================================
; SECTION 1: PREPROCESSOR INTEGRATION FLOW
; ============================================================================

; Location: masm_nasm_universal.asm, line ~2100 in preprocess_macros

; Current code (before integration):
;   else if (current_char == '%' && next_char == 'm')
;       // Handle %macro definition
;       if (keyword == "macro")
;           parse_macro_definition()
;           skip_to_endmacro()
;   else if (is_macro_name(token))
;       // Macro invocation detected
;       parse_macro_arguments()
;       // *** MISSING: Expansion call should go here ***
;       output_expanded_text()

; After integration:
;   else if (is_macro_name(token))
;       // Macro invocation detected
;       
;       ; Step 1: Parse arguments into ArgVec array
;       rax = parse_macro_arguments(rsi, arg_vec, &argc)
;       if (rax < 0) error(rax)
;       
;       ; Step 2: Look up macro in table
;       rax = macro_table_lookup(macro_name)
;       if (rax == NULL) error(ERR_UNDEFINED_MACRO)
;       macro_entry = rax
;       
;       ; Step 3: Validate argument count
;       if (argc < macro_entry.min_required) 
;           error(ERR_MACRO_ARGS, "Too few arguments")
;       if (argc > macro_entry.param_count) 
;           error(ERR_MACRO_ARGS, "Too many arguments")
;       
;       ; Step 4: Call expansion engine
;       rax = expand_macro_with_args(
;           macro_entry,        ; MacroEntry structure
;           arg_vec,            ; ArgVec array
;           argc,               ; Argument count
;           expand_buf,         ; Output buffer
;           EXPAND_BUF_SIZE,    ; Buffer size
;           depth_level         ; Current recursion depth
;       )
;       if (rax < 0) error(rax)
;       expanded_len = rax     ; Bytes written to expand_buf
;       
;       ; Step 5: Token injection (insert expanded text back into stream)
;       inject_tokens(expand_buf, expanded_len, current_pos)
;       
;       ; Step 6: Continue processing from expanded text
;       rsi = expand_buf
;       continue

; ============================================================================
; SECTION 2: DATA STRUCTURE LAYOUT
; ============================================================================

; MacroEntry structure (64 bytes):
; Offset  Size  Field
; ------  ----  -----
; +0      8     name_ptr        (pointer to macro name string)
; +8      8     body_ptr        (pointer to macro body tokens)
; +16     4     body_len        (length in bytes)
; +20     4     param_count     (max parameters, 0 if variadic with +)
; +24     4     min_required    (minimum required parameters)
; +28     4     max_params      (maximum parameters, or -1 for unlimited)
; +32     8     defaults_ptr    (pointer to array of default token ptrs)
; +40     4     depth_guard     (0-32, recursion counter)
; +44     20    _reserved       (padding to 64 bytes)

; ArgVec structure (16 bytes each in array):
; Offset  Size  Field
; ------  ----  -----
; +0      8     arg_ptr         (pointer to argument text)
; +8      4     arg_len         (argument length)
; +12     4     is_default      (bool: 1 if using default value)

; ExpandCtx stack frame (~256 bytes):
; Offset  Size  Field
; ------  ----  -----
; +0      8     macro_entry     (pointer to MacroEntry)
; +8      8     arg_vec         (pointer to ArgVec array)
; +16     4     argc            (argument count)
; +20     4     depth_level     (0-32, prevent infinite recursion)
; +24     8     input_ptr       (current position in body)
; +32     8     output_ptr      (write position in expand_buf)
; +40     8     expand_buf      (pointer to output buffer)
; +48     4     expand_buf_size (buffer size, typically 64KB)
; +52     4     bytes_written   (output position)
; +56     192   token_buf       (temporary for token operations)

; ============================================================================
; SECTION 3: ARGUMENT PARSING (ParseMacroArguments)
; ============================================================================

; Input:  rsi = pointer to token stream after macro name
;         rdi = pointer to ArgVec array (pre-allocated, 16*MACRO_MAX_ARGS)
;         rcx = MACRO_MAX_ARGS (max slots available)

; Output: rax = argc (argument count) if success
;         rax < 0 if error (ERR_MACRO_ARGS)

; Logic:
; 1. Skip whitespace after macro name
; 2. Check for opening paren (optional)
; 3. For each argument:
;    a. Skip whitespace and commas
;    b. Mark start of argument
;    c. Walk until comma or closing paren
;    d. Handle nested parens: ( ) tracking depth
;    e. Store {ptr, len} in ArgVec[argc++]
; 4. Validate closing paren/end of line
; 5. Return argc

; Example parse of: "add rax, rbx"
; Argument 0: {ptr=>&"rax", len=3}
; Argument 1: {ptr=>&"rbx", len=3}
; Return: argc=2

; ============================================================================
; SECTION 4: TOKEN SUBSTITUTION ENGINE (ExpandMacroWithArgs)
; ============================================================================

; Input:  rsi -> MacroEntry structure
;         rdi -> ArgVec array (argc entries)
;         rcx = argc
;         rdx -> expand_buf (output buffer)
;         r8 = expand_buf_size
;         r9d = depth_level (0-32)

; Output: rax = bytes_written if success
;         rax < 0 if error

; Pseudo-code:
;   
;   if (depth_level >= 32)
;       return ERR_MACRO_REC
;   
;   macro_entry->depth_guard = depth_level + 1
;   
;   input_ptr = macro_entry->body_ptr
;   output_ptr = expand_buf
;   
;   while (input_ptr < body_end)
;       token = *input_ptr
;       
;       if (token == TOK_PERCENT)
;           next_token = *(input_ptr + 1)
;           
;           if (next_token >= '0' && next_token <= '9')
;               param_index = next_token - '0'
;               if (param_index >= argc && no_default)
;                   return ERR_MACRO_ARGS
;               copy_arg_tokens(arg_vec[param_index], output_ptr)
;               input_ptr += 2  ; skip %N
;           
;           else if (next_token == '*')
;               ; %* = all args comma-separated
;               for i in 0..argc-1
;                   copy_arg_tokens(arg_vec[i], output_ptr)
;                   if (i < argc-1)
;                       *output_ptr++ = ','
;               input_ptr += 2
;           
;           else if (next_token == '=' or next_token == '0')
;               ; %= or %0 = arg count (emit decimal)
;               emit_decimal(argc, output_ptr)
;               input_ptr += 2
;           
;           else if (next_token == '%')
;               ; %% = literal percent
;               *output_ptr++ = '%'
;               input_ptr += 2
;           
;           else
;               return ERR_INVALID_SUBST
;       
;       else
;           ; Regular token: copy as-is
;           copy_token(input_ptr, output_ptr)
;           input_ptr += get_token_len(input_ptr)
;   
;   return output_ptr - expand_buf  ; total bytes

; ============================================================================
; SECTION 5: TOKEN INJECTION (inject_tokens)
; ============================================================================

; Input:  rsi -> expanded_text (from expand_buf)
;         rcx = expanded_len (bytes to inject)
;         rdi -> current_pos in main token stream

; After token injection, the main token stream should look like:
;
; Before: [... prev_tokens ...][MACRO_NAME][arg1,arg2][... rest ...]
;                               ↑ current_pos
;
; After:  [... prev_tokens ...][EXPANDED_BODY][... rest ...]
;                               ↑ current_pos points here

; Algorithm:
; 1. Save current position (to resume after expansion)
; 2. Memmove data after macro invocation to make space
; 3. Copy expanded text into hole
; 4. Update position to start of expanded text
; 5. Continue preprocessor loop

; ============================================================================
; SECTION 6: ERROR CODES (macro_errors.inc)
; ============================================================================

; Macro-related error codes:
; ERR_UNDEFINED_MACRO      = -50   ; Macro name not in table
; ERR_MACRO_ARGS           = -51   ; Argument count mismatch
; ERR_MACRO_REC            = -52   ; Recursion depth exceeded (>32)
; ERR_INVALID_SUBST        = -53   ; Invalid parameter like %A
; ERR_MACRO_NESTED_PAREN   = -54   ; Mismatched parens in args
; ERR_MACRO_BODY_TRUNC     = -55   ; Body text corrupted/truncated

; Error strings:
; szErrMacroUndef:    "Undefined macro: %s"
; szErrMacroArgs:     "Macro %s: expected %d args, got %d"
; szErrMacroRec:      "Macro recursion depth exceeded (>32 levels)"
; szErrMacroSubst:    "Invalid parameter substitution: %%%c"
; szErrMacroNested:   "Mismatched parentheses in macro arguments"

; ============================================================================
; SECTION 7: INTEGRATION CHECKLIST
; ============================================================================

; [ ] 1. Copy macro_substitution_engine.asm core logic into masm_nasm_universal.asm
;
; [ ] 2. In preprocess_macros loop, add macro invocation detection:
;        - Check if current token is in macro_table
;        - If yes, mark it as macro invocation
;
; [ ] 3. Implement ParseMacroArguments function:
;        - Input: token stream after macro name
;        - Output: ArgVec array and argc
;        - Handle nesting: parens, brackets
;        - Return error if mismatch
;
; [ ] 4. Implement ExpandMacroWithArgs function:
;        - Walk body tokens
;        - Detect %N, %*, %0, %=, %%, %%
;        - Inject argument tokens
;        - Update depth_guard counter
;        - Return expanded length or error
;
; [ ] 5. Implement token injection logic:
;        - Insert expanded text back into stream
;        - Update main loop position pointer
;        - Adjust all following positions
;
; [ ] 6. Add recursion depth field to MacroEntry:
;        - Initialize to 0 on macro definition
;        - Increment on entry to ExpandMacroWithArgs
;        - Decrement on exit
;        - Check >= 32 for error
;
; [ ] 7. Add default parameter support:
;        - Parse default values from macro signature
;        - Store as array of token pointers
;        - Fall back to defaults if argc < min_required + defaults_count
;
; [ ] 8. Test with macro_tests.asm:
;        - Run each of 16 test cases
;        - Verify expansions are correct
;        - Check error messages for invalid cases
;
; [ ] 9. Stress test:
;        - Nested macro expansion (level 1->2->3)
;        - Variadic with many arguments (10+ args to %*)
;        - Recursion at 32 levels (expect error)
;        - Mixed parameter types (%1, %*, %=, %%)
;
; [ ] 10. Performance validation:
;         - Macro expansion < 1ms per invocation
;         - No memory leaks in stack frames
;         - Token injection O(n) where n = expansion size

; ============================================================================
; SECTION 8: DEBUGGING TIPS
; ============================================================================

; If macro expansions are wrong:
;
; 1. Add DEBUG_DUMP_EXPANSION flag:
;    - Before/after token dumps
;    - Show parameter substitution details
;    - Output: "[MACRO EXPAND] %macro add 2 / mov rax,%1 / add rax,%2"
;           "[ARG 0] ptr=rsi len=3"
;           "[ARG 1] ptr=rcx len=3"
;           "[OUTPUT] mov rsi / add rsi, rcx"
;
; 2. Check argument parsing:
;    - Verify ArgVec array has correct {ptr, len} pairs
;    - Look for off-by-one errors in argument boundaries
;    - Trace comma/paren handling
;
; 3. Check token walking:
;    - Verify %N detection is working
;    - Confirm token copy is byte-exact
;    - Check boundary conditions (last token, partial token)
;
; 4. Check recursion guard:
;    - Verify depth_guard incremented on entry
;    - Verify depth_guard checked against 32 limit
;    - Confirm decrement on return (even on error)
;
; 5. Memory corruption detection:
;    - Add canary values around expand_buf
;    - Verify output_ptr never exceeds expand_buf_size
;    - Check for buffer overrun before injection

; ============================================================================
; SECTION 9: SAMPLE MACRO DEFINITION AND EXPANSION
; ============================================================================

; Definition (from macro_tests.asm):
;   %macro add 2
;     mov rax, %1
;     add rax, %2
;   %endmacro

; Invocation:
;   add rcx, rdx

; Parsing phase:
;   macro_name = "add"
;   argc = 2
;   arg_vec[0] = {ptr="rcx", len=3}
;   arg_vec[1] = {ptr="rdx", len=3}

; Expansion phase:
;   Body: "mov rax, %1 / add rax, %2"
;   Walk: "mov" → copy → "rax," → copy → " " → copy → "%1" → substitute arg_vec[0]
;   Walk: "add" → copy → "rax," → copy → " " → copy → "%2" → substitute arg_vec[1]
;   Output: "mov rax, rcx / add rax, rdx"

; Injection phase:
;   Replace "add rcx, rdx" in main token stream with expanded output
;   Continue processing from next token after expansion

; Final assembly:
;   mov rax, rcx        (from expansion)
;   add rax, rdx        (from expansion)

; ============================================================================

end ; END OF INTEGRATION DOCUMENT
