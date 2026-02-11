#!/usr/bin/env python3
import re

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'r') as f:
    lines = f.readlines()

output = []
i = 0
while i < len(lines):
    line = lines[i]
    
    # Fix shr/shl/sar/sal with immediate > 31
    # Pattern: shr reg, 32 -> mov ecx, 32; shr reg, cl
    if re.search(r'\b(shr|shl|sar|sal)\s+(\w+),\s*32\b', line):
        match = re.match(r'(\s*)(shr|shl|sar|sal)\s+(\w+),\s*32', line)
        if match:
            indent, op, reg = match.groups()
            output.append(f'{indent}mov ecx, 32\n')
            output.append(f'{indent}{op} {reg}, cl\n')
            i += 1
            continue
    
    output.append(line)
    i += 1

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'w') as f:
    f.writelines(output)

print("✓ Fixed shr/shl immediate shift values")
