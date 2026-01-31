#!/usr/bin/env python3
import re

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'r') as f:
    content = f.read()

# Fix 1: Remove all FRAME directives from PROC declarations
content = re.sub(r'(\w+)\s+PROC\s+FRAME', r'\1 PROC', content)

# Fix 2: Fix shr rax, 32 - move 32 to a register first
#lines = content.split('\n')
#for i, line in enumerate(lines):
#    if 'shr rax, 32' in line or 'shr rcx, 32' in line or 'shr rdx, 32' in line:
#        # Find the register being shifted
#        match = re.search(r'shr\s+(r\w+),\s*32', line)
#        if match:
#            reg = match.group(1)
#            indent = len(line) - len(line.lstrip())
#            lines[i] = ' ' * indent + f'mov ecx, 32\n' + ' ' * indent + f'shr {reg}, cl'

# Convert list back to string
#content = '\n'.join(lines)

with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'w') as f:
    f.write(content)

print("✓ Fixed all FRAME directives")
