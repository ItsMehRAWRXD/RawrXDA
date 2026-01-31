#!/usr/bin/env python3
"""
Compiler Pattern Generator
Generates different compiler patterns to avoid detection
Can change up patterns if ever flagged by antivirus
"""

import random
import hashlib
import struct
import time
from datetime import datetime

class CompilerPatternGenerator:
    """Generates different compiler patterns to avoid detection"""
    
    def __init__(self):
        self.pattern_variants = {
            "gcc": self._generate_gcc_patterns(),
            "clang": self._generate_clang_patterns(), 
            "msvc": self._generate_msvc_patterns(),
            "mingw": self._generate_mingw_patterns(),
            "custom": self._generate_custom_patterns()
        }
        
        print("Compiler Pattern Generator initialized")
        print(f"Available patterns: {list(self.pattern_variants.keys())}")
    
    def get_random_pattern(self):
        """Get a random compiler pattern"""
        pattern_type = random.choice(list(self.pattern_variants.keys()))
        return self.pattern_variants[pattern_type]
    
    def get_pattern_by_hash(self, source_hash):
        """Get consistent pattern based on source hash"""
        hash_int = int(hashlib.md5(source_hash.encode()).hexdigest(), 16)
        pattern_index = hash_int % len(self.pattern_variants)
        pattern_type = list(self.pattern_variants.keys())[pattern_index]
        return self.pattern_variants[pattern_type]
    
    def _generate_gcc_patterns(self):
        """Generate GCC-like compiler patterns"""
        return {
            "compiler_name": "GCC (GNU Compiler Collection)",
            "version": f"gcc version {random.randint(9, 15)}.{random.randint(0, 9)}.{random.randint(0, 9)}",
            "company": "Free Software Foundation",
            "product": "GNU Compiler Collection",
            "copyright": "Copyright (C) 2023 Free Software Foundation",
            "pe_characteristics": 0x0102 | 0x0020,
            "subsystem": 3,  # Console
            "dll_characteristics": 0x0160,
            "entry_point_offset": 0x1000,
            "code_section_name": ".text",
            "data_section_name": ".data",
            "rdata_section_name": ".rdata",
            "import_table_style": "gcc_style",
            "string_encoding": "utf8",
            "timestamp_style": "unix"
        }
    
    def _generate_clang_patterns(self):
        """Generate Clang-like compiler patterns"""
        return {
            "compiler_name": "Clang",
            "version": f"clang version {random.randint(14, 18)}.{random.randint(0, 9)}.{random.randint(0, 9)}",
            "company": "LLVM Project",
            "product": "Clang Compiler",
            "copyright": "Copyright (C) 2023 LLVM Project",
            "pe_characteristics": 0x0102 | 0x0020 | 0x0002,
            "subsystem": 3,  # Console
            "dll_characteristics": 0x0160 | 0x0100,
            "entry_point_offset": 0x1000,
            "code_section_name": ".text",
            "data_section_name": ".data",
            "rdata_section_name": ".rdata",
            "import_table_style": "clang_style",
            "string_encoding": "utf8",
            "timestamp_style": "unix"
        }
    
    def _generate_msvc_patterns(self):
        """Generate MSVC-like compiler patterns"""
        return {
            "compiler_name": "Microsoft Visual C++",
            "version": f"Microsoft (R) C/C++ Optimizing Compiler Version {random.randint(19, 20)}.{random.randint(20, 40)}.{random.randint(30000, 40000)}",
            "company": "Microsoft Corporation",
            "product": "Microsoft Visual C++",
            "copyright": "Copyright (C) Microsoft Corporation",
            "pe_characteristics": 0x0102 | 0x0020,
            "subsystem": 3,  # Console
            "dll_characteristics": 0x0160,
            "entry_point_offset": 0x1000,
            "code_section_name": ".text",
            "data_section_name": ".data",
            "rdata_section_name": ".rdata",
            "import_table_style": "msvc_style",
            "string_encoding": "utf16",
            "timestamp_style": "windows"
        }
    
    def _generate_mingw_patterns(self):
        """Generate MinGW-like compiler patterns"""
        return {
            "compiler_name": "MinGW-w64",
            "version": f"gcc version {random.randint(10, 13)}.{random.randint(0, 9)}.{random.randint(0, 9)} (MinGW-W64)",
            "company": "MinGW-w64 Project",
            "product": "MinGW-w64 GCC",
            "copyright": "Copyright (C) 2023 MinGW-w64 Project",
            "pe_characteristics": 0x0102 | 0x0020,
            "subsystem": 3,  # Console
            "dll_characteristics": 0x0160,
            "entry_point_offset": 0x1000,
            "code_section_name": ".text",
            "data_section_name": ".data",
            "rdata_section_name": ".rdata",
            "import_table_style": "mingw_style",
            "string_encoding": "utf8",
            "timestamp_style": "unix"
        }
    
    def _generate_custom_patterns(self):
        """Generate custom compiler patterns"""
        custom_names = [
            "RawrZ Custom Compiler",
            "EON Programming Language Compiler", 
            "Advanced Code Generator",
            "Universal Compiler System",
            "Native Code Generator"
        ]
        
        return {
            "compiler_name": random.choice(custom_names),
            "version": f"v{random.randint(1, 5)}.{random.randint(0, 9)}.{random.randint(0, 99)}",
            "company": "Custom Development",
            "product": "Custom Compiler",
            "copyright": f"Copyright (C) {datetime.now().year} Custom Development",
            "pe_characteristics": 0x0102 | 0x0020,
            "subsystem": 3,  # Console
            "dll_characteristics": 0x0160,
            "entry_point_offset": 0x1000,
            "code_section_name": ".text",
            "data_section_name": ".data", 
            "rdata_section_name": ".rdata",
            "import_table_style": "custom_style",
            "string_encoding": "utf8",
            "timestamp_style": "unix"
        }
    
    def generate_dynamic_pattern(self, source_code):
        """Generate dynamic pattern based on source code content"""
        # Analyze source code to determine best pattern
        source_lower = source_code.lower()
        
        if "windows" in source_lower or "win32" in source_lower:
            return self._generate_msvc_patterns()
        elif "linux" in source_lower or "posix" in source_lower:
            return self._generate_gcc_patterns()
        elif "clang" in source_lower or "llvm" in source_lower:
            return self._generate_clang_patterns()
        elif "mingw" in source_lower or "gcc" in source_lower:
            return self._generate_mingw_patterns()
        else:
            return self.get_pattern_by_hash(source_code)
    
    def rotate_patterns(self):
        """Rotate through different patterns to avoid detection"""
        current_time = int(time.time())
        pattern_index = (current_time // 3600) % len(self.pattern_variants)  # Change every hour
        pattern_type = list(self.pattern_variants.keys())[pattern_index]
        return self.pattern_variants[pattern_type]
    
    def get_stealth_pattern(self):
        """Get stealth pattern that mimics legitimate tools"""
        stealth_patterns = [
            {
                "compiler_name": "Microsoft Visual Studio Build Tools",
                "version": "16.11.47",
                "company": "Microsoft Corporation", 
                "product": "MSBuild",
                "copyright": "Copyright (C) Microsoft Corporation",
                "pe_characteristics": 0x0102 | 0x0020,
                "subsystem": 2,  # Windows GUI
                "dll_characteristics": 0x0160,
                "entry_point_offset": 0x1000,
                "code_section_name": ".text",
                "data_section_name": ".data",
                "rdata_section_name": ".rdata",
                "import_table_style": "msvc_style",
                "string_encoding": "utf16",
                "timestamp_style": "windows"
            },
            {
                "compiler_name": "CMake",
                "version": f"3.{random.randint(20, 30)}.{random.randint(0, 9)}",
                "company": "Kitware Inc.",
                "product": "CMake",
                "copyright": "Copyright (C) Kitware Inc.",
                "pe_characteristics": 0x0102 | 0x0020,
                "subsystem": 3,  # Console
                "dll_characteristics": 0x0160,
                "entry_point_offset": 0x1000,
                "code_section_name": ".text",
                "data_section_name": ".data",
                "rdata_section_name": ".rdata",
                "import_table_style": "gcc_style",
                "string_encoding": "utf8",
                "timestamp_style": "unix"
            }
        ]
        
        return random.choice(stealth_patterns)

class DynamicCompilerGenerator:
    """Generates compilers with dynamic patterns"""
    
    def __init__(self):
        self.pattern_generator = CompilerPatternGenerator()
        self.current_pattern = None
        self.pattern_history = []
        
        print("Dynamic Compiler Generator initialized")
        print("Can change patterns to avoid detection")
    
    def generate_compiler_with_pattern(self, source_code, output_file, pattern_type="auto"):
        """Generate compiler with specific pattern"""
        
        if pattern_type == "auto":
            pattern = self.pattern_generator.generate_dynamic_pattern(source_code)
        elif pattern_type == "random":
            pattern = self.pattern_generator.get_random_pattern()
        elif pattern_type == "stealth":
            pattern = self.pattern_generator.get_stealth_pattern()
        elif pattern_type == "rotate":
            pattern = self.pattern_generator.rotate_patterns()
        else:
            pattern = self.pattern_generator.pattern_variants.get(pattern_type, 
                                                               self.pattern_generator.get_random_pattern())
        
        self.current_pattern = pattern
        self.pattern_history.append(pattern)
        
        print(f"Using pattern: {pattern['compiler_name']}")
        print(f"Version: {pattern['version']}")
        print(f"Company: {pattern['company']}")
        
        return self._generate_pe_with_pattern(source_code, output_file, pattern)
    
    def _generate_pe_with_pattern(self, source_code, output_file, pattern):
        """Generate PE with specific compiler pattern"""
        
        pe_data = bytearray()
        
        # Generate PE with pattern-specific characteristics
        pe_data.extend(self._create_pattern_dos_header(pattern))
        pe_data.extend(self._create_pattern_dos_stub(pattern))
        
        # Pad to PE header
        while len(pe_data) < 0x80:
            pe_data.append(0)
        
        # PE signature
        pe_data.extend(b'PE\x00\x00')
        
        # Pattern-specific COFF header
        pe_data.extend(self._create_pattern_coff_header(pattern))
        
        # Pattern-specific optional header
        pe_data.extend(self._create_pattern_optional_header(pattern))
        
        # Pattern-specific sections
        pe_data.extend(self._create_pattern_sections(pattern))
        
        # Pad to first section
        while len(pe_data) < 0x400:
            pe_data.append(0)
        
        # Pattern-specific code sections
        pe_data.extend(self._create_pattern_code_section(source_code, pattern))
        pe_data.extend(self._create_pattern_data_sections(pattern))
        
        # Write PE file
        with open(output_file, 'wb') as f:
            f.write(pe_data)
        
        print(f"Generated {pattern['compiler_name']} compatible executable: {output_file}")
        return {"success": True, "pattern": pattern, "output_file": output_file}
    
    def _create_pattern_dos_header(self, pattern):
        """Create DOS header with pattern characteristics"""
        dos_header = bytearray(64)
        dos_header[0:2] = b'MZ'
        dos_header[60:64] = struct.pack('<L', 0x80)
        return dos_header
    
    def _create_pattern_dos_stub(self, pattern):
        """Create DOS stub with pattern characteristics"""
        return bytes([
            0x0e, 0x1f, 0xba, 0x0e, 0x00, 0xb4, 0x09, 0xcd,
            0x21, 0xb8, 0x01, 0x4c, 0xcd, 0x21, 0x54, 0x68,
            0x69, 0x73, 0x20, 0x70, 0x72, 0x6f, 0x67, 0x72,
            0x61, 0x6d, 0x20, 0x63, 0x61, 0x6e, 0x6e, 0x6f,
            0x74, 0x20, 0x62, 0x65, 0x20, 0x72, 0x75, 0x6e,
            0x20, 0x69, 0x6e, 0x20, 0x44, 0x4f, 0x53, 0x20,
            0x6d, 0x6f, 0x64, 0x65, 0x2e, 0x0d, 0x0d, 0x0a,
            0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        ])
    
    def _create_pattern_coff_header(self, pattern):
        """Create COFF header with pattern characteristics"""
        coff_header = bytearray()
        coff_header.extend(struct.pack('<H', 0x8664))  # x64
        coff_header.extend(struct.pack('<H', 3))       # 3 sections
        coff_header.extend(struct.pack('<L', int(time.time())))
        coff_header.extend(struct.pack('<L', 0))       # Symbol table
        coff_header.extend(struct.pack('<L', 0))       # Number of symbols
        coff_header.extend(struct.pack('<H', 240))     # Optional header size
        coff_header.extend(struct.pack('<H', pattern['pe_characteristics']))
        return coff_header
    
    def _create_pattern_optional_header(self, pattern):
        """Create optional header with pattern characteristics"""
        optional_header = bytearray()
        optional_header.extend(struct.pack('<H', 0x020b))  # PE32+
        optional_header.extend(struct.pack('<BB', 14, 0))  # Linker version
        optional_header.extend(struct.pack('<L', 0x1000))  # Code size
        optional_header.extend(struct.pack('<L', 0x1000))  # Data size
        optional_header.extend(struct.pack('<L', 0))       # Uninitialized data
        optional_header.extend(struct.pack('<L', pattern['entry_point_offset']))
        optional_header.extend(struct.pack('<L', 0x1000))  # Base of code
        optional_header.extend(struct.pack('<Q', 0x140000000))  # Image base
        optional_header.extend(struct.pack('<L', 0x1000))  # Section alignment
        optional_header.extend(struct.pack('<L', 0x200))   # File alignment
        optional_header.extend(struct.pack('<H', 10))      # Major OS version
        optional_header.extend(struct.pack('<H', 0))       # Minor OS version
        optional_header.extend(struct.pack('<H', 0))       # Major image version
        optional_header.extend(struct.pack('<H', 0))       # Minor image version
        optional_header.extend(struct.pack('<H', 10))      # Major subsystem version
        optional_header.extend(struct.pack('<H', 0))       # Minor subsystem version
        optional_header.extend(struct.pack('<L', 0))       # Win32 version
        optional_header.extend(struct.pack('<L', 0x4000))  # Size of image
        optional_header.extend(struct.pack('<L', 0x400))   # Size of headers
        optional_header.extend(struct.pack('<L', 0))       # Checksum
        optional_header.extend(struct.pack('<H', pattern['subsystem']))
        optional_header.extend(struct.pack('<H', pattern['dll_characteristics']))
        optional_header.extend(struct.pack('<Q', 0x100000))  # Stack reserve
        optional_header.extend(struct.pack('<Q', 0x1000))    # Stack commit
        optional_header.extend(struct.pack('<Q', 0x100000))  # Heap reserve
        optional_header.extend(struct.pack('<Q', 0x1000))    # Heap commit
        optional_header.extend(struct.pack('<L', 0))       # Loader flags
        optional_header.extend(struct.pack('<L', 16))      # Number of RVA
        
        # Data directories
        for i in range(16):
            optional_header.extend(struct.pack('<LL', 0, 0))
        
        return optional_header
    
    def _create_pattern_sections(self, pattern):
        """Create sections with pattern characteristics"""
        sections = bytearray()
        
        # .text section
        text_section = bytearray(40)
        text_section[0:8] = pattern['code_section_name'].encode('ascii').ljust(8, b'\x00')
        text_section[8:12] = struct.pack('<L', 0x1000)
        text_section[12:16] = struct.pack('<L', 0x1000)
        text_section[16:20] = struct.pack('<L', 0x200)
        text_section[20:24] = struct.pack('<L', 0x400)
        text_section[36:40] = struct.pack('<L', 0x60000020)
        sections.extend(text_section)
        
        # .rdata section
        rdata_section = bytearray(40)
        rdata_section[0:8] = pattern['rdata_section_name'].encode('ascii').ljust(8, b'\x00')
        rdata_section[8:12] = struct.pack('<L', 0x1000)
        rdata_section[12:16] = struct.pack('<L', 0x2000)
        rdata_section[16:20] = struct.pack('<L', 0x200)
        rdata_section[20:24] = struct.pack('<L', 0x600)
        rdata_section[36:40] = struct.pack('<L', 0x40000040)
        sections.extend(rdata_section)
        
        # .data section
        data_section = bytearray(40)
        data_section[0:8] = pattern['data_section_name'].encode('ascii').ljust(8, b'\x00')
        data_section[8:12] = struct.pack('<L', 0x1000)
        data_section[12:16] = struct.pack('<L', 0x3000)
        data_section[16:20] = struct.pack('<L', 0x200)
        data_section[20:24] = struct.pack('<L', 0x800)
        data_section[36:40] = struct.pack('<L', 0xC0000040)
        sections.extend(data_section)
        
        return sections
    
    def _create_pattern_code_section(self, source_code, pattern):
        """Create code section with pattern characteristics"""
        code_section = bytearray()
        
        # Pattern-specific entry code
        entry_code = bytearray([
            0x48, 0x83, 0xEC, 0x28,                    # sub rsp, 40
            0x48, 0x8D, 0x0D, 0x00, 0x00, 0x00, 0x00,  # lea rcx, [message]
            0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,        # call [printf]
            0x48, 0x31, 0xC9,                          # xor rcx, rcx
            0xFF, 0x15, 0x00, 0x00, 0x00, 0x00,        # call [exit]
            0xC3                                        # ret
        ])
        
        code_section.extend(entry_code)
        
        # Pad to section size
        while len(code_section) < 0x200:
            code_section.append(0x90)  # NOP
        
        return code_section
    
    def _create_pattern_data_sections(self, pattern):
        """Create data sections with pattern characteristics"""
        data_sections = bytearray()
        
        # .rdata section content
        rdata_content = bytearray()
        rdata_content.extend(pattern['compiler_name'].encode('utf-8') + b'\x00')
        rdata_content.extend(pattern['version'].encode('utf-8') + b'\x00')
        rdata_content.extend(pattern['copyright'].encode('utf-8') + b'\x00')
        
        while len(rdata_content) < 0x200:
            rdata_content.append(0)
        
        data_sections.extend(rdata_content)
        
        # .data section content
        data_content = bytearray(0x200)
        data_sections.extend(data_content)
        
        return data_sections

# Test the pattern generator
if __name__ == "__main__":
    print("Testing Compiler Pattern Generator...")
    
    generator = DynamicCompilerGenerator()
    
    # Test different patterns
    test_source = """// Test EON source
module TestApp

function main() -> int {
    println("Hello from dynamic pattern compiler!")
    return 0
}
"""
    
    with open("test_dynamic.eon", 'w') as f:
        f.write(test_source)
    
    # Test different pattern types
    patterns_to_test = ["gcc", "clang", "msvc", "random", "stealth", "rotate"]
    
    for pattern_type in patterns_to_test:
        print(f"\nTesting {pattern_type} pattern...")
        result = generator.generate_compiler_with_pattern(
            "test_dynamic.eon", 
            f"test_{pattern_type}.exe", 
            pattern_type
        )
        
        if result["success"]:
            print(f"{pattern_type} pattern successful!")
            print(f"   Compiler: {result['pattern']['compiler_name']}")
            print(f"   Version: {result['pattern']['version']}")
        else:
            print(f"{pattern_type} pattern failed!")
    
    print("\nPattern generator testing complete!")
    print("Can change patterns to avoid detection")
    print("Multiple compiler patterns available")
    print("Dynamic pattern selection working")
