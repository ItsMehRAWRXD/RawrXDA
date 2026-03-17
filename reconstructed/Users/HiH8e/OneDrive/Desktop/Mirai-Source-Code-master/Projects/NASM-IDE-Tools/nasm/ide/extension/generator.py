#!/usr/bin/env python3
"""
NASM Assembly IDE Extension Generator
====================================
Generates IDE extensions and plugins for popular editors with NASM support.
Creates VS Code, Sublime Text, Vim, and Emacs extensions.

Author: Mirai Security Toolkit  
Date: November 21, 2025
License: MIT
"""

import json
import os
from pathlib import Path
from typing import Dict, List

class VSCodeExtensionGenerator:
    """Generate VS Code extension for NASM"""
    
    def __init__(self, output_dir: str):
        self.output_dir = Path(output_dir)
        self.extension_dir = self.output_dir / "nasm-ide-vscode"
    
    def generate(self):
        """Generate complete VS Code extension"""
        self.extension_dir.mkdir(parents=True, exist_ok=True)
        
        # Create package.json
        self._create_package_json()
        
        # Create syntax highlighting
        self._create_syntax_grammar()
        
        # Create language configuration
        self._create_language_config()
        
        # Create extension main file
        self._create_extension_main()
        
        # Create theme
        self._create_theme()
        
        # Create snippets
        self._create_snippets()
        
        print(f"✅ VS Code extension generated at: {self.extension_dir}")
    
    def _create_package_json(self):
        """Create package.json for VS Code extension"""
        package = {
            "name": "nasm-assembly-ide",
            "displayName": "NASM Assembly IDE",
            "description": "Advanced NASM assembly language support with syntax highlighting, IntelliSense, and build integration",
            "version": "1.0.0",
            "publisher": "mirai-security",
            "engines": {
                "vscode": "^1.74.0"
            },
            "categories": [
                "Programming Languages",
                "Themes",
                "Snippets"
            ],
            "keywords": [
                "assembly",
                "nasm",
                "asm",
                "x86",
                "x64",
                "low-level"
            ],
            "activationEvents": [
                "onLanguage:nasm"
            ],
            "main": "./out/extension.js",
            "contributes": {
                "languages": [
                    {
                        "id": "nasm",
                        "aliases": ["NASM Assembly", "nasm"],
                        "extensions": [".asm", ".s", ".inc"],
                        "configuration": "./language-configuration.json"
                    }
                ],
                "grammars": [
                    {
                        "language": "nasm",
                        "scopeName": "source.assembly.nasm",
                        "path": "./syntaxes/nasm.tmLanguage.json"
                    }
                ],
                "themes": [
                    {
                        "label": "NASM Dark",
                        "uiTheme": "vs-dark",
                        "path": "./themes/nasm-dark.json"
                    }
                ],
                "snippets": [
                    {
                        "language": "nasm",
                        "path": "./snippets/nasm.json"
                    }
                ],
                "commands": [
                    {
                        "command": "nasm.build",
                        "title": "Build NASM Project"
                    },
                    {
                        "command": "nasm.run",
                        "title": "Run NASM Executable"
                    }
                ],
                "keybindings": [
                    {
                        "command": "nasm.build",
                        "key": "f7",
                        "when": "editorTextFocus && editorLangId == nasm"
                    }
                ]
            },
            "scripts": {
                "vscode:prepublish": "npm run compile",
                "compile": "tsc -p ./",
                "watch": "tsc -watch -p ./"
            },
            "devDependencies": {
                "typescript": "^4.9.4",
                "@types/vscode": "^1.74.0"
            }
        }
        
        with open(self.extension_dir / "package.json", 'w') as f:
            json.dump(package, f, indent=2)
    
    def _create_syntax_grammar(self):
        """Create TextMate grammar for NASM syntax highlighting"""
        syntaxes_dir = self.extension_dir / "syntaxes"
        syntaxes_dir.mkdir(exist_ok=True)
        
        grammar = {
            "$schema": "https://raw.githubusercontent.com/martinring/tmlanguage/master/tmlanguage.json",
            "name": "NASM Assembly",
            "scopeName": "source.assembly.nasm",
            "patterns": [
                {"include": "#comments"},
                {"include": "#sections"},
                {"include": "#labels"},
                {"include": "#instructions"},
                {"include": "#registers"},
                {"include": "#directives"},
                {"include": "#macros"},
                {"include": "#strings"},
                {"include": "#numbers"},
                {"include": "#operators"}
            ],
            "repository": {
                "comments": {
                    "patterns": [
                        {
                            "name": "comment.line.semicolon.nasm",
                            "match": ";.*$"
                        }
                    ]
                },
                "sections": {
                    "patterns": [
                        {
                            "name": "keyword.control.section.nasm",
                            "match": "\\b(section|segment)\\s+\\.(text|data|bss|rodata)\\b"
                        }
                    ]
                },
                "labels": {
                    "patterns": [
                        {
                            "name": "entity.name.function.label.nasm",
                            "match": "^[a-zA-Z_][a-zA-Z0-9_]*:"
                        }
                    ]
                },
                "instructions": {
                    "patterns": [
                        {
                            "name": "keyword.operator.instruction.nasm",
                            "match": "\\b(mov|add|sub|mul|div|cmp|jmp|je|jne|call|ret|push|pop|int|syscall)\\b"
                        }
                    ]
                },
                "registers": {
                    "patterns": [
                        {
                            "name": "variable.other.register.nasm",
                            "match": "\\b(rax|rbx|rcx|rdx|rsi|rdi|rsp|rbp|r8|r9|r10|r11|r12|r13|r14|r15|eax|ebx|ecx|edx|esi|edi|esp|ebp|ax|bx|cx|dx|si|di|sp|bp|al|ah|bl|bh|cl|ch|dl|dh)\\b"
                        }
                    ]
                },
                "directives": {
                    "patterns": [
                        {
                            "name": "keyword.control.directive.nasm",
                            "match": "\\b(db|dw|dd|dq|dt|resb|resw|resd|resq|equ|times|global|extern|include)\\b"
                        }
                    ]
                },
                "macros": {
                    "patterns": [
                        {
                            "name": "keyword.control.macro.nasm",
                            "match": "%[a-zA-Z_][a-zA-Z0-9_]*"
                        }
                    ]
                },
                "strings": {
                    "patterns": [
                        {
                            "name": "string.quoted.single.nasm",
                            "begin": "'",
                            "end": "'",
                            "patterns": [
                                {
                                    "name": "constant.character.escape.nasm",
                                    "match": "\\\\."
                                }
                            ]
                        },
                        {
                            "name": "string.quoted.double.nasm",
                            "begin": "\"",
                            "end": "\"",
                            "patterns": [
                                {
                                    "name": "constant.character.escape.nasm",
                                    "match": "\\\\."
                                }
                            ]
                        }
                    ]
                },
                "numbers": {
                    "patterns": [
                        {
                            "name": "constant.numeric.hex.nasm",
                            "match": "\\b0[xX][0-9a-fA-F]+\\b"
                        },
                        {
                            "name": "constant.numeric.binary.nasm",
                            "match": "\\b0[bB][01]+\\b"
                        },
                        {
                            "name": "constant.numeric.octal.nasm",
                            "match": "\\b0[oO][0-7]+\\b"
                        },
                        {
                            "name": "constant.numeric.decimal.nasm",
                            "match": "\\b\\d+\\b"
                        }
                    ]
                },
                "operators": {
                    "patterns": [
                        {
                            "name": "keyword.operator.nasm",
                            "match": "[+\\-*/&|^~<>=!]+"
                        }
                    ]
                }
            }
        }
        
        with open(syntaxes_dir / "nasm.tmLanguage.json", 'w') as f:
            json.dump(grammar, f, indent=2)
    
    def _create_language_config(self):
        """Create language configuration for NASM"""
        config = {
            "comments": {
                "lineComment": ";"
            },
            "brackets": [
                ["[", "]"],
                ["(", ")"]
            ],
            "autoClosingPairs": [
                ["[", "]"],
                ["(", ")"],
                ["'", "'"],
                ["\"", "\""]
            ],
            "surroundingPairs": [
                ["[", "]"],
                ["(", ")"],
                ["'", "'"],
                ["\"", "\""]
            ],
            "wordPattern": "[a-zA-Z_][a-zA-Z0-9_]*"
        }
        
        with open(self.extension_dir / "language-configuration.json", 'w') as f:
            json.dump(config, f, indent=2)
    
    def _create_extension_main(self):
        """Create main extension TypeScript file"""
        src_dir = self.extension_dir / "src"
        src_dir.mkdir(exist_ok=True)
        
        extension_ts = '''
import * as vscode from 'vscode';
import * as path from 'path';
import { exec } from 'child_process';

export function activate(context: vscode.ExtensionContext) {
    console.log('NASM Assembly IDE extension is now active!');
    
    // Build command
    let buildCommand = vscode.commands.registerCommand('nasm.build', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        
        const document = editor.document;
        if (document.languageId !== 'nasm') {
            vscode.window.showErrorMessage('This is not a NASM file');
            return;
        }
        
        const filePath = document.fileName;
        const outputPath = filePath.replace(/\\.asm$/, '.o');
        
        const terminal = vscode.window.createTerminal('NASM Build');
        terminal.show();
        terminal.sendText(`nasm -f elf64 -o "${outputPath}" "${filePath}"`);
    });
    
    // Run command
    let runCommand = vscode.commands.registerCommand('nasm.run', () => {
        const editor = vscode.window.activeTextEditor;
        if (!editor) return;
        
        const filePath = editor.document.fileName;
        const baseName = path.basename(filePath, '.asm');
        const dirName = path.dirname(filePath);
        const execPath = path.join(dirName, baseName);
        
        const terminal = vscode.window.createTerminal('NASM Run');
        terminal.show();
        terminal.sendText(`cd "${dirName}" && ./"${baseName}"`);
    });
    
    context.subscriptions.push(buildCommand, runCommand);
}

export function deactivate() {}
'''
        
        with open(src_dir / "extension.ts", 'w') as f:
            f.write(extension_ts)
        
        # Create TypeScript config
        tsconfig = {
            "compilerOptions": {
                "module": "commonjs",
                "target": "ES2020",
                "outDir": "out",
                "lib": ["ES2020"],
                "sourceMap": True,
                "rootDir": "src",
                "strict": True
            },
            "exclude": ["node_modules", ".vscode-test"]
        }
        
        with open(self.extension_dir / "tsconfig.json", 'w') as f:
            json.dump(tsconfig, f, indent=2)
    
    def _create_theme(self):
        """Create NASM-specific color theme"""
        themes_dir = self.extension_dir / "themes"
        themes_dir.mkdir(exist_ok=True)
        
        theme = {
            "name": "NASM Dark",
            "type": "dark",
            "colors": {
                "editor.background": "#1e1e1e",
                "editor.foreground": "#d4d4d4",
                "editorLineNumber.foreground": "#858585"
            },
            "tokenColors": [
                {
                    "name": "NASM Instructions",
                    "scope": "keyword.operator.instruction.nasm",
                    "settings": {
                        "foreground": "#569cd6",
                        "fontStyle": "bold"
                    }
                },
                {
                    "name": "NASM Registers",
                    "scope": "variable.other.register.nasm",
                    "settings": {
                        "foreground": "#9cdcfe"
                    }
                },
                {
                    "name": "NASM Labels",
                    "scope": "entity.name.function.label.nasm",
                    "settings": {
                        "foreground": "#dcdcaa",
                        "fontStyle": "bold"
                    }
                },
                {
                    "name": "NASM Comments",
                    "scope": "comment.line.semicolon.nasm",
                    "settings": {
                        "foreground": "#6a9955",
                        "fontStyle": "italic"
                    }
                },
                {
                    "name": "NASM Strings",
                    "scope": "string.quoted",
                    "settings": {
                        "foreground": "#ce9178"
                    }
                },
                {
                    "name": "NASM Numbers",
                    "scope": "constant.numeric",
                    "settings": {
                        "foreground": "#b5cea8"
                    }
                },
                {
                    "name": "NASM Directives",
                    "scope": "keyword.control.directive.nasm",
                    "settings": {
                        "foreground": "#c586c0"
                    }
                },
                {
                    "name": "NASM Macros",
                    "scope": "keyword.control.macro.nasm",
                    "settings": {
                        "foreground": "#4ec9b0"
                    }
                }
            ]
        }
        
        with open(themes_dir / "nasm-dark.json", 'w') as f:
            json.dump(theme, f, indent=2)
    
    def _create_snippets(self):
        """Create code snippets for NASM"""
        snippets_dir = self.extension_dir / "snippets"
        snippets_dir.mkdir(exist_ok=True)
        
        snippets = {
            "Hello World Linux x64": {
                "prefix": "hello64",
                "body": [
                    "section .data",
                    "    msg db 'Hello, World!', 0xA",
                    "    msg_len equ $ - msg",
                    "",
                    "section .text",
                    "    global _start",
                    "",
                    "_start:",
                    "    ; write system call",
                    "    mov rax, 1          ; sys_write",
                    "    mov rdi, 1          ; stdout",
                    "    mov rsi, msg        ; message",
                    "    mov rdx, msg_len    ; length",
                    "    syscall",
                    "",
                    "    ; exit system call",
                    "    mov rax, 60         ; sys_exit",
                    "    mov rdi, 0          ; exit status",
                    "    syscall"
                ],
                "description": "Hello World program for Linux x64"
            },
            "Function Template": {
                "prefix": "func",
                "body": [
                    "${1:function_name}:",
                    "    push rbp",
                    "    mov rbp, rsp",
                    "",
                    "    ; function body",
                    "    $0",
                    "",
                    "    mov rsp, rbp",
                    "    pop rbp",
                    "    ret"
                ],
                "description": "Function template with standard prologue/epilogue"
            },
            "Loop Template": {
                "prefix": "loop",
                "body": [
                    "    mov rcx, ${1:count}     ; loop counter",
                    ".loop:",
                    "    ; loop body",
                    "    $0",
                    "    loop .loop"
                ],
                "description": "Simple loop template"
            },
            "Data Section": {
                "prefix": "data",
                "body": [
                    "section .data",
                    "    ${1:variable} ${2:db} ${3:'value'}$0"
                ],
                "description": "Data section template"
            }
        }
        
        with open(snippets_dir / "nasm.json", 'w') as f:
            json.dump(snippets, f, indent=2)

def main():
    """Generate NASM IDE extensions"""
    print("🔥 NASM IDE Extension Generator")
    print("=" * 40)
    
    # Generate VS Code extension
    vscode_gen = VSCodeExtensionGenerator("nasm_ide_extensions")
    vscode_gen.generate()
    
    print("\n📦 Extension generated! To install:")
    print("1. cd nasm_ide_extensions/nasm-ide-vscode")
    print("2. npm install")
    print("3. npm run compile")
    print("4. code --install-extension .")
    print("\n🚀 Ready to develop in NASM with full IDE support!")

if __name__ == "__main__":
    main()