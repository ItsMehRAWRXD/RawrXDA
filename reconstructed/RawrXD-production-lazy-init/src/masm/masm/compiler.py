#!/usr/bin/env python3
"""
RawrXD MASM Minimal Compiler - Pure Python PE64 Generator
No external dependencies except Python stdlib
Generates valid x64 PE executables from simple MASM-like assembly

Usage: python3 masm_compiler.py input.asm output.exe
"""

import sys
import struct
import os

# ============================================================================
# TOKEN TYPES & AST
# ============================================================================
class TokenType:
    EOF = 0
    IDENT = 1
    NUMBER = 2
    COLON = 3
    COMMA = 4
    LBRACKET = 5
    RBRACKET = 6
    PLUS = 7
    MINUS = 8
    MULTIPLY = 9
    DIRECTIVE = 10
    INSTRUCTION = 11
    REGISTER = 12

class ASTNodeType:
    PROGRAM = 0
    INSTRUCTION = 1
    LABEL = 2
    DIRECTIVE = 3
    SECTION = 4

# ============================================================================
# LEXER
# ============================================================================
class Lexer:
    def __init__(self, source):
        self.source = source
        self.pos = 0
        self.line = 1
        self.tokens = []
        self.tokenize()
    
    def current(self):
        if self.pos >= len(self.source):
            return '\0'
        return self.source[self.pos]
    
    def peek(self, offset=1):
        p = self.pos + offset
        if p >= len(self.source):
            return '\0'
        return self.source[p]
    
    def advance(self):
        if self.pos < len(self.source):
            if self.source[self.pos] == '\n':
                self.line += 1
            self.pos += 1
    
    def skip_whitespace(self):
        while self.current() in ' \t\r\n':
            self.advance()
    
    def skip_comment(self):
        if self.current() == ';':
            while self.current() != '\n' and self.current() != '\0':
                self.advance()
    
    def read_identifier(self):
        start = self.pos
        while self.current().isalnum() or self.current() in '_$.':
            self.advance()
        return self.source[start:self.pos]
    
    def read_number(self):
        start = self.pos
        is_hex = False
        
        if self.current() == '0' and self.peek() in 'xX':
            is_hex = True
            self.advance()  # skip 0
            self.advance()  # skip x
        
        while self.current().isdigit() or (is_hex and self.current().lower() in 'abcdef'):
            self.advance()
        
        num_str = self.source[start:self.pos]
        if is_hex:
            return int(num_str, 16)
        else:
            return int(num_str, 10)
    
    def tokenize(self):
        while self.pos < len(self.source):
            self.skip_whitespace()
            self.skip_comment()
            
            if self.pos >= len(self.source):
                break
            
            c = self.current()
            
            if c == ':':
                self.tokens.append((':', TokenType.COLON))
                self.advance()
            elif c == ',':
                self.tokens.append((',', TokenType.COMMA))
                self.advance()
            elif c == '[':
                self.tokens.append(('[', TokenType.LBRACKET))
                self.advance()
            elif c == ']':
                self.tokens.append((']', TokenType.RBRACKET))
                self.advance()
            elif c == '+':
                self.tokens.append(('+', TokenType.PLUS))
                self.advance()
            elif c == '-':
                self.tokens.append(('-', TokenType.MINUS))
                self.advance()
            elif c == '*':
                self.tokens.append(('*', TokenType.MULTIPLY))
                self.advance()
            elif c == '.':
                ident = self.read_identifier()
                self.tokens.append((ident, TokenType.DIRECTIVE))
            elif c.isalpha() or c == '_':
                ident = self.read_identifier()
                ident_upper = ident.upper()
                
                # Check if it's a register
                if ident_upper in ['RAX', 'RCX', 'RDX', 'RBX', 'RSP', 'RBP', 'RSI', 'RDI',
                                   'R8', 'R9', 'R10', 'R11', 'R12', 'R13', 'R14', 'R15',
                                   'EAX', 'ECX', 'EDX', 'EBX', 'ESP', 'EBP', 'ESI', 'EDI']:
                    self.tokens.append((ident_upper, TokenType.REGISTER))
                # Check if it's an instruction
                elif ident_upper in ['MOV', 'XOR', 'ADD', 'SUB', 'RET', 'PUSH', 'POP', 'CALL', 'JMP',
                                     'CMP', 'TEST', 'JE', 'JNE', 'JZ', 'JNZ', 'LEA', 'INT3']:
                    self.tokens.append((ident_upper, TokenType.INSTRUCTION))
                else:
                    self.tokens.append((ident, TokenType.IDENT))
            elif c.isdigit():
                num = self.read_number()
                self.tokens.append((num, TokenType.NUMBER))
            else:
                self.advance()

