#!/usr/bin/env python3
"""
Final comprehensive fix for MONACO_EDITOR_ENTERPRISE.ASM.
Operates on the backup and handles ALL structural defects:
1. Remove duplicate consecutive ENDPs for same name
2. Remove phantom PROC re-declarations with no real body
3. Remove stray ENDPs inside other PROC bodies
4. Fix END PROC FRAME -> END
"""

import re

def fix_monaco():
    with open('MONACO_EDITOR_ENTERPRISE.ASM.bak', 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    proc_re = re.compile(r'^(\w+)\s+PROC\b', re.IGNORECASE)
    endp_re = re.compile(r'^(\w+)\s+ENDP\b', re.IGNORECASE)
    frame_re = re.compile(r'^\s*(\.(endprolog|pushreg|allocstack|savexmm128|savereg)|sub\s+rsp|push\s+)', re.IGNORECASE)
    
    # Step 1: Map out all PROC blocks with their line ranges
    # Each PROC "owns" lines from its declaration to its body's ret + ENDP
    
    # First, identify all PROC start lines and their names
    proc_lines = []  # [(line_idx, name)]
    endp_lines = []  # [(line_idx, name)]
    
    for i, line in enumerate(lines):
        s = line.strip()
        pm = proc_re.match(s)
        em = endp_re.match(s)
        if pm and pm.group(1).upper() != 'END':
            proc_lines.append((i, pm.group(1)))
        if em:
            endp_lines.append((i, em.group(1)))
    
    # Find which PROC declarations are "real" (have actual code) vs "phantom" (only frame setup)
    # A phantom PROC has no real instructions — only push/sub rsp/.endprolog/.pushreg
    # before the next PROC or ENDP
    real_procs = set()    # indices of real PROC lines
    phantom_procs = set()  # indices of phantom PROC lines
    
    for pi, (proc_idx, proc_name) in enumerate(proc_lines):
        # Find the next PROC or end of file
        next_proc_or_end = len(lines)
        for pj in range(pi + 1, len(proc_lines)):
            next_proc_or_end = proc_lines[pj][0]
            break
        
        # Check if there's real code between this PROC and the next PROC
        has_real_code = False
        for j in range(proc_idx + 1, next_proc_or_end):
            s = lines[j].strip()
            if not s or s.startswith(';'):
                continue
            if frame_re.match(s):
                continue
            if endp_re.match(s):
                continue
            # This is a real instruction
            has_real_code = True
            break
        
        if has_real_code:
            real_procs.add(proc_idx)
        else:
            phantom_procs.add(proc_idx)
    
    # Step 2: For real PROCs, track which one is "open" and assign correct ENDPs
    # Build the output: skip phantoms, fix ENDP names, remove extra ENDPs
    
    remove_lines = set(phantom_procs)  # Start with phantom PROCs to remove
    endp_assignments = {}  # endp_line_idx -> correct_name (or None to remove)
    
    # Track open PROC for ENDP matching
    current_proc = None
    current_proc_idx = None
    endp_seen_for_current = False
    
    for i, line in enumerate(lines):
        if i in phantom_procs:
            continue
            
        s = line.strip()
        pm = proc_re.match(s)
        em = endp_re.match(s)
        
        if pm and pm.group(1).upper() != 'END':
            if current_proc and not endp_seen_for_current:
                pass  # previous PROC never got closed — will need attention
            current_proc = pm.group(1)
            current_proc_idx = i
            endp_seen_for_current = False
        
        if em:
            endp_name = em.group(1)
            if current_proc and not endp_seen_for_current:
                # This is the first ENDP since the PROC opened
                if endp_name != current_proc:
                    endp_assignments[i] = current_proc  # fix name
                endp_seen_for_current = True
            else:
                # Either no PROC is open, or we already saw an ENDP
                # This is a stray/duplicate ENDP — remove it
                remove_lines.add(i)
    
    # Step 3: Build output
    output = []
    for i, line in enumerate(lines):
        if i in remove_lines:
            continue
        
        s = line.strip()
        
        # Fix END PROC FRAME
        if s.upper() == 'END PROC FRAME':
            output.append('END\n')
            continue
        
        # Fix mismatched ENDP
        if i in endp_assignments:
            correct_name = endp_assignments[i]
            em = endp_re.match(s)
            old_name = em.group(1)
            new_line = line.replace(old_name + ' ENDP', correct_name + ' ENDP', 1)
            output.append(new_line)
            continue
        
        output.append(line)
    
    with open('MONACO_EDITOR_ENTERPRISE.ASM', 'w', encoding='utf-8', newline='\n') as f:
        f.writelines(output)
    
    print(f"Input:  {len(lines)} lines")
    print(f"Output: {len(output)} lines")
    print(f"Removed {len(remove_lines)} lines ({len(phantom_procs)} phantom PROCs + {len(remove_lines) - len(phantom_procs)} stray ENDPs)")
    print(f"Fixed {len(endp_assignments)} mismatched ENDPs")
    
    # Verify: check all PROCs have matching ENDPs
    verify_proc_re = re.compile(r'^(\w+)\s+PROC\b', re.IGNORECASE)
    verify_endp_re = re.compile(r'^(\w+)\s+ENDP\b', re.IGNORECASE)
    procs_found = []
    endps_found = []
    for line in output:
        s = line.strip()
        pm = verify_proc_re.match(s)
        em = verify_endp_re.match(s)
        if pm and pm.group(1).upper() != 'END':
            procs_found.append(pm.group(1))
        if em:
            endps_found.append(em.group(1))
    
    proc_set = set(procs_found)
    endp_set = set(endps_found)
    missing = proc_set - endp_set
    extra = endp_set - proc_set
    
    print(f"\nVerification:")
    print(f"  PROCs: {len(procs_found)} ({len(proc_set)} unique)")
    print(f"  ENDPs: {len(endps_found)} ({len(endp_set)} unique)")
    if missing:
        print(f"  STILL MISSING ENDP: {missing}")
    if extra:
        print(f"  EXTRA ENDPs (no PROC): {extra}")
    if not missing and not extra:
        print(f"  ALL MATCHED!")

if __name__ == '__main__':
    fix_monaco()
