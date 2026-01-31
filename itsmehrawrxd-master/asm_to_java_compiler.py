#!/usr/bin/env python3
"""
ASM to Java Compiler - 0day Style
Converts assembly code to Java bytecode and Java source
Uses your existing ASM infrastructure
"""

import os
import sys
import struct
import binascii
from pathlib import Path
from typing import Dict, List, Optional, Any, Tuple
from dataclasses import dataclass
from enum import Enum

class JavaOpcode(Enum):
    """Java bytecode opcodes"""
    # Load/Store
    ALOAD_0 = 0x2A
    ALOAD_1 = 0x2B
    ALOAD_2 = 0x2C
    ALOAD_3 = 0x2D
    ALOAD = 0x19
    ASTORE_0 = 0x4B
    ASTORE_1 = 0x4C
    ASTORE_2 = 0x4D
    ASTORE_3 = 0x4E
    ASTORE = 0x3A
    
    # Integer operations
    ILOAD_0 = 0x1A
    ILOAD_1 = 0x1B
    ILOAD_2 = 0x1C
    ILOAD_3 = 0x1D
    ILOAD = 0x15
    ISTORE_0 = 0x3B
    ISTORE_1 = 0x3C
    ISTORE_2 = 0x3D
    ISTORE_3 = 0x3E
    ISTORE = 0x36
    
    # Arithmetic
    IADD = 0x60
    ISUB = 0x64
    IMUL = 0x68
    IDIV = 0x6C
    IREM = 0x70
    
    # Constants
    ICONST_M1 = 0x02
    ICONST_0 = 0x03
    ICONST_1 = 0x04
    ICONST_2 = 0x05
    ICONST_3 = 0x06
    ICONST_4 = 0x07
    ICONST_5 = 0x08
    BIPUSH = 0x10
    SIPUSH = 0x11
    LDC = 0x12
    
    # Control flow
    IFEQ = 0x99
    IFNE = 0x9A
    IFLT = 0x9B
    IFGE = 0x9C
    IFGT = 0x9D
    IFLE = 0x9E
    GOTO = 0xA7
    RETURN = 0xB1
    IRETURN = 0xAC
    ARETURN = 0xB0
    
    # Method calls
    INVOKEVIRTUAL = 0xB6
    INVOKESPECIAL = 0xB7
    INVOKESTATIC = 0xB8
    INVOKEINTERFACE = 0xB9
    
    # Stack operations
    POP = 0x57
    DUP = 0x59
    SWAP = 0x5F

@dataclass
class JavaInstruction:
    """Java bytecode instruction"""
    opcode: JavaOpcode
    operands: List[int] = None
    label: str = None
    
    def __post_init__(self):
        if self.operands is None:
            self.operands = []

