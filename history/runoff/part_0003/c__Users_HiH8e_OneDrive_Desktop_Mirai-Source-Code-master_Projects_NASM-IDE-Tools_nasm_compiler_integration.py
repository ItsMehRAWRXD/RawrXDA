#!/usr/bin/env python3
"""
NASM Compiler Integration & Build System
========================================
Advanced NASM compilation, linking, and project management tools.
Supports Windows, Linux, and macOS targets with various output formats.

Author: Mirai Security Toolkit
Date: November 21, 2025
License: MIT
"""

import os
import subprocess
import platform
import json
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Dict, List, Optional, Tuple
from enum import Enum

class OutputFormat(Enum):
    """NASM output formats"""
    BIN = "bin"          # Pure binary
    ELF32 = "elf32"      # Linux 32-bit
    ELF64 = "elf64"      # Linux 64-bit  
    WIN32 = "win32"      # Windows 32-bit
    WIN64 = "win64"      # Windows 64-bit
    MACHO32 = "macho32"  # macOS 32-bit
    MACHO64 = "macho64"  # macOS 64-bit
    COFF = "coff"        # Common Object File Format
    OBJ = "obj"          # Microsoft Object Format

class TargetArch(Enum):
    """Target architectures"""
    X86 = "i386"
    X64 = "x86_64"
    ARM64 = "aarch64"

@dataclass
class CompilerConfig:
    """NASM compiler configuration"""
    source_file: str
    output_file: str
    output_format: OutputFormat
    target_arch: TargetArch
    debug_info: bool = False
    optimize: bool = True
    include_dirs: List[str] = None
    defines: Dict[str, str] = None
    warnings: bool = True
    listing_file: str = None
    
    def __post_init__(self):
        if self.include_dirs is None:
            self.include_dirs = []
        if self.defines is None:
            self.defines = {}

@dataclass
class LinkConfig:
    """Linker configuration"""
    object_files: List[str]
    output_file: str
    entry_point: str = "_start"
    libraries: List[str] = None
    library_dirs: List[str] = None
    strip_symbols: bool = False
    
    def __post_init__(self):
        if self.libraries is None:
            self.libraries = []
        if self.library_dirs is None:
            self.library_dirs = []

