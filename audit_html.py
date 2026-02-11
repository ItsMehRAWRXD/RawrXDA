import re
import sys

filepath = 'gui/ide_chatbot_standalone.html'
with open(filepath, 'r', encoding='utf-8') as f:
    lines = f.readlines()

total = len(lines)
print(f"Total lines: {total}")

# ==========================================
# PART 1: Extract function refs from HTML event handlers (body: ~3818-5282)
# Also check embedded script at lines 4294-4353
# ==========================================
html_funcs = {}
for i in range(3817, 5282):
    line = lines[i]
    # Match on*="..." and on*='...'
    handlers = re.findall(r'on(?:click|change|keydown|keyup|keypress|input|submit|mouseover|mouseout|focus|blur|load|error)\s*=\s*"([^"]+)"', line)
    handlers += re.findall(r"on(?:click|change|keydown|keyup|keypress|input|submit|mouseover|mouseout|focus|blur|load|error)\s*=\s*'([^']+)'", line)
    for handler in handlers:
        # Extract function calls like funcName( or obj.method(
        fn_matches = re.findall(r'([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*)\s*\(', handler)
        for fn in fn_matches:
            # Skip built-in JS like parseInt, document.getElementById, etc.
            builtins = {'parseInt', 'parseFloat', 'Number', 'isNaN', 'Math.Min', 'Math.min', 'Math.max'}
            if fn in builtins:
                continue
            if fn not in html_funcs:
                html_funcs[fn] = []
            html_funcs[fn].append(i+1)

print("\n=== PART 1: FUNCTION NAMES REFERENCED IN HTML EVENT HANDLERS (lines 3818-5282) ===")
for fn in sorted(html_funcs.keys()):
    unique_lines = sorted(set(html_funcs[fn]))
    print(f"  {fn}  (lines: {', '.join(map(str, unique_lines))})")
print(f"\nTOTAL: {len(html_funcs)} unique function/method references in HTML")

# ==========================================
# PART 2: Extract all function definitions from script block (5283-11627)
# ==========================================
js_funcs = {}
js_objects = {}
# Also check the small embedded script at 4294-4353
script_ranges = [(4293, 4353), (5282, 11627)]

for start, end in script_ranges:
    for i in range(start, min(end, total)):
        line = lines[i]
        
        # function funcName( or async function funcName(
        for m in re.finditer(r'(?:async\s+)?function\s+([A-Za-z_$][\w$]*)\s*\(', line):
            name = m.group(1)
            if name not in js_funcs:
                js_funcs[name] = []
            js_funcs[name].append(i+1)
        
        # var/let/const funcName = function
        for m in re.finditer(r'(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s+)?function', line):
            name = m.group(1)
            if name not in js_funcs:
                js_funcs[name] = []
            js_funcs[name].append(i+1)
        
        # var/let/const funcName = async? (...) =>  or = () =>
        for m in re.finditer(r'(?:var|let|const)\s+([A-Za-z_$][\w$]*)\s*=\s*(?:async\s+)?\(', line):
            name = m.group(1)
            if name not in js_funcs:
                js_funcs[name] = []
            js_funcs[name].append(i+1)

        # Object/namespace definitions: var/const ObjName = { or window.ObjName = {
        for m in re.finditer(r'(?:var|let|const)\s+([A-Z][A-Za-z_$\d]*)\s*=\s*\{', line):
            name = m.group(1)
            if name not in js_objects:
                js_objects[name] = []
            js_objects[name].append(i+1)
        
        for m in re.finditer(r'window\.([A-Za-z_$][\w$]*)\s*=\s*\{', line):
            name = m.group(1)
            if name not in js_objects:
                js_objects[name] = []
            js_objects[name].append(i+1)

        # Method definitions inside objects: methodName: function or methodName: async function
        # or methodName(...) { (shorthand)
        for m in re.finditer(r'^\s+([A-Za-z_$][\w$]*)\s*:\s*(?:async\s+)?function', line):
            name = m.group(1)
            if name not in js_funcs:
                js_funcs[name] = []
            js_funcs[name].append(i+1)

