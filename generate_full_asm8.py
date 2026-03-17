import re

with open("D:/rawrxd/gpu_dma_production_final_target7.asm", "r") as f:
    text = f.read()

text = re.sub(r"(?i)\bPROC\s+FRAME.*", "PROC", text)
text = re.sub(r"^\s*\.(?:ENDPROLOG|ALLOCSTACK|PUSHREG|SAVEREG).*$", "", text, flags=re.MULTILINE)

with open("D:/rawrxd/gpu_dma_production_final_target8.asm", "w") as f:
    f.write(text)
