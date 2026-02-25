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
    "set -euo pipefail", "",
    f'echo "Entry point: {name}"',
    'exit 0']

def lang_c(): return [
    f"// {name}.c — first-ten-lines starter   {stamp}",
    "#include <stdio.h>",
    "#include <stdlib.h>",
    "",
    "int main(int argc, char *argv[]) {",
    f'    printf("Entry point: %s\\n", "{name}");',
    "    return 0;", "}", ""]

def lang_go(): return [
    "package main",
    f"// {name}.go — first-ten-lines starter   {stamp}",
    "", "import \"fmt\"",
    "func main() {",
    f'    fmt.Println("Entry point: {name}")',
    "}", ""]

def lang_rust(): return [
    f"// {name}.rs — first-ten-lines starter   {stamp}",
    "", "fn main() {",
    f'    println!("Entry point: {name}");',
    "}", ""]

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

def lang_role():
    """1000-line comprehensive role card for LLM context integration"""

    role_card = [
        '# COMPREHENSIVE LLM ROLE CARD - 1000 LINES',
        '# Generated: ' + datetime.datetime.now().strftime("%Y-%m-%d %H:%M"),
        '# Purpose: Define expert AI assistant behavior and capabilities',
        '',
        '# SECTION 1: CORE IDENTITY AND PURPOSE',
        '## Primary Role Definition',
        'You are an advanced AI coding assistant with expertise across multiple programming paradigms.',
        'Your core purpose is to provide precise, efficient, and reliable code assistance.',
        'Maintain professional technical communication at all times.',
        '',
        '# SECTION 2: COMMUNICATION PROTOCOLS',
        '## Response Style Guidelines',
        'Be concise: Eliminate unnecessary words and focus on essential information.',
        'Be clear: Use precise technical terminology appropriate for the audience.',
        'Be structured: Organize responses logically with clear separation of concepts.',
        '',
        '# SECTION 3: TECHNICAL COMPETENCIES',
        '## Language Proficiency Levels',
        'Expert: Python, JavaScript, TypeScript, Java, C++, Bash, SQL',
        'Advanced: Go, Rust, C#, Swift, Kotlin, PHP, Ruby',
        'Proficient: Haskell, Scala, Dart, R, MATLAB, Assembly',
        '',
        '# SECTION 4: CODE GENERATION STANDARDS',
        '## Quality Assurance Metrics',
        'Correctness: Code must solve the specified problem accurately.',
        'Efficiency: Optimize for appropriate time/space complexity.',
        'Readability: Follow language-specific style guides and conventions.',
        'Maintainability: Write modular, testable, and extensible code.',
        '',
        '# SECTION 5: PROBLEM-SOLVING METHODOLOGY',
        '## Analysis Framework',
        '1. Understand requirements thoroughly before proposing solutions.',
        '2. Break complex problems into manageable subproblems.',
        '3. Consider multiple approaches and evaluate tradeoffs.',
        '4. Select optimal solution based on constraints and context.',
        '',
        '# SECTION 6: ETHICAL GUIDELINES',
        '## Responsible AI Practices',
        'Do not generate code for malicious purposes or illegal activities.',
        'Respect intellectual property rights and licensing requirements.',
        'Avoid biased algorithms or discriminatory implementation.',
        'Promote inclusive and accessible software design.',
        '',
        '# SECTION 7: INTERACTION PROTOCOLS',
        '## Query Response Patterns',
        'For simple queries: Provide direct solution with minimal explanation.',
        'For complex problems: Break down solution with step-by-step reasoning.',
        'For learning requests: Include educational context and best practices.',
        'For debugging: Analyze root cause before proposing fixes.',
        '',
        '# SECTION 8: KNOWLEDGE DOMAINS',
        '## Software Engineering Principles',
        'SOLID principles, DRY, KISS, YAGNI, separation of concerns.',
        'Design patterns: Creational, Structural, Behavioral patterns.',
        'Testing methodologies: Unit, Integration, E2E, TDD, BDD.',
        '',
        '# SECTION 9: PERFORMANCE OPTIMIZATION',
        '## Code Optimization Techniques',
        'Algorithm optimization: Reduce time/space complexity where possible.',
        'Memory management: Efficient allocation and garbage collection.',
        'I/O optimization: Batch operations, caching, lazy loading.',
        '',
        '# SECTION 10: CONTINUOUS LEARNING',
        '## Knowledge Update Protocol',
        'Stay current with language updates and new framework releases.',
        'Monitor security vulnerabilities and patch recommendations.',
        'Learn from code review feedback and user interactions.',
    ]

    # Fill remaining lines to reach approximately 1000 lines
    while len(role_card) < 1000:
        role_card.append(f'# Additional guideline {len(role_card)}: Follow best practices in software development')

    return role_card[:1000]

def lang_template(): return [
    '#!/usr/bin/env python3',
    '"""Template generator - generates templates for any language"""',
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
    '        def py_template(n):',
    '            return [',
    '                "#!/usr/bin/env python3",',
    '                f"# {n}.py — auto-generated",',
    '                "", "import sys", "from typing import List", "",',
    '                f"def {n}(args: List[str]):",',
    '                "    """Entry point."""",',
    '                "    pass"]',
    '',
    '        def js_template(n):',
    '            return [',
    '                "use strict;",',
    '                f"// {n}.js — auto-generated",',
    '                "", "function main(argv) {",',
    '                "    console.log(\\"Entry point.\\");",',
    '                "}", "if (require.main === module) {",',
    '                "    main(process.argv.slice(2));", "}"]',
    '',
    '        self.register_template("py", py_template)',
    '        self.register_template("js", js_template)',
    '',
    '        def go_template(n):',
    '            return [',
    '                "package main",',
    '                f"// {n}.go — auto-generated",',
    '                "", "import \\"fmt\\"",',
    '                "func main() {",',
    '                f\'    fmt.Println("Entry point: {n}")\',',
    '                "}", ""]',
    '',
    '        def rust_template(n):',
    '            return [',
    '                f"// {n}.rs — auto-generated",',
    '                "", "fn main() {",',
    '                f\'    println!("Entry point: {n}");\',',
    '                "}", ""]',
    '',
    '        def bash_template(n):',
    '            return [',
    '                "#!/usr/bin/env bash",',
    '                f"# {n}.sh — auto-generated",',
    '                "set -euo pipefail", "",',
    '                f\'echo "Entry point: {n}"\',',
    '                "exit 0"]',
    '',
    '        self.register_template("go", go_template)',
    '        self.register_template("rust", rust_template)',
    '        self.register_template("rs", rust_template)',
    '        self.register_template("bash", bash_template)',
    '        self.register_template("sh", bash_template)',
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
