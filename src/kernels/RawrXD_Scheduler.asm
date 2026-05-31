option casemap:none

.data
ollama_ready db 0
local_ready  db 0
winner       db 0

.code

start_scheduler proc
    call start_ollama_thread
    call start_local_thread

wait_loop:
    cmp ollama_ready, 1
    je ollama_wins

    cmp local_ready, 1
    je local_wins

    pause
    jmp wait_loop

ollama_wins:
    mov winner, 1
    call cancel_local
    jmp stream_ollama

local_wins:
    mov winner, 2
    call cancel_ollama
    jmp stream_local
start_scheduler endp

start_ollama_thread proc
    ret
start_ollama_thread endp

start_local_thread proc
    ret
start_local_thread endp

cancel_local proc
    ret
cancel_local endp

stream_ollama proc
    ret
stream_ollama endp

cancel_ollama proc
    ret
cancel_ollama endp

stream_local proc
    ret
stream_local endp

end