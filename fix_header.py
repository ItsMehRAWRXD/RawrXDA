"""Fix corrupted duplicate declarations in Win32IDE.h"""
import re

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "r", encoding="utf-8") as f:
    content = f.read()

lines = content.split("\n")
new_lines = []
i = 0
while i < len(lines):
    line = lines[i]
    stripped = line.rstrip("\r")
    
    # Fix double semicolons (e.g., "bool m_searchCaseSensitive;;")
    # But NOT for(;;) patterns
    if ";;" in stripped and "for" not in stripped:
        stripped = stripped.replace(";;", ";")
    
    # Fix lines with two declarations squished together:
    # Pattern: "declaration1;declaration2" where declaration2 starts with "void " or similar
    # e.g., "void parseCodeForOutline();void goToOutlineItem(int index);"
    m = re.match(r'^(\s+)(.*?;)(void \w+|bool \w+|int \w+|std::string \w+|HWND \w+|static \w+|LRESULT \w+)(.*)', stripped)
    if m and not stripped.strip().startswith("//") and not stripped.strip().startswith("#"):
        indent = m.group(1)
        first_part = m.group(2).strip()
        second_start = m.group(3)
        second_rest = m.group(4)
        second_part = (second_start + second_rest).strip()
        
        # Only split if the first part looks like a valid declaration
        if first_part and not first_part.startswith("//"):
            new_lines.append(indent + first_part)
            new_lines.append(indent + second_part)
            # Check if next line is the same as second_part (duplicate)
            if i + 1 < len(lines):
                next_stripped = lines[i+1].rstrip("\r").strip()
                if next_stripped == second_part.strip() or next_stripped == second_part.rstrip(";").strip() + ";":
                    i += 2  # Skip the duplicate
                    continue
            i += 1
            continue
    
    # Fix lines with garbled text after a semicolon:
    # e.g., "void updateActivityBarState();rContent();"  
    # These are lines where text from a different declaration leaked in after the semicolon
    m2 = re.match(r'^(\s+)((?:void|bool|int|HWND|std::string|static|LRESULT|float|double|size_t)\s+\w+\(.*?\);)((?:\w|::|&|\*|\s)+\(.*)', stripped)
    if m2 and not stripped.strip().startswith("//"):
        indent = m2.group(1)
        valid_decl = m2.group(2).strip()
        garbage = m2.group(3).strip()
        # The garbled part is from a missing line - just keep the valid declaration
        new_lines.append(indent + valid_decl)
        # Check if next line has the correct full version
        if i + 1 < len(lines):
            next_stripped = lines[i+1].rstrip("\r").strip()
            # If next line is a duplicate or the full version, skip it
            if next_stripped == valid_decl.strip():
                i += 2
                continue
        i += 1
        continue
    
    # Fix garbled comment lines like "    pection" (from "// Variable & Stack Inspection")
    if stripped.strip() in ["pection", "rContent", "st;", "st"]:
        i += 1
        continue
    
    # Fix: "void onAgentOutput(const char* text);  void postAgentOutputSafe(...)"
    # Already handled by the squished-together pattern above
    
    new_lines.append(stripped)
    i += 1

result = "\n".join(new_lines)
with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "w", encoding="utf-8") as f:
    f.write(result)

print(f"Fixed. Lines: {len(lines)} -> {len(new_lines)}")
