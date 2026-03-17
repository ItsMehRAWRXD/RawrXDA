import re

with open("D:/rawrxd/gpu_dma_production_final_target8.asm", "r") as f:
    text = f.read()

text = re.sub(r"(?i)^([a-zA-Z0-9_]+)\s+PROC.*$", r"\1 PROC FRAME\n    .ENDPROLOG", text, flags=re.MULTILINE)

with open("D:/rawrxd/gpu_dma_production_final_target9.asm", "w") as f:
    f.write(text)
