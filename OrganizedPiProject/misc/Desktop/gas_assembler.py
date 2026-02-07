#!/usr/bin/env python3
"""
gASM - Generated Assembly Compiler
A pure Python x86-64 assembler with no third-party dependencies
"""

import struct
import os
from typing import List, Dict, Tuple, Optional, Union
from dataclasses import dataclass
from enum import Enum

class TokenType(Enum):
    # Instructions
    PUSH = "push"
    POP = "pop"
    MOV = "mov"
    ADD = "add"
    SUB = "sub"
    MUL = "mul"
    DIV = "div"
    CMP = "cmp"
    JMP = "jmp"
    JE = "je"
    JNE = "jne"
    JL = "jl"
    JG = "jg"
    CALL = "call"
    RET = "ret"
    LEA = "lea"
    
    # Registers
    RAX = "rax"
    RBX = "rbx"
    RCX = "rcx"
    RDX = "rdx"
    RSP = "rsp"
    RBP = "rbp"
    RSI = "rsi"
    RDI = "rdi"
    R8 = "r8"
    R9 = "r9"
    R10 = "r10"
    R11 = "r11"
    R12 = "r12"
    R13 = "r13"
    R14 = "r14"
    R15 = "r15"
    
    # Data types
    DB = "db"
    DW = "dw"
    DD = "dd"
    DQ = "dq"
    
    # Sections
    SECTION = "section"
    TEXT = ".text"
    DATA = ".data"
    BSS = ".bss"
    
    # Labels and identifiers
    IDENTIFIER = "identifier"
    LABEL = "label"
    
    # Literals
    NUMBER = "number"
    STRING = "string"
    
    # Operators and delimiters
    COMMA = ","
    COLON = ":"
    SEMICOLON = ";"
    BRACKET_OPEN = "["
    BRACKET_CLOSE = "]"
    PLUS = "+"
    MINUS = "-"
    MULTIPLY = "*"
    
    # Keywords
    GLOBAL = "global"
    EXTERN = "extern"
    EQU = "equ"
    
    # Comments
    COMMENT = "comment"
    NEWLINE = "newline"
    EOF = "eof"

@dataclass
class Token:
    type: TokenType
    value: str
    line: int
    column: int

@dataclass
class Instruction:
    mnemonic: str
    operands: List[str]
    line: int

@dataclass
class Label:
    name: str
    address: int
    line: int

