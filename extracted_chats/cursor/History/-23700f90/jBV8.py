#!/usr/bin/env python3
import sys, pathlib, datetime, inspect
try:
    from typing import List, Dict, Callable, Optional
except ImportError:
    # For older Python versions
    List = list
    Dict = dict
    Callable = object
    Optional = object

# Enhanced argument parsing with validation
def parse_args():
    """Parse command line arguments with enhanced validation."""
    if len(sys.argv) < 2:
        return "py", "example", False, False, False

    # Check for special flags
    if sys.argv[1] in ["--help", "-h"]:
        return None, None, True, False, False
    elif sys.argv[1] == "--list":
        return None, None, False, True, False
    elif sys.argv[1] == "--preview":
        preview = True
        lang = sys.argv[2] if len(sys.argv) > 2 else "py"
        name = sys.argv[3] if len(sys.argv) > 3 else "example"
        return lang.lower(), name, False, False, preview
    elif sys.argv[1] == "--role-only":
        return "role", sys.argv[2] if len(sys.argv) > 2 else "assistant", False, False, False

    # Standard language + name format
    lang = sys.argv[1].lower()
    name = sys.argv[2] if len(sys.argv) > 2 else "example"
    return lang, name, False, False, False

Lang, name, show_help_flag, list_flag, preview_flag = parse_args()
stamp = datetime.datetime.now().strftime("%Y-%m-%d %H:%M")

# ---------- enhanced language breeders ----------
def lang_py(): return [
    "#!/usr/bin/env python3",
    f'"""{name}.py — first-ten-lines starter   {stamp}"""',
    "", "import sys, pathlib",
    "from typing import List, Optional", "",
    f"def {name}(args: List[str]) -> int:",
    '    """Entry point."""',
    '    print("Hello from", __file__)',
    "    return 0", "",
    'if __name__ == "__main__":',
    f"    sys.exit({name}(sys.argv[1:]))"]

def lang_js(): return [
    "'use strict';",
    f"// {name}.js — first-ten-lines starter   {stamp}",
    "", "const fs = require('fs');",
    "const path = require('path');",
    "function main(argv) {",
    '    console.log("Entry point:", argv);',
    "    return 0;", "}",
    "if (require.main === module) {",
    "    process.exitCode = main(process.argv.slice(2));", "}"]

def lang_go(): return [
    "package main",
    f"// {name}.go — first-ten-lines starter   {stamp}",
    "", "import (",
    '\t"fmt"',
    '\t"os"',
    ")",
    "func main() {",
    '\tfmt.Println("Entry point:", os.Args[1:])',
    "}"]

def lang_rs(): return [
    f"// {name}.rs — first-ten-lines starter   {stamp}",
    "", "fn main() {",
    '    println!("Entry point: {:?}", std::env::args().skip(1).collect::<Vec<_>>());',
    "}"]

def lang_sh(): return [
    "#!/usr/bin/env bash",
    f"# {name}.sh — first-ten-lines starter   {stamp}",
    "set -euo pipefail", "",
    'echo "Entry point: $@"',
    'exit 0']

def lang_boot(): return [
    '#!/usr/bin/env bash',
    '# bootcc.tiny — 1-line self-building compiler',
    'set -euo pipefail',
    'src="${1:-bootcc.tiny}"; shift || true',
    '[ -f "$src" ] || { echo "File not found: $src" >&2; exit 1; }',
    '# ---- compilation ----',
    'gcc -x c -O2 -o "${src%.tiny}" "$src" \\',
    '   -D_GNU_SOURCE -std=c11 \\',
    '   -Wall -Wextra -pedantic 2>&1 || exit 1',
    'echo "compiled -> ${src%.tiny}"',
    '"${src%.tiny}" "$@"']

