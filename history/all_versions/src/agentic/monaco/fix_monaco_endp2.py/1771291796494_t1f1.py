#!/usr/bin/env python3
"""
Robust fix for shifted-ENDP structural defects in MONACO_EDITOR_ENTERPRISE.ASM.

Strategy:
1. Restore from backup first
2. Parse ALL PROC/ENDP pairs
3. For each "WrongName ENDP" -> "CorrectName PROC FRAME" (empty body) pattern, 
   fix the ENDP name and remove the phantom
4. Handle the case where error-handling code sits between ENDP and phantom PROC
5. Fix the "END PROC FRAME" malformed directive
"""

import re

def fix_monaco():
    with open('MONACO_EDITOR_ENTERPRISE.ASM.bak', 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    proc_re = re.compile(r'^(\w+)\s+PROC\b', re.IGNORECASE)
    endp_re = re.compile(r'^(\w+)\s+ENDP\b', re.IGNORECASE)
    
    # Build a map: for each line, what is its PROC/ENDP status
    line_info = []
    for idx, line in enumerate(lines):
        stripped = line.strip()
        pm = proc_re.match(stripped)
        em = endp_re.match(stripped)
        info = {'line': line, 'idx': idx, 'proc': None, 'endp': None, 'remove': False}
        if pm and pm.group(1).upper() != 'END':
            info['proc'] = pm.group(1)
        if em:
            info['endp'] = em.group(1)
        line_info.append(info)
    
    # Identify unique PROCs by finding the FIRST occurrence of each name
    # A "phantom" is a PROC that's immediately followed by another PROC with no real body between
    proc_first_occurrence = {}
    phantoms = set()
    
    for i, info in enumerate(line_info):
        if info['proc']:
            name = info['proc']
            if name not in proc_first_occurrence:
                proc_first_occurrence[name] = i
            else:
                # This is a duplicate PROC declaration
                # Check if it has a real body (more than just empty lines before next PROC/ENDP)
                has_body = False
                for j in range(i+1, min(i+5, len(line_info))):
                    s = line_info[j]['line'].strip()
                    if s and not line_info[j]['proc'] and not line_info[j]['endp']:
                        # There's a non-empty, non-PROC, non-ENDP line
                        if not s.startswith('.endprolog') and not s.startswith('.pushreg') and not s.startswith('.allocstack') and not s.startswith('sub rsp'):
                            has_body = True
                            break
                    if line_info[j]['proc'] or line_info[j]['endp']:
                        break
                
                if not has_body:
                    phantoms.add(i)
                    info['remove'] = True
    
    # Now fix ENDPs: for each ENDP, check if its name matches the current open PROC
    # Track the open PROC stack
    open_proc = None
    fixes = 0
    
    for i, info in enumerate(line_info):
        if info['remove']:
            continue
        if info['proc']:
            open_proc = info['proc']
        if info['endp']:
            endp_name = info['endp']
            if open_proc and endp_name != open_proc:
                # Mismatched ENDP — fix it
                old_line = info['line']
                new_line = old_line.replace(endp_name + ' ENDP', open_proc + ' ENDP', 1)
                if new_line == old_line:
                    # Try case-insensitive
                    new_line = re.sub(
                        re.escape(endp_name) + r'\s+ENDP',
                        open_proc + ' ENDP',
                        old_line, count=1, flags=re.IGNORECASE
                    )
                info['line'] = new_line
                fixes += 1
            open_proc = None  # ENDP closes the current PROC
    
    # Fix "END PROC FRAME" on last line
    for i in range(len(line_info)-1, -1, -1):
        if line_info[i]['line'].strip().upper() == 'END PROC FRAME':
            line_info[i]['line'] = 'END\n'
            break
    
    # Write output
    output = [info['line'] for info in line_info if not info['remove']]
    
    with open('MONACO_EDITOR_ENTERPRISE.ASM', 'w', encoding='utf-8', newline='\n') as f:
        f.writelines(output)
    
    print(f"Input:  {len(lines)} lines")
    print(f"Output: {len(output)} lines")
    print(f"Removed {len(phantoms)} phantom PROC re-declarations")
    print(f"Fixed {fixes} mismatched ENDPs")

if __name__ == '__main__':
    fix_monaco()
