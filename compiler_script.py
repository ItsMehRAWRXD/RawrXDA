import os
import re
import subprocess

def fix_asm():
    with open("D:\\rawrxd\\gpu_dma_complete_reverse_engineered_final_v4.asm", "r") as f:
        lines = f.readlines()
        
    out = ["option casemap:none\n", ".code\n"]
    for line in lines:
        # Avoid structs entirely
        line = line.replace("MEMORY_PATCH STRUCT", "MEM_PATCH_DATA STRUCT")
        line = line.replace("MEMORY_PATCH ENDS", "MEM_PATCH_DATA ENDS")
        line = line.replace("(MEMORY_PATCH ptr", "(MEM_PATCH_DATA ptr")
        line = line.replace(".Size", ".PatchSize")
        
        # Kill frames
        line = re.sub(r"PROC\s+FRAME", "PROC", line)
        if re.search(r"^\s*\.(ENDPROLOG|PUSHREG|ALLOCSTACK|SETFRAME|SAVEREG)", line):
            continue
            
        # Kill locals and map explicitly
        if re.search(r"LOCAL\s+(pContext|pPatch|result)", line):
            continue
            
        line = re.sub(r"\bpContext\b", "rcx", line)
        line = re.sub(r"\bpPatch\b", "rdx", line)
        line = re.sub(r"\bresult\b", "eax", line)
        line = re.sub(r"\bsize\b", "r9", line)
        
        if re.search(r"mov\s+rcx,\s*rcx", line) or re.search(r"mov\s+rdx,\s*rdx", line) or re.search(r"mov\s+eax,\s*eax", line):
            continue
            
        # Offset maps
        line = line.replace("[rbx].HostAddress", "QWORD PTR [rbx+0]")
        line = line.replace("[rbx].DeviceAddress", "QWORD PTR [rbx+8]")
        line = line.replace("[rbx].PatchSize", "QWORD PTR [rbx+16]")
        line = line.replace("[rbx].Size", "QWORD PTR [rbx+16]")
        
        line = line.replace("mov [rbx], 0", "mov QWORD PTR [rbx], 0")
        line = line.replace("QWORD PTR QWORD PTR", "QWORD PTR")
        
        # Externals
        if "PROTO" in line or "EXTERN" in line:
            continue
        if re.search(r"(QueryPerformanceFrequency|GetVersion|LoadLibraryA|GetProcAddress|CreateThread)", line):
            continue
            
        if "operator in expression" in line: # Avoid the weird macro bug
            continue
            
        # Size mismatch fixes (usually mov rax, DWORD PTR or similar)
        # We enforce QWORD for untyped memory writes of 0
        line = re.sub(r"mov\s+\[(r[a-z0-9]+)\],\s*0", r"mov QWORD PTR [\1], 0", line)
        line = re.sub(r"mov\s+\[(r[a-z0-9]+\+[0-9]+)\],\s*0", r"mov QWORD PTR [\1], 0", line)
        line = re.sub(r"mov\s+\[(r[a-z0-9]+)\],\s*(r[a-z0-9]+)", r"mov QWORD PTR [\1], \2", line)
        line = re.sub(r"mov\s+\[(r[a-z0-9]+\+[0-9]+)\],\s*(r[a-z0-9]+)", r"mov QWORD PTR [\1], \2", line)

        out.append(line)
        
    with open("D:\\rawrxd\\gpu_dma_clean.asm", "w") as f:
        f.writelines(out)

fix_asm()

vcvars = '"C:\\Program Files (x86)\\Microsoft Visual Studio\\2022\\BuildTools\\VC\\Auxiliary\\Build\\vcvars64.bat"'
cmd = f'{vcvars} && ml64.exe /c /Fo D:\\rawrxd\\gpu_dma.obj D:\\rawrxd\\gpu_dma_clean.asm > D:\\rawrxd\\ml64_log.txt 2>&1'
subprocess.call(["cmd.exe", "/c", cmd])