def lang_ollama(): return [
    '#!/usr/bin/env python3',
    f'"""{name}.py — Ollama integration starter   {stamp}"""',
    'import requests, json, sys',
    'def query_ollama(prompt: str, model: str = "llama2") -> str:',
    '    """Query Ollama API."""',
    '    try:',
    '        response = requests.post("http://localhost:11434/api/generate",',
    '            json={"model": model, "prompt": prompt, "stream": False})',
    '        response.raise_for_status()',
    '        return response.json()["response"]',
    '    except Exception as e:',
    '        return f"Error: {e}"',
    'if __name__ == "__main__":',
    '    result = query_ollama(" ".join(sys.argv[1:]))',
    '    print(result)']

def lang_c(): return [
    f"// {name}.c — first-ten-lines starter   {stamp}",
    "#include <stdio.h>",
    "#include <stdlib.h>",
    "",
    "int main(int argc, char *argv[]) {",
    '    printf("Entry point: %d args\\n", argc - 1);',
    "    return 0;", "}"]

def lang_cpp(): return [
    f"// {name}.cpp — first-ten-lines starter   {stamp}",
    "#include <iostream>",
    "#include <vector>",
    "#include <string>",
    "",
    "int main(int argc, char* argv[]) {",
    '    std::cout << "Entry point: " << argc - 1 << " args" << std::endl;',
    "    return 0;", "}"]

def lang_java(): return [
    f"// {name}.java — first-ten-lines starter   {stamp}",
    f"public class {name.capitalize()} {{",
    "    public static void main(String[] args) {",
    '        System.out.println("Entry point: " + args.length + " args");',
    "    }",
    "}"]

def lang_ollama(): return [
    '#!/usr/bin/env python3',
    f'"""{name}.py — Ollama integration starter   {stamp}"""',
    'import requests, json, sys',
    'def query_ollama(prompt: str, model: str = "llama2") -> str:',
    '    """Query Ollama API."""',
    '    try:',
    '        response = requests.post("http://localhost:11434/api/generate",',
    '            json={"model": model, "prompt": prompt, "stream": False})',
    '        response.raise_for_status()',
    '        return response.json()["response"]',
    '    except Exception as e:',
    '        return f"Error: {e}"',
    'if __name__ == "__main__":',
    '    result = query_ollama(" ".join(sys.argv[1:]))',
    '    print(result)']

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

# ---------- validation and utilities ----------
def validate_filename(filename: str, ext: str) -> str:
    """Ensure valid filename."""
    clean = "".join(c for c in filename if c.isalnum() or c in '_-')
    return clean or "example"

def get_available_languages() -> List[str]:
    """Get all available language templates."""
    return [func.replace("lang_", "") for func in globals()
            if func.startswith("lang_") and func != "lang_role"]

def validate_language(lang: str) -> bool:
    """Validate language support."""
    return lang in get_available_languages()

def show_help():
    """Display usage instructions and available languages."""
    print("Usage: python tenliner.py <language> <name>")
    print("Available languages:")
    for lang in get_available_languages():
        print(f"  {lang}")
    print("\nOptions:")
    print("  --list          Show all available templates")
    print("  --preview       Preview template without writing file")
    print("  --role-only     Generate only role card")

