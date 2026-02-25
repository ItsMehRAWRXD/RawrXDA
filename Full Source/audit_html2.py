import re
import sys

filepath = 'D:/rawrxd/gui/ide_chatbot_standalone.html'
with open(filepath, 'r', encoding='utf-8') as f:
    content = f.read()
    f.seek(0)
    lines = f.readlines()

total = len(lines)
print(f"File: {filepath}")
print(f"Total lines: {total}")

# Find all script blocks
script_blocks = []
in_script = False
script_start = 0
for i, line in enumerate(lines):
    if re.search(r'<script\b', line, re.IGNORECASE) and 'src=' not in line.lower():
        in_script = True
        script_start = i
    if re.search(r'</script', line, re.IGNORECASE) and in_script:
        in_script = False
        script_blocks.append((script_start+1, i+1))

print(f"\nScript blocks found:")
for s, e in script_blocks:
    print(f"  Lines {s}-{e} ({e-s} lines)")

# Find body start/end
body_start = body_end = 0
for i, line in enumerate(lines):
    if '<body' in line.lower():
        body_start = i+1
    if '</body' in line.lower():
        body_end = i+1

print(f"\nBody: lines {body_start}-{body_end}")

# ==========================================
# PART 1: Extract function refs from ALL HTML event handlers
# ==========================================
html_funcs = {}
for i in range(total):
    line = lines[i]
    # Skip lines inside script blocks
    in_js = False
    for s, e in script_blocks:
        if s <= i+1 <= e:
            in_js = True
            break
    if in_js:
        continue
    
    handlers = re.findall(r'on(?:click|change|keydown|keyup|keypress|input|submit|mouseover|mouseout|focus|blur|load|error)\s*=\s*"([^"]+)"', line)
    handlers += re.findall(r"on(?:click|change|keydown|keyup|keypress|input|submit|mouseover|mouseout|focus|blur|load|error)\s*=\s*'([^']+)'", line)
    for handler in handlers:
        fn_matches = re.findall(r'([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*)\s*\(', handler)
        for fn in fn_matches:
            builtins = {'parseInt', 'parseFloat', 'Number', 'isNaN', 'Math.Min', 'Math.min', 'Math.max',
                        'document.getElementById', 'document.querySelector', 'document.querySelectorAll',
                        'window.location.reload', 'console.log', 'console.error',
                        'this.value.split', 'toFixed', 'toLocaleString', 'filter', 'map', 'function',
                        'click'}
            if fn in builtins:
                continue
            if fn.startswith('document.') or fn.startswith('window.') or fn.startswith('this.') or fn.startswith('console.'):
                continue
            if fn not in html_funcs:
                html_funcs[fn] = []
            html_funcs[fn].append(i+1)

print("\n" + "="*80)
print("PART 1: FUNCTION NAMES REFERENCED IN HTML EVENT HANDLERS")
print("="*80)
for fn in sorted(html_funcs.keys()):
    unique_lines = sorted(set(html_funcs[fn]))
    print(f"  {fn}  (lines: {', '.join(map(str, unique_lines))})")
print(f"\nTOTAL: {len(html_funcs)} unique function/method references in HTML")

# ==========================================
# PART 2: Extract ALL function/method definitions from script blocks
# ==========================================
js_defs = set()
js_defs_details = {}  # name -> [(line, pattern)]
js_objects = set()
js_objects_details = {}

def add_def(name, linenum, pattern):
    js_defs.add(name)
    if name not in js_defs_details:
        js_defs_details[name] = []
    js_defs_details[name].append((linenum, pattern))

def add_obj(name, linenum):
    js_objects.add(name)
    if name not in js_objects_details:
        js_objects_details[name] = []
    js_objects_details[name].append(linenum)

