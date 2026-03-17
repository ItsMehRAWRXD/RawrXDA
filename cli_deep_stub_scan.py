import re, os

files = [
    r"d:\rawrxd\src\cli\agentic_decision_tree.cpp",
    r"d:\rawrxd\src\cli\agentic_decision_tree.h",
    r"d:\rawrxd\src\cli\cli_autonomy_loop.cpp",
    r"d:\rawrxd\src\cli\cli_autonomy_loop.h",
    r"d:\rawrxd\src\cli\cli_headless_systems.cpp",
    r"d:\rawrxd\src\cli\cli_headless_systems.h",
    r"d:\rawrxd\src\cli\rawrxd_cli_compiler.cpp",
    r"d:\rawrxd\src\cli\swarm_orchestrator.cpp",
    r"d:\rawrxd\src\cli\swarm_orchestrator.h",
    r"d:\rawrxd\src\cli_shell.cpp",
    r"d:\rawrxd\src\cli_streaming_enhancements.cpp",
    r"d:\rawrxd\src\rawrxd_cli.cpp",
    r"d:\rawrxd\src\main_old_cli.cpp",
    r"d:\rawrxd\src\model_router_cli_test.cpp",
    r"d:\rawrxd\tools\cli_main.cpp",
    r"d:\rawrxd\Ship\RawrXD_CLI_Standalone.cpp",
    r"d:\rawrxd\Ship\src\cli_main.cpp",
    r"d:\rawrxd\Ship\tools\cli_main.cpp",
]

# Soft stub indicators in comments
soft_stubs = re.compile(r'/[/*].*\b(would|simplified|placeholder|dummy|not yet|no-op|noop|unfinished|incomplete|skeleton|basic.?impl|stubbed|fake|mock|hardcoded|hard.?coded|magic.?number)\b', re.IGNORECASE)

# Hard stub indicators
hard_stubs = re.compile(r'//\s*(TODO|FIXME|STUB|HACK|XXX|NOT.?IMPLEMENTED)\b', re.IGNORECASE)

results = []

for filepath in files:
    if not os.path.exists(filepath):
        continue
    lines = open(filepath, 'r', errors='replace').readlines()
    relpath = filepath.replace('d:\\rawrxd\\', '')
    
    # 1. Hard TODO/FIXME/STUB comments
    for i, line in enumerate(lines):
        m = hard_stubs.search(line)
        if m:
            results.append((relpath, i+1, 'HARD_STUB', line.rstrip()))

    # 2. Soft stub indicators
    for i, line in enumerate(lines):
        m = soft_stubs.search(line)
        if m:
            results.append((relpath, i+1, 'SOFT_STUB', line.rstrip()))
    
    # 3. Empty function bodies - multi-line scan
    # Look for: function signature followed by { } with nothing inside
    i = 0
    while i < len(lines):
        line = lines[i].rstrip()
        # Detect function signature (type + name + params + { or just type + name + params)
        # Then check if body is empty or trivial
        func_pat = re.match(r'^(\s*)((?:static\s+)?(?:virtual\s+)?(?:inline\s+)?(?:void|bool|int|auto|std::\w+|float|double|size_t|uint\d+_t|int\d+_t|PatchResult|CorrectionResult|const\s+\w+&?)\s+\w[\w:]*\s*\([^;]*\))\s*(?:const\s*)?(?:override\s*)?(\{?)\s*$', line)
        if func_pat:
            indent = func_pat.group(1)
            sig = func_pat.group(2).strip()
            has_brace = func_pat.group(3) == '{'
            
            brace_line = i
            if not has_brace and i + 1 < len(lines) and lines[i+1].strip().startswith('{'):
                brace_line = i + 1
                has_brace = True
            
            if has_brace:
                # Scan body
                body_lines = []
                j = brace_line + 1
                depth = 1
                while j < len(lines) and j < brace_line + 20:
                    s = lines[j].strip()
                    depth += s.count('{') - s.count('}')
                    if depth <= 0:
                        break
                    if s and not s.startswith('//'):
                        body_lines.append((j+1, s))
                    elif s.startswith('//'):
                        body_lines.append((j+1, s))
                    j += 1
                
                if depth <= 0:
                    # Check if trivially empty
                    non_comment_body = [b for b in body_lines if not b[1].startswith('//')]
                    comment_body = [b for b in body_lines if b[1].startswith('//')]
                    
                    if len(non_comment_body) == 0 and len(comment_body) == 0:
                        results.append((relpath, i+1, 'EMPTY_FUNC', sig))
                    elif len(non_comment_body) == 0 and len(comment_body) > 0:
                        # Only comments inside - likely a stub
                        ctext = '; '.join([c[1] for c in comment_body])
                        results.append((relpath, i+1, 'COMMENT_ONLY_FUNC', f"{sig} -> {ctext[:150]}"))
                    elif len(non_comment_body) == 1 and non_comment_body[0][1].startswith('return'):
                        ret_val = non_comment_body[0][1]
                        results.append((relpath, i+1, 'RETURN_ONLY_FUNC', f"{sig} -> {ret_val}"))
        i += 1

# Deduplicate
seen = set()
unique = []
for r in results:
    key = (r[0], r[1], r[2])
    if key not in seen:
        seen.add(key)
        unique.append(r)

unique.sort(key=lambda x: (x[0], x[1]))

print(f"=== CLI STUB AUDIT: {len(unique)} findings ===\n")
for relpath, line, category, detail in unique:
    print(f"[{category}] {relpath}:L{line}")
    print(f"    {detail.strip()}")
    print()
