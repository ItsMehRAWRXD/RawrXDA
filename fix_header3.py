"""
Comprehensive fix for Win32IDE.h corruption.
The corruption pattern is: lines have been partially overwritten with content from 
adjacent or nearby lines, resulting in:
1. Garbled text after semicolons (tail of overlapping line)
2. Duplicate declarations (correct line follows corrupted one)
3. Fragment-only lines (just a piece of text without valid syntax)
"""
import re

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "r", encoding="utf-8") as f:
    lines = f.readlines()

# First: build the "clean" version of lines 1180-end
# We know lines before 1180 are clean
CORRUPT_START = 1180  # 0-indexed (line 1181 in file)

clean_before = lines[:CORRUPT_START]
corrupt_section = [l.rstrip("\n").rstrip("\r") for l in lines[CORRUPT_START:]]

# The clean replacement for lines 1181+ based on the valid declarations we can see
# I'll reconstruct the correct version by:
# 1. For each line with garbage after ;, keep only up to the first valid ;
# 2. Remove pure garbage lines
# 3. Remove exact duplicates that follow a corruption-fixed line

fixed_lines = []
skip_next = False

for i, line in enumerate(corrupt_section):
    if skip_next:
        skip_next = False
        continue
    
    stripped = line.strip()
    
    # Skip pure fragment lines (no valid C++ syntax)
    if stripped in ["pection", "ate", "items", "", "========", " (additional)"]:
        continue
    
    # Skip lines that are just closing fragments
    if re.match(r'^\s*\);?\s*$', stripped) and not re.match(r'^\s*\}\s*;?\s*$', stripped):
        # This is likely a stray ");" but keep "};" patterns
        if stripped not in ["};", "};"]:
            continue
    
    # Handle the specific corruption patterns:
    
    # Pattern 1: "valid_decl;garbage_text"
    # e.g., "    HWND m_hwndPanelSplitTerminalBtn;Btn;"
    # Fix: remove everything after the first valid semicolon
    m = re.match(r'^(\s+)((?:HWND|bool|int|float|double|size_t|std::string|std::vector|std::unique_ptr|std::map|std::unordered_map|HIMAGELIST|PanelTab|StatusBarInfo|HFONT|HBRUSH|COLORREF|std::atomic|std::mutex|std::thread|std::function|HANDLE)\s+\w+(?:\s*=\s*[^;]+)?)\s*;(.+)$', line)
    if m:
        indent = m.group(1)
        decl = m.group(2).strip()
        garbage = m.group(3).strip()
        fixed_line = f"{indent}{decl};"
        fixed_lines.append(fixed_line)
        # Check if next line is a duplicate of this declaration (the "clean" copy)
        if i + 1 < len(corrupt_section):
            next_stripped = corrupt_section[i + 1].strip()
            if next_stripped.rstrip(";") == decl.rstrip(";") or next_stripped == f"{decl};":
                skip_next = True
        continue
    
    # Pattern 2: "void funcname(...);garbage"
    m2 = re.match(r'^(\s+)((?:void|bool|int|float|double|size_t|std::string|HWND|LRESULT|HTREEITEM|static)\s+\w+\([^)]*\)(?:\s*const)?)\s*;(.+)$', line)
    if m2:
        indent = m2.group(1)
        decl = m2.group(2).strip()
        garbage = m2.group(3).strip()
        # Check if garbage starts with a known type (two declarations on one line)
        type_start = re.match(r'^(void|bool|int|float|double|size_t|std::string|HWND|const|static)\s', garbage)
        if type_start:
            # Two declarations merged - keep both
            fixed_lines.append(f"{indent}{decl};")
            fixed_lines.append(f"{indent}{garbage}")
            # Skip next if it's a duplicate
            if i + 1 < len(corrupt_section):
                next_stripped = corrupt_section[i + 1].strip()
                if next_stripped.startswith(garbage.split("(")[0].strip().split()[-1] if "(" in garbage else ""):
                    skip_next = True
            continue
        else:
            # Garbage after semicolon - just keep the valid part
            fixed_lines.append(f"{indent}{decl};")
            # Skip next if duplicate
            if i + 1 < len(corrupt_section):
                next_stripped = corrupt_section[i + 1].strip()
                if next_stripped.rstrip(";") == decl.rstrip(";") or next_stripped == f"{decl};":
                    skip_next = True
            continue
    
    # Pattern 3: "struct Name {garbage"  
    m3 = re.match(r'^(\s+)(struct\s+\w+\s*\{)(.+)$', line)
    if m3:
        indent = m3.group(1)
        struct_start = m3.group(2)
        rest = m3.group(3).strip()
        # Check if rest is garbage or valid field
        if not re.match(r'^\s*(?:std::string|int|bool|float|double|void|HWND)', rest):
            fixed_lines.append(f"{indent}{struct_start}")
        else:
            fixed_lines.append(line)
        continue
    
    # Pattern 4: "};garbage" 
    m4 = re.match(r'^(\s*\}\s*;)(.+)$', line)
    if m4:
        close = m4.group(1)
        rest = m4.group(2).strip()
        fixed_lines.append(close)
        # If rest starts with a type, it might be a real declaration
        if re.match(r'^(?:std::vector|std::string|HWND|int|bool)', rest):
            fixed_lines.append(f"    {rest}")
        # Skip next if duplicate
        if i + 1 < len(corrupt_section):
            next_stripped = corrupt_section[i + 1].strip()
            if next_stripped.startswith("std::vector") or next_stripped.startswith(close.strip()):
                skip_next = True
        continue
    
    # Pattern 5: "// Comment textgarbage" (comment with garbage appended)
    m5 = re.match(r'^(\s+//\s+\S+(?:\s+\S+)*?(?:ment|tion|ems|face|fig|Bar|tab|ols|ler|te|nel|ger|ion|ack|iew|ole|ods|cks|bar|ngs|nel|ger))\s*(.*)$', line)
    if m5:
        # This is tricky - some comments have garbled endings
        # Just keep the line as-is if it starts with //
        pass
    
    # Pattern 6: enum with garbled values
    m6 = re.match(r'^(\s+)(enum\s+class\s+\w+\s*\{)(.+)$', line)
    if m6:
        indent = m6.group(1)
        enum_start = m6.group(2)
        rest = m6.group(3).strip()
        # Check if rest contains valid enum values
        if not re.match(r'^\s*\w+\s*=\s*\d+', rest):
            fixed_lines.append(f"{indent}{enum_start}")
        else:
            fixed_lines.append(line)
        continue
    
    # Pattern 7: "  message_field;  int severity_field;  // comment"
    # Two struct fields on one line
    m7 = re.match(r'^(\s+)(std::string\s+\w+)\s*;\s*(int\s+\w+\s*;.*)$', line)
    if m7:
        indent = m7.group(1)
        field1 = m7.group(2).strip()
        field2 = m7.group(3).strip()
        fixed_lines.append(f"{indent}{field1};")
        fixed_lines.append(f"{indent}{field2}")
        # Skip duplicate next line
        if i + 1 < len(corrupt_section):
            next_stripped = corrupt_section[i + 1].strip()
            if next_stripped.startswith("int severity") or next_stripped.startswith(field2.split(";")[0].strip()):
                skip_next = True
        continue
    
    # Pattern 8: Lines with garbled comment starts
    # "    // Debugger Callbackst line);" -> "    // Debugger Callbacks"
    m8 = re.match(r'^(\s+//\s+(?:Panel|Debugger|Helper|Watch|Variable|Problems|Enhanced|File|Model|Ollama)\s+\w*?)(?:st\b|ng\b|er\b|ab\b|ol\b)', line)
    if m8 and ";" not in m8.group(0):
        # It's a garbled section comment, try to fix
        pass  # Let it through as-is for now
    
    # Pattern 9: "    bool m_copilotActive;  int ..." - struct field overlap
    m9 = re.match(r'^(\s+)(bool\s+\w+)\s*;\s*(int\s+\w+\s*;.*)$', line)
    if m9:
        indent = m9.group(1)
        f1 = m9.group(2).strip()
        f2 = m9.group(3).strip()
        fixed_lines.append(f"{indent}{f1};")
        fixed_lines.append(f"{indent}{f2}")
        if i + 1 < len(corrupt_section):
            next_stripped = corrupt_section[i + 1].strip()
            if next_stripped.startswith("int copilot") or next_stripped.startswith(f2.split(";")[0].strip()):
                skip_next = True
        continue
    
    # Pattern 10: "    std::string m_ollamaBaseUrl;      // ...    std::string m_ollamaModel..."
    m10 = re.match(r'^(\s+)(std::string\s+\w+\s*;[^"]*?)(std::string\s+\w+.*)$', line)
    if m10:
        indent = m10.group(1)
        f1 = m10.group(2).strip()
        f2 = m10.group(3).strip()
        fixed_lines.append(f"{indent}{f1}")
        fixed_lines.append(f"{indent}{f2}")
        if i + 1 < len(corrupt_section):
            next_stripped = corrupt_section[i + 1].strip()
            if f2.split(";")[0].strip() in next_stripped or next_stripped.startswith("std::string m_ollamaModel"):
                skip_next = True
        continue
    
    # Default: keep the line
    fixed_lines.append(line)