class NASMCompiler:
    """Advanced NASM compiler with IDE integration"""
    
    def __init__(self, nasm_path: str = "nasm", ld_path: str = "ld"):
        self.nasm_path = nasm_path
        self.ld_path = ld_path
        self.platform = platform.system().lower()
        self._verify_tools()
    
    def _verify_tools(self):
        """Verify NASM and linker are available"""
        try:
            result = subprocess.run([self.nasm_path, "-version"], 
                                  capture_output=True, text=True)
            if result.returncode != 0:
                raise FileNotFoundError(f"NASM not found at {self.nasm_path}")
        except FileNotFoundError:
            print("⚠️ NASM not found. Please install NASM:")
            print("   Windows: choco install nasm")
            print("   Linux: sudo apt install nasm")
            print("   macOS: brew install nasm")
    
    def compile(self, config: CompilerConfig) -> Tuple[bool, str, str]:
        """Compile NASM source file"""
        cmd = [self.nasm_path]
        
        # Output format
        cmd.extend(["-f", config.output_format.value])
        
        # Output file
        cmd.extend(["-o", config.output_file])
        
        # Debug information
        if config.debug_info:
            if config.output_format in [OutputFormat.ELF32, OutputFormat.ELF64]:
                cmd.append("-g")
                cmd.append("-F")
                cmd.append("dwarf")
        
        # Include directories
        for include_dir in config.include_dirs:
            cmd.extend(["-I", include_dir])
        
        # Defines
        for name, value in config.defines.items():
            cmd.extend(["-D", f"{name}={value}"])
        
        # Warnings
        if config.warnings:
            cmd.append("-w+all")
        
        # Listing file
        if config.listing_file:
            cmd.extend(["-l", config.listing_file])
        
        # Source file
        cmd.append(config.source_file)
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, 
                                  cwd=os.path.dirname(config.source_file) or ".")
            
            success = result.returncode == 0
            stdout = result.stdout
            stderr = result.stderr
            
            if success:
                print(f"✅ Compiled: {config.source_file} -> {config.output_file}")
            else:
                print(f"❌ Compilation failed: {stderr}")
            
            return success, stdout, stderr
            
        except Exception as e:
            return False, "", str(e)
    
    def link(self, config: LinkConfig) -> Tuple[bool, str, str]:
        """Link object files"""
        if self.platform == "windows":
            return self._link_windows(config)
        else:
            return self._link_unix(config)
    
    def _link_unix(self, config: LinkConfig) -> Tuple[bool, str, str]:
        """Link on Unix-like systems"""
        cmd = [self.ld_path]
        
        # Entry point
        cmd.extend(["-e", config.entry_point])
        
        # Output file
        cmd.extend(["-o", config.output_file])
        
        # Object files
        cmd.extend(config.object_files)
        
        # Library directories
        for lib_dir in config.library_dirs:
            cmd.extend(["-L", lib_dir])
        
        # Libraries
        for lib in config.libraries:
            cmd.extend(["-l", lib])
        
        # Strip symbols
        if config.strip_symbols:
            cmd.append("-s")
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            success = result.returncode == 0
            
            if success:
                print(f"✅ Linked: {' '.join(config.object_files)} -> {config.output_file}")
                # Make executable
                os.chmod(config.output_file, 0o755)
            else:
                print(f"❌ Linking failed: {result.stderr}")
            
            return success, result.stdout, result.stderr
            
        except Exception as e:
            return False, "", str(e)
    
    def _link_windows(self, config: LinkConfig) -> Tuple[bool, str, str]:
        """Link on Windows using Microsoft linker"""
        cmd = ["link"]
        
        # Entry point
        cmd.extend(["/ENTRY:" + config.entry_point])
        
        # Output file
        cmd.extend(["/OUT:" + config.output_file])
        
        # Object files
        cmd.extend(config.object_files)
        
        # Libraries
        cmd.extend(config.libraries)
        
        try:
            result = subprocess.run(cmd, capture_output=True, text=True)
            success = result.returncode == 0
            
            if success:
                print(f"✅ Linked: {' '.join(config.object_files)} -> {config.output_file}")
            else:
                print(f"❌ Linking failed: {result.stderr}")
            
            return success, result.stdout, result.stderr
            
        except Exception as e:
            return False, "", str(e)

