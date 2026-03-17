#!/usr/bin/env python3
"""
Fix the shifted-ENDP structural defect in MONACO_EDITOR_ENTERPRISE.ASM.

The file has a systematic pattern where:
1. A real PROC body is closed by the WRONG procedure's ENDP
2. A phantom re-declaration of the correct PROC appears immediately after (empty)
3. This repeats for every PROC from line ~1522 onward

Strategy:
- Parse the file line by line
- Track the PROC/ENDP stack
- When we see "WrongName ENDP" followed by "CorrectName PROC FRAME" (empty),
  replace the ENDP with "CorrectName ENDP" and remove the phantom PROC re-declaration
- Fix the final "END PROC FRAME" malformed directive
"""

import re

def fix_monaco():
    with open('MONACO_EDITOR_ENTERPRISE.ASM', 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    proc_pattern = re.compile(r'^(\w+)\s+PROC\b', re.IGNORECASE)
    endp_pattern = re.compile(r'^(\w+)\s+ENDP\b', re.IGNORECASE)
    
    # First pass: identify the current PROC we're inside
    # and find all shifted ENDPs
    proc_stack = []  # stack of (name, line_number)
    output_lines = []
    skip_next_empty_proc = None
    i = 0
    
    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        
        # Check for the malformed END directive
        if stripped.upper() == 'END PROC FRAME':
            output_lines.append('END\n')
            i += 1
            continue
        
        proc_match = proc_pattern.match(stripped)
        endp_match = endp_pattern.match(stripped)
        
        if skip_next_empty_proc and proc_match:
            proc_name = proc_match.group(1)
            if proc_name == skip_next_empty_proc:
                # This is the phantom re-declaration — skip it
                skip_next_empty_proc = None
                i += 1
                continue
            else:
                skip_next_empty_proc = None
        
        if endp_match:
            endp_name = endp_match.group(1)
            # Check if next non-empty line is a phantom PROC re-declaration
            j = i + 1
            while j < len(lines) and lines[j].strip() == '':
                j += 1
            if j < len(lines):
                next_proc = proc_pattern.match(lines[j].strip())
                if next_proc:
                    real_name = next_proc.group(1)
                    # Check if the line after the phantom PROC is ANOTHER PROC
                    # (meaning the phantom is empty/has no body)
                    k = j + 1
                    while k < len(lines) and lines[k].strip() == '':
                        k += 1
                    if k < len(lines):
                        next_next_proc = proc_pattern.match(lines[k].strip())
                        if next_next_proc and endp_name != real_name:
                            # Shifted pattern detected!
                            # Replace ENDP with correct name
                            output_lines.append(f'{real_name} ENDP\n')
                            skip_next_empty_proc = real_name
                            i += 1
                            continue
            
            # Also check: if this is a correctly named ENDP followed by
            # error label (like @@jc_fail:) then a WRONG endp, then phantom PROC
            output_lines.append(line)
            i += 1
            continue
        
        if proc_match:
            proc_name = proc_match.group(1)
            proc_stack.append(proc_name)
        
        output_lines.append(line)
        i += 1
    
    with open('MONACO_EDITOR_ENTERPRISE.ASM', 'w', encoding='utf-8', newline='\n') as f:
        f.writelines(output_lines)
    
    print(f"Processed {len(lines)} lines -> {len(output_lines)} lines")
    print(f"Removed {len(lines) - len(output_lines)} phantom re-declaration lines")

if __name__ == '__main__':
    fix_monaco()