def lang_template(): return [
    '#!/usr/bin/env python3',
    '"""Template generator - generates templates for any language"""',
    'import sys, pathlib, datetime',
    'try:',
    '    from typing import List, Dict, Callable',
    'except ImportError:',
    '    List = list',
    '    Dict = dict',
    '    Callable = object',
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
    '            print("Enter additional code OR number of lines to generate (press Ctrl+D on Unix/Linux or Ctrl+Z+Enter on Windows to finish):")',
    '            try:',
    '                extra = sys.stdin.read().strip()',
    '                if extra:',
    '                    # Check if input is just a number (request to generate more lines)',
    '                    if extra.isdigit():',
    '                        num_lines = int(extra)',
    '                        if num_lines > 0:',
    '                            # Generate additional template lines',
    '                            template_lines = []',
    '                            for i in range(num_lines):',
    '                                template_lines.append(f"    # Additional line {i+1} - extend this template")',
    '                            with open(f"{target_name}.{template_type}", "a") as fh:',
    '                                fh.write("\n" + "\n".join(template_lines))',
    '                            print(f"✅ Generated {num_lines} additional template lines in {target_name}.{template_type}")',
    '                        else:',
    '                            print("Number must be greater than 0.")',
    '                    else:',
    '                        # Treat as manual code input
    '                        with open(f"{target_name}.{template_type}", "a") as fh:',
    '                            fh.write(extra + ("\n" if extra and not extra.endswith("\n") else ""))',
    '                        line_count = len(extra.splitlines())',
    '                        print(f"✅ Appended {line_count} lines to {target_name}.{template_type}")',
    '                else:',
    '                    print("No additional code entered.")',
    '            except (EOFError, KeyboardInterrupt):',
    '                print(f"\n✅ Cancelled. File {target_name}.{template_type} saved with generated template.")',
    '    else:',
    '        print("Usage: python script.py <template_type> <target_name>")',
    '        print(f"Available templates: {", ".join(generator.list_templates())}")'
    ]

# ---------- enhanced dispatcher with validation ----------
def validate_language(lang: str) -> bool:
    """Validate language support."""
    return lang in get_available_languages()

