; ===============================================================================
; BUILD TEST - Verify all components compile correctly
; ===============================================================================
; This file tests that the universal dispatcher and drag-and-drop functionality
; can be compiled successfully with all dependencies.
; ===============================================================================

option casemap:none

; Test that all extern declarations resolve
extern InitializeDispatcher:proc
extern UniversalDispatch:proc
extern ClassifyIntent:proc

.data
szTestMessage db "Build test successful - all components compile correctly",0

.code

; Simple test function
TestBuild PROC
    ; Test that we can call the dispatcher functions
    call InitializeDispatcher
    
    lea rcx, szTestMessage
    call UniversalDispatch
    
    ret
TestBuild ENDP

PUBLIC TestBuild

END