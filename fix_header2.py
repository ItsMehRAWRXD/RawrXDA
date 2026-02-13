"""Fix remaining corrupted declarations in Win32IDE.h - pass 2"""
import re

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "r", encoding="utf-8") as f:
    lines = f.readlines()

new_lines = []
for i, line in enumerate(lines):
    stripped = line.rstrip("\n").rstrip("\r")
    
    # Pattern: valid declaration ending with );  followed by garbage text
    # e.g., "void createActivityBarUI(HWND hwndParent);wndParent);"
    # The garbage is tail characters from an overlapping line
    m = re.match(r'^(\s+)((?:void|bool|int|HWND|std::string|static|LRESULT|float|double|size_t|HTREEITEM|std::unique_ptr)\s+\w+\([^)]*\)\s*(?:const)?\s*);(.+)$', stripped)
    if m:
        indent = m.group(1)
        valid = m.group(2).strip()
        garbage = m.group(3).strip()
        # Check if garbage looks like truncated text (not a valid declaration start)
        if not garbage.startswith("void ") and not garbage.startswith("bool ") and not garbage.startswith("int ") and not garbage.startswith("std::"):
            new_lines.append(indent + valid + ";\n")
            continue
    
    # Pattern: ";and(..." — garbled function call fragment
    m2 = re.match(r'^(\s+)(.*?);(and|em|ol|r|st|t|nt|e)\b(.*)', stripped)
    if m2 and not stripped.strip().startswith("//") and not stripped.strip().startswith("#"):
        indent = m2.group(1)
        first = m2.group(2).strip()
        if first.endswith(")"):
            new_lines.append(indent + first + ";\n")
            continue
    
    new_lines.append(line)

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "w", encoding="utf-8") as f:
    f.writelines(new_lines)

print(f"Pass 2 done. Lines: {len(lines)} -> {len(new_lines)}")