# Now do a second pass to remove remaining duplicates
final_lines = []
for i, line in enumerate(fixed_lines):
    stripped = line.strip()
    # Check if this exact line (ignoring whitespace) appeared in the previous line
    if i > 0 and stripped and stripped == fixed_lines[i-1].strip():
        continue
    final_lines.append(line)

# Also fix the specific garbled comment/enum issues we know about:
output = []
for line in final_lines:
    # Fix: "erminal, Output, Problems, Debug Console"
    if line.strip() == "erminal, Output, Problems, Debug Console":
        continue
    # Fix: "// Panel (Bottom) - Terminal, Output, Problems, Debug Consoleab {"
    line = re.sub(r'Debug Consoleab \{', 'Debug Console', line)
    # Fix: "enum class PanelTab {0,"
    line = re.sub(r'PanelTab \{0,', 'PanelTab {', line)
    # Fix: "Problems = 2,  DebugConsole = 3"
    line = re.sub(r'Problems = 2,\s+DebugConsole = 3', 'Problems = 2,', line)
    # Fix: "};iner;" -> "};"
    line = re.sub(r'\};iner;', '};', line)
    # Fix: "int syncAhead;                // commits aheadind;" 
    line = re.sub(r'aheadind;', 'ahead', line)
    # Fix: "int errors;ngs;"
    line = re.sub(r'errors;ngs;', 'errors;', line)
    # Fix: "int column;Width;"
    line = re.sub(r'column;Width;', 'column;', line)
    # Fix: "std::string eolSequence;      // e.g., \"LF\" or \"CRLF\"eMode;"
    line = re.sub(r'"CRLF"eMode;', '"CRLF"', line)
    # Fix: "bool m_chatMode;" duplicate detection
    # Fix: "// Model Chat statebool"
    line = re.sub(r'statebool', 'state', line)
    # Fix: "// Model Chat InterfaceeToModel"
    line = re.sub(r'InterfaceeToModel', 'Interface', line)
    # Fix: "t+B" line
    if line.strip() == "t+B":
        continue
    # Fix "// Debugger Callbackst line);"
    line = re.sub(r'Callbackst line\);\s*$', 'Callbacks', line)
    # Fix "// Helper Methodstd::string"
    line = re.sub(r'Methodstd::string', 'Methods', line)
    # Fix: "// File Explorer);"
    line = re.sub(r'Explorer\);', 'Explorer', line)
    
    output.append(line)

# Write result
result = "".join(l + "\n" if not l.endswith("\n") else l for l in clean_before)
result += "\n".join(output) + "\n"

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "w", encoding="utf-8") as f:
    f.write(result)

print(f"Lines: {len(lines)} -> {len(clean_before) + len(output)}")
print(f"Removed {len(lines) - len(clean_before) - len(output)} lines")
