#!/usr/bin/env python3
import re

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'r') as f:
    lines = f.readlines()

output = []
i = 0
while i < len(lines):
    line = lines[i]
    
    # Fix large constants that can't fit in immediate operands
    # Pattern: cmp/mov regX, <large constant>
    if re.search(r'\b(cmp|mov|lea|add|sub)\s+(\w+),\s*([0-9]+)(?!h)', line):
        match = re.search(r'\b(cmp|mov|lea|add|sub)\s+(\w+),\s*([0-9]+)(?!h)', line)
        if match:
            op, reg, const = match.groups()
            const_val = int(const)
            # If it's a large constant (doesn't fit in 32-bit signed)
            if const_val > 2147483647:
                indent = len(line) - len(line.lstrip())
                if op == 'cmp':
                    # cmp X, largeconst -> mov r10, largeconst; cmp X, r10
                    output.append(' ' * indent + f'mov r10, {const_val}\n')
                    output.append(' ' * indent + f'{op} {reg}, r10\n')
                    i += 1
                    continue
                elif op == 'mov':
                    # Already fine as mov reg, largeconst works
                    pass
    
    # Fix multi-register push/pop (if still present after previous fixes)
    if re.match(r'\s*(push|pop)\s+\w+\s+\w+', line):
        indent = len(line) - len(line.lstrip())
        parts = line.split()
        if parts[0] in ['push', 'pop']:
            op = parts[0]
            regs = parts[1:]
            for reg in regs:
                output.append(' ' * indent + f'{op} {reg}\n')
            i += 1
            continue
    
    output.append(line)
    i += 1

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'w') as f:
    f.writelines(output)

print("✓ Fixed large constants and multi-register ops")
