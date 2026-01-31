#!/usr/bin/env python3
"""
External Toolchain Support System
Integrates external compilers, linkers, and build tools with the IDE
"""

import os
import sys
import subprocess
import platform
import json
import time
from pathlib import Path

class ExternalToolchainSupport:
    """Support for external toolchains and build systems"""
    
    def __init__(self):
        self.platform = platform.system().lower()
        self.architecture = platform.machine().lower()
        self.detected_toolchains = []
        self.available_compilers = []
        self.available_linkers = []
        self.available_build_systems = []
        
        print(f"External Toolchain Support initialized for {self.platform} {self.architecture}")
        self.detect_external_toolchains()
    
    def detect_external_toolchains(self):
        """Detect available external toolchains"""
        # Compilers
        compilers = [
            ('gcc', 'GCC (GNU Compiler Collection)'),
            ('g++', 'G++ (GNU C++ Compiler)'),
            ('clang', 'Clang (LLVM C/C++ Compiler)'),
            ('clang++', 'Clang++ (LLVM C++ Compiler)'),
            ('msvc', 'Microsoft Visual C++'),
            ('cl', 'Microsoft Visual C++ (cl.exe)'),
            ('icc', 'Intel C++ Compiler'),
            ('icpc', 'Intel C++ Compiler'),
            ('rustc', 'Rust Compiler'),
            ('go', 'Go Compiler'),
            ('javac', 'Java Compiler'),
            ('node', 'Node.js Runtime'),
            ('python', 'Python Interpreter'),
            ('ruby', 'Ruby Interpreter'),
            ('php', 'PHP Interpreter'),
            ('swift', 'Swift Compiler'),
            ('kotlinc', 'Kotlin Compiler'),
            ('scalac', 'Scala Compiler'),
            ('fpc', 'Free Pascal Compiler'),
            ('dmd', 'D Compiler'),
            ('zig', 'Zig Compiler'),
            ('crystal', 'Crystal Compiler'),
            ('nim', 'Nim Compiler'),
            ('v', 'V Compiler'),
            ('odin', 'Odin Compiler')
        ]
        
        # Linkers
        linkers = [
            ('ld', 'GNU Linker'),
            ('lld', 'LLVM Linker'),
            ('link', 'Microsoft Linker'),
            ('gold', 'GNU Gold Linker'),
            ('mold', 'Mold Linker'),
            ('ld.lld', 'LLD Linker'),
            ('wasm-ld', 'WebAssembly Linker')
        ]
        
        # Build Systems
        build_systems = [
            ('make', 'GNU Make'),
            ('cmake', 'CMake'),
            ('ninja', 'Ninja Build'),
            ('meson', 'Meson Build'),
            ('bazel', 'Bazel Build'),
            ('buck', 'Buck Build'),
            ('pants', 'Pants Build'),
            ('gradle', 'Gradle Build'),
            ('maven', 'Apache Maven'),
            ('ant', 'Apache Ant'),
            ('scons', 'SCons Build'),
            ('waf', 'Waf Build'),
            ('xmake', 'XMake Build'),
            ('premake', 'Premake Build'),
            ('qmake', 'Qt Make'),
            ('autotools', 'GNU Autotools'),
            ('conan', 'Conan Package Manager'),
            ('vcpkg', 'vcpkg Package Manager'),
            ('spack', 'Spack Package Manager'),
            ('conda', 'Conda Package Manager'),
            ('pip', 'Python Package Installer'),
            ('npm', 'Node Package Manager'),
            ('yarn', 'Yarn Package Manager'),
            ('cargo', 'Rust Package Manager'),
            ('go', 'Go Module System'),
            ('composer', 'PHP Composer'),
            ('gem', 'Ruby Gems'),
            ('swift', 'Swift Package Manager'),
            ('sbt', 'Scala Build Tool'),
            ('stack', 'Haskell Stack'),
            ('cabal', 'Haskell Cabal')
        ]
        
        # Detect compilers
        for command, name in compilers:
            if self.is_command_available(command):
                self.available_compilers.append((command, name))
                print(f"✓ {name} detected")
            else:
                print(f"✗ {name} not available")
        
        # Detect linkers
        for command, name in linkers:
            if self.is_command_available(command):
                self.available_linkers.append((command, name))
                print(f"✓ {name} detected")
            else:
                print(f"✗ {name} not available")
        
        # Detect build systems
        for command, name in build_systems:
            if self.is_command_available(command):
                self.available_build_systems.append((command, name))
                print(f"✓ {name} detected")
            else:
                print(f"✗ {name} not available")
        
        print(f"\nFound {len(self.available_compilers)} compilers, {len(self.available_linkers)} linkers, {len(self.available_build_systems)} build systems")
    
    def is_command_available(self, command):
        """Check if a command is available in PATH"""
        try:
            subprocess.run([command, '--version'], 
                         capture_output=True, check=True, timeout=5)
            return True
        except (subprocess.CalledProcessError, FileNotFoundError, subprocess.TimeoutExpired):
            return False
    
    def create_toolchain_config(self, compiler, linker=None, build_system=None):
        """Create toolchain configuration"""
        config = {
            "name": f"External Toolchain - {compiler}",
            "compiler": compiler,
            "linker": linker or self.get_default_linker(compiler),
            "build_system": build_system or self.get_default_build_system(),
            "platform": self.platform,
            "architecture": self.architecture,
            "created": time.strftime("%Y-%m-%d %H:%M:%S")
        }
        
        return config
    
    def get_default_linker(self, compiler):
        """Get default linker for compiler"""
        linker_map = {
            'gcc': 'ld',
            'g++': 'ld',
            'clang': 'ld',
            'clang++': 'ld',
            'msvc': 'link',
            'cl': 'link',
            'rustc': 'rustc',
            'go': 'go',
            'javac': 'java'
        }
        return linker_map.get(compiler, 'ld')
    
    def get_default_build_system(self):
        """Get default build system"""
        if self.platform == 'windows':
            return 'cmake'
        else:
            return 'make'
    
    def create_build_script(self, config, project_type='c'):
        """Create build script for external toolchain"""
        script_name = f"build_{config['compiler']}.sh"
        
        if project_type == 'c':
            script_content = f"""#!/bin/bash
# Build script for {config['name']}

echo "Building with {config['compiler']}..."

# Set compiler and linker
CC={config['compiler']}
LD={config['linker']}

# Compile source files
echo "Compiling source files..."
$CC -c src/*.c -I include/

# Link object files
echo "Linking object files..."
$LD -o build/program *.o

echo "Build complete!"
"""
        elif project_type == 'cpp':
            script_content = f"""#!/bin/bash
# Build script for {config['name']}

echo "Building C++ project with {config['compiler']}..."

# Set compiler and linker
CXX={config['compiler']}
LD={config['linker']}

# Compile source files
echo "Compiling source files..."
$CXX -c src/*.cpp -I include/ -std=c++17

# Link object files
echo "Linking object files..."
$LD -o build/program *.o

echo "Build complete!"
"""
        elif project_type == 'rust':
            script_content = f"""#!/bin/bash
# Build script for Rust project

echo "Building Rust project..."

# Build with cargo
cargo build --release

echo "Build complete!"
"""
        elif project_type == 'go':
            script_content = f"""#!/bin/bash
# Build script for Go project

echo "Building Go project..."

# Build with go
go build -o build/program src/main.go

echo "Build complete!"
"""
        else:
            script_content = f"""#!/bin/bash
# Generic build script for {config['name']}

echo "Building with {config['compiler']}..."

# Add your build commands here
{config['compiler']} -o build/program src/*.c

echo "Build complete!"
"""
        
        with open(script_name, 'w') as f:
            f.write(script_content)
        os.chmod(script_name, 0o755)
        
        return {"success": True, "script": script_name}
    
    def create_cmake_config(self, config):
        """Create CMake configuration for external toolchain"""
        cmake_content = f"""cmake_minimum_required(VERSION 3.10)
project(ExternalToolchainProject)

# Set compiler
set(CMAKE_C_COMPILER {config['compiler']})
set(CMAKE_CXX_COMPILER {config['compiler']})

# Set linker
set(CMAKE_LINKER {config['linker']})

# Set build type
set(CMAKE_BUILD_TYPE Release)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.c" "src/*.cpp")
file(GLOB_RECURSE HEADERS "include/*.h" "include/*.hpp")

# Create executable
add_executable(${{PROJECT_NAME}} ${{SOURCES}} ${{HEADERS}})

# Set include directories
target_include_directories(${{PROJECT_NAME}} PRIVATE include/)

# Set compiler flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(${{PROJECT_NAME}} PRIVATE -Wall -Wextra -O2)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${{PROJECT_NAME}} PRIVATE -Wall -Wextra -O2)
elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${{PROJECT_NAME}} PRIVATE /W4 /O2)
endif()

# Set linker flags
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(${{PROJECT_NAME}} PRIVATE -static-libgcc -static-libstdc++)
elseif(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_link_options(${{PROJECT_NAME}} PRIVATE -static-libgcc -static-libstdc++)
endif()
"""
        
        with open('CMakeLists.txt', 'w') as f:
            f.write(cmake_content)
        
        return {"success": True, "file": "CMakeLists.txt"}
    
    def create_makefile(self, config):
        """Create Makefile for external toolchain"""
        makefile_content = f"""# Makefile for {config['name']}

# Compiler and linker
CC = {config['compiler']}
LD = {config['linker']}

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c99
CXXFLAGS = -Wall -Wextra -O2 -std=c++17

# Linker flags
LDFLAGS = 

# Directories
SRCDIR = src
INCDIR = include
BUILDDIR = build
OBJDIR = $(BUILDDIR)/obj

# Source files
SOURCES = $(wildcard $(SRCDIR)/*.c $(SRCDIR)/*.cpp)
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
OBJECTS := $(OBJECTS:$(SRCDIR)/%.cpp=$(OBJDIR)/%.o)

# Target
TARGET = $(BUILDDIR)/program

# Default target
all: $(TARGET)

# Create directories
$(OBJDIR):
\tmkdir -p $(OBJDIR)

$(BUILDDIR):
\tmkdir -p $(BUILDDIR)

# Compile source files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
\t$(CC) $(CFLAGS) -I$(INCDIR) -c $< -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.cpp | $(OBJDIR)
\t$(CC) $(CXXFLAGS) -I$(INCDIR) -c $< -o $@

# Link object files
$(TARGET): $(OBJECTS) | $(BUILDDIR)
\t$(LD) $(OBJECTS) -o $@ $(LDFLAGS)

# Clean build files
clean:
\trm -rf $(BUILDDIR)

# Install target
install: $(TARGET)
\tcp $(TARGET) /usr/local/bin/

# Uninstall target
uninstall:
\trm -f /usr/local/bin/program

# Help target
help:
\t@echo "Available targets:"
\t@echo "  all      - Build the program"
\t@echo "  clean    - Remove build files"
\t@echo "  install  - Install the program"
\t@echo "  uninstall- Remove the program"
\t@echo "  help     - Show this help"

.PHONY: all clean install uninstall help
"""
        
        with open('Makefile', 'w') as f:
            f.write(makefile_content)
        
        return {"success": True, "file": "Makefile"}
    
    def create_meson_config(self, config):
        """Create Meson configuration for external toolchain"""
        meson_content = f"""# Meson configuration for {config['name']}

project('ExternalToolchainProject', 'c', 'cpp',
  version : '1.0.0',
  default_options : ['warning_level=3',
                     'cpp_std=c++17'])

# Compiler
cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

# Set external compiler if specified
if '{config['compiler']}' != 'gcc'
  cc = find_program('{config['compiler']}')
endif

# Source files
sources = [
  'src/main.c',
  'src/utils.c',
]

# Include directories
incdir = include_directories('include')

# Create executable
executable('program',
  sources,
  include_directories : incdir,
  install : true)

# Tests
test('basic', executable('test_basic', 'tests/test_basic.c'))
"""
        
        with open('meson.build', 'w') as f:
            f.write(meson_content)
        
        return {"success": True, "file": "meson.build"}
    
    def create_ninja_config(self, config):
        """Create Ninja configuration for external toolchain"""
        ninja_content = f"""# Ninja configuration for {config['name']}

# Variables
cc = {config['compiler']}
ld = {config['linker']}
cflags = -Wall -Wextra -O2
ldflags = 

# Rules
rule cc
  command = $cc $cflags -c $in -o $out
  description = CC $out

rule link
  command = $ld $ldflags -o $out $in
  description = LINK $out

# Build targets
build obj/main.o: cc src/main.c
build obj/utils.o: cc src/utils.c

build program: link obj/main.o obj/utils.o

# Default target
default program
"""
        
        with open('build.ninja', 'w') as f:
            f.write(ninja_content)
        
        return {"success": True, "file": "build.ninja"}
    
    def create_vscode_config(self, config):
        """Create VS Code configuration for external toolchain"""
        vscode_config = {
            "version": "2.0.0",
            "tasks": [
                {
                    "label": "Build with External Toolchain",
                    "type": "shell",
                    "command": f"{config['compiler']}",
                    "args": [
                        "-Wall",
                        "-Wextra",
                        "-O2",
                        "-std=c99",
                        "-Iinclude",
                        "src/*.c",
                        "-o",
                        "build/program"
                    ],
                    "group": {
                        "kind": "build",
                        "isDefault": True
                    },
                    "presentation": {
                        "echo": True,
                        "reveal": "always",
                        "focus": False,
                        "panel": "shared"
                    },
                    "problemMatcher": ["$gcc"]
                }
            ],
            "configurations": [
                {
                    "name": "External Toolchain Debug",
                    "type": "cppdbg",
                    "request": "launch",
                    "program": "${workspaceFolder}/build/program",
                    "args": [],
                    "stopAtEntry": False,
                    "cwd": "${workspaceFolder}",
                    "environment": [],
                    "externalConsole": False,
                    "MIMode": "gdb",
                    "miDebuggerPath": "/usr/bin/gdb",
                    "setupCommands": [
                        {
                            "description": "Enable pretty-printing for gdb",
                            "text": "-enable-pretty-printing",
                            "ignoreFailures": True
                        }
                    ]
                }
            ]
        }
        
        # Create .vscode directory
        os.makedirs('.vscode', exist_ok=True)
        
        with open('.vscode/tasks.json', 'w') as f:
            json.dump(vscode_config, f, indent=2)
        
        return {"success": True, "file": ".vscode/tasks.json"}
    
    def create_dockerfile(self, config):
        """Create Dockerfile for external toolchain"""
        dockerfile_content = f"""# Dockerfile for {config['name']}

FROM ubuntu:22.04

# Install system dependencies
RUN apt-get update && apt-get install -y \\
    {config['compiler']} \\
    {config['linker']} \\
    make \\
    cmake \\
    ninja-build \\
    git \\
    curl \\
    wget \\
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Copy source code
COPY . /workspace/

# Build the project
RUN make

# Expose port
EXPOSE 8080

# Run the program
CMD ["./build/program"]
"""
        
        with open('Dockerfile.external', 'w') as f:
            f.write(dockerfile_content)
        
        return {"success": True, "file": "Dockerfile.external"}
    
    def create_docker_compose(self, config):
        """Create docker-compose.yml for external toolchain"""
        compose_content = f"""version: '3.8'

services:
  external-toolchain:
    build:
      context: .
      dockerfile: Dockerfile.external
    container_name: external-toolchain-dev
    ports:
      - "8080:8080"
    volumes:
      - .:/workspace
    environment:
      - CC={config['compiler']}
      - LD={config['linker']}
    networks:
      - external-network

networks:
  external-network:
    driver: bridge
"""
        
        with open('docker-compose.external.yml', 'w') as f:
            f.write(compose_content)
        
        return {"success": True, "file": "docker-compose.external.yml"}
    
    def get_toolchain_status(self):
        """Get status of external toolchains"""
        return {
            "platform": self.platform,
            "architecture": self.architecture,
            "available_compilers": len(self.available_compilers),
            "available_linkers": len(self.available_linkers),
            "available_build_systems": len(self.available_build_systems),
            "compilers": self.available_compilers,
            "linkers": self.available_linkers,
            "build_systems": self.available_build_systems
        }
    
    def create_complete_toolchain_setup(self, config):
        """Create complete toolchain setup"""
        print(f"Creating complete toolchain setup for {config['name']}...")
        
        results = []
        
        # Create build script
        script_result = self.create_build_script(config)
        results.append(script_result)
        
        # Create CMake configuration
        cmake_result = self.create_cmake_config(config)
        results.append(cmake_result)
        
        # Create Makefile
        makefile_result = self.create_makefile(config)
        results.append(makefile_result)
        
        # Create Meson configuration
        meson_result = self.create_meson_config(config)
        results.append(meson_result)
        
        # Create Ninja configuration
        ninja_result = self.create_ninja_config(config)
        results.append(ninja_result)
        
        # Create VS Code configuration
        vscode_result = self.create_vscode_config(config)
        results.append(vscode_result)
        
        # Create Dockerfile
        dockerfile_result = self.create_dockerfile(config)
        results.append(dockerfile_result)
        
        # Create docker-compose
        compose_result = self.create_docker_compose(config)
        results.append(compose_result)
        
        return {
            "success": True,
            "config": config,
            "files_created": [r.get("file", r.get("script", "unknown")) for r in results if r.get("success")]
        }

def main():
    """Test external toolchain support system"""
    print("Testing External Toolchain Support System...")
    
    toolchain_support = ExternalToolchainSupport()
    
    # Show status
    status = toolchain_support.get_toolchain_status()
    print(f"\nPlatform: {status['platform']} {status['architecture']}")
    print(f"Available compilers: {status['available_compilers']}")
    print(f"Available linkers: {status['available_linkers']}")
    print(f"Available build systems: {status['available_build_systems']}")
    
    # Test creating toolchain config
    if status['available_compilers'] > 0:
        compiler = status['compilers'][0][0]
        print(f"\nTesting toolchain configuration for {compiler}...")
        
        config = toolchain_support.create_toolchain_config(compiler)
        print(f"Created config: {config}")
        
        # Test complete setup
        setup_result = toolchain_support.create_complete_toolchain_setup(config)
        if setup_result["success"]:
            print(f"✅ Complete toolchain setup created!")
            print(f"Files created: {', '.join(setup_result['files_created'])}")
        else:
            print("❌ Failed to create complete toolchain setup")
    
    print("\nExternal Toolchain Support System test complete!")

if __name__ == "__main__":
    main()
