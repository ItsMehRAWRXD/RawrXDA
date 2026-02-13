#!/usr/bin/env python3
"""
validate-copilot-instructions.py - Verify copilot instructions match actual codebase

This script checks that documented patterns, file locations, and configurations
in .github/copilot-instructions.md match the actual codebase state.

Run in CI/CD pipeline to catch outdated instructions early.
"""

import os
import re
import sys
import json
from pathlib import Path
from typing import List, Tuple, Dict

class CopilotInstructionValidator:
    def __init__(self, repo_root: str = "."):
        self.repo_root = Path(repo_root)
        self.instructions_file = self.repo_root / ".github" / "copilot-instructions.md"
        self.errors = []
        self.warnings = []
        
    def read_instructions(self) -> str:
        """Read the copilot instructions file."""
        if not self.instructions_file.exists():
            self.errors.append(f"❌ Copilot instructions not found: {self.instructions_file}")
            return ""
        return self.instructions_file.read_text()
    
    def check_file_references(self, instructions: str) -> None:
        """Verify all referenced files actually exist."""
        # Extract file paths from backtick references and paths
        file_pattern = r'`src/[^`]+\.(?:hpp|cpp|h|c)`'
        referenced_files = re.findall(file_pattern, instructions)
        
        for ref in referenced_files:
            file_path = ref.strip('`')
            full_path = self.repo_root / file_path
            if not full_path.exists():
                self.errors.append(f"❌ Referenced file not found: {file_path}")
            else:
                # Warn if file has .disabled suffix (indicates disabled component)
                if full_path.with_suffix(file_path.split('.')[-1] + '.disabled').exists():
                    self.warnings.append(f"⚠️  File may be disabled: {file_path}")
    
    def check_directory_structure(self, instructions: str) -> None:
        """Verify documented directory structure matches reality."""
        dirs_to_check = [
            "src/qtapp",
            "src/agent",
            ".github",
        ]
        
        for dir_ref in dirs_to_check:
            if dir_ref in instructions:
                full_path = self.repo_root / dir_ref
                if not full_path.is_dir():
                    self.errors.append(f"❌ Referenced directory not found: {dir_ref}")
    
    def check_build_targets(self, instructions: str) -> None:
        """Verify CMakeLists.txt contains documented build targets."""
        cmake_file = self.repo_root / "CMakeLists.txt"
        if not cmake_file.exists():
            self.errors.append("❌ CMakeLists.txt not found")
            return
        
        cmake_content = cmake_file.read_text()
        targets = ["RawrXD-QtShell", "self_test_gate", "quant_utils"]
        
        for target in targets:
            if f"add_executable({target}" not in cmake_content and f"add_library({target}" not in cmake_content:
                self.errors.append(f"❌ Build target not found in CMakeLists.txt: {target}")
    
    def check_qt_configuration(self, instructions: str) -> None:
        """Verify Qt configuration matches CMakeLists.txt."""
        cmake_file = self.repo_root / "CMakeLists.txt"
        if not cmake_file.exists():
            return
        
        cmake_content = cmake_file.read_text()
        
        # Check Qt MOC setting
        if "CMAKE_AUTOMOC ON" in instructions:
            if "CMAKE_AUTOMOC ON" not in cmake_content:
                self.warnings.append("⚠️  Instructions mention CMAKE_AUTOMOC ON but not found in CMakeLists.txt")
        
        # Check C++ standard
        if "C++20" in instructions:
            if "CMAKE_CXX_STANDARD 20" not in cmake_content:
                self.warnings.append("⚠️  Instructions require C++20 but CMakeLists.txt may differ")
    
    def check_hotpatch_files(self, instructions: str) -> None:
        """Verify all documented hotpatch files exist and are enabled."""
        hotpatch_files = [
            "src/qtapp/model_memory_hotpatch.hpp",
            "src/qtapp/model_memory_hotpatch.cpp",
            "src/qtapp/byte_level_hotpatcher.hpp",
            "src/qtapp/byte_level_hotpatcher.cpp",
            "src/qtapp/gguf_server_hotpatch.hpp",
            "src/qtapp/gguf_server_hotpatch.cpp",
            "src/qtapp/unified_hotpatch_manager.hpp",
            "src/qtapp/unified_hotpatch_manager.cpp",
            "src/qtapp/proxy_hotpatcher.hpp",
            "src/qtapp/proxy_hotpatcher.cpp",
        ]
        
        for hotpatch_file in hotpatch_files:
            full_path = self.repo_root / hotpatch_file
            if not full_path.exists():
                self.errors.append(f"❌ Hotpatch file missing: {hotpatch_file}")
            else:
                # Check if commented out in CMakeLists
                cmake_file = self.repo_root / "CMakeLists.txt"
                if cmake_file.exists():
                    cmake_content = cmake_file.read_text()
                    if f"# {hotpatch_file}" in cmake_content and hotpatch_file not in [line for line in cmake_content.split('\n') if not line.strip().startswith('#')]:
                        self.warnings.append(f"⚠️  Hotpatch file may be disabled in CMakeLists: {hotpatch_file}")
    
    def check_agentic_files(self, instructions: str) -> None:
        """Verify agentic system files exist."""
        agentic_files = [
            "src/agent/agentic_failure_detector.hpp",
            "src/agent/agentic_failure_detector.cpp",
            "src/agent/agentic_puppeteer.hpp",
            "src/agent/agentic_puppeteer.cpp",
        ]
        
        for agentic_file in agentic_files:
            full_path = self.repo_root / agentic_file
            if not full_path.exists():
                self.errors.append(f"❌ Agentic file missing: {agentic_file}")
    
    def check_class_patterns(self, instructions: str) -> None:
        """Verify documented class patterns exist in code."""
        patterns = {
            "PatchResult": "src/qtapp/model_memory_hotpatch.hpp",
            "UnifiedResult": "src/qtapp/unified_hotpatch_manager.hpp",
            "CorrectionResult": "src/agent/agentic_puppeteer.hpp",
            "QObject": None,  # Qt standard, no check needed
        }
        
        for pattern, file_ref in patterns.items():
            if file_ref and pattern in instructions:
                full_path = self.repo_root / file_ref
                if full_path.exists():
                    content = full_path.read_text()
                    if f"struct {pattern}" not in content and f"class {pattern}" not in content:
                        self.warnings.append(f"⚠️  Pattern '{pattern}' mentioned in instructions but may not be defined in {file_ref}")
    
    def check_build_command_accuracy(self, instructions: str) -> None:
        """Verify documented build commands are achievable."""
        cmake_file = self.repo_root / "CMakeLists.txt"
        if not cmake_file.exists():
            return
        
        # Extract cmake commands from instructions
        command_pattern = r'cmake --build \. --config Release --target (\S+)'
        targets = re.findall(command_pattern, instructions)
        
        cmake_content = cmake_file.read_text()
        for target in targets:
            if target and not any(f"add_executable({target}" in cmake_content or f"add_library({target}" in cmake_content for _ in [1]):
                if f"add_executable({target}" not in cmake_content and f"add_library({target}" not in cmake_content:
                    self.warnings.append(f"⚠️  Build command references target '{target}' but may not be defined: cmake --build . --config Release --target {target}")
    
    def validate(self) -> Tuple[bool, str]:
        """Run all validations and return results."""
        instructions = self.read_instructions()
        if not instructions:
            return False, "Failed to read copilot instructions"
        
        # Run all checks
        self.check_file_references(instructions)
        self.check_directory_structure(instructions)
        self.check_build_targets(instructions)
        self.check_qt_configuration(instructions)
        self.check_hotpatch_files(instructions)
        self.check_agentic_files(instructions)
        self.check_class_patterns(instructions)
        self.check_build_command_accuracy(instructions)
        
        # Generate report
        report = self._generate_report()
        success = len(self.errors) == 0
        
        return success, report
    
    def _generate_report(self) -> str:
        """Generate validation report."""
        lines = ["=" * 60]
        lines.append("COPILOT INSTRUCTIONS VALIDATION REPORT")
        lines.append("=" * 60)
        
        if not self.errors and not self.warnings:
            lines.append("✅ All validations passed!")
            return "\n".join(lines)
        
        if self.errors:
            lines.append("\n❌ ERRORS (must fix):")
            for error in self.errors:
                lines.append(f"  {error}")
        
        if self.warnings:
            lines.append("\n⚠️  WARNINGS (review):")
            for warning in self.warnings:
                lines.append(f"  {warning}")
        
        lines.append("\n" + "=" * 60)
        lines.append(f"Summary: {len(self.errors)} errors, {len(self.warnings)} warnings")
        lines.append("=" * 60)
        
        return "\n".join(lines)


def main():
    """Main entry point."""
    validator = CopilotInstructionValidator()
    success, report = validator.validate()
    
    print(report)
    
    if not success:
        sys.exit(1)
    sys.exit(0)


if __name__ == "__main__":
    main()
