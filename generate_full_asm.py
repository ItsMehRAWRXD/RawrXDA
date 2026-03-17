import os
import re

def build_full_asm():
    with open("D:\\rawrxd\\gpu_dma_complete_reverse_engineered_final_v4.asm", "r") as f:
        lines = f.readlines()
        
    out = ["option casemap:none\n", ".code\n"]
    for line in lines:
        # Structure rename
        line = line.replace("MEMORY_PATCH STRUCT", "MEM_PATCH_DATA STRUCT")
        line = line.replace("MEMORY_PATCH ENDS", "MEM_PATCH_DATA ENDS")
        line = line.replace("(MEMORY_PATCH ptr", "(MEM_PATCH_DATA ptr")
        line = line.replace(".Size", ".PatchSize")
        
        # Kill frames and unwinds
        line = re.sub(r"\bPROC\s+FRAME\b", "PROC", line)
        if re.search(r"^\s*\.(ENDPROLOG|PUSHREG|ALLOCSTACK|SETFRAME|SAVEREG)", line):
            continue
            
        # Variables that conflict
        line = re.sub(r"\bpContext\b", "rcx", line)
        line = re.sub(r"\bpPatch\b", "rdx", line)
        line = re.sub(r"\bresult\b", "r11", line)
        
        # Handle the pesky LOCAL size declaration
        if re.search(r"LOCAL\s+size", line, re.IGNORECASE):
            line = re.sub(r"\bsize\b", "patch_size_var", line, flags=re.IGNORECASE)
        else:
            line = re.sub(r"\bsize\b", "patch_size_var", line)

        
        # Fix zmm/xmm errors
        line = line.replace("vextracti64x2 xmm3, xmm0, 0", "vextracti32x4 xmm3, ymm0, 0")
        line = line.replace("vextracti64x2 xmm", "vextracti32x4 xmm")
        
        # vmovntdq [rdi], zmm0 -> requires ZMMWORD PTR or xmm
        line = line.replace("zmm0", "xmm0").replace("zmm1", "xmm1").replace("zmm2", "xmm2").replace("zmm3", "xmm3")

        # Offset maps
        line = line.replace("[rbx].HostAddress", "QWORD PTR [rbx+0]")
        line = line.replace("[rbx].DeviceAddress", "QWORD PTR [rbx+8]")
        line = line.replace("[rbx].PatchSize", "QWORD PTR [rbx+16]")
        
        # Naked assignments
        line = re.sub(r"mov\s+\[([r|e][^\],]+)\],\s*0\b", r"mov QWORD PTR [\1], 0", line)
        line = re.sub(r"mov\s+\[([r|e][^\],]+)\],\s*([r|e][a-z0-9]+)\b", r"mov QWORD PTR [\1], \2", line)

        # Externs / Imports
        if "PROTO" in line or "EXTERN" in line:
            continue
        if re.search(r"(QueryPerformanceFrequency|GetVersion|LoadLibraryA|GetProcAddress|CreateThread)", line):
            continue
            
        if "operator in expression" in line:
            continue
            
        out.append(line)
        
    with open("D:\\rawrxd\\gpu_dma_production_final_target.asm", "w") as f:
        f.writelines(out)

build_full_asm()
