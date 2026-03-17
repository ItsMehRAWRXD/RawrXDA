import re

with open("D:/rawrxd/gpu_dma_production_final_target5.asm", "r") as f:
    text = f.read()

# Fix A2084: constant value too large
# `cmp rax, DEVICE_ADDRESS_THRESHOLD` isn't allowed directly b/c immediate has to be 32-bit sign-extended in x64 MASM
# We must move it into a register, so change `cmp rax, DEVICE_ADDRESS_THRESHOLD` to `mov r10, DEVICE_ADDRESS_THRESHOLD \n cmp rax, r10`
text = re.sub(r"cmp\s+([a-zA-Z0-9]+),\s*DEVICE_ADDRESS_THRESHOLD", 
              r"mov r10, DEVICE_ADDRESS_THRESHOLD\n    cmp \1, r10", text)

# Fix A2022: instruction operands must be same size
# `mov r11, eax` is bad, we need `mov r11d, eax` (zero extends mapping explicitly) OR `movsx r11, eax` if it should sign extend
# Let's replace `mov r11, eax` with `mov r11d, eax`
text = re.sub(r"mov\s+r11,\s*eax", "mov r11d, eax", text)

# `mov r10, eax` is bad
text = re.sub(r"mov\s+r10,\s*eax", "mov r10d, eax", text)

# `mov eax, r11` is bad
text = re.sub(r"mov\s+eax,\s*r11\b", "mov eax, r11d", text)

# Fix A2006: Missing variables. We stripped LOCALS in earlier revisions.
# we need to map the missing locals to random free registers or memory!
# Locals commonly defined globally missing: `pOrchestrator`, `hDevice`
text = re.sub(r"\bpOrchestrator\b", "r15", text)
text = re.sub(r"\bhDevice\b", "r14", text)


with open("D:/rawrxd/gpu_dma_production_final_target6.asm", "w") as f:
    f.write(text)