for s, e in script_blocks:
    for i in range(s-1, e):
        line = lines[i]
        
        # function funcName(
        for m in re.finditer(r'(?:async\s+)?function\s+([A-Za-z_$][\w$]*)\s*\(', line):
            add_def(m.group(1), i+1, 'function decl')
        
        # var/let/const funcName = function
        for m in re.finditer(r'(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s+)?function', line):
            add_def(m.group(1), i+1, 'var = function')
        
        # var/let/const funcName = (...) => or = async (
        for m in re.finditer(r'(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s+)?\([^)]*\)\s*=>', line):
            add_def(m.group(1), i+1, 'arrow fn')
        
        # window.funcName = function or window.funcName = (...) =>
        for m in re.finditer(r'window\.([A-Za-z_$][\w$]*)\s*=\s*(?:(?:async\s+)?function|\()', line):
            add_def(m.group(1), i+1, 'window.X = function')
        
        # Object/namespace: var/const ObjName = {
        for m in re.finditer(r'(?:var|let|const)\s+([A-Z][A-Za-z_$\d]*)\s*=\s*\{', line):
            add_obj(m.group(1), i+1)
        
        # window.ObjName = {
        for m in re.finditer(r'window\.([A-Za-z_$][\w$]*)\s*=\s*\{', line):
            add_obj(m.group(1), i+1)
        
        # Method in object: methodName: function or methodName: async function
        for m in re.finditer(r'^\s+([a-zA-Z_$][\w$]*)\s*:\s*(?:async\s+)?function', line):
            add_def(m.group(1), i+1, 'obj method')
        
        # Shorthand method: methodName(args) {
        for m in re.finditer(r'^\s+([a-zA-Z_$][\w$]*)\s*\([^)]*\)\s*\{', line):
            name = m.group(1)
            if name not in ('if', 'for', 'while', 'switch', 'catch', 'else', 'return', 'function', 'try'):
                add_def(name, i+1, 'shorthand method')

# Also scan for functions assigned to variables outside formal declarations
# e.g. funcName = function(...) at top level
for s, e in script_blocks:
    for i in range(s-1, e):
        line = lines[i]
        # Direct assignment (not var/let/const): funcName = function  (but only at reasonable indent)
        for m in re.finditer(r'^(\s{0,4})([a-zA-Z_$][\w$]*)\s*=\s*(?:async\s+)?function\s*\(', line):
            name = m.group(2)
            if name not in js_defs and name not in ('if', 'for', 'while', 'switch', 'catch', 'else', 'return', 'function', 'try'):
                add_def(name, i+1, 'bare assignment')

print("\n" + "="*80)
print("PART 2: FUNCTION DEFINITIONS IN SCRIPT BLOCKS")
print("="*80)
for fn in sorted(js_defs):
    details = js_defs_details[fn]
    locs = [f"{line} ({pat})" for line, pat in details]
    dup_marker = " *** DUPLICATE ***" if len(details) > 1 else ""
    print(f"  {fn}  [{', '.join(locs)}]{dup_marker}")
print(f"\nTOTAL: {len(js_defs)} unique function definitions")

print(f"\nOBJECT/NAMESPACE DEFINITIONS:")
for obj in sorted(js_objects):
    details = js_objects_details[obj]
    dup_marker = " *** DUPLICATE ***" if len(details) > 1 else ""
    print(f"  {obj}  (lines: {', '.join(map(str, details))}){dup_marker}")

# ==========================================
# PART 3: Cross-reference — HTML refs missing from JS
# ==========================================
print("\n" + "="*80)
print("PART 3: HTML FUNCTION REFERENCES MISSING FROM JS DEFINITIONS")
print("="*80)

# For Obj.method references, we need to check if both the object AND method exist
# Let's also scan for methods within known objects
# First build a map of object methods
obj_methods = {}  # object_name -> set of method names
for obj_name in js_objects:
    obj_methods[obj_name] = set()

# Scan script blocks for method definitions inside known objects
# This requires understanding context which is tricky with regex
# Let's do a simpler check: scan for ObjName.methodName = or methodName: in the script

for s, e in script_blocks:
    for i in range(s-1, e):
        line = lines[i]
        for obj_name in js_objects:
            # ObjName.method = 
            for m in re.finditer(rf'{re.escape(obj_name)}\.([a-zA-Z_$][\w$]*)\s*=', line):
                obj_methods.setdefault(obj_name, set()).add(m.group(1))

missing = []
for fn in sorted(html_funcs.keys()):
    parts = fn.split('.')
    if len(parts) == 1:
        # Simple function name
        if fn not in js_defs:
            missing.append((fn, 'function not found'))
    elif len(parts) == 2:
        obj, method = parts
        if obj == 'State' or obj == 'Conversation':
            continue  # These are known objects referenced in inline handlers
        if obj not in js_objects:
            missing.append((fn, f'namespace "{obj}" not defined'))
        else:
            # Check if method exists on object
            if obj in obj_methods and method not in obj_methods[obj]:
                missing.append((fn, f'method "{method}" not found on {obj}'))
    else:
        # Deeper nesting like State.gen.tensorHop
        if parts[0] in ('State', 'Conversation', 'document', 'window'):
            continue
        missing.append((fn, 'complex reference - manual check needed'))

