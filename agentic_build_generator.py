#!/usr/bin/env python3
"""
RawrXD Autonomous Build System Generator
Reverse engineers CMake configurations and generates custom build files.
"""

import os
import re
import json
import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from dataclasses import dataclass, asdict
import yaml

@dataclass
class BuildTarget:
    name: str
    type: str  # executable, library, test
    sources: List[str]
    dependencies: List[str]
    compile_flags: List[str]
    link_flags: List[str]
    definitions: List[str]
    include_dirs: List[str]
    output_dir: str = "bin"

@dataclass 
class BuildConfiguration:
    project_name: str
    version: str
    cxx_standard: int
    targets: List[BuildTarget]
    global_definitions: List[str]
    global_include_dirs: List[str]
    external_dependencies: Dict[str, str]

class CMakeReverseEngineer:
    def __init__(self, project_root: str):
        self.project_root = Path(project_root)
        self.cmake_file = self.project_root / "CMakeLists.txt"
        self.build_config = None
        
    def analyze_cmake(self) -> BuildConfiguration:
        """Reverse engineer CMakeLists.txt to extract build configuration."""
        print("🔍 Analyzing CMakeLists.txt...")
        
        with open(self.cmake_file, 'r', encoding='utf-8') as f:
            cmake_content = f.read()
        
        # Extract project info
        project_match = re.search(r'project\(([^)]+)\)', cmake_content)
        project_name = "RawrXD"
        version = "1.0.0"
        
        if project_match:
            project_line = project_match.group(1)
            name_match = re.search(r'([^\s]+)', project_line)
            if name_match:
                project_name = name_match.group(1)
            version_match = re.search(r'VERSION\s+([0-9.]+)', project_line)
            if version_match:
                version = version_match.group(1)
        
        # Extract C++ standard
        cxx_std_match = re.search(r'CMAKE_CXX_STANDARD\s+(\d+)', cmake_content)
        cxx_standard = int(cxx_std_match.group(1)) if cxx_std_match else 20
        
        # Extract global definitions
        global_defs = []
        for match in re.finditer(r'add_definitions\(([^)]+)\)', cmake_content):
            defs = [d.strip() for d in match.group(1).split()]
            global_defs.extend(defs)
        
        # Extract targets
        targets = self._extract_targets(cmake_content)
        
        # Extract dependencies
        dependencies = self._extract_dependencies(cmake_content)
        
        self.build_config = BuildConfiguration(
            project_name=project_name,
            version=version,
            cxx_standard=cxx_standard,
            targets=targets,
            global_definitions=global_defs,
            global_include_dirs=["include", "src"],
            external_dependencies=dependencies
        )
        
        print(f"✅ Found {len(targets)} build targets")
        return self.build_config
    
    def _extract_targets(self, cmake_content: str) -> List[BuildTarget]:
        """Extract all executable and library targets."""
        targets = []
        
        # Find add_executable and add_library commands
        target_pattern = r'add_(executable|library)\(\s*([^\s]+)\s*((?:[^)]+|\n)*)\)'
        
        for match in re.finditer(target_pattern, cmake_content, re.MULTILINE | re.DOTALL):
            target_type = match.group(1)
            target_name = match.group(2)
            target_sources = match.group(3)
            
            # Parse sources
            sources = []
            for line in target_sources.split('\n'):
                line = line.strip()
                if line and not line.startswith('#') and not line.startswith('$'):
                    # Remove comments and clean up
                    line = re.sub(r'#.*', '', line)
                    if line.endswith('.cpp') or line.endswith('.c') or line.endswith('.hpp') or line.endswith('.h'):
                        sources.append(line.strip())
            
            # Extract target properties
            target_info = self._extract_target_properties(cmake_content, target_name)
            
            target = BuildTarget(
                name=target_name,
                type=target_type,
                sources=sources,
                dependencies=target_info.get('dependencies', []),
                compile_flags=target_info.get('compile_flags', []),
                link_flags=target_info.get('link_flags', []),
                definitions=target_info.get('definitions', []),
                include_dirs=target_info.get('include_dirs', [])
            )
            
            targets.append(target)
        
        return targets
    
    def _extract_target_properties(self, cmake_content: str, target_name: str) -> Dict:
        """Extract properties for a specific target."""
        properties = {
            'dependencies': [],
            'compile_flags': [],
            'link_flags': [],
            'definitions': [],
            'include_dirs': []
        }
        
        # Find target_link_libraries
        link_pattern = rf'target_link_libraries\(\s*{re.escape(target_name)}\s+[^)]*\s+([^)]+)\)'
        for match in re.finditer(link_pattern, cmake_content):
            libs = [lib.strip() for lib in match.group(1).split() if lib.strip()]
            properties['dependencies'].extend(libs)
        
        # Find target_include_directories
        include_pattern = rf'target_include_directories\(\s*{re.escape(target_name)}\s+[^)]*\s+([^)]+)\)'
        for match in re.finditer(include_pattern, cmake_content):
            dirs = [d.strip() for d in match.group(1).split() if d.strip() and not d.startswith('$')]
            properties['include_dirs'].extend(dirs)
        
        # Find target_compile_definitions
        def_pattern = rf'target_compile_definitions\(\s*{re.escape(target_name)}\s+[^)]*\s+([^)]+)\)'
        for match in re.finditer(def_pattern, cmake_content):
            defs = [d.strip() for d in match.group(1).split() if d.strip()]
            properties['definitions'].extend(defs)
        
        return properties
    
    def _extract_dependencies(self, cmake_content: str) -> Dict[str, str]:
        """Extract external dependencies."""
        deps = {}
        
        # Find find_package commands
        for match in re.finditer(r'find_package\(([^)]+)\)', cmake_content):
            package_line = match.group(1)
            package_name = package_line.split()[0]
            
            # Determine if required
            if "REQUIRED" in package_line:
                deps[package_name] = "REQUIRED"
            elif "QUIET" in package_line:
                deps[package_name] = "OPTIONAL"
            else:
                deps[package_name] = "OPTIONAL"
        
        return deps

