; Inject into Explorer_LazyLoad
; Enable TVS_EX_VIRTUALMODE if item count > 1000
mov rcx, g_hExplorerTree
mov edx, TVM_GETCOUNT
call SendMessageA
cmp rax, 1000
jl @F
mov rcx, g_hExplorerTree
mov edx, TVM_SETEXTENDEDSTYLE
mov r8, TVS_EX_VIRTUALMODE
mov r9, TVS_EX_VIRTUALMODE
call SendMessageA
@@:
