import re

with open("D:/rawrxd/gpu_dma_production_final_target3.asm", "r") as f:
    text = f.read()

# Fix A2070 invalid operands for `movss` (requires explicit DWORD PTR on memory operand)
text = re.sub(r"movss\s+(xmm\d+),\s*\[([^\]]+)\]", r"movss \1, DWORD PTR [\2]", text)
text = re.sub(r"movss\s+\[([^\]]+)\],\s*(xmm\d+)", r"movss DWORD PTR [\1], \2", text)

# Fix A2221 missing frame proc. ML64 expects `.ALLOCSTACK` and `.ENDPROLOG` which we stripped.
# The remaining `.DATA`, `.CODE`, etc are fine but things like `.XMM` might cause issues. 
# Let's target strictly the remaining SEH and pseudo-ops.
text = re.sub(r"^\s*\.(?:SETFRAME|ALLOCSTACK|PUSHREG|SAVEREG).*$", "", text, flags=re.MULTILINE)

# Remove the `.PROC FRAME` entirely if remaining
text = re.sub(r"(?i)PROC\s+FRAME", "PROC", text)

with open("D:/rawrxd/gpu_dma_production_final_target4.asm", "w") as f:
    f.write(text)