class AgenticBuildGenerator:
    def __init__(self, build_config: BuildConfiguration, output_dir: str):
        self.config = build_config
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
        
    def generate_ninja_build(self):
        """Generate Ninja build files."""
        print("🔨 Generating Ninja build files...")
        
        build_ninja = self.output_dir / "build.ninja"
        
        with open(build_ninja, 'w') as f:
            # Write header
            f.write("# Auto-generated by RawrXD Agentic Build System\n")
            f.write(f"# Project: {self.config.project_name} v{self.config.version}\n\n")
            
            # Variables
            f.write("CXX = cl.exe\n")
            f.write("LINK = link.exe\n")
            f.write(f"CXXFLAGS = /std:c++{self.config.cxx_standard} /EHsc /MD /O2\n")
            f.write("LINKFLAGS = /SUBSYSTEM:CONSOLE\n\n")
            
            # Global definitions
            if self.config.global_definitions:
                defs = " ".join(self.config.global_definitions)
                f.write(f"DEFINES = {defs}\n\n")
            
            # Rules
            f.write("rule compile\n")
            f.write("  command = $CXX $CXXFLAGS $DEFINES /I$includes /c $in /Fo$out\n")
            f.write("  description = Compiling $in\n\n")
            
            f.write("rule link\n") 
            f.write("  command = $LINK $LINKFLAGS $in /OUT:$out\n")
            f.write("  description = Linking $out\n\n")
            
            # Build statements
            for target in self.config.targets:
                self._write_ninja_target(f, target)
        
        print(f"✅ Generated {build_ninja}")
    
    def _write_ninja_target(self, f, target: BuildTarget):
        """Write Ninja build statements for a target."""
        obj_files = []
        
        # Compile each source file
        for source in target.sources:
            if source.endswith(('.cpp', '.c')):
                obj_file = f"obj/{target.name}/{Path(source).stem}.obj"
                obj_files.append(obj_file)
                
                includes = " ".join([f"/I{inc}" for inc in target.include_dirs + self.config.global_include_dirs])
                
                f.write(f"build {obj_file}: compile {source}\n")
                f.write(f"  includes = {includes}\n")
        
        # Link target
        if target.type == "executable":
            output = f"bin/{target.name}.exe"
        else:
            output = f"lib/{target.name}.lib"
        
        f.write(f"\nbuild {output}: link {' '.join(obj_files)}\n\n")
    
    def generate_makefile(self):
        """Generate traditional Makefile."""
        print("🔨 Generating Makefile...")
        
        makefile = self.output_dir / "Makefile"
        
        with open(makefile, 'w') as f:
            # Write header
            f.write("# Auto-generated by RawrXD Agentic Build System\n")
            f.write(f"# Project: {self.config.project_name} v{self.config.version}\n\n")
            
            # Variables
            f.write("CXX = cl.exe\n")
            f.write("LINK = link.exe\n")
            f.write(f"CXXFLAGS = /std:c++{self.config.cxx_standard} /EHsc /MD /O2\n")
            f.write("LINKFLAGS = /SUBSYSTEM:CONSOLE\n")
            
            # Global definitions and includes
            if self.config.global_definitions:
                f.write(f"DEFINES = {' '.join(self.config.global_definitions)}\n")
            
            includes = " ".join([f"/I{inc}" for inc in self.config.global_include_dirs])
            f.write(f"INCLUDES = {includes}\n\n")
            
            # Targets
            all_targets = [t.name for t in self.config.targets if t.type == "executable"]
            f.write(f"all: {' '.join(all_targets)}\n\n")
            
            for target in self.config.targets:
                self._write_makefile_target(f, target)
            
            # Clean target
            f.write("clean:\n")
            f.write("\t@echo Cleaning build artifacts...\n")
            f.write("\t@rmdir /s /q obj bin lib 2>nul || echo Clean complete\n\n")
        
        print(f"✅ Generated {makefile}")
    
    def _write_makefile_target(self, f, target: BuildTarget):
        """Write Makefile rules for a target."""
        if target.type == "executable":
            output = f"bin\\{target.name}.exe"
        else:
            output = f"lib\\{target.name}.lib"
        
        # Object files
        obj_files = []
        for source in target.sources:
            if source.endswith(('.cpp', '.c')):
                obj_file = f"obj\\{target.name}\\{Path(source).stem}.obj"
                obj_files.append(obj_file)
        
        # Target rule
        f.write(f"{target.name}: {output}\n\n")
        f.write(f"{output}: {' '.join(obj_files)}\n")
        f.write(f"\t@echo Linking {target.name}...\n")
        f.write(f"\t@if not exist bin mkdir bin\n")
        f.write(f"\t$(LINK) $(LINKFLAGS) {' '.join(obj_files)} /OUT:{output}\n\n")
        
        # Object file rules
        for source in target.sources:
            if source.endswith(('.cpp', '.c')):
                obj_file = f"obj\\{target.name}\\{Path(source).stem}.obj"
                f.write(f"{obj_file}: {source}\n")
                f.write(f"\t@echo Compiling {source}...\n")
                f.write(f"\t@if not exist obj\\{target.name} mkdir obj\\{target.name}\n")
                f.write(f"\t$(CXX) $(CXXFLAGS) $(DEFINES) $(INCLUDES) /c {source} /Fo{obj_file}\n\n")
    
    def generate_powershell_build(self):
        """Generate PowerShell build script."""
        print("🔨 Generating PowerShell build script...")
        
        ps1_file = self.output_dir / "build.ps1"
        
        with open(ps1_file, 'w') as f:
            f.write("#!/usr/bin/env pwsh\n")
            f.write("# Auto-generated by RawrXD Agentic Build System\n")
            f.write(f"# Project: {self.config.project_name} v{self.config.version}\n\n")
            
            f.write("param(\n")
            f.write("    [string]$Target = 'all',\n")
            f.write("    [switch]$Clean,\n")
            f.write("    [switch]$Verbose\n")
            f.write(")\n\n")
            
            # Functions
            f.write("function Write-BuildInfo { param($msg) Write-Host \"[BUILD] $msg\" -ForegroundColor Green }\n")
            f.write("function Write-BuildError { param($msg) Write-Host \"[ERROR] $msg\" -ForegroundColor Red }\n\n")
            
            # Build configuration
            f.write("$CXX = 'cl.exe'\n")
            f.write("$LINK = 'link.exe'\n")
            f.write(f"$CXXFLAGS = '/std:c++{self.config.cxx_standard}', '/EHsc', '/MD', '/O2'\n")
            f.write("$LINKFLAGS = '/SUBSYSTEM:CONSOLE'\n\n")
            
            # Global definitions and includes
            if self.config.global_definitions:
                defs = "', '".join(self.config.global_definitions)
                f.write(f"$DEFINES = '{defs}'\n")
            
            includes = "', '".join([f"/I{inc}" for inc in self.config.global_include_dirs])
            f.write(f"$INCLUDES = '{includes}'\n\n")
            
            # Clean function
            f.write("if ($Clean) {\n")
            f.write("    Write-BuildInfo 'Cleaning build artifacts...'\n")
            f.write("    Remove-Item -Recurse -Force obj, bin, lib -ErrorAction SilentlyContinue\n")
            f.write("    exit 0\n")
            f.write("}\n\n")
            
            # Create directories
            f.write("Write-BuildInfo 'Creating build directories...'\n")
            f.write("New-Item -ItemType Directory -Force obj, bin, lib | Out-Null\n\n")
            
            # Build targets
            for target in self.config.targets:
                self._write_powershell_target(f, target)
            
            f.write("Write-BuildInfo 'Build completed successfully!'\n")
        
        print(f"✅ Generated {ps1_file}")
    
    def _write_powershell_target(self, f, target: BuildTarget):
        """Write PowerShell build commands for a target."""
        f.write(f"# Building {target.name}\n")
        f.write(f"Write-BuildInfo 'Building {target.name}...'\n")
        f.write(f"New-Item -ItemType Directory -Force obj\\{target.name} | Out-Null\n\n")
        
        obj_files = []
        for source in target.sources:
            if source.endswith(('.cpp', '.c')):
                obj_file = f"obj\\{target.name}\\{Path(source).stem}.obj"
                obj_files.append(obj_file)
                
                f.write(f"# Compile {source}\n")
                f.write(f"& $CXX $CXXFLAGS $DEFINES $INCLUDES /c '{source}' '/Fo{obj_file}'\n")
                f.write("if ($LASTEXITCODE -ne 0) { Write-BuildError 'Compilation failed'; exit 1 }\n\n")
        
        # Link
        if target.type == "executable":
            output = f"bin\\{target.name}.exe"
        else:
            output = f"lib\\{target.name}.lib"
        
        f.write(f"# Link {target.name}\n")
        f.write(f"& $LINK $LINKFLAGS {' '.join(obj_files)} '/OUT:{output}'\n")
        f.write("if ($LASTEXITCODE -ne 0) { Write-BuildError 'Linking failed'; exit 1 }\n\n")
    
    def generate_json_config(self):
        """Generate JSON configuration for build automation."""
        print("📄 Generating JSON build configuration...")
        
        config_file = self.output_dir / "build_config.json"
        
        config_data = {
            "project": {
                "name": self.config.project_name,
                "version": self.config.version,
                "cxx_standard": self.config.cxx_standard
            },
            "global": {
                "definitions": self.config.global_definitions,
                "include_dirs": self.config.global_include_dirs,
                "dependencies": self.config.external_dependencies
            },
            "targets": [asdict(target) for target in self.config.targets],
            "generated_by": "RawrXD Agentic Build System",
            "timestamp": "2026-01-17T12:00:00Z"
        }
        
        with open(config_file, 'w') as f:
            json.dump(config_data, f, indent=2)
        
        print(f"✅ Generated {config_file}")

