import re

with open("D:/rawrxd/gpu_dma_production_final_target4.asm", "r") as f:
    text = f.read()

# Fix A2084: constant value too large
# In x64 `and reg, 0FFFFFFFFFFFFFFF0h` requires sign extension for immediate -> 32-bit.
# For truly large unsigned constants we need an intermediary register
text = re.sub(r"(and|or|xor)\s+([a-zA-Z0-9]+),\s*0?[Ff]{8,15}([0-9A-Fa-f]+)h", 
              r"mov r11, 0FFFFFFFFFFFFFF\3h\n    \1 \2, r11", text)

# Map LOCALs inside DMA_Engine_CopyData
# If `pSource`, `pDest`, `size`, `copyType` were defined as arguments or locals, we need to bind them.
# The original arguments to DMA_Engine_CopyData (rcx = pContext, rdx = pSource, r8 = pDest, r9 = size)
# and probably stack for copyType at [rsp+40h]

text = re.sub(r"\bpSource\b", "rdx", text)
text = re.sub(r"\bpDest\b", "r8", text)
text = re.sub(r"\bcopyType\b", "DWORD PTR [rsp+40h]", text) # Assuming it's the 5th param
text = re.sub(r"\bsrcIsDevice\b", "r12d", text) # Arbitrary volatile binds
text = re.sub(r"\bdstIsDevice\b", "r13d", text)

# For A2022 instruction operands must be same size on line 388, 473
text = re.sub(r"cmp\s+DWORD PTR \[([^\]]+)\],\s*CL", r"cmp byte ptr [\1], cl", text) 
text = re.sub(r"mov\s+DWORD PTR \[([^\]]+)\],\s*CL", r"mov byte ptr [\1], cl", text) 
text = re.sub(r"movzx\s+eax,\s*DWORD PTR \[([^\]]+)\]", r"movzx eax, byte ptr [\1]", text)

with open("D:/rawrxd/gpu_dma_production_final_target5.asm", "w") as f:
    f.write(text)
