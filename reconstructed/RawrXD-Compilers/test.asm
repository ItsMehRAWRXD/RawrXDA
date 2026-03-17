OPTION CASEMAP:NONE
OPTION DOTNAME

PE_BUILDER_CONTEXT struct
    Field dword ?
PE_BUILDER_CONTEXT ends

.code
test proc
    mov dword ptr [rcx].PE_BUILDER_CONTEXT.Field, 1
    ret
test endp
end
