import re, os, glob

files = glob.glob(r'd:\rawrxd\src\cli\*.cpp') + glob.glob(r'd:\rawrxd\src\cli\*.h') + [
    r'd:\rawrxd\src\cli_shell.cpp',
    r'd:\rawrxd\src\cli_streaming_enhancements.cpp',
    r'd:\rawrxd\src\rawrxd_cli.cpp',
    r'd:\rawrxd\src\main_old_cli.cpp',
    r'd:\rawrxd\src\model_router_cli_test.cpp',
    r'd:\rawrxd\tools\cli_main.cpp',
    r'd:\rawrxd\Ship\src\cli_main.cpp',
    r'd:\rawrxd\Ship\tools\cli_main.cpp',
    r'd:\rawrxd\Ship\RawrXD_CLI_Standalone.cpp',
]

# Pattern 1: Functions with empty bodies (not constructors with initializer lists)
pattern_empty = re.compile(
    r'((?:void|bool|int|std::\w+|auto|float|double|size_t|uint\d+_t|PatchResult|CorrectionResult)\s+\w[\w:]*\s*\([^)]*\))\s*\{\s*\}',
    re.MULTILINE
)

# Pattern 2: Functions where body is just a single return
pattern_return_only = re.compile(
    r'((?:void|bool|int|std::\w+|auto|float|double|size_t|uint\d+_t|PatchResult|CorrectionResult)\s+\w[\w:]*\s*\([^)]*\))\s*\{\s*\n\s*return[^}]*;\s*\n\s*\}',
    re.MULTILINE
)

# Pattern 3: TODO/FIXME/STUB/HACK/XXX in comments
pattern_todo = re.compile(r'.*(//.*(?:TODO|FIXME|STUB|HACK|XXX|not.?implemented|placeholder).*)', re.IGNORECASE)

# Pattern 4: Very short functions (opening brace, 1-2 lines, closing brace)
# We'll do a line-by-line analysis for this

results = []

for f in files:
    if not os.path.exists(f):
        continue
    try:
        content = open(f, 'r', errors='replace').read()
        lines = content.split('\n')
        basename = os.path.basename(f)
        relpath = f.replace('d:\\rawrxd\\', '')
    except:
        continue

    # Check TODO/FIXME/STUB patterns
    for i, line in enumerate(lines, 1):
        m = pattern_todo.match(line)
        if m:
            results.append((f, i, 'TODO/STUB', line.strip()))

    # Check empty function bodies
    for m in pattern_empty.finditer(content):
        line_num = content[:m.start()].count('\n') + 1
        sig = m.group(1).strip()
        results.append((f, line_num, 'EMPTY_BODY', sig))

    # Check return-only functions  
    for m in pattern_return_only.finditer(content):
        line_num = content[:m.start()].count('\n') + 1
        sig = m.group(1).strip()
        body = m.group(0).strip()
        results.append((f, line_num, 'RETURN_ONLY', body[:200]))

    # Line-by-line: find functions where opening { and closing } are within 3 lines
    # and the body is trivially empty or just return
    brace_stack = []
    func_sig_pattern = re.compile(r'^((?:void|bool|int|std::\w+|auto|float|double|size_t|uint\d+_t|PatchResult|CorrectionResult|const\s+\w+)\s+\w[\w:]*\s*\([^{;]*\))\s*\{?\s*$')
    
    i = 0
    while i < len(lines):
        m = func_sig_pattern.match(lines[i].strip())
        if m:
            sig = m.group(1)
            # Find opening brace
            j = i
            found_open = '{' in lines[i]
            if not found_open and i + 1 < len(lines) and '{' in lines[i+1]:
                j = i + 1
                found_open = True
            if found_open:
                # Check if next few lines are just return or empty
                body_lines = []
                k = j + 1
                while k < len(lines) and k < j + 5:
                    stripped = lines[k].strip()
                    if stripped == '}':
                        # Found closing brace
                        if len(body_lines) == 0:
                            results.append((f, i+1, 'TRIVIAL_EMPTY', sig))
                        elif len(body_lines) == 1 and body_lines[0].startswith('return'):
                            results.append((f, i+1, 'TRIVIAL_RETURN', f"{sig} -> {body_lines[0]}"))
                        break
                    elif stripped and not stripped.startswith('//'):
                        body_lines.append(stripped)
                    elif stripped.startswith('//') and any(kw in stripped.upper() for kw in ['TODO', 'FIXME', 'STUB', 'HACK', 'XXX']):
                        body_lines.append(stripped)
                        results.append((f, k+1, 'COMMENT_STUB', f"{sig} has: {stripped}"))
                    k += 1
        i += 1

# Deduplicate and print
seen = set()
print(f"Found {len(results)} potential stubs across {len(files)} files\n")

for filepath, line, category, detail in sorted(results, key=lambda x: (x[0], x[1])):
    key = (filepath, line)
    if key in seen:
        continue
    seen.add(key)
    relpath = filepath.replace('d:\\rawrxd\\', '')
    print(f"[{category}] {relpath}:L{line}")
    print(f"    {detail[:200]}")
    print()