# ============================================================================
# PARSER
# ============================================================================
class Parser:
    def __init__(self, tokens):
        self.tokens = tokens
        self.pos = 0
        self.ast = []
        self.labels = {}
        self.parse()
    
    def current(self):
        if self.pos >= len(self.tokens):
            return (None, TokenType.EOF)
        return self.tokens[self.pos]
    
    def peek(self, offset=1):
        p = self.pos + offset
        if p >= len(self.tokens):
            return (None, TokenType.EOF)
        return self.tokens[p]
    
    def advance(self):
        self.pos += 1
    
    def expect(self, token_type):
        val, typ = self.current()
        if typ != token_type:
            raise Exception(f"Parse error: expected {token_type}, got {typ}")
        self.advance()
        return val
    
    def parse(self):
        while self.current()[1] != TokenType.EOF:
            val, typ = self.current()
            
            if typ == TokenType.DIRECTIVE:
                self.parse_directive(val)
            elif typ == TokenType.IDENT:
                # Check for label
                if self.peek()[1] == TokenType.COLON:
                    label_name = val
                    self.advance()  # skip ident
                    self.advance()  # skip colon
                    self.labels[label_name] = len(self.ast)
                    node = (ASTNodeType.LABEL, label_name)
                    self.ast.append(node)
                else:
                    self.advance()
            elif typ == TokenType.INSTRUCTION:
                self.parse_instruction(val)
            else:
                self.advance()
    
    def parse_directive(self, directive):
        self.advance()
        
        if directive.upper() == '.CODE':
            self.ast.append((ASTNodeType.SECTION, 'CODE'))
        elif directive.upper() == '.DATA':
            self.ast.append((ASTNodeType.SECTION, 'DATA'))
        elif directive.upper() in ['.ENDS', '.ENDP', '.END']:
            # Ignore terminator directives
            pass
        elif directive.upper() in ['.PROC', '.ENDP']:
            # Ignore procedure markers
            pass
    
    def parse_instruction(self, mnemonic):
        self.advance()
        
        # Parse operands
        operands = []
        while self.current()[1] not in [TokenType.EOF, TokenType.DIRECTIVE, TokenType.IDENT]:
            val, typ = self.current()
            
            if typ == TokenType.COMMA:
                self.advance()
                continue
            elif typ == TokenType.REGISTER:
                operands.append(('REG', val))
                self.advance()
            elif typ == TokenType.NUMBER:
                operands.append(('NUM', val))
                self.advance()
            elif typ == TokenType.IDENT:
                operands.append(('LABEL', val))
                self.advance()
            else:
                break
        
        node = (ASTNodeType.INSTRUCTION, mnemonic, operands)
        self.ast.append(node)