for fn, reason in missing:
    loc = ', '.join(map(str, sorted(set(html_funcs[fn]))))
    print(f"  MISSING: {fn}  -- {reason}  (HTML lines: {loc})")

print(f"\nTOTAL MISSING: {len(missing)}")

# ==========================================
# PART 4: Duplicate function definitions
# ==========================================
print("\n" + "="*80)
print("PART 4: DUPLICATE FUNCTION DEFINITIONS")
print("="*80)
dup_count = 0
for fn in sorted(js_defs):
    details = js_defs_details[fn]
    if len(details) > 1:
        locs = [f"line {line} ({pat})" for line, pat in details]
        print(f"  DUPLICATE: {fn}  defined at: {'; '.join(locs)}")
        dup_count += 1
if dup_count == 0:
    print("  None found")
print(f"\nTOTAL DUPLICATES: {dup_count}")

# ==========================================
# PART 5: Namespace references
# ==========================================
print("\n" + "="*80)
print("PART 5: NAMESPACE/OBJECT METHOD REFERENCES IN HTML")
print("="*80)
for fn in sorted(html_funcs.keys()):
    if '.' in fn:
        parts = fn.split('.')
        obj = parts[0]
        if obj in ('State', 'Conversation', 'document', 'window', 'console', 'localStorage', 'this'):
            status = "BUILT-IN/STATE"
        elif obj in js_objects:
            method = parts[1] if len(parts) > 1 else '?'
            if obj in obj_methods and method in obj_methods[obj]:
                status = "DEFINED"
            else:
                status = f"OBJECT EXISTS but method '{method}' not confirmed"
        else:
            status = f"UNDEFINED NAMESPACE '{obj}'"
        loc = ', '.join(map(str, sorted(set(html_funcs[fn]))))
        print(f"  {fn}  [{status}] (lines: {loc})")

# ==========================================
# PART 6: Functions called WITHIN script that are never defined
# ==========================================
print("\n" + "="*80)
print("PART 6: FUNCTIONS CALLED IN SCRIPT BUT NEVER DEFINED (sampling key names)")
print("="*80)

# Collect all function calls in script blocks
script_calls = set()
for s, e in script_blocks:
    for i in range(s-1, e):
        line = lines[i]
        # Skip comments
        stripped = line.strip()
        if stripped.startswith('//') or stripped.startswith('*') or stripped.startswith('/*'):
            continue
        for m in re.finditer(r'(?<![.\w$])([a-zA-Z_$][\w$]*)\s*\(', line):
            name = m.group(1)
            keywords = {'if', 'for', 'while', 'switch', 'catch', 'else', 'return', 'function', 'async',
                       'try', 'typeof', 'new', 'var', 'let', 'const', 'class', 'import', 'export',
                       'throw', 'delete', 'void', 'in', 'of', 'case', 'default', 'break', 'continue',
                       'with', 'do', 'yield', 'await', 'from'}
            builtins = {'parseInt', 'parseFloat', 'isNaN', 'isFinite', 'encodeURIComponent',
                       'decodeURIComponent', 'encodeURI', 'decodeURI', 'setTimeout', 'setInterval',
                       'clearTimeout', 'clearInterval', 'fetch', 'alert', 'confirm', 'prompt',
                       'requestAnimationFrame', 'cancelAnimationFrame', 'atob', 'btoa',
                       'Array', 'Object', 'String', 'Number', 'Boolean', 'Date', 'RegExp',
                       'Map', 'Set', 'WeakMap', 'WeakSet', 'Promise', 'Error', 'TypeError',
                       'Symbol', 'Proxy', 'Reflect', 'JSON', 'Math', 'Intl', 'URL',
                       'require', 'define', 'DOMPurify', 'hljs', 'marked'}
            if name in keywords or name in builtins:
                continue
            script_calls.add(name)

# Find calls that have no definition
undefined_calls = script_calls - js_defs
# Filter out known object names and common patterns
undefined_calls -= js_objects
undefined_calls -= {'el', 'log', 'warn', 'error', 'info', 'debug'}  # might be aliased

# Show the most interesting ones (filter noise)
interesting = sorted([c for c in undefined_calls if len(c) > 2 and not c[0].isupper()])
print(f"  Found {len(undefined_calls)} function names called but not formally defined.")
print(f"  Showing potentially interesting ones (lowercase, >2 chars):")
for name in interesting[:60]:
    print(f"    {name}")
if len(interesting) > 60:
    print(f"    ... and {len(interesting)-60} more")
