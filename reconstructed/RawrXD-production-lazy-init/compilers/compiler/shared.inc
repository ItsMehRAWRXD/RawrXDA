; Shared extern declarations for NASM compiler_from_scratch modules
; Resolves against universal_compiler_runtime_clean.asm

extern compiler_state
extern error_count
extern warning_count

extern compiler_init
extern compiler_cleanup
extern compiler_error
extern compiler_warning
extern compiler_get_error_count
extern compiler_get_warning_count

extern runtime_print_string
extern runtime_print_char
extern runtime_print_int
extern runtime_read_string
extern runtime_read_char
extern runtime_read_int

extern runtime_malloc
extern runtime_free
extern runtime_realloc
extern runtime_memset
extern runtime_memcpy
extern runtime_memcmp