# ============================================================================
# CODE GENERATOR (x64 Encoder)
# ============================================================================
class CodeGen:
    def __init__(self, ast):
        self.ast = ast
        self.code = bytearray()
        self.labels = {}
        self.fixups = []
    
    def generate(self):
        # First pass: collect labels
        offset = 0
        for node in self.ast:
            if node[0] == ASTNodeType.LABEL:
                self.labels[node[1]] = offset
            elif node[0] == ASTNodeType.INSTRUCTION:
                mnemonic, operands = node[1], node[2]
                size = self.estimate_size(mnemonic, operands)
                offset += size
        
        # Second pass: emit code
        for node in self.ast:
            if node[0] == ASTNodeType.INSTRUCTION:
                mnemonic, operands = node[1], node[2]
                self.emit_instruction(mnemonic, operands)
        
        # Third pass: resolve fixups
        for fixup_offset, target_label in self.fixups:
            if target_label in self.labels:
                target = self.labels[target_label]
                current = fixup_offset + 4
                rel32 = (target - current) & 0xFFFFFFFF
                self.code[fixup_offset:fixup_offset+4] = struct.pack('<I', rel32)
        
        return bytes(self.code)
    
    def estimate_size(self, mnemonic, operands):
        """Rough estimate of instruction size for first pass"""
        if mnemonic in ['MOV', 'ADD', 'SUB', 'XOR', 'CMP', 'TEST']:
            return 4 if len(operands) == 2 else 1
        elif mnemonic in ['PUSH', 'POP']:
            return 2
        elif mnemonic in ['CALL', 'JMP', 'JE', 'JNE', 'JZ', 'JNZ']:
            return 5
        elif mnemonic == 'RET':
            return 1
        elif mnemonic == 'INT3':
            return 1
        return 1
    
    def emit_instruction(self, mnemonic, operands):
        mnemonic = mnemonic.upper()
        
        if mnemonic == 'RET':
            self.code.append(0xC3)
        elif mnemonic == 'INT3':
            self.code.append(0xCC)
        elif mnemonic == 'NOP':
            self.code.append(0x90)
        
        elif mnemonic == 'PUSH':
            reg = self.reg_code(operands[0][1])
            if reg >= 8:
                self.code.append(0x41)  # REX.B
                reg -= 8
            self.code.append(0x50 + reg)
        
        elif mnemonic == 'POP':
            reg = self.reg_code(operands[0][1])
            if reg >= 8:
                self.code.append(0x41)  # REX.B
                reg -= 8
            self.code.append(0x58 + reg)
        
        elif mnemonic == 'MOV':
            self.emit_mov(operands)
        
        elif mnemonic == 'XOR':
            self.emit_alu_reg_reg(0x31, operands)
        
        elif mnemonic == 'ADD':
            self.emit_alu_reg_reg(0x01, operands)
        
        elif mnemonic == 'SUB':
            self.emit_alu_reg_reg(0x29, operands)
        
        elif mnemonic == 'CMP':
            self.emit_alu_reg_reg(0x39, operands)
        
        elif mnemonic == 'TEST':
            self.emit_alu_reg_reg(0x85, operands)
        
        elif mnemonic in ['CALL', 'JMP', 'JE', 'JNE', 'JZ', 'JNZ']:
            self.emit_branch(mnemonic, operands)
        
        elif mnemonic == 'LEA':
            self.emit_lea(operands)
    
    def emit_mov(self, operands):
        """MOV instruction encoding"""
        if len(operands) != 2:
            return
        
        dst_type, dst = operands[0]
        src_type, src = operands[1]
        
        if dst_type == 'REG' and src_type == 'REG':
            # MOV r64, r64
            self.emit_alu_reg_reg(0x89, operands)
        
        elif dst_type == 'REG' and src_type == 'NUM':
            # MOV r64, imm64
            dst_reg = self.reg_code(dst)
            imm = src if isinstance(src, int) else int(src)
            
            # REX.W prefix
            rex = 0x48
            if dst_reg >= 8:
                rex |= 0x01  # REX.B
                dst_reg -= 8
            
            self.code.append(rex)
            self.code.append(0xB8 + dst_reg)
            self.code.extend(struct.pack('<Q', imm))
    
    def emit_alu_reg_reg(self, opcode, operands):
        """Generic ALU reg,reg instruction"""
        if len(operands) < 2:
            return
        
        dst = self.reg_code(operands[0][1])
        src = self.reg_code(operands[1][1])
        
        rex = 0x48  # REX.W for 64-bit
        if dst >= 8:
            rex |= 0x04  # REX.R
            dst -= 8
        if src >= 8:
            rex |= 0x01  # REX.B
            src -= 8
        
        self.code.append(rex)
        self.code.append(opcode)
        
        # ModR/M: mod=11 (reg-to-reg), reg=src, r/m=dst
        modrm = 0xC0 | (src << 3) | dst
        self.code.append(modrm)
    
    def emit_branch(self, mnemonic, operands):
        """JMP/CALL/Jcc instruction encoding"""
        opcodes = {
            'CALL': 0xE8,
            'JMP': 0xE9,
            'JE': 0x84,   # Conditional needs 0x0F prefix
            'JNE': 0x85,
            'JZ': 0x84,
            'JNZ': 0x85,
        }
        
        if len(operands) < 1:
            return
        
        target_type, target = operands[0]
        
        if mnemonic in ['JE', 'JNE', 'JZ', 'JNZ']:
            # Two-byte conditional jump
            self.code.append(0x0F)
            self.code.append(opcodes[mnemonic])
        else:
            self.code.append(opcodes.get(mnemonic, 0xE8))
        
        # Placeholder for rel32
        self.fixups.append((len(self.code), target))
        self.code.extend(b'\x00\x00\x00\x00')
    
    def emit_lea(self, operands):
        """LEA instruction (simplified - reg only)"""
        if len(operands) != 2:
            return
        
        dst = self.reg_code(operands[0][1])
        src = self.reg_code(operands[1][1])
        
        rex = 0x48
        if dst >= 8:
            rex |= 0x04
            dst -= 8
        if src >= 8:
            rex |= 0x01
            src -= 8
        
        self.code.append(rex)
        self.code.append(0x8D)
        modrm = 0xC0 | (dst << 3) | src
        self.code.append(modrm)
    
    @staticmethod
    def reg_code(reg_name):
        """Map register name to code (0-15)"""
        regs = {
            'RAX': 0, 'RCX': 1, 'RDX': 2, 'RBX': 3,
            'RSP': 4, 'RBP': 5, 'RSI': 6, 'RDI': 7,
            'R8': 8, 'R9': 9, 'R10': 10, 'R11': 11,
            'R12': 12, 'R13': 13, 'R14': 14, 'R15': 15,
            'EAX': 0, 'ECX': 1, 'EDX': 2, 'EBX': 3,
            'ESP': 4, 'EBP': 5, 'ESI': 6, 'EDI': 7,
        }
        return regs.get(reg_name.upper(), 0)

