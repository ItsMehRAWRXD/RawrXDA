; MASM64 COMPILATION FIX PATCH for omega.asm
; 
; This patch resolves the 3 remaining compilation errors:
; 1. Line 620: movss addressing mode 
; 2. Line 1916: undefined symbol g_KickPitch
; 3. Line 1967: undefined symbol g_SnarePitch
;
; SOLUTION STRATEGY:
; Replace real4 floating-point constants with DQ (qword) encoded IEEE 754 floats
; This avoids forward reference issues during 1st pass assembly
;
; IEEE 754 encoding for key values:
; 55.0 Hz (Kick pitch)    = 42580000h (as 32-bit float dword)
; 200.0 Hz (Snare pitch)  = 43480000h (as 32-bit float dword)
; 8000.0 Hz (HiHat pitch) = 45fa0000h (as 32-bit float dword)
; 0.95 (Kick decay)       = 3f733333h (as 32-bit float dword)
;
; Fix approach: Use DWORD aligned allocation with immediate values
; Or reference via register load before use

;==============================================
; FIX 1: MOVSS addressing at line 620
;==============================================
; OLD:
;    lea rsi, [rdx + rax*4]           
;    movss [rsi], xmm0
; 
; Why this fails: SSE instructions have strict addressing mode requirements
; ML64 may not accept [rsi] as valid operand for movss
;
; NEW: Use MOVD for memory store (safer for MASM64 compatibility)
;    lea rsi, [rdx + rax*4]
;    movd dword ptr [rsi], xmm0     ; Move SSE->memory via MOVD

;==============================================
; FIX 2 & 3: Undefined symbols
;==============================================
; Problem: g_KickPitch, g_SnarePitch defined as real4 in .data
; But MASM64 can't resolve them when used in .code section
; 
; Root cause: real4 directive may not create public symbols properly
; 
; Solution: Load immediate values at startup or use absolute offsets
;
; Option A - Inline the values:
;    mov edx, 42580000h              ; g_fKickPitch = 55.0
;    movd xmm1, edx
;    cvtdq2ps xmm1, xmm1
;
; Option B - Use symbolic equates:
;    g_fKickPitch equ 42580000h
;    mov edx, g_fKickPitch
;    movd xmm1, edx
;    cvtdq2ps xmm1, xmm1
;
; Option C - Initialize globals at runtime from lookup table

print "PATCH INSTRUCTIONS:"
print "1. At line 620, change:"
print "   lea rsi, [rdx + rax*4]"
print "   movss [rsi], xmm0"
print "   TO:"
print "   lea rsi, [rdx + rax*4]"
print "   movd dword ptr [rsi], xmm0"
print ""
print "2. Add these equates after the .data section header:"
print "   g_fKickPitch    equ 42580000h   ; 55.0 Hz as IEEE 754"
print "   g_fSnarePitch   equ 43480000h   ; 200.0 Hz as IEEE 754"
print ""
print "3. At lines 1916 and 1967, change:"
print "   movss xmm1, g_KickPitch"
print "   TO:"
print "   mov edx, g_fKickPitch"
print "   movd xmm1, edx"
print ""
print "ALTERNATIVE SIMPLER FIX:"
print "Replace real4 declarations with explicitly initialized DWORD values"
print "in .data? (uninitialized) section, then initialize at runtime"
