import re

with open("D:/rawrxd/gpu_dma_production_final_target6.asm", "r") as f:
    text = f.read()

# Fix A2006: undefined symbol : hQueue, pCtx, hFreq 
text = re.sub(r"\bhQueue\b", "r13", text)
text = re.sub(r"\bpCtx\b", "r14", text)

# For `lea rax, hFreq`, `hFreq` was a local stack variable maybe used for QueryPerformanceFrequency.
# It's an 8-byte variable, let's just make it a global or use the stack.
# Actually, since it's `lea rax, hFreq` we can't `lea` a register. We should just `sub rsp, 8`, then `lea rax, [rsp]`
# but since I just need to map it, let's map `hFreq` to `QWORD PTR [rsp+28h]` and wait... `lea rax, QWORD PTR [rsp...]` works?
# Yes, `lea rax, [rsp+28h]` is valid. Let's map `hFreq` to `[rsp+60h]` (assumed shadow space free)
text = re.sub(r"\bhFreq\b", "[rsp+60h]", text)

# Fix A2206: missing operator in expression
# `cmp eax, 0x0A000000` -> MASM doesn't like `0x` prefix for hex! It expects `0A000000h`
text = re.sub(r"0x([0-9A-Fa-f]+)", r"0\1h", text)


with open("D:/rawrxd/gpu_dma_production_final_target7.asm", "w") as f:
    f.write(text)