class NASMProject:
    """NASM project management"""
    
    def __init__(self, project_dir: str):
        self.project_dir = Path(project_dir)
        self.config_file = self.project_dir / "nasm_project.json"
        self.compiler = NASMCompiler()
        
    def create_project(self, name: str, target: OutputFormat):
        """Create new NASM project"""
        self.project_dir.mkdir(exist_ok=True)
        
        # Create directory structure
        (self.project_dir / "src").mkdir(exist_ok=True)
        (self.project_dir / "include").mkdir(exist_ok=True)
        (self.project_dir / "build").mkdir(exist_ok=True)
        (self.project_dir / "docs").mkdir(exist_ok=True)
        
        # Create project configuration
        config = {
            "name": name,
            "version": "1.0.0",
            "target": target.value,
            "entry_file": "src/main.asm",
            "output_dir": "build",
            "include_dirs": ["include"],
            "defines": {},
            "debug": True
        }
        
        with open(self.config_file, 'w') as f:
            json.dump(config, f, indent=2)
        
        # Create sample main.asm
        main_asm = self._get_sample_code(target)
        with open(self.project_dir / "src" / "main.asm", 'w') as f:
            f.write(main_asm)
        
        # Create Makefile
        makefile = self._generate_makefile(target)
        with open(self.project_dir / "Makefile", 'w') as f:
            f.write(makefile)
        
        print(f"✅ Created NASM project: {name}")
        print(f"📁 Directory: {self.project_dir}")
    
    def build(self) -> bool:
        """Build the project"""
        if not self.config_file.exists():
            print("❌ No project configuration found")
            return False
        
        with open(self.config_file) as f:
            config = json.load(f)
        
        # Determine output format
        output_format = OutputFormat(config["target"])
        
        # Set up compilation
        source_file = str(self.project_dir / config["entry_file"])
        object_file = str(self.project_dir / config["output_dir"] / "main.o")
        output_file = str(self.project_dir / config["output_dir"] / config["name"])
        
        # Create build directory
        (self.project_dir / config["output_dir"]).mkdir(exist_ok=True)
        
        # Compile
        compile_config = CompilerConfig(
            source_file=source_file,
            output_file=object_file,
            output_format=output_format,
            target_arch=TargetArch.X64,
            debug_info=config.get("debug", False),
            include_dirs=[str(self.project_dir / d) for d in config["include_dirs"]],
            defines=config["defines"]
        )
        
        success, stdout, stderr = self.compiler.compile(compile_config)
        if not success:
            return False
        
        # Link (for executable formats)
        if output_format not in [OutputFormat.BIN]:
            link_config = LinkConfig(
                object_files=[object_file],
                output_file=output_file
            )
            success, stdout, stderr = self.compiler.link(link_config)
        
        return success
    
    def _get_sample_code(self, target: OutputFormat) -> str:
        """Generate sample code for target platform"""
        if target in [OutputFormat.ELF64]:
            return """
; Hello World for Linux x64
section .data
    msg db 'Hello from NASM!', 0xA
    msg_len equ $ - msg

section .text
    global _start

_start:
    ; write system call
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout  
    mov rsi, msg        ; message
    mov rdx, msg_len    ; length
    syscall
    
    ; exit system call
    mov rax, 60         ; sys_exit
    mov rdi, 0          ; exit status
    syscall
"""
        elif target in [OutputFormat.WIN64]:
            return """
; Hello World for Windows x64
extern ExitProcess
extern GetStdHandle
extern WriteConsoleA

section .data
    msg db 'Hello from NASM!', 13, 10, 0
    msg_len equ $ - msg - 1

section .bss
    written resd 1

section .text
    global _start

_start:
    ; Get stdout handle
    mov rcx, -11        ; STD_OUTPUT_HANDLE
    call GetStdHandle
    mov rbx, rax        ; Save handle
    
    ; Write to console
    mov rcx, rbx        ; Handle
    mov rdx, msg        ; Buffer
    mov r8, msg_len     ; Length
    mov r9, written     ; Written
    push 0              ; Reserved
    call WriteConsoleA
    
    ; Exit
    mov rcx, 0
    call ExitProcess
"""
        else:
            return """
; Generic NASM template
section .data
    msg db 'Hello, NASM!'

section .text
    global _start

_start:
    ; Your code here
    nop
    ret
"""
    
    def _generate_makefile(self, target: OutputFormat) -> str:
        """Generate Makefile for project"""
        if target in [OutputFormat.ELF64]:
            return """
# NASM Linux Makefile
NAME = main
SRCDIR = src
BUILDDIR = build
INCLUDEDIR = include

NASM = nasm
LD = ld

NASMFLAGS = -f elf64 -g -F dwarf -I $(INCLUDEDIR)/
LDFLAGS = -e _start

SRCFILES = $(wildcard $(SRCDIR)/*.asm)
OBJFILES = $(SRCFILES:$(SRCDIR)/%.asm=$(BUILDDIR)/%.o)

.PHONY: all clean run

all: $(BUILDDIR)/$(NAME)

$(BUILDDIR)/%.o: $(SRCDIR)/%.asm | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) $< -o $@

$(BUILDDIR)/$(NAME): $(OBJFILES)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

run: $(BUILDDIR)/$(NAME)
	./$(BUILDDIR)/$(NAME)
"""
        else:
            return """
# Generic NASM Makefile  
NAME = main
SRCDIR = src
BUILDDIR = build

NASM = nasm
NASMFLAGS = -f bin

.PHONY: all clean

all: $(BUILDDIR)/$(NAME).bin

$(BUILDDIR)/$(NAME).bin: $(SRCDIR)/main.asm | $(BUILDDIR)
	$(NASM) $(NASMFLAGS) $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)
"""

def main():
    """Demo the NASM compiler integration"""
    print("🔥 NASM Compiler Integration Demo")
    print("=" * 40)
    
    # Create a test project
    project = NASMProject("test_project")
    project.create_project("hello_nasm", OutputFormat.ELF64)
    
    # Build the project  
    success = project.build()
    
    if success:
        print("🚀 Project built successfully!")
    else:
        print("❌ Build failed")

if __name__ == "__main__":
    main()