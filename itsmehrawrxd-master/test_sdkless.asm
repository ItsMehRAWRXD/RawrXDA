; SDKless Test Framework 
section .data 
    test_msg db 'Running SDKless IDE Tests...', 0 
section .text 
    global test_main 
test_main: 
    ; Test without any dependencies 
    call test_compiler 
    call test_ide 
    call test_debugger 
    ret 
test_compiler: 
    mov rax, 1 
    ret 
test_ide: 
    mov rax, 1 
    ret 
test_debugger: 
    mov rax, 1 
    ret 
