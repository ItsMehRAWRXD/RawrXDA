#!/usr/bin/env python3
"""
NASM Assembly Syntax Highlighter & IDE Integration
==================================================
Advanced NASM syntax highlighting, code analysis, and IDE integration tools.
Supports x86, x64, and various NASM-specific features.

Author: Mirai Security Toolkit
Date: November 21, 2025
License: MIT
"""

import re
import json
from typing import Dict, List, Tuple, Set
from dataclasses import dataclass
from enum import Enum

class TokenType(Enum):
    INSTRUCTION = "instruction"
    REGISTER = "register" 
    DIRECTIVE = "directive"
    LABEL = "label"
    COMMENT = "comment"
    STRING = "string"
    NUMBER = "number"
    OPERATOR = "operator"
    MACRO = "macro"
    SECTION = "section"

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int
    length: int

class NASMSyntaxHighlighter:
    """Advanced NASM syntax highlighter with IDE integration"""
    
    def __init__(self):
        self.instructions = {
            # Data Movement
            'mov', 'movb', 'movw', 'movl', 'movq', 'movsx', 'movzx',
            'push', 'pop', 'pusha', 'popa', 'pushf', 'popf',
            'lea', 'lds', 'les', 'lfs', 'lgs', 'lss',
            
            # Arithmetic
            'add', 'adc', 'sub', 'sbb', 'mul', 'imul', 'div', 'idiv',
            'inc', 'dec', 'neg', 'cmp', 'test',
            
            # Logical
            'and', 'or', 'xor', 'not', 'shl', 'shr', 'sal', 'sar',
            'rol', 'ror', 'rcl', 'rcr',
            
            # Control Flow
            'jmp', 'je', 'jz', 'jne', 'jnz', 'jg', 'jl', 'jge', 'jle',
            'ja', 'jb', 'jae', 'jbe', 'jc', 'jnc', 'jo', 'jno',
            'js', 'jns', 'jp', 'jnp', 'call', 'ret', 'retf', 'retn',
            'loop', 'loope', 'loopz', 'loopne', 'loopnz',
            
            # String Operations
            'movs', 'movsb', 'movsw', 'movsd', 'movsq',
            'cmps', 'cmpsb', 'cmpsw', 'cmpsd', 'cmpsq',
            'scas', 'scasb', 'scasw', 'scasd', 'scasq',
            'lods', 'lodsb', 'lodsw', 'lodsd', 'lodsq',
            'stos', 'stosb', 'stosw', 'stosd', 'stosq',
            
            # System
            'int', 'into', 'iret', 'hlt', 'nop', 'wait', 'lock',
            'cli', 'sti', 'cld', 'std', 'lahf', 'sahf',
            
            # SSE/AVX (common ones)
            'movaps', 'movups', 'movss', 'movsd', 'addps', 'addss',
            'subps', 'subss', 'mulps', 'mulss', 'divps', 'divss',
            
            # x64 specific
            'syscall', 'sysret', 'movsxd'
        }
        
        self.registers = {
            # 8-bit registers
            'al', 'bl', 'cl', 'dl', 'ah', 'bh', 'ch', 'dh',
            'r8b', 'r9b', 'r10b', 'r11b', 'r12b', 'r13b', 'r14b', 'r15b',
            'spl', 'bpl', 'sil', 'dil',
            
            # 16-bit registers
            'ax', 'bx', 'cx', 'dx', 'sp', 'bp', 'si', 'di',
            'r8w', 'r9w', 'r10w', 'r11w', 'r12w', 'r13w', 'r14w', 'r15w',
            
            # 32-bit registers
            'eax', 'ebx', 'ecx', 'edx', 'esp', 'ebp', 'esi', 'edi',
            'r8d', 'r9d', 'r10d', 'r11d', 'r12d', 'r13d', 'r14d', 'r15d',
            
            # 64-bit registers
            'rax', 'rbx', 'rcx', 'rdx', 'rsp', 'rbp', 'rsi', 'rdi',
            'r8', 'r9', 'r10', 'r11', 'r12', 'r13', 'r14', 'r15',
            
            # Segment registers
            'cs', 'ds', 'es', 'fs', 'gs', 'ss',
            
            # Control registers
            'cr0', 'cr1', 'cr2', 'cr3', 'cr4', 'cr8',
            
            # Debug registers
            'dr0', 'dr1', 'dr2', 'dr3', 'dr6', 'dr7',
            
            # SSE/XMM registers
            'xmm0', 'xmm1', 'xmm2', 'xmm3', 'xmm4', 'xmm5', 'xmm6', 'xmm7',
            'xmm8', 'xmm9', 'xmm10', 'xmm11', 'xmm12', 'xmm13', 'xmm14', 'xmm15',
            
            # YMM registers (AVX)
            'ymm0', 'ymm1', 'ymm2', 'ymm3', 'ymm4', 'ymm5', 'ymm6', 'ymm7',
            'ymm8', 'ymm9', 'ymm10', 'ymm11', 'ymm12', 'ymm13', 'ymm14', 'ymm15'
        }
        
        self.directives = {
            # Data definition
            'db', 'dw', 'dd', 'dq', 'dt', 'do', 'dy', 'dz',
            'resb', 'resw', 'resd', 'resq', 'rest', 'reso', 'resy', 'resz',
            
            # NASM specific
            'equ', 'times', 'incbin', 'include', 'macro', 'endmacro',
            'struc', 'endstruc', 'istruc', 'iend', 'at',
            
            # Conditional assembly
            '%if', '%ifdef', '%ifndef', '%ifmacro', '%ifnmacro',
            '%else', '%elif', '%endif',
            
            # Preprocessor
            '%define', '%xdefine', '%undef', '%assign', '%strlen',
            '%substr', '%rotate', '%rep', '%endrep', '%exitrep',
            '%push', '%pop', '%repl', '%arg', '%stacksize', '%local',
            
            # Output format
            '%ifidn', '%ifidni', '%ifnidni', '%error', '%warning',
            '%fatal', '%line', '%use'
        }
        
        self.sections = {
            'section', 'segment', '.text', '.data', '.bss', '.rodata'
        }
        
        # Compile regex patterns for efficiency
        self._compile_patterns()
    
    def _compile_patterns(self):
        """Compile regex patterns for tokenization"""
        self.patterns = {
            'comment': re.compile(r';.*$', re.MULTILINE),
            'string': re.compile(r'''(?:'[^']*'|"[^"]*")'''),
            'number': re.compile(r'\b(?:0[xX][0-9a-fA-F]+|0[bB][01]+|0[oO][0-7]+|\d+)\b'),
            'label': re.compile(r'^[a-zA-Z_][a-zA-Z0-9_]*:', re.MULTILINE),
            'directive': re.compile(r'\b(?:' + '|'.join(re.escape(d) for d in self.directives) + r')\b', re.IGNORECASE),
            'instruction': re.compile(r'\b(?:' + '|'.join(re.escape(i) for i in self.instructions) + r')\b', re.IGNORECASE),
            'register': re.compile(r'\b(?:' + '|'.join(re.escape(r) for r in self.registers) + r')\b', re.IGNORECASE),
            'section': re.compile(r'\b(?:' + '|'.join(re.escape(s) for s in self.sections) + r')\b', re.IGNORECASE),
            'operator': re.compile(r'[+\-*/&|^~<>!=]+'),
            'macro': re.compile(r'%[a-zA-Z_][a-zA-Z0-9_]*')
        }
    
    def tokenize(self, code: str) -> List[Token]:
        """Tokenize NASM assembly code"""
        tokens = []
        lines = code.split('\n')
        
        for line_num, line in enumerate(lines, 1):
            column = 0
            remaining = line
            
            while remaining.strip():
                matched = False
                
                # Try each pattern
                for token_type, pattern in self.patterns.items():
                    match = pattern.match(remaining)
                    if match:
                        value = match.group(0)
                        tokens.append(Token(
                            type=TokenType(token_type),
                            value=value,
                            line=line_num,
                            column=column,
                            length=len(value)
                        ))
                        
                        column += match.end()
                        remaining = remaining[match.end():]
                        matched = True
                        break
                
                if not matched:
                    # Skip whitespace or unknown characters
                    if remaining[0].isspace():
                        column += 1
                        remaining = remaining[1:]
                    else:
                        column += 1
                        remaining = remaining[1:]
        
        return tokens
    
    def highlight_html(self, code: str) -> str:
        """Generate HTML with syntax highlighting"""
        tokens = self.tokenize(code)
        
        # CSS classes for different token types
        css_classes = {
            TokenType.INSTRUCTION: 'nasm-instruction',
            TokenType.REGISTER: 'nasm-register',
            TokenType.DIRECTIVE: 'nasm-directive',
            TokenType.LABEL: 'nasm-label',
            TokenType.COMMENT: 'nasm-comment',
            TokenType.STRING: 'nasm-string',
            TokenType.NUMBER: 'nasm-number',
            TokenType.OPERATOR: 'nasm-operator',
            TokenType.MACRO: 'nasm-macro',
            TokenType.SECTION: 'nasm-section'
        }
        
        # Build highlighted HTML
        lines = code.split('\n')
        highlighted_lines = []
        
        for line_num, line in enumerate(lines, 1):
            highlighted_line = line
            line_tokens = [t for t in tokens if t.line == line_num]
            
            # Sort tokens by column in reverse order for replacement
            line_tokens.sort(key=lambda t: t.column, reverse=True)
            
            for token in line_tokens:
                css_class = css_classes.get(token.type, '')
                if css_class:
                    start = token.column
                    end = start + token.length
                    replacement = f'<span class="{css_class}">{token.value}</span>'
                    highlighted_line = highlighted_line[:start] + replacement + highlighted_line[end:]
            
            highlighted_lines.append(highlighted_line)
        
        return '\n'.join(highlighted_lines)
    
    def generate_css(self) -> str:
        """Generate CSS for NASM syntax highlighting"""
        return """
/* NASM Assembly Syntax Highlighting CSS */
.nasm-code {
    font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
    background-color: #1e1e1e;
    color: #d4d4d4;
    padding: 16px;
    border-radius: 8px;
    overflow-x: auto;
}

.nasm-instruction {
    color: #569cd6;  /* Blue - Instructions */
    font-weight: bold;
}

.nasm-register {
    color: #9cdcfe;  /* Light Blue - Registers */
}

.nasm-directive {
    color: #c586c0;  /* Purple - Directives */
}

.nasm-label {
    color: #dcdcaa;  /* Yellow - Labels */
    font-weight: bold;
}

.nasm-comment {
    color: #6a9955;  /* Green - Comments */
    font-style: italic;
}

.nasm-string {
    color: #ce9178;  /* Orange - Strings */
}

.nasm-number {
    color: #b5cea8;  /* Light Green - Numbers */
}

.nasm-operator {
    color: #d4d4d4;  /* White - Operators */
}

.nasm-macro {
    color: #4ec9b0;  /* Cyan - Macros */
}

.nasm-section {
    color: #ffd700;  /* Gold - Sections */
    font-weight: bold;
}

.nasm-line-numbers {
    color: #858585;
    user-select: none;
    padding-right: 12px;
    border-right: 1px solid #3c3c3c;
}
"""