def main():
    """Main entry point for the agentic build system."""
    print("🚀 RawrXD Autonomous Build System Generator")
    print("=" * 50)
    
    project_root = "E:\\RawrXD"
    output_dir = "E:\\RawrXD\\agentic_build"
    
    # Step 1: Reverse engineer CMake configuration
    print("\n📋 Phase 1: Reverse Engineering CMake Configuration")
    cmake_analyzer = CMakeReverseEngineer(project_root)
    
    if not cmake_analyzer.cmake_file.exists():
        print(f"❌ CMakeLists.txt not found at {cmake_analyzer.cmake_file}")
        return
    
    build_config = cmake_analyzer.analyze_cmake()
    
    # Step 2: Generate custom build files
    print("\n🔧 Phase 2: Generating Custom Build Files")
    generator = AgenticBuildGenerator(build_config, output_dir)
    
    generator.generate_ninja_build()
    generator.generate_makefile()
    generator.generate_powershell_build()
    generator.generate_json_config()
    
    # Step 3: Generate autonomous build wrapper
    print("\n🤖 Phase 3: Generating Autonomous Build Wrapper")
    generate_autonomous_wrapper(output_dir)
    
    print("\n" + "=" * 50)
    print("🎉 Agentic Build System Generation Complete!")
    print(f"📁 Output directory: {output_dir}")
    print("\nGenerated files:")
    print("  - build.ninja      (Ninja build file)")
    print("  - Makefile         (Traditional makefile)")
    print("  - build.ps1        (PowerShell build script)")
    print("  - build_config.json (JSON configuration)")
    print("  - auto_build.py    (Autonomous wrapper)")

