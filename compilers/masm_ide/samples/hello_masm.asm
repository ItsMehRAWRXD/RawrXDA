EXTERN ExitProcess:PROCEXTERN ExitProcess:PROC












ENDWinMain ENDP  ret  add rsp, 32  ; not reached  call ExitProcess  xor ecx, ecx  sub rsp, 32WinMain PROC.code
.code
WinMain PROC
  sub rsp, 32
  xor ecx, ecx
  call ExitProcess
  ; not reached
  add rsp, 32
  ret
WinMain ENDP
END
