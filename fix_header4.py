"""
Fix ALL corrupted lines in Win32IDE.h.
Pattern: "valid_declaration;garbage_suffix" where garbage is 2-10 chars ending in ;
Also handles member declarations with garbage after the semicolon.
"""
import re

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "r", encoding="utf-8") as f:
    content = f.read()

lines = content.split("\n")
new_lines = []
skip_next = False

for i, line in enumerate(lines):
    if skip_next:
        skip_next = False
        continue
    
    stripped = line.rstrip()
    
    # ===== MEMBER VARIABLE corruption: "type name;garbage;" =====
    # e.g., "    HWND m_hwndSplitter;ragging;"
    # The pattern is: valid member declaration semicolon, then lowercase text, then optional semicolon
    m = re.match(r'^(\s+)((?:HWND|bool|int|float|double|size_t|uint64_t|std::string|std::vector<[^>]+>|std::unique_ptr<[^>]+>|std::map<[^>]+>|std::unordered_map<[^>]+>|std::shared_ptr<[^>]+>|std::function<[^>]+>|std::atomic<[^>]+>|std::mutex|std::thread|HIMAGELIST|HFONT|HBRUSH|COLORREF|HANDLE|PanelTab|StatusBarInfo|InferenceConfig)\s+\w+(?:\s*=\s*[^;]+)?)\s*;([a-zA-Z_][a-zA-Z0-9_]*;?)$', stripped)
    if m:
        indent = m.group(1)
        decl = m.group(2).strip()
        garbage = m.group(3)
        new_lines.append(f"{indent}{decl};")
        # Check if next line is a clean version of a different declaration
        # that the garbage was from
        if i + 1 < len(lines):
            next_stripped = lines[i+1].strip().rstrip()
            # If next line is exact duplicate, skip it
            if next_stripped == f"{decl};":
                skip_next = True
        continue
    
    # ===== FUNCTION DECLARATION corruption: "void func(...);garbage" =====
    m2 = re.match(r'^(\s+)((?:void|bool|int|float|double|size_t|std::string|HWND|LRESULT|HTREEITEM|static\s+\w+)\s+\w+\([^)]*\)(?:\s*const)?)\s*;([a-zA-Z_:][a-zA-Z0-9_:&*\s,]*(?:\([^)]*\))?;?)$', stripped)
    if m2:
        indent = m2.group(1)
        decl = m2.group(2).strip()
        garbage = m2.group(3).strip()
        
        # Check if garbage is actually another valid declaration
        if re.match(r'^(?:void|bool|int|float|std::)', garbage):
            new_lines.append(f"{indent}{decl};")
            new_lines.append(f"{indent}{garbage}")
            # Skip next if duplicate
            if i + 1 < len(lines):
                next_stripped = lines[i+1].strip()
                garb_start = garbage.split("(")[0].split()[-1] if "(" in garbage else garbage.split(";")[0]
                if garb_start in next_stripped or next_stripped == garbage:
                    skip_next = True
        else:
            new_lines.append(f"{indent}{decl};")
            # Skip duplicate next
            if i + 1 < len(lines):
                next_stripped = lines[i+1].strip()
                if next_stripped == f"{decl};":
                    skip_next = True
        continue
    
    # ===== STRUCT FIELD with inline comment corruption =====
    # "std::string message;  int severity;  // 0=Error..." (two fields on one line from struct)
    m3 = re.match(r'^(\s+)(std::string\s+\w+)\s*;\s+(int\s+\w+\s*;\s*//.*)', stripped)
    if m3:
        indent = m3.group(1)
        new_lines.append(f"{indent}{m3.group(2).strip()};")
        new_lines.append(f"{indent}{m3.group(3).strip()}")
        if i + 1 < len(lines) and lines[i+1].strip().startswith("int severity"):
            skip_next = True
        continue
    
    # ===== BOOL + INT field corruption =====
    m4 = re.match(r'^(\s+)(bool\s+\w+)\s*;\s+(int\s+\w+\s*;.*)', stripped)
    if m4:
        indent = m4.group(1)
        new_lines.append(f"{indent}{m4.group(2).strip()};")
        new_lines.append(f"{indent}{m4.group(3).strip()}")
        if i + 1 < len(lines) and "copilotSuggestions" in lines[i+1]:
            skip_next = True
        continue
    
    # ===== Struct definition corruption =====
    # "struct ProblemItem {ng file;"  ->  "struct ProblemItem {"
    m5 = re.match(r'^(\s+)(struct\s+\w+\s*\{)[a-z]+\s+\w+;', stripped)
    if m5:
        new_lines.append(f"{m5.group(1)}{m5.group(2)}")
        continue
    
    # ===== Enum corruption =====
    # "enum class PanelTab {0," -> "enum class PanelTab {"
    if "enum class PanelTab {0," in stripped:
        new_lines.append(stripped.replace("{0,", "{"))
        continue
    
    # ===== Line ending with garbage semicolons =====
    # Double semicolons: "HWND m_foo;;" -> "HWND m_foo;"
    if stripped.endswith(";;") and "for" not in stripped:
        new_lines.append(stripped[:-1])
        continue
    
    # ===== Remove pure garbage/fragment lines =====
    pure_garbage = [
        "erminal, Output, Problems, Debug Console",
        "t+B",
        "items",
        " (additional)",
        "ate",
        "========",
        "pection",
    ]
    if stripped.strip() in pure_garbage:
        continue
    
    # ===== Remove stray ");" lines =====
    if stripped.strip() == ");":
        continue
    
    # Keep the line as-is
    new_lines.append(stripped)

# Second pass: remove exact consecutive duplicates
final = []
for i, line in enumerate(new_lines):
    if i > 0 and line.strip() and line.strip() == new_lines[i-1].strip():
        continue
    final.append(line)

# Third pass: fix remaining known patterns
output = []
for line in final:
    # Fix garbled comments
    line = re.sub(r'Debug Consoleab \{', 'Debug Console', line)
    line = re.sub(r'Callbackst line\);', 'Callbacks', line)
    line = re.sub(r'Methodstd::string', 'Methods', line)
    line = re.sub(r'InterfaceeToModel', 'Interface', line)
    line = re.sub(r'statebool', 'state', line)
    line = re.sub(r'Explorer\);', 'Explorer', line)
    line = re.sub(r'aheadind;', 'ahead', line)
    line = re.sub(r'(int errors);ngs;', r'\1;', line)
    line = re.sub(r'(int column);Width;', r'\1;', line)
    line = re.sub(r'"CRLF"eMode;', '"CRLF"', line)
    line = re.sub(r'Problems = 2,\s+DebugConsole = 3', 'Problems = 2,', line)
    line = re.sub(r'\};iner;', '};', line)
    # Fix: "    // some comment      // e.g...."  where comment text duplicated
    # Fix: two string declarations on one line
    line = re.sub(r'(std::string\s+\w+\s*;\s*//[^"]*"[^"]*")\s+std::string\s+', r'\1\n    std::string ', line)
    output.append(line)

# Flatten any lines that have embedded newlines from the substitution
flat = []
for line in output:
    for part in line.split("\n"):
        flat.append(part)

with open(r"D:\rawrxd\src\win32app\Win32IDE.h", "w", encoding="utf-8") as f:
    f.write("\n".join(flat))

print(f"Original: {len(lines)} lines")
print(f"Fixed: {len(flat)} lines")
print(f"Removed: {len(lines) - len(flat)} lines")
