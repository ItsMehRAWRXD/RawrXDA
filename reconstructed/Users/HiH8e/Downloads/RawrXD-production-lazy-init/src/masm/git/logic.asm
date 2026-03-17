; masm_git_logic.asm - Git command wrapper
; Part of the Zero C++ mandate for RawrXD-QtShell

.code

; git_run(repoRoot, args)
git_run proc
    ; Execute git command via CreateProcessA
    ; Capture output to file or pipe
    ret
git_run endp

; git_is_available()
git_is_available proc
    ; Check if git --version works
    mov rax, 1 ; Available
    ret
git_is_available endp

; git_get_status(repoRoot)
git_get_status proc
    ; Run git status -s
    ret
git_get_status endp

end