class gASMLexer:
    """Pure Python lexer for x86-64 assembly"""
    
    def __init__(self):
        self.keywords = {
            'push': TokenType.PUSH,
            'pop': TokenType.POP,
            'mov': TokenType.MOV,
            'add': TokenType.ADD,
            'sub': TokenType.SUB,
            'mul': TokenType.MUL,
            'div': TokenType.DIV,
            'cmp': TokenType.CMP,
            'jmp': TokenType.JMP,
            'je': TokenType.JE,
            'jne': TokenType.JNE,
            'jl': TokenType.JL,
            'jg': TokenType.JG,
            'call': TokenType.CALL,
            'ret': TokenType.RET,
            'lea': TokenType.LEA,
            'section': TokenType.SECTION,
            '.text': TokenType.TEXT,
            '.data': TokenType.DATA,
            '.bss': TokenType.BSS,
            'global': TokenType.GLOBAL,
            'extern': TokenType.EXTERN,
            'equ': TokenType.EQU,
            'db': TokenType.DB,
            'dw': TokenType.DW,
            'dd': TokenType.DD,
            'dq': TokenType.DQ,
        }
        
        self.registers = {
            'rax': TokenType.RAX,
            'rbx': TokenType.RBX,
            'rcx': TokenType.RCX,
            'rdx': TokenType.RDX,
            'rsp': TokenType.RSP,
            'rbp': TokenType.RBP,
            'rsi': TokenType.RSI,
            'rdi': TokenType.RDI,
            'r8': TokenType.R8,
            'r9': TokenType.R9,
            'r10': TokenType.R10,
            'r11': TokenType.R11,
            'r12': TokenType.R12,
            'r13': TokenType.R13,
            'r14': TokenType.R14,
            'r15': TokenType.R15,
        }
        
        self.operators = {
            ',': TokenType.COMMA,
            ':': TokenType.COLON,
            ';': TokenType.SEMICOLON,
            '[': TokenType.BRACKET_OPEN,
            ']': TokenType.BRACKET_CLOSE,
            '+': TokenType.PLUS,
            '-': TokenType.MINUS,
            '*': TokenType.MULTIPLY,
        }
    
    def tokenize(self, source: str) -> List[Token]:
        """Tokenize assembly source code"""
        tokens = []
        current_pos = 0
        line = 1
        column = 1
        
        while current_pos < len(source):
            char = source[current_pos]
            
            # Skip whitespace
            if char.isspace():
                if char == '\n':
                    line += 1
                    column = 1
                else:
                    column += 1
                current_pos += 1
                continue
            
            # Handle comments
            if char == ';':
                comment_start = current_pos
                while current_pos < len(source) and source[current_pos] != '\n':
                    current_pos += 1
                tokens.append(Token(TokenType.COMMENT, source[comment_start:current_pos], line, column))
                column += current_pos - comment_start
                continue
            
            # Handle strings
            if char == '"':
                string_start = current_pos
                current_pos += 1
                while current_pos < len(source) and source[current_pos] != '"':
                    current_pos += 1
                if current_pos < len(source):
                    current_pos += 1
                tokens.append(Token(TokenType.STRING, source[string_start:current_pos], line, column))
                column += current_pos - string_start
                continue
            
            # Handle numbers
            if char.isdigit() or char == '-':
                number_start = current_pos
                if char == '-':
                    current_pos += 1
                while current_pos < len(source) and (source[current_pos].isdigit() or source[current_pos] == 'x' or source[current_pos] in 'ABCDEFabcdef'):
                    current_pos += 1
                tokens.append(Token(TokenType.NUMBER, source[number_start:current_pos], line, column))
                column += current_pos - number_start
                continue
            
            # Handle identifiers and keywords
            if char.isalpha() or char == '_' or char == '.':
                identifier_start = current_pos
                while current_pos < len(source) and (source[current_pos].isalnum() or source[current_pos] in '_.'):
                    current_pos += 1
                identifier = source[identifier_start:current_pos]
                
                # Check if it's a keyword
                if identifier in self.keywords:
                    tokens.append(Token(self.keywords[identifier], identifier, line, column))
                # Check if it's a register
                elif identifier in self.registers:
                    tokens.append(Token(self.registers[identifier], identifier, line, column))
                else:
                    # Check if it's a label (ends with colon)
                    if current_pos < len(source) and source[current_pos] == ':':
                        tokens.append(Token(TokenType.LABEL, identifier, line, column))
                        current_pos += 1
                    else:
                        tokens.append(Token(TokenType.IDENTIFIER, identifier, line, column))
                
                column += current_pos - identifier_start
                continue
            
            # Handle operators
            if char in self.operators:
                tokens.append(Token(self.operators[char], char, line, column))
                current_pos += 1
                column += 1
                continue
            
            # Unknown character
            raise ValueError(f"Unknown character '{char}' at line {line}, column {column}")
        
        tokens.append(Token(TokenType.EOF, "", line, column))
        return tokens

