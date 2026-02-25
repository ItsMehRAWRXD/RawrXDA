#!/usr/bin/env python3
import sys, pathlib, datetime
from typing import List

Lang = (sys.argv[1:2] + ["py"])[0]                 # user picks language
name = (sys.argv[2:3] + ["example"])[0]            # user picks file name
stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

# ---------- language breeders ----------
def lang_py(): return [
    "#!/usr/bin/env python3",
    f'"""{name}.py — first-ten-lines starter   {stamp}"""',
    "", "import sys, pathlib",
    "from typing import List", "",
    f"def {name}(args: List[str]):",
    '    """Entry point."""',
    "    pass", ""]

def lang_js(): return [
    "'use strict';",
    f"// {name}.js — first-ten-lines starter   {stamp}",
    "", "const fs = require('fs');",
    "function main(argv) {",
    '    console.log("Entry point.");',
    "}", "if (require.main === module) {",
    "    main(process.argv.slice(2));", "}"]

def lang_go(): return [
    "package main",
    f"// {name}.go — first-ten-lines starter   {stamp}",
    "", "import \"fmt\"",
    "func main() {",
    '\tfmt.Println("Entry point.")',
    "}", ""]

def lang_rs(): return [
    f"// {name}.rs — first-ten-lines starter   {stamp}",
    "fn main() {",
    '    println!("Entry point.");', "}", ""]

def lang_sh(): return [
    "#!/usr/bin/env bash",
    f"# {name}.sh — first-ten-lines starter   {stamp}",
    "", f'echo "Entry point."', ""]

def lang_boot(): return [
    '#!/usr/bin/env bash',
    '# bootcc.tiny — 1-line self-building compiler',
    'set -euo pipefail',
    'src="${1:-bootcc.tiny}"; shift || true',
    '# ---- compilation ----',
    'gcc -x c -O2 -o "${src%.tiny}" "$src" \\',
    '   -D_GNU_SOURCE -std=c11 \\',
    '   -Wall -Wextra -pedantic 2>&1 || exit 1',
    'echo "compiled -> ${src%.tiny}"',
    '"${src%.tiny}" "$@"'          # run the freshly built binary
]

def lang_ollama(): return [
    '#!/usr/bin/env python3',
    f'"""{name}.py — Ollama integration starter   {stamp}"""',
    'import requests, json, sys',
    'def query_ollama(prompt, model="llama2"):',
    '    response = requests.post("http://localhost:11434/api/generate",',
    '        json={"model": model, "prompt": prompt, "stream": False})',
    '    return response.json()["response"]',
    'if __name__ == "__main__":',
    '    result = query_ollama(" ".join(sys.argv[1:]))',
    '    print(result)'
]

def lang_role(): return [
    '# Role',
    'You are an expert in code assistance: concise, clear, effective.',
    'Comment only when essential; keep responses to the point.',
    '# Example Usage',
    'Given a problem, provide a direct solution without unnecessary commentary.',
    'Keep responses short and to the point.',
    '# Ethical Considerations',
    'Follow ethical guidelines at all times.',
    'Provide accurate and reliable information.',
    '# Context',
    'You are designed to work within a large language model (LLM) context.'
]

def lang_template(): return [
    '#!/usr/bin/env python3',
    f'"""{name}.py — universal template generator   {stamp}"""',
    'import sys, pathlib, datetime',
    'from typing import List, Dict, Callable',
    '',
    'class TemplateGenerator:',
    '    def __init__(self):',
    '        self.templates: Dict[str, Callable[[str], List[str]]] = {}',
    '        self.register_defaults()',
    '',
    '    def register_template(self, name: str, func: Callable[[str], List[str]]):',
    '        """Register a new template generator."""',
    '        self.templates[name] = func',
    '',
    '    def generate(self, template_type: str, target_name: str) -> str:',
    '        """Generate a template file."""',
    '        if template_type in self.templates:',
    '            content = self.templates[template_type](target_name)',
    '            return "\n".join(content)',
    '        return f"# Template {template_type} not found"',
    '',
    '    def list_templates(self) -> List[str]:',
    '        """List all available templates."""',
    '        return list(self.templates.keys())',
    '',
    '    def add_template(self, name: str, template_func: Callable[[str], List[str]]):',
    '        """Add a new template type."""',
    '        self.templates[name] = template_func',
    '',
    '    def register_defaults(self):',
    '        """Register default templates."""',
    '        self.register_template("py", lambda n: [',
    '            "#!/usr/bin/env python3",',
    '            f"# {n}.py — auto-generated",
    '            "", "import sys", "from typing import List", "",',
    '            f"def {n}(args: List[str]):",',
    '            "# Entry point."',
    '            "    pass"])',
    '',
    '        self.register_template("js", lambda n: [',
    '            "use strict;",',
    '            f"// {n}.js — auto-generated   {datetime.datetime.now().strftime(\\"%Y-%m-%d %H:%M\\")}",
    '            "", "function main(argv) {",',
    '            \'    console.log("Entry point.");\',',
    '            "}", "if (require.main === module) {",',
    '            "    main(process.argv.slice(2));", "}"])',
    '',
    'if __name__ == "__main__":',
    '    generator = TemplateGenerator()',
    '    if len(sys.argv) < 2 or sys.argv[1] in ["-h", "--help"]:',
    '        print("Template Generator - Generate starter templates for any language")',
    '        print(f"Available templates: {", ".join(generator.list_templates())}")',
    '        print("Usage: python script.py <template_type> <target_name>")',
    '        print("       python script.py list  (show all templates)")',
    '        sys.exit(0)',
    '    elif sys.argv[1] == "list":',
    '        print("Available templates:")',
    '        for template in generator.list_templates():',
    '            print(f"  {template}")',
    '        sys.exit(0)',
    '    elif len(sys.argv) >= 3:',
    '        template_type = sys.argv[1]',
    '        target_name = sys.argv[2]',
    '        result = generator.generate(template_type, target_name)',
    '        if "not found" in result:',
    '            print(f"Error: {result}")',
    '            sys.exit(1)',
    '        print(result)',
    '        pathlib.Path(f"{target_name}.{template_type}").write_text(result + "\n")',
    '        print(f"Generated {target_name}.{template_type}")',
    '',
    '        # Live continuation prompt',
    '        if input("Add more lines (y/n)? ").lower().startswith("y"):',
    '            print("Enter additional code (Ctrl-D / Ctrl-Z to finish):")',
    '            extra = sys.stdin.read().strip()',
    '            with open(f"{target_name}.{template_type}", "a") as fh:',
    '                fh.write(extra + ("\n" if extra and not extra.endswith("\n") else ""))',
    '            print(f"Appended to {target_name}.{template_type}")',
    '    else:',
    '        print("Usage: python script.py <template_type> <target_name>")',
    '        print(f"Available templates: {", ".join(generator.list_templates())}")'
    ]

# ---------- dispatcher ----------
core = globals().get(f"lang_{Lang}", lang_py)()
print("\n".join(core))
pathlib.Path(f"{name}.{Lang}").write_text("\n".join(core) + "\n")

# ---------- live continuation ----------
if input("Add more lines (y/n)? ").lower().startswith("y"):
    print("# extra lines (Ctrl-D / Ctrl-Z to finish):")
    extra = sys.stdin.read().strip()
    with open(f"{name}.{Lang}", "a") as fh:
        fh.write(extra + ("\n" if extra and not extra.endswith("\n") else ""))
    print(f"Appended {extra.count(chr(10))+1} lines to {name}.{Lang}")