# Main execution with enhanced error handling
if __name__ == "__main__":
    # Handle special flags
    if show_help_flag:
        show_help()
        sys.exit(0)
    elif list_flag:
        print("Available templates:")
        for lang in get_available_languages():
            print(f"  {lang}")
        print("\nOptions:")
        print("  --list          Show all available templates")
        print("  --preview       Preview template without writing file")
        print("  --role-only     Generate only role card")
        print("  --help          Show this help message")
        sys.exit(0)
    elif preview_flag:
        # Preview mode - just show the template without writing
        core = globals().get(f"lang_{Lang}", lang_py)()
        print("Template preview:")
        print("\n".join(core))
        sys.exit(0)

    # Validate language support
    if not validate_language(Lang):
        print(f"❌ Error: Language '{Lang}' not supported.")
        print(f"Available languages: {', '.join(get_available_languages())}")
        sys.exit(1)

    # Generate and display template
    try:
        core = globals().get(f"lang_{Lang}", lang_py)()
        print("\n".join(core))

        # Write to file unless it's a role-only request
        if Lang != "role":
            pathlib.Path(f"{name}.{Lang}").write_text("\n".join(core) + "\n")
            print(f"✅ Generated {name}.{Lang}")

        # Offer role template for non-role requests
        if Lang != "role" and input("Add role template (y/n)? ").lower().startswith("y"):
            with open(f"{name}.role", "w") as fh:
                fh.write("\n".join(lang_role()))
            print(f"✅ Role template written to {name}.role")

        # ---------- live continuation ----------
        # Only offer live continuation for non-template languages (template generator has its own)
        if Lang != "template":
            try:
                if input("Add more lines (y/n)? ").lower().startswith("y"):
                    print("Enter additional code OR number of lines to generate (press Ctrl+D on Unix/Linux or Ctrl+Z+Enter on Windows to finish):")
                    try:
                        extra = sys.stdin.read().strip()
                        if extra:
                            # Check if input is just a number (request to generate more lines)
                            if extra.isdigit():
                                num_lines = int(extra)
                                if num_lines > 0:
                                    # Generate additional template lines based on language
                                    template_lines = []
                                    for i in range(num_lines):
                                        template_lines.append(f"    # Additional line {i+1} - extend this template")

                                    with open(f"{name}.{Lang}", "a") as fh:
                                        fh.write("\n" + "\n".join(template_lines))
                                    print(f"✅ Generated {num_lines} additional template lines in {name}.{Lang}")
                                else:
                                    print("❌ Number must be greater than 0.")
                            else:
                                # Treat as manual code input
                                with open(f"{name}.{Lang}", "a") as fh:
                                    fh.write(extra + ("\n" if extra and not extra.endswith("\n") else ""))
                                line_count = len(extra.splitlines())
                                print(f"✅ Appended {line_count} lines to {name}.{Lang}")
                        else:
                            print("No additional code entered.")
                    except (EOFError, KeyboardInterrupt):
                        print(f"\n✅ Cancelled. File {name}.{Lang} saved with generated template.")
            except (EOFError, KeyboardInterrupt):
                print(f"\n✅ Cancelled. File {name}.{Lang} saved with generated template.")

    except Exception as e:
        print(f"❌ Error generating template: {e}")
        sys.exit(1)
    elif list_flag:
        print("Available templates:")
        for lang in get_available_languages():
            print(f"  {lang}")
        print("\nOptions:")
        print("  --list          Show all available templates")
        print("  --preview       Preview template without writing file")
        print("  --role-only     Generate only role card")
        print("  --help          Show this help message")
        sys.exit(0)
    elif preview_flag:
        # Preview mode - just show the template without writing
        core = globals().get(f"lang_{Lang}", lang_py)()
        print("Template preview:")
        print("\n".join(core))
        sys.exit(0)

    # Validate language support
    if not validate_language(Lang):
        print(f"❌ Error: Language '{Lang}' not supported.")
        print(f"Available languages: {', '.join(get_available_languages())}")
        sys.exit(1)

    # Generate and display template
    try:
        core = globals().get(f"lang_{Lang}", lang_py)()
        print("\n".join(core))

        # Write to file unless it's a role-only request
        if Lang != "role":
            pathlib.Path(f"{name}.{Lang}").write_text("\n".join(core) + "\n")
            print(f"✅ Generated {name}.{Lang}")

        # Offer role template for non-role requests
        if Lang != "role" and input("Add role template (y/n)? ").lower().startswith("y"):
            with open(f"{name}.role", "w") as fh:
                fh.write("\n".join(lang_role()))
            print(f"✅ Role template written to {name}.role")

        # ---------- live continuation ----------
        # Only offer live continuation for non-template languages (template generator has its own)
        if Lang != "template":
            try:
                if input("Add more lines (y/n)? ").lower().startswith("y"):
                    print("Enter additional code OR number of lines to generate (press Ctrl+D on Unix/Linux or Ctrl+Z+Enter on Windows to finish):")
                    try:
                        extra = sys.stdin.read().strip()
                        if extra:
                            # Check if input is just a number (request to generate more lines)
                            if extra.isdigit():
                                num_lines = int(extra)
                                if num_lines > 0:
                                    # Generate additional template lines based on language
                                    template_lines = []
                                    for i in range(num_lines):
                                        template_lines.append(f"    # Additional line {i+1} - extend this template")

                                    with open(f"{name}.{Lang}", "a") as fh:
                                        fh.write("\n" + "\n".join(template_lines))
                                    print(f"✅ Generated {num_lines} additional template lines in {name}.{Lang}")
                                else:
                                    print("❌ Number must be greater than 0.")
                            else:
                                # Treat as manual code input
                                with open(f"{name}.{Lang}", "a") as fh:
                                    fh.write(extra + ("\n" if extra and not extra.endswith("\n") else ""))
                                line_count = len(extra.splitlines())
                                print(f"✅ Appended {line_count} lines to {name}.{Lang}")
                        else:
                            print("No additional code entered.")
                    except (EOFError, KeyboardInterrupt):
                        print(f"\n✅ Cancelled. File {name}.{Lang} saved with generated template.")
            except (EOFError, KeyboardInterrupt):
                print(f"\n✅ Cancelled. File {name}.{Lang} saved with generated template.")

    except Exception as e:
        print(f"❌ Error generating template: {e}")
        sys.exit(1)
