default reldefault rel








  call ExitProcess  xor ecx, ecx  sub rsp, 32start:global startsection .textextern ExitProcessextern ExitProcess
section .text
global start
start:
  sub rsp, 32
  xor ecx, ecx
  call ExitProcess