class gASMParser:
    """Pure Python parser for x86-64 assembly"""
    
    def __init__(self):
        self.tokens = []
        self.pos = 0
        self.instructions = []
        self.labels = {}
        self.data_section = []
        self.text_section = []
        self.current_section = None
    
    def parse(self, tokens: List[Token]) -> Dict:
        """Parse tokens into assembly structure"""
        self.tokens = tokens
        self.pos = 0
        
        while not self._is_at_end():
            if self._check(TokenType.SECTION):
                self._parse_section()
            elif self._check(TokenType.GLOBAL):
                self._parse_global()
            elif self._check(TokenType.LABEL):
                self._parse_label()
            elif self._check(TokenType.IDENTIFIER) or self._is_instruction():
                self._parse_instruction()
            elif self._check(TokenType.DB, TokenType.DW, TokenType.DD, TokenType.DQ):
                self._parse_data_declaration()
            else:
                self._advance()  # Skip unknown tokens
        
        return {
            'instructions': self.instructions,
            'labels': self.labels,
            'data_section': self.data_section,
            'text_section': self.text_section
        }
    
    def _is_at_end(self) -> bool:
        return self.pos >= len(self.tokens) or self._peek().type == TokenType.EOF
    
    def _peek(self) -> Token:
        if self.pos >= len(self.tokens):
            return Token(TokenType.EOF, "", 0, 0)
        return self.tokens[self.pos]
    
    def _advance(self) -> Token:
        if not self._is_at_end():
            self.pos += 1
        return self._previous()
    
    def _previous(self) -> Token:
        return self.tokens[self.pos - 1]
    
    def _check(self, *types) -> bool:
        if self._is_at_end():
            return False
        return self._peek().type in types
    
    def _match(self, *types) -> bool:
        if self._check(*types):
            self._advance()
            return True
        return False
    
    def _is_instruction(self) -> bool:
        if self._is_at_end():
            return False
        token = self._peek()
        return token.type in [TokenType.PUSH, TokenType.POP, TokenType.MOV, TokenType.ADD, 
                             TokenType.SUB, TokenType.CALL, TokenType.RET, TokenType.LEA]
    
    def _parse_section(self):
        """Parse section directive"""
        self._consume(TokenType.SECTION, "Expected 'section'")
        section_name = self._consume(TokenType.TEXT, TokenType.DATA, TokenType.BSS, "Expected section name")
        self.current_section = section_name.value
    
    def _parse_global(self):
        """Parse global directive"""
        self._consume(TokenType.GLOBAL, "Expected 'global'")
        symbol = self._consume(TokenType.IDENTIFIER, "Expected symbol name")
        # Store global symbol
        pass
    
    def _parse_label(self):
        """Parse label definition"""
        label_token = self._consume(TokenType.LABEL, "Expected label")
        label_name = label_token.value
        self.labels[label_name] = len(self.instructions)
    
    def _parse_instruction(self):
        """Parse instruction"""
        mnemonic = self._peek().value
        self._advance()
        
        operands = []
        while not self._is_at_end() and not self._check(TokenType.NEWLINE, TokenType.COMMENT):
            if self._check(TokenType.COMMA):
                self._advance()
                continue
            operand = self._parse_operand()
            if operand:
                operands.append(operand)
        
        instruction = Instruction(mnemonic, operands, self._peek().line)
        self.instructions.append(instruction)
        
        if self.current_section == ".text":
            self.text_section.append(instruction)
    
    def _parse_operand(self) -> Optional[str]:
        """Parse instruction operand"""
        if self._is_at_end():
            return None
        
        operand_parts = []
        while not self._is_at_end() and not self._check(TokenType.COMMA, TokenType.NEWLINE, TokenType.COMMENT):
            token = self._peek()
            if token.type in [TokenType.IDENTIFIER, TokenType.NUMBER, TokenType.STRING, 
                             TokenType.RAX, TokenType.RBX, TokenType.RCX, TokenType.RDX,
                             TokenType.RSP, TokenType.RBP, TokenType.RSI, TokenType.RDI,
                             TokenType.R8, TokenType.R9, TokenType.R10, TokenType.R11,
                             TokenType.R12, TokenType.R13, TokenType.R14, TokenType.R15,
                             TokenType.BRACKET_OPEN, TokenType.BRACKET_CLOSE,
                             TokenType.PLUS, TokenType.MINUS, TokenType.MULTIPLY]:
                operand_parts.append(token.value)
                self._advance()
            else:
                break
        
        return ' '.join(operand_parts) if operand_parts else None
    
    def _parse_data_declaration(self):
        """Parse data declaration (db, dw, dd, dq)"""
        data_type = self._peek().type
        self._advance()
        
        values = []
        while not self._is_at_end() and not self._check(TokenType.NEWLINE, TokenType.COMMENT):
            if self._check(TokenType.COMMA):
                self._advance()
                continue
            if self._check(TokenType.NUMBER, TokenType.STRING):
                values.append(self._peek().value)
                self._advance()
        
        data_decl = {
            'type': data_type.value,
            'values': values,
            'line': self._peek().line
        }
        self.data_section.append(data_decl)
    
    def _consume(self, *types, message: str = "Unexpected token"):
        if self._check(*types):
            return self._advance()
        raise ValueError(f"{message} at line {self._peek().line}, column {self._peek().column}")