class ASMToJavaCompiler:
    """Converts ASM to Java bytecode and source"""
    
    def __init__(self):
        self.java_opcodes = {}
        self.asm_patterns = {}
        self.java_methods = []
        self.constant_pool = []
        self._init_asm_patterns()
    
    def _init_asm_patterns(self):
        """Initialize ASM to Java pattern mappings"""
        self.asm_patterns = {
            # x86-64 to Java patterns
            "mov eax, 5": "ICONST_5",
            "mov eax, 0": "ICONST_0", 
            "mov eax, 1": "ICONST_1",
            "add eax, ebx": "IADD",
            "sub eax, ebx": "ISUB",
            "mul eax, ebx": "IMUL",
            "div eax, ebx": "IDIV",
            "ret": "RETURN",
            "call": "INVOKESTATIC",
            "push": "DUP",
            "pop": "POP",
            
            # EON to Java patterns
            "load": "ILOAD",
            "store": "ISTORE", 
            "add": "IADD",
            "sub": "ISUB",
            "mul": "IMUL",
            "div": "IDIV",
            "return": "IRETURN",
            "call": "INVOKEVIRTUAL",
        }
    
    def compile_asm_to_java(self, asm_source: str, class_name: str = "GeneratedClass") -> Dict[str, Any]:
        """Compile ASM source to Java"""
        print(f"🔨 Compiling ASM to Java: {class_name}")
        
        try:
            # Parse ASM source
            asm_instructions = self._parse_asm_source(asm_source)
            
            # Convert to Java bytecode
            java_bytecode = self._convert_to_java_bytecode(asm_instructions)
            
            # Generate Java source
            java_source = self._generate_java_source(asm_instructions, class_name)
            
            # Create .class file
            class_file = self._create_class_file(java_bytecode, class_name)
            
            return {
                "success": True,
                "java_source": java_source,
                "bytecode": java_bytecode,
                "class_file": class_file,
                "class_name": class_name
            }
            
        except Exception as e:
            return {
                "success": False,
                "error": str(e),
                "java_source": None,
                "bytecode": None,
                "class_file": None
            }
    
    def _parse_asm_source(self, asm_source: str) -> List[Dict[str, Any]]:
        """Parse ASM source into structured instructions"""
        instructions = []
        lines = asm_source.strip().split('\n')
        
        for line_num, line in enumerate(lines, 1):
            line = line.strip()
            if not line or line.startswith(';') or line.startswith('#'):
                continue
                
            # Parse ASM instruction
            parts = line.split()
            if len(parts) < 1:
                continue
                
            instruction = {
                "line": line_num,
                "opcode": parts[0].lower(),
                "operands": parts[1:] if len(parts) > 1 else [],
                "original": line
            }
            instructions.append(instruction)
            
        return instructions
    
    def _convert_to_java_bytecode(self, asm_instructions: List[Dict[str, Any]]) -> List[JavaInstruction]:
        """Convert ASM instructions to Java bytecode"""
        java_instructions = []
        
        for asm_instr in asm_instructions:
            opcode = asm_instr["opcode"]
            operands = asm_instr["operands"]
            
            # Map ASM to Java opcodes
            if opcode == "mov":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    if src.isdigit():
                        # mov eax, 5 -> ICONST_5
                        value = int(src)
                        if value == -1:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_M1))
                        elif value == 0:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_0))
                        elif value == 1:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_1))
                        elif value == 2:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_2))
                        elif value == 3:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_3))
                        elif value == 4:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_4))
                        elif value == 5:
                            java_instructions.append(JavaInstruction(JavaOpcode.ICONST_5))
                        else:
                            java_instructions.append(JavaInstruction(JavaOpcode.BIPUSH, [value]))
                    else:
                        # mov eax, ebx -> ILOAD
                        java_instructions.append(JavaInstruction(JavaOpcode.ILOAD))
            
            elif opcode == "add":
                java_instructions.append(JavaInstruction(JavaOpcode.IADD))
            
            elif opcode == "sub":
                java_instructions.append(JavaInstruction(JavaOpcode.ISUB))
            
            elif opcode == "mul":
                java_instructions.append(JavaInstruction(JavaOpcode.IMUL))
            
            elif opcode == "div":
                java_instructions.append(JavaInstruction(JavaOpcode.IDIV))
            
            elif opcode == "ret":
                java_instructions.append(JavaInstruction(JavaOpcode.RETURN))
            
            elif opcode == "call":
                # call function -> INVOKESTATIC
                if operands:
                    method_name = operands[0]
                    java_instructions.append(JavaInstruction(JavaOpcode.INVOKESTATIC, [0]))  # Method ref index
            
            elif opcode == "push":
                java_instructions.append(JavaInstruction(JavaOpcode.DUP))
            
            elif opcode == "pop":
                java_instructions.append(JavaInstruction(JavaOpcode.POP))
        
        return java_instructions
    
    def _generate_java_source(self, asm_instructions: List[Dict[str, Any]], class_name: str) -> str:
        """Generate Java source from ASM instructions"""
        java_lines = [
            f"public class {class_name} {{",
            "    ",
            "    public static void main(String[] args) {",
        ]
        
        # Convert ASM to Java statements
        for asm_instr in asm_instructions:
            opcode = asm_instr["opcode"]
            operands = asm_instr["operands"]
            
            if opcode == "mov":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    if src.isdigit():
                        java_lines.append(f"        int {dest} = {src};")
                    else:
                        java_lines.append(f"        int {dest} = {src};")
            
            elif opcode == "add":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    java_lines.append(f"        {dest} = {dest} + {src};")
            
            elif opcode == "sub":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    java_lines.append(f"        {dest} = {dest} - {src};")
            
            elif opcode == "mul":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    java_lines.append(f"        {dest} = {dest} * {src};")
            
            elif opcode == "div":
                if operands and len(operands) >= 2:
                    dest, src = operands[0], operands[1]
                    java_lines.append(f"        {dest} = {dest} / {src};")
            
            elif opcode == "call":
                if operands:
                    method_name = operands[0]
                    java_lines.append(f"        {method_name}();")
        
        java_lines.extend([
            "    }",
            "    ",
            "    public static int add(int a, int b) {",
            "        return a + b;",
            "    }",
            "    ",
            "    public static int multiply(int a, int b) {",
            "        return a * b;",
            "    }",
            "}"
        ])
        
        return '\n'.join(java_lines)
    
    def _create_class_file(self, java_bytecode: List[JavaInstruction], class_name: str) -> str:
        """Create Java .class file from bytecode"""
        class_file = f"{class_name}.class"
        
        # Create minimal .class file structure
        class_data = bytearray()
        
        # Magic number (0xCAFEBABE)
        class_data.extend([0xCA, 0xFE, 0xBA, 0xBE])
        
        # Version (Java 8)
        class_data.extend([0x00, 0x00, 0x00, 0x34])  # Major: 52, Minor: 0
        
        # Constant pool count
        class_data.extend([0x00, 0x10])  # 16 constants
        
        # Constant pool entries
        # UTF8 strings, class refs, method refs, etc.
        for i in range(15):
            class_data.extend([0x01, 0x00, 0x00])  # UTF8 entries
        
        # Access flags (public)
        class_data.extend([0x00, 0x21])
        
        # This class index
        class_data.extend([0x00, 0x01])
        
        # Super class index (Object)
        class_data.extend([0x00, 0x02])
        
        # Interfaces count
        class_data.extend([0x00, 0x00])
        
        # Fields count
        class_data.extend([0x00, 0x00])
        
        # Methods count
        class_data.extend([0x00, 0x02])  # main and constructor
        
        # Method 1: main
        class_data.extend([0x00, 0x09])  # Access flags (public static)
        class_data.extend([0x00, 0x03])  # Name index
        class_data.extend([0x00, 0x04])  # Descriptor index
        class_data.extend([0x00, 0x01])  # Attributes count
        
        # Code attribute
        class_data.extend([0x00, 0x05])  # Attribute name index
        class_data.extend([0x00, 0x00, 0x00, 0x10])  # Attribute length
        
        # Code length
        code_length = len(java_bytecode) * 2
        class_data.extend([0x00, 0x00, 0x00, code_length])
        
        # Max stack
        class_data.extend([0x00, 0x02])
        
        # Max locals
        class_data.extend([0x00, 0x01])
        
        # Bytecode
        for instr in java_bytecode:
            class_data.append(instr.opcode.value)
            for operand in instr.operands:
                class_data.append(operand)
        
        # Method 2: constructor
        class_data.extend([0x00, 0x01])  # Access flags (public)
        class_data.extend([0x00, 0x06])  # Name index
        class_data.extend([0x00, 0x07])  # Descriptor index
        class_data.extend([0x00, 0x01])  # Attributes count
        
        # Code attribute for constructor
        class_data.extend([0x00, 0x05])  # Attribute name index
        class_data.extend([0x00, 0x00, 0x00, 0x05])  # Attribute length
        class_data.extend([0x00, 0x00, 0x00, 0x01])  # Code length
        class_data.extend([0x00, 0x01])  # Max stack
        class_data.extend([0x00, 0x01])  # Max locals
        class_data.append(JavaOpcode.RETURN.value)  # return
        
        # Attributes count
        class_data.extend([0x00, 0x00])
        
        # Write class file
        with open(class_file, 'wb') as f:
            f.write(class_data)
        
        return class_file

def test_asm_to_java_compiler():
    """Test the ASM to Java compiler"""
    print("🧪 Testing ASM to Java Compiler...")
    
    compiler = ASMToJavaCompiler()
    
    # Test ASM source
    asm_source = """
; Simple ASM program
mov eax, 5
mov ebx, 3
add eax, ebx
mov ecx, eax
mul ecx, 2
ret
"""
    
    print("🔨 Compiling ASM to Java...")
    result = compiler.compile_asm_to_java(asm_source, "ASMGenerated")
    
    if result["success"]:
        print("✅ ASM to Java compilation successful!")
        print(f"📁 Class file: {result['class_file']}")
        print(f"📝 Java source generated")
        
        # Save Java source
        with open("ASMGenerated.java", 'w') as f:
            f.write(result["java_source"])
        print("💾 Java source saved to ASMGenerated.java")
        
        return True
    else:
        print("❌ ASM to Java compilation failed!")
        print(f"Error: {result['error']}")
        return False

if __name__ == "__main__":
    test_asm_to_java_compiler()
