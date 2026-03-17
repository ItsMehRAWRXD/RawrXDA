; RawrXD Macro Argument Substitution - Comprehensive Test Suite
; Tests all Phase 2 functionality: %1-%9, %*, %=, %%, defaults, nesting
; 
; Compile: ml64.exe /Fo macro_tests.obj macro_tests.asm
;          link /SUBSYSTEM:CONSOLE /OUT:macro_tests.exe macro_tests.obj

.code

; ============================================================================
; TEST 1: Basic Parameter Substitution (%1, %2, %3)
; ============================================================================
%macro mov_imm 2
  ; Expands: mov %1, %2
  mov %1, %2
%endmacro

test1_basic_substitution:
  mov_imm rax, 0x1234        ; Should expand to: mov rax, 0x1234
  mov_imm rcx, 0x5678        ; Should expand to: mov rcx, 0x5678
  mov_imm rdx, rbx           ; Should expand to: mov rdx, rbx
  mov_imm rsi, [rax]         ; Should expand to: mov rsi, [rax]
  ret

; ============================================================================
; TEST 2: Argument Count (%0, %=)
; ============================================================================
%macro show_count 0,3
  ; %0 or %= should emit decimal argument count
  db %0                      ; Expected: 0, 1, 2, or 3
%endmacro

test2_argument_count:
  show_count                 ; %0 = 0
  show_count 1               ; %0 = 1
  show_count 1,2             ; %0 = 2
  show_count 1,2,3           ; %0 = 3
  ret

; ============================================================================
; TEST 3: Variadic Arguments (%*)
; ============================================================================
%macro concat_args 1+
  ; All args concatenated with commas
  db %*
%endmacro

test3_variadic:
  ; Single arg: %* = arg1
  concat_args "single"
  
  ; Multiple: %* = arg1, arg2, arg3
  concat_args "a", "b", "c"
  
  ; Numeric: %* = 1, 2, 3, 4, 5
  concat_args 1, 2, 3, 4, 5
  
  ret

; ============================================================================
; TEST 4: Escaped Percent (%%)
; ============================================================================
%macro define_error 1
  ; %% should produce literal % in output
  db "Error: %1 code=%%1", 0
%endmacro

test4_escaped_percent:
  ; Expansion should have literal %1 at end
  define_error 42            ; → db "Error: 42 code=%1", 0
  ret

; ============================================================================
; TEST 5: Default Parameters
; ============================================================================
%macro exit_code 0,1 0
  ; First param optional, defaults to 0
  mov rax, 60                ; exit syscall
  mov rdi, %1
  syscall
%endmacro

test5_default_params:
  exit_code                  ; Use default: %1 = 0
  exit_code 1                ; Override: %1 = 1
  exit_code 42               ; Override: %1 = 42
  ret

; ============================================================================
; TEST 6: Nested Macros (Recursion Guard)
; ============================================================================
%macro outer_macro 1
  ; Calls inner_macro with %1
  inner_macro %1
%endmacro

%macro inner_macro 1
  ; Simple expansion
  nop
  mov rax, %1
  nop
%endmacro

test6_nested_macros:
  outer_macro 0x1234
  outer_macro 0x5678
  ret

; ============================================================================
; TEST 7: Multiple Parameters (%1 through %9)
; ============================================================================
%macro multi_param 9
  ; All 9 parameters used
  mov r8,  %1
  mov r9,  %2
  mov r10, %3
  mov r11, %4
  mov r12, %5
  mov r13, %6
  mov r14, %7
  mov r15, %8
  mov rax, %9
%endmacro

test7_multiple_params:
  multi_param 1, 2, 3, 4, 5, 6, 7, 8, 9
  ret

; ============================================================================
; TEST 8: Argument Mismatch Error Handling
; ============================================================================
%macro strict_two 2
  ; Must have exactly 2 args - too few/many should error
  mov rax, %1
  mov rbx, %2
%endmacro

test8_argument_validation:
  ; Valid: 2 args
  strict_two rax, rbx
  
  ; These should error during preprocessing:
  ; strict_two rax           ; Error: too few arguments
  ; strict_two rax,rbx,rcx   ; Error: too many arguments
  
  ret

; ============================================================================
; TEST 9: Complex Parameter References in Expressions
; ============================================================================
%macro compute 2
  ; %1 and %2 used in address expressions
  mov rax, [%1 + %2*8]
  add rax, %1
  sub rax, %2
%endmacro

test9_complex_expressions:
  compute rsi, rcx           ; mov rax, [rsi + rcx*8]
  compute [rax], [rbx]       ; mov rax, [[rax] + [rbx]*8]
  ret

; ============================================================================
; TEST 10: Macro with No Parameters
; ============================================================================
%macro nop_macro 0
  ; Zero params - %* should be empty
  nop
  nop
  nop
%endmacro

test10_no_params:
  nop_macro
  nop_macro
  nop_macro
  ret

; ============================================================================
; TEST 11: Redefining Macros (Last Definition Wins)
; ============================================================================
%macro redef 1
  ; Version 1
  mov rax, %1
%endmacro

%macro redef 1
  ; Version 2 (should override version 1)
  mov rbx, %1
%endmacro

test11_macro_redefinition:
  redef 42                   ; Should use version 2: mov rbx, 42
  ret

; ============================================================================
; TEST 12: Macro Inside Macro Call (Stress Test)
; ============================================================================
%macro level1 1
  level2 %1
%endmacro

%macro level2 1
  level3 %1
%endmacro

%macro level3 1
  mov rax, %1
%endmacro

test12_deep_nesting:
  level1 0xDEADBEEF          ; Should expand all 3 levels
  ret

; ============================================================================
; TEST 13: Variadic with Parentheses (Argument Preservation)
; ============================================================================
%macro call_func 2
  ; First arg is function name, second is args
  call %1(%2)
%endmacro

test13_preserve_grouping:
  call_func printf, ("Hello", 42)  ; Should preserve parens
  ret

; ============================================================================
; TEST 14: Single Argument Variadic
; ============================================================================
%macro singleton 1+
  ; With 1 arg, %* = arg1
  db %*
%endmacro

test14_single_variadic:
  singleton 0xFF             ; %* = 0xFF
  ret

; ============================================================================
; TEST 15: Empty Default (No Args Provided)
; ============================================================================
%macro maybe_pop 0,1
  % if %0 > 0
    pop %1
  % endif
%endmacro

test15_conditional_expansion:
  maybe_pop                  ; No args - no pop emitted
  maybe_pop rax              ; With arg - pop rax emitted
  ret

; ============================================================================
; TEST 16: Deeply Nested Macro Invocation (Recursion Limit at 32)
; ============================================================================
%macro level32_a 0
  level32_b
%endmacro

%macro level32_b 0
  level32_c
%endmacro

%macro level32_c 0
  ; At level 3 of nesting - well under 32 limit
  nop
%endmacro

test16_recursion_under_limit:
  level32_a
  ret

; ============================================================================
; Main Entry Point
; ============================================================================
public main

main:
  ; Call each test routine
  call test1_basic_substitution
  call test2_argument_count
  call test3_variadic
  call test4_escaped_percent
  call test5_default_params
  call test6_nested_macros
  call test7_multiple_params
  call test8_argument_validation
  call test9_complex_expressions
  call test10_no_params
  call test11_macro_redefinition
  call test12_deep_nesting
  call test13_preserve_grouping
  call test14_single_variadic
  call test15_conditional_expansion
  call test16_recursion_under_limit
  
  ; Exit successfully
  xor eax, eax
  ret

end main