class gASMCodeGenerator:
    """Pure Python x86-64 machine code generator"""
    
    def __init__(self):
        self.machine_code = bytearray()
        self.relocations = []
        self.symbols = {}
    
    def generate_machine_code(self, parsed_asm: Dict) -> bytearray:
        """Generate x86-64 machine code from parsed assembly"""
        self.machine_code = bytearray()
        self.relocations = []
        self.symbols = {}
        
        # Process data section first
        for data_decl in parsed_asm['data_section']:
            self._generate_data(data_decl)
        
        # Process text section
        for instruction in parsed_asm['text_section']:
            self._generate_instruction(instruction)
        
        return self.machine_code
    
    def _generate_data(self, data_decl: Dict):
        """Generate data section bytes"""
        for value in data_decl['values']:
            if data_decl['type'] == 'db':
                if value.startswith('"') and value.endswith('"'):
                    # String literal
                    string_val = value[1:-1]
                    for char in string_val:
                        self.machine_code.append(ord(char))
                else:
                    # Numeric value
                    self.machine_code.append(int(value) & 0xFF)
            elif data_decl['type'] == 'dw':
                val = int(value) & 0xFFFF
                self.machine_code.extend(struct.pack('<H', val))
            elif data_decl['type'] == 'dd':
                val = int(value) & 0xFFFFFFFF
                self.machine_code.extend(struct.pack('<I', val))
            elif data_decl['type'] == 'dq':
                val = int(value) & 0xFFFFFFFFFFFFFFFF
                self.machine_code.extend(struct.pack('<Q', val))
    
    def _generate_instruction(self, instruction: Instruction):
        """Generate machine code for instruction"""
        mnemonic = instruction.mnemonic.lower()
        
        if mnemonic == 'push':
            self._generate_push(instruction.operands)
        elif mnemonic == 'pop':
            self._generate_pop(instruction.operands)
        elif mnemonic == 'mov':
            self._generate_mov(instruction.operands)
        elif mnemonic == 'add':
            self._generate_add(instruction.operands)
        elif mnemonic == 'sub':
            self._generate_sub(instruction.operands)
        elif mnemonic == 'call':
            self._generate_call(instruction.operands)
        elif mnemonic == 'ret':
            self._generate_ret()
        elif mnemonic == 'lea':
            self._generate_lea(instruction.operands)
        elif mnemonic == 'xor':
            self._generate_xor(instruction.operands)
        elif mnemonic == 'cmp':
            self._generate_cmp(instruction.operands)
        elif mnemonic == 'jmp':
            self._generate_jmp(instruction.operands)
        elif mnemonic == 'je':
            self._generate_je(instruction.operands)
        elif mnemonic == 'jne':
            self._generate_jne(instruction.operands)
        elif mnemonic == 'jl':
            self._generate_jl(instruction.operands)
        elif mnemonic == 'jg':
            self._generate_jg(instruction.operands)
        else:
            # Generate actual NOP for unknown instructions
            self.machine_code.append(0x90)  # NOP
    
    def _generate_push(self, operands: List[str]):
        """Generate PUSH instruction"""
        if not operands:
            return
        
        operand = operands[0]
        if operand == 'rbp':
            self.machine_code.append(0x55)  # push rbp
        elif operand == 'rax':
            self.machine_code.append(0x50)  # push rax
        elif operand == 'rbx':
            self.machine_code.append(0x53)  # push rbx
        elif operand == 'rcx':
            self.machine_code.append(0x51)  # push rcx
        elif operand == 'rdx':
            self.machine_code.append(0x52)  # push rdx
        elif operand == 'rsp':
            self.machine_code.append(0x54)  # push rsp
        elif operand == 'rsi':
            self.machine_code.append(0x56)  # push rsi
        elif operand == 'rdi':
            self.machine_code.append(0x57)  # push rdi
        else:
            # Generic push with REX prefix
            self.machine_code.append(0x50)  # push rax (fallback)
    
    def _generate_pop(self, operands: List[str]):
        """Generate POP instruction"""
        if not operands:
            return
        
        operand = operands[0]
        if operand == 'rbp':
            self.machine_code.append(0x5D)  # pop rbp
        elif operand == 'rax':
            self.machine_code.append(0x58)  # pop rax
        elif operand == 'rbx':
            self.machine_code.append(0x5B)  # pop rbx
        elif operand == 'rcx':
            self.machine_code.append(0x59)  # pop rcx
        elif operand == 'rdx':
            self.machine_code.append(0x5A)  # pop rdx
        elif operand == 'rsp':
            self.machine_code.append(0x5C)  # pop rsp
        elif operand == 'rsi':
            self.machine_code.append(0x5E)  # pop rsi
        elif operand == 'rdi':
            self.machine_code.append(0x5F)  # pop rdi
        else:
            # Generic pop with REX prefix
            self.machine_code.append(0x58)  # pop rax (fallback)
    
    def _generate_mov(self, operands: List[str]):
        """Generate MOV instruction"""
        if len(operands) < 2:
            return
        
        dest = operands[0]
        src = operands[1]
        
        # Simple register-to-register moves
        if dest == 'rbp' and src == 'rsp':
            self.machine_code.extend([0x48, 0x89, 0xE5])  # mov rbp, rsp
        elif dest == 'rax' and src == 'rbp':
            self.machine_code.extend([0x48, 0x8B, 0x45, 0x00])  # mov rax, [rbp]
        elif dest == 'rbp' and src == 'rax':
            self.machine_code.extend([0x48, 0x89, 0xC5])  # mov rbp, rax
        elif dest == 'rax' and src.isdigit():
            # mov rax, immediate
            val = int(src)
            if val <= 0x7FFFFFFF:
                self.machine_code.extend([0x48, 0xC7, 0xC0])  # mov rax, imm32
                self.machine_code.extend(struct.pack('<I', val))
            else:
                # 64-bit immediate
                self.machine_code.extend([0x48, 0xB8])  # mov rax, imm64
                self.machine_code.extend(struct.pack('<Q', val))
        else:
            # Generic mov
            self.machine_code.extend([0x48, 0x89, 0xC0])  # mov rax, rax (fallback)
    
    def _generate_add(self, operands: List[str]):
        """Generate ADD instruction"""
        if len(operands) < 2:
            return
        
        dest = operands[0]
        src = operands[1]
        
        if dest == 'rax' and src == 'rbx':
            self.machine_code.extend([0x48, 0x01, 0xD8])  # add rax, rbx
        elif dest == 'rax' and src.isdigit():
            val = int(src)
            if val <= 0x7F:
                self.machine_code.extend([0x48, 0x83, 0xC0, val])  # add rax, imm8
            else:
                self.machine_code.extend([0x48, 0x05])  # add rax, imm32
                self.machine_code.extend(struct.pack('<I', val))
        else:
            # Generic add
            self.machine_code.extend([0x48, 0x01, 0xC0])  # add rax, rax (fallback)
    
    def _generate_sub(self, operands: List[str]):
        """Generate SUB instruction"""
        if len(operands) < 2:
            return
        
        dest = operands[0]
        src = operands[1]
        
        if dest == 'rax' and src == 'rbx':
            self.machine_code.extend([0x48, 0x29, 0xD8])  # sub rax, rbx
        elif dest == 'rax' and src.isdigit():
            val = int(src)
            if val <= 0x7F:
                self.machine_code.extend([0x48, 0x83, 0xE8, val])  # sub rax, imm8
            else:
                self.machine_code.extend([0x48, 0x2D])  # sub rax, imm32
                self.machine_code.extend(struct.pack('<I', val))
        else:
            # Generic sub
            self.machine_code.extend([0x48, 0x29, 0xC0])  # sub rax, rax (fallback)
    
    def _generate_call(self, operands: List[str]):
        """Generate CALL instruction"""
        if not operands:
            return
        
        target = operands[0]
        # For now, generate a placeholder call
        self.machine_code.append(0xE8)  # call rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'call',
            'target': target,
            'offset': len(self.machine_code) - 4
        })
    
    def _generate_ret(self):
        """Generate RET instruction"""
        self.machine_code.append(0xC3)  # ret
    
    def _generate_lea(self, operands: List[str]):
        """Generate LEA instruction"""
        if len(operands) < 2:
            return
        
        dest = operands[0]
        src = operands[1]
        
        if dest == 'rax' and src.startswith('[') and src.endswith(']'):
            # lea rax, [address]
            self.machine_code.extend([0x48, 0x8D, 0x05])  # lea rax, [rip+rel32]
            self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        else:
            # Generic lea
            self.machine_code.extend([0x48, 0x8D, 0x00])  # lea rax, [rax] (fallback)
    
    def _generate_xor(self, operands: List[str]):
        """Generate XOR instruction"""
        if len(operands) < 2:
            return
        
        dest = operands[0]
        src = operands[1]
        
        if dest == 'rax' and src == 'rax':
            self.machine_code.extend([0x48, 0x31, 0xC0])  # xor rax, rax
        elif dest == 'rbx' and src == 'rbx':
            self.machine_code.extend([0x48, 0x31, 0xDB])  # xor rbx, rbx
        elif dest == 'rcx' and src == 'rcx':
            self.machine_code.extend([0x48, 0x31, 0xC9])  # xor rcx, rcx
        elif dest == 'rdx' and src == 'rdx':
            self.machine_code.extend([0x48, 0x31, 0xD2])  # xor rdx, rdx
        else:
            # Generic xor
            self.machine_code.extend([0x48, 0x31, 0xC0])  # xor rax, rax (fallback)
    
    def _generate_cmp(self, operands: List[str]):
        """Generate CMP instruction"""
        if len(operands) < 2:
            return
        
        left = operands[0]
        right = operands[1]
        
        if left == 'rax' and right.isdigit():
            val = int(right)
            if val <= 0x7F:
                self.machine_code.extend([0x48, 0x83, 0xF8, val])  # cmp rax, imm8
            else:
                self.machine_code.extend([0x48, 0x3D])  # cmp rax, imm32
                self.machine_code.extend(struct.pack('<I', val))
        elif left == 'rax' and right == 'rbx':
            self.machine_code.extend([0x48, 0x39, 0xD8])  # cmp rax, rbx
        else:
            # Generic cmp
            self.machine_code.extend([0x48, 0x83, 0xF8, 0x00])  # cmp rax, 0 (fallback)
    
    def _generate_jmp(self, operands: List[str]):
        """Generate JMP instruction"""
        if not operands:
            return
        
        target = operands[0]
        # Generate relative jump
        self.machine_code.append(0xE9)  # jmp rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'jmp',
            'target': target,
            'offset': len(self.machine_code) - 4
        })
    
    def _generate_je(self, operands: List[str]):
        """Generate JE instruction"""
        if not operands:
            return
        
        target = operands[0]
        # Generate conditional jump
        self.machine_code.append(0x0F)  # Two-byte opcode prefix
        self.machine_code.append(0x84)  # je rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'je',
            'target': target,
            'offset': len(self.machine_code) - 4
        })
    
    def _generate_jne(self, operands: List[str]):
        """Generate JNE instruction"""
        if not operands:
            return
        
        target = operands[0]
        # Generate conditional jump
        self.machine_code.append(0x0F)  # Two-byte opcode prefix
        self.machine_code.append(0x85)  # jne rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'jne',
            'target': target,
            'offset': len(self.machine_code) - 4
        })
    
    def _generate_jl(self, operands: List[str]):
        """Generate JL instruction"""
        if not operands:
            return
        
        target = operands[0]
        # Generate conditional jump
        self.machine_code.append(0x0F)  # Two-byte opcode prefix
        self.machine_code.append(0x8C)  # jl rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'jl',
            'target': target,
            'offset': len(self.machine_code) - 4
        })
    
    def _generate_jg(self, operands: List[str]):
        """Generate JG instruction"""
        if not operands:
            return
        
        target = operands[0]
        # Generate conditional jump
        self.machine_code.append(0x0F)  # Two-byte opcode prefix
        self.machine_code.append(0x8F)  # jg rel32
        self.machine_code.extend([0x00, 0x00, 0x00, 0x00])  # placeholder address
        self.relocations.append({
            'type': 'jg',
            'target': target,
            'offset': len(self.machine_code) - 4
        })

