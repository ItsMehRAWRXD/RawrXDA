
%macro emit_args 0+
    db %*
%endmacro

%macro show_percent 0
    db '%%'
%endmacro

section .data
emit_args 1, 2, 3, 4
show_percent