def main():
    """Demo the NASM syntax highlighter"""
    sample_code = """
; NASM Assembly Example - Hello World for Linux x64
section .data
    msg db 'Hello, World!', 0xA    ; our string with newline
    msg_len equ $ - msg              ; length of string

section .text
    global _start

_start:
    ; write system call
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, msg        ; message to write
    mov rdx, msg_len    ; message length
    syscall             ; call kernel
    
    ; exit system call
    mov rax, 60         ; sys_exit
    mov rdi, 0          ; exit status
    syscall             ; call kernel

.loop:
    inc rax
    cmp rax, 10
    jl .loop
    ret
"""
    
    highlighter = NASMSyntaxHighlighter()
    
    # Generate highlighted HTML
    highlighted = highlighter.highlight_html(sample_code)
    css = highlighter.generate_css()
    
    # Save as HTML file
    html_content = f"""
<!DOCTYPE html>
<html>
<head>
    <title>NASM Syntax Highlighting Demo</title>
    <style>
{css}
    </style>
</head>
<body>
    <h1>NASM Assembly Syntax Highlighting</h1>
    <pre class="nasm-code">{highlighted}</pre>
</body>
</html>
"""
    
    print("✅ NASM Syntax Highlighter Ready!")
    print(f"📁 Tokens found: {len(highlighter.tokenize(sample_code))}")
    print("🎨 HTML output generated")
    
    return html_content

if __name__ == "__main__":
    main()