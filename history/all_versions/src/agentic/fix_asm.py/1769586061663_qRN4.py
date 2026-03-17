import re

# Read the file
with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'r') as f:
    lines = f.readlines()

# Fix function:
# 1. Split multi-register push/pop
# 2. Fix "align 64" in DATA section
# 3. Fix large immediates
# 4. Fix invalid expressions

fixed_lines = []
for i, line in enumerate(lines):
    # Fix OPTION ALIGN:64 -> OPTION CASEMAP:NONE
    if 'OPTION ALIGN:64' in line:
        line = line.replace('OPTION ALIGN:64', 'OPTION CASEMAP:NONE')
    
    # Fix "align 64" directives in DATA section - remove them
    if re.match(r'^\s*align\s+64\s*$', line):
        continue  # Skip this line
    
    # Fix multi-register push (push r1 r2 r3 -> push r1; push r2; push r3)
    if re.match(r'^\s*push\s+\w+\s+\w+', line):
        indent = re.match(r'^(\s*)', line).group(1)
        regs = re.findall(r'push\s+([\w\s]+)', line)[0].split()
        for reg in regs:
            fixed_lines.append(f'{indent}push {reg}\n')
        continue
    
    # Fix multi-register pop (pop r1 r2 r3 -> pop r3; pop r2; pop r1 - reversed)
    if re.match(r'^\s*pop\s+\w+\s+\w+', line):
        indent = re.match(r'^(\s*)', line).group(1)
        regs = re.findall(r'pop\s+([\w\s]+)', line)[0].split()
        for reg in reversed(regs):
            fixed_lines.append(f'{indent}pop {reg}\n')
        continue
    
    fixed_lines.append(line)

# Write back
with open(r'D:\rawrxd\src\agentic\RawrXD_Complete_Explicit.asm', 'w') as f:
    f.writelines(fixed_lines)

print("✓ Fixed all multi-register push/pop and align directives")