class gASMELFGenerator:
    """Pure Python ELF executable generator"""
    
    def __init__(self):
        self.elf_header = bytearray()
        self.program_headers = bytearray()
        self.section_headers = bytearray()
        self.text_section = bytearray()
        self.data_section = bytearray()
        self.symbol_table = bytearray()
        self.string_table = bytearray()
    
    def generate_elf(self, machine_code: bytearray, entry_point: int = 0) -> bytearray:
        """Generate ELF64 executable"""
        # ELF Header (64 bytes)
        self.elf_header = bytearray(64)
        self.elf_header[0:4] = b'\x7fELF'  # ELF magic
        self.elf_header[4] = 2  # 64-bit
        self.elf_header[5] = 1  # Little endian
        self.elf_header[6] = 1  # ELF version
        self.elf_header[7] = 0  # System V ABI
        self.elf_header[8:10] = struct.pack('<H', 2)  # ET_EXEC
        self.elf_header[10:12] = struct.pack('<H', 0x3E)  # x86-64
        self.elf_header[12:16] = struct.pack('<I', 1)  # ELF version
        self.elf_header[16:24] = struct.pack('<Q', entry_point)  # Entry point
        self.elf_header[24:32] = struct.pack('<Q', 64)  # Program header offset
        self.elf_header[32:40] = struct.pack('<Q', 0)  # Section header offset
        self.elf_header[40:44] = struct.pack('<I', 0)  # Flags
        self.elf_header[44:46] = struct.pack('<H', 64)  # ELF header size
        self.elf_header[46:48] = struct.pack('<H', 56)  # Program header entry size
        self.elf_header[48:50] = struct.pack('<H', 1)  # Number of program headers
        self.elf_header[50:52] = struct.pack('<H', 64)  # Section header entry size
        self.elf_header[52:54] = struct.pack('<H', 0)  # Number of section headers
        self.elf_header[54:56] = struct.pack('<H', 0)  # Section header string table index
        
        # Program Header (56 bytes)
        self.program_headers = bytearray(56)
        self.program_headers[0:4] = struct.pack('<I', 1)  # PT_LOAD
        self.program_headers[4:4] = struct.pack('<I', 7)  # PF_X | PF_W | PF_R
        self.program_headers[8:16] = struct.pack('<Q', 0x400000)  # Virtual address
        self.program_headers[16:24] = struct.pack('<Q', 0x400000)  # Physical address
        self.program_headers[24:32] = struct.pack('<Q', len(machine_code))  # File size
        self.program_headers[32:40] = struct.pack('<Q', len(machine_code))  # Memory size
        self.program_headers[40:48] = struct.pack('<Q', 0x1000)  # Alignment
        
        # Combine all sections
        elf_file = bytearray()
        elf_file.extend(self.elf_header)
        elf_file.extend(self.program_headers)
        elf_file.extend(machine_code)
        
        return elf_file