def generate_autonomous_wrapper(output_dir: str):
    """Generate the autonomous build wrapper script."""
    wrapper_file = Path(output_dir) / "auto_build.py"
    
    wrapper_code = '''#!/usr/bin/env python3
"""
Autonomous Build Wrapper for RawrXD
Automatically detects and runs the best available build system.
"""

import os
import sys
import subprocess
import json
from pathlib import Path

def detect_build_tools():
    """Detect available build tools."""
    tools = {}
    
    # Check for Ninja
    try:
        subprocess.run(["ninja", "--version"], capture_output=True, check=True)
        tools["ninja"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["ninja"] = False
    
    # Check for Make (nmake on Windows)
    try:
        subprocess.run(["nmake", "/?"], capture_output=True, check=True)
        tools["nmake"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["nmake"] = False
    
    # Check for PowerShell
    try:
        subprocess.run(["pwsh", "--version"], capture_output=True, check=True)
        tools["pwsh"] = True
    except (subprocess.CalledProcessError, FileNotFoundError):
        tools["pwsh"] = False
    
    return tools

def run_build(build_tool: str, target: str = "all"):
    """Run the build with the specified tool."""
    print(f"🔨 Building with {build_tool}...")
    
    if build_tool == "ninja":
        cmd = ["ninja"]
        if target != "all":
            cmd.append(target)
    elif build_tool == "nmake":
        cmd = ["nmake"]
        if target != "all":
            cmd.append(target)
    elif build_tool == "pwsh":
        cmd = ["pwsh", "-File", "build.ps1"]
        if target != "all":
            cmd.extend(["-Target", target])
    else:
        raise ValueError(f"Unknown build tool: {build_tool}")
    
    result = subprocess.run(cmd, capture_output=False)
    return result.returncode == 0

def main():
    """Main autonomous build function."""
    print("🤖 RawrXD Autonomous Build System")
    print("=" * 40)
    
    # Load configuration
    config_file = Path("build_config.json")
    if config_file.exists():
        with open(config_file) as f:
            config = json.load(f)
        print(f"📋 Project: {config['project']['name']} v{config['project']['version']}")
    
    # Detect available tools
    tools = detect_build_tools()
    print(f"🔍 Available build tools: {[k for k, v in tools.items() if v]}")
    
    # Choose best available tool
    if tools["ninja"]:
        build_tool = "ninja"
    elif tools["nmake"]:
        build_tool = "nmake"
    elif tools["pwsh"]:
        build_tool = "pwsh"
    else:
        print("❌ No supported build tools found!")
        print("Install one of: ninja, nmake, or PowerShell")
        return 1
    
    # Run the build
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    success = run_build(build_tool, target)
    
    if success:
        print("✅ Build completed successfully!")
        return 0
    else:
        print("❌ Build failed!")
        return 1

if __name__ == "__main__":
    sys.exit(main())
'''
    
    with open(wrapper_file, 'w') as f:
        f.write(wrapper_code)
    
    print(f"✅ Generated {wrapper_file}")

if __name__ == "__main__":
    main()