print("\n=== PART 2: FUNCTION DEFINITIONS IN SCRIPT BLOCKS ===")
for fn in sorted(js_funcs.keys()):
    unique_lines = sorted(set(js_funcs[fn]))
    dup_marker = " ** DUPLICATE **" if len(unique_lines) > 1 else ""
    print(f"  {fn}  (lines: {', '.join(map(str, unique_lines))}){dup_marker}")
print(f"\nTOTAL: {len(js_funcs)} unique function definitions")

print("\n=== PART 2b: OBJECT/NAMESPACE DEFINITIONS ===")
for obj in sorted(js_objects.keys()):
    unique_lines = sorted(set(js_objects[obj]))
    dup_marker = " ** DUPLICATE **" if len(unique_lines) > 1 else ""
    print(f"  {obj}  (lines: {', '.join(map(str, unique_lines))}){dup_marker}")

# ==========================================
# PART 3: Find HTML-referenced functions MISSING from JS definitions
# ==========================================
print("\n=== PART 3: HTML FUNCTION REFERENCES MISSING FROM JS DEFINITIONS ===")
# Build a set of all defined names (including object-namespaced ones)
all_defined = set(js_funcs.keys())
all_defined.update(js_objects.keys())

# For object.method refs like CoT.toggle, check if CoT object exists and toggle method exists
missing = []
for fn in sorted(html_funcs.keys()):
    parts = fn.split('.')
    if len(parts) == 1:
        # Simple function name
        if fn not in all_defined:
            missing.append(fn)
    else:
        # Object.method - check if object is defined
        obj = parts[0]
        if obj == 'document' or obj == 'window' or obj == 'this' or obj == 'console' or obj == 'localStorage':
            continue  # Built-in
        if obj not in all_defined and obj not in js_objects:
            missing.append(fn)

if missing:
    for fn in missing:
        loc = ', '.join(map(str, sorted(set(html_funcs[fn]))))
        print(f"  MISSING: {fn}  (referenced at lines: {loc})")
else:
    print("  None found - all HTML references have JS definitions")

print(f"\nTOTAL MISSING: {len(missing)}")

# ==========================================
# PART 4: Duplicate function definitions
# ==========================================
print("\n=== PART 4: DUPLICATE FUNCTION DEFINITIONS ===")
dup_count = 0
for fn in sorted(js_funcs.keys()):
    unique_lines = sorted(set(js_funcs[fn]))
    if len(unique_lines) > 1:
        print(f"  DUPLICATE: {fn}  (defined at lines: {', '.join(map(str, unique_lines))})")
        dup_count += 1
if dup_count == 0:
    print("  None found")
print(f"\nTOTAL DUPLICATES: {dup_count}")

# ==========================================
# PART 5: Check for EngineAPI and other namespace references in HTML
# ==========================================
print("\n=== PART 5: NAMESPACE REFERENCES (EngineAPI, etc.) IN HTML ===")
namespace_refs = {}
for i in range(3817, 5282):
    line = lines[i]
    # Find references like EngineAPI.something, SomeObj.something
    for m in re.findall(r'([A-Z][A-Za-z_$\d]+)\.([\w$]+)\s*\(', line):
        ns = m[0]
        method = m[1]
        key = f"{ns}.{method}"
        if ns in ('Math', 'JSON', 'Object', 'Array', 'Date', 'Number', 'String', 'RegExp', 'Promise', 'Error'):
            continue
        if key not in namespace_refs:
            namespace_refs[key] = []
        namespace_refs[key].append(i+1)

for ref in sorted(namespace_refs.keys()):
    ns = ref.split('.')[0]
    status = "DEFINED" if ns in js_objects else "UNDEFINED NAMESPACE"
    loc = ', '.join(map(str, sorted(set(namespace_refs[ref]))))
    print(f"  {ref}  [{status}] (lines: {loc})")