# ============================================================================
# PE64 FILE WRITER
# ============================================================================
class PE64Writer:
    def __init__(self, code, entry_offset=0):
        self.code = code
        self.entry_offset = entry_offset
    
    def write(self, filename):
        pe = bytearray()
        
        # DOS stub (minimal)
        dos = bytearray(64)
        dos[0:2] = b'MZ'
        dos[0x3C:0x40] = struct.pack('<I', 0x40)  # PE offset at 0x40
        pe.extend(dos)
        
        # PE signature
        pe.extend(b'PE\x00\x00')
        
        # COFF header
        pe.extend(struct.pack('<H', 0x8664))  # Machine (x64)
        pe.extend(struct.pack('<H', 1))       # Number of sections
        pe.extend(struct.pack('<I', 0))       # Timestamp
        pe.extend(struct.pack('<I', 0))       # Pointer to symbol table
        pe.extend(struct.pack('<I', 0))       # Number of symbols
        pe.extend(struct.pack('<H', 240))     # Size of optional header
        pe.extend(struct.pack('<H', 0x22))    # Characteristics
        
        # Optional header (PE32+)
        pe.extend(struct.pack('<H', 0x020B)) # Magic (PE32+)
        pe.extend(struct.pack('<I', 0))      # Linker version
        
        # Sizes
        code_size = len(self.code)
        pe.extend(struct.pack('<I', code_size))  # Size of code
        pe.extend(struct.pack('<I', 0))          # Size of initialized data
        pe.extend(struct.pack('<I', 0))          # Size of uninitialized data
        pe.extend(struct.pack('<I', 0x1000 + self.entry_offset))  # Entry point
        pe.extend(struct.pack('<I', 0x1000))     # Base of code
        
        # Image base and alignment
        pe.extend(struct.pack('<Q', 0x140000000))  # Image base
        pe.extend(struct.pack('<I', 0x1000))       # Section alignment
        pe.extend(struct.pack('<I', 0x200))        # File alignment
        
        # OS version
        pe.extend(struct.pack('<I', 0x00060000))  # OS version
        pe.extend(struct.pack('<I', 0x00000000))  # Image version
        pe.extend(struct.pack('<I', 0x00060000))  # Subsystem version
        pe.extend(struct.pack('<I', 0))           # Win32 version value
        
        # Image size
        image_size = 0x2000 + ((len(self.code) + 0xFFF) & ~0xFFF)
        pe.extend(struct.pack('<I', image_size))  # Size of image
        pe.extend(struct.pack('<I', 0x200))       # Size of headers
        pe.extend(struct.pack('<I', 0))           # Checksum
        
        pe.extend(struct.pack('<H', 3))   # Subsystem (CONSOLE)
        pe.extend(struct.pack('<H', 0))   # DLL characteristics
        
        # Stack/Heap
        pe.extend(struct.pack('<Q', 0x100000))   # Stack reserve
        pe.extend(struct.pack('<Q', 0x1000))     # Stack commit
        pe.extend(struct.pack('<Q', 0x100000))   # Heap reserve
        pe.extend(struct.pack('<Q', 0x1000))     # Heap commit
        
        pe.extend(struct.pack('<I', 0))          # Loader flags
        pe.extend(struct.pack('<I', 16))         # Number of data directories
        
        # Data directories (16 entries, empty)
        for _ in range(16):
            pe.extend(struct.pack('<II', 0, 0))
        
        # Section header (.text)
        pe.extend(b'.text\x00\x00\x00')
        pe.extend(struct.pack('<I', code_size))         # Virtual size
        pe.extend(struct.pack('<I', 0x1000))            # Virtual address
        pe.extend(struct.pack('<I', ((code_size + 0x1FF) & ~0x1FF)))  # Raw size
        pe.extend(struct.pack('<I', 0x200))             # Raw address
        pe.extend(struct.pack('<I', 0))                 # Relocations
        pe.extend(struct.pack('<I', 0))                 # Line numbers
        pe.extend(struct.pack('<H', 0))                 # # Relocations
        pe.extend(struct.pack('<H', 0))                 # # Line numbers
        pe.extend(struct.pack('<I', 0x60000020))        # Characteristics
        
        # Pad to file alignment
        while len(pe) < 0x200:
            pe.append(0)
        
        # Emit code
        pe.extend(self.code)
        
        # Pad to next 0x200 boundary
        while len(pe) % 0x200 != 0:
            pe.append(0)
        
        # Write file
        with open(filename, 'wb') as f:
            f.write(pe)
        
        print(f"✅ Generated PE64: {filename} ({len(self.code)} bytes code)")

# ============================================================================
# MAIN
# ============================================================================
def main():
    if len(sys.argv) < 3:
        print("RawrXD MASM Minimal Compiler")
        print("Usage: python3 masm_compiler.py <input.asm> <output.exe>")
        sys.exit(1)
    
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    
    try:
        # Read source
        with open(input_file, 'r') as f:
            source = f.read()
        
        print(f"📄 Reading: {input_file} ({len(source)} bytes)")
        
        # Lex
        print("🔍 Lexical analysis...")
        lexer = Lexer(source)
        print(f"   Tokens: {len(lexer.tokens)}")
        
        # Parse
        print("🌳 Parsing...")
        parser = Parser(lexer.tokens)
        print(f"   AST nodes: {len(parser.ast)}")
        
        # Codegen
        print("⚙️  Code generation...")
        codegen = CodeGen(parser.ast)
        code = codegen.generate()
        print(f"   Generated: {len(code)} bytes")
        
        # Write PE
        print("📝 Writing PE64 executable...")
        writer = PE64Writer(code)
        writer.write(output_file)
        
        return 0
    
    except Exception as e:
        print(f"❌ Error: {e}", file=sys.stderr)
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())