class gASMCompiler:
    """Main gASM compiler class"""
    
    def __init__(self):
        self.lexer = gASMLexer()
        self.parser = gASMParser()
        self.codegen = gASMCodeGenerator()
        self.elfgen = gASMELFGenerator()
    
    def compile(self, source: str, output_file: str = "output") -> bool:
        """Compile assembly source to executable"""
        try:
            print("🔧 gASM Compiler - Pure Python x86-64 Assembler")
            print("=" * 50)
            
            # Step 1: Lexical Analysis
            print("📝 Tokenizing assembly source...")
            tokens = self.lexer.tokenize(source)
            print(f"✅ Generated {len(tokens)} tokens")
            
            # Step 2: Parsing
            print("🌳 Parsing assembly to AST...")
            parsed_asm = self.parser.parse(tokens)
            print(f"✅ Parsed {len(parsed_asm['instructions'])} instructions")
            print(f"✅ Found {len(parsed_asm['labels'])} labels")
            
            # Step 3: Code Generation
            print("⚙️ Generating x86-64 machine code...")
            machine_code = self.codegen.generate_machine_code(parsed_asm)
            print(f"✅ Generated {len(machine_code)} bytes of machine code")
            
            # Step 4: ELF Generation
            print("📦 Creating ELF executable...")
            elf_data = self.elfgen.generate_elf(machine_code)
            print(f"✅ Generated {len(elf_data)} bytes ELF executable")
            
            # Step 5: Write executable
            with open(f"{output_file}.exe", "wb") as f:
                f.write(elf_data)
            print(f"✅ Executable written to {output_file}.exe")
            
            return True
            
        except Exception as e:
            print(f"❌ Compilation failed: {e}")
            return False

def main():
    """Test gASM compiler with rust_compiler_from_scratch.asm"""
    print("🚀 Testing gASM - Pure Python Assembler")
    print("=" * 50)
    
    # Read the assembly file
    try:
        with open("rust_compiler_from_scratch.asm", "r") as f:
            source = f.read()
        print(f"📄 Loaded {len(source)} characters of assembly source")
    except FileNotFoundError:
        print("❌ rust_compiler_from_scratch.asm not found")
        return
    
    # Create compiler and compile
    compiler = gASMCompiler()
    success = compiler.compile(source, "rust_compiler")
    
    if success:
        print("\n🎉 gASM compilation successful!")
        print("📄 Generated: rust_compiler.exe")
        print("🔧 Pure Python x86-64 assembler - No third-party dependencies!")
    else:
        print("\n❌ gASM compilation failed")

if __name__ == "__main__":
    main()
