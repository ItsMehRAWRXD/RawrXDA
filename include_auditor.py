#!/usr/bin/env python3
# ============================================================================
# include_auditor.py — Module Inclusion Consistency Validator
# ============================================================================
# FIX #7: Automated verification of canonical include patterns
# 
# Usage:
#   python include_auditor.py --path=src/ --strict
#   python include_auditor.py --graph  (output dependency graph)
# ============================================================================

import os
import re
import sys
from collections import defaultdict
from pathlib import Path

class IncludeAuditor:
    def __init__(self, root_path):
        self.root_path = Path(root_path)
        self.includes = defaultdict(list)  # file -> list of includes
        self.dependencies = defaultdict(set)  # file -> set of files it depends on
        self.errors = []
        self.warnings = []
        
    def extract_includes(self, file_path):
        """Extract all #include statements from a file."""
        includes = []
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                for line_num, line in enumerate(f, 1):
                    # Match: #include "file.h" or #include <file.h>
                    match = re.match(r'\s*#include\s+[<"]([^>"]+)[>"]', line)
                    if match:
                        includes.append({
                            'line': line_num,
                            'file': match.group(1),
                            'is_system': '<' in line,
                            'raw': line.strip()
                        })
        except Exception as e:
            self.errors.append(f"Cannot read {file_path}: {e}")
        return includes
    
    def classify_include(self, inc_file):
        """Classify include into layer."""
        # System/stdlib headers
        if inc_file.startswith(('windows', 'cstd', 'c++', 'vector', 'string', 
                               'map', 'mutex', 'thread', 'atomic', 'functional',
                               'memory', 'algorithm', 'fstream', 'sstream', 'iostream')):
            return 'SYSTEM'
        
        # Third-party
        if 'nlohmann' in inc_file or 'json' in inc_file:
            return 'THIRDPARTY'
        
        # Local includes (heuristic)
        if inc_file.endswith('.h') or inc_file.endswith('.hpp'):
            if 'win32app' in inc_file or 'agentic' in inc_file:
                return 'LAYER3_OR_4'
            if 'core' in inc_file or 'utils' in inc_file:
                return 'LAYER1_OR_2'
        
        return 'UNKNOWN'
    
    def validate_ordering(self, file_path, includes):
        """Check if includes follow canonical ordering."""
        if not includes:
            return []
        
        violations = []
        groups = defaultdict(list)
        
        # Group by classification
        for inc in includes:
            classification = self.classify_include(inc['file'])
            groups[classification].append(inc)
        
        # Expected order: SYSTEM -> THIRDPARTY -> LAYER -> LOCAL
        expected_order = ['SYSTEM', 'THIRDPARTY', 'LAYER1_OR_2', 'LAYER3_OR_4', 'UNKNOWN']
        last_group_idx = -1
        
        for inc in includes:
            classification = self.classify_include(inc['file'])
            group_idx = expected_order.index(classification) if classification in expected_order else 999
            
            if group_idx < last_group_idx:
                violations.append({
                    'file': file_path,
                    'line': inc['line'],
                    'issue': f"Misordered include: {inc['file']} ({classification}) after previous group",
                    'severity': 'WARNING'
                })
            last_group_idx = group_idx
        
        # Check if system headers are alphabetized
        system_incs = [inc['file'] for inc in groups.get('SYSTEM', [])]
        if system_incs != sorted(system_incs):
            violations.append({
                'file': file_path,
                'line': includes[0]['line'],
                'issue': "System headers not alphabetized",
                'severity': 'WARNING'
            })
        
        return violations
    
    def audit_directory(self, dir_path=None):
        """Recursively audit all .cpp and .h files."""
        if dir_path is None:
            dir_path = self.root_path
        
        for root, dirs, files in os.walk(dir_path):
            # Skip build directory
            if 'build' in root or 'obj' in root:
                continue
            
            for file in files:
                if file.endswith(('.cpp', '.h', '.hpp')):
                    file_path = Path(root) / file
                    includes = self.extract_includes(file_path)
                    self.includes[str(file_path)] = includes
                    
                    # Validate ordering
                    violations = self.validate_ordering(str(file_path), includes)
                    self.warnings.extend(violations)
    
    def print_report(self):
        """Print audit report."""
        print("=" * 80)
        print("MODULE INCLUSION CONSISTENCY AUDIT")
        print("=" * 80)
        
        total_files = len(self.includes)
        files_with_issues = len([f for f, inc in self.includes.items() if any(
            v['file'] == f for v in self.warnings)])
        
        print(f"\nTotal files scanned: {total_files}")
        print(f"Files with issues: {files_with_issues}")
        print(f"Total violations: {len(self.warnings)}")
        print(f"Total errors: {len(self.errors)}")
        
        if self.errors:
            print("\nERRORS:")
            for error in self.errors[:10]:
                print(f"  ❌ {error}")
        
        if self.warnings:
            print("\nWARNINGS (first 20):")
            for warning in self.warnings[:20]:
                print(f"  ⚠️  {warning['file']}:{warning['line']}")
                print(f"      {warning['issue']}")
        
        # Compliance score
        compliance_percent = ((total_files - files_with_issues) / total_files * 100) if total_files > 0 else 0
        print(f"\nCompliance Score: {compliance_percent:.1f}%")
        
        return compliance_percent >= 95  # Pass if 95%+ compliant
    
    def generate_summary(self):
        """Generate FIX #7 summary."""
        return {
            'total_files_audited': len(self.includes),
            'violations': len(self.warnings),
            'errors': len(self.errors),
            'compliance_percent': ((len(self.includes) - len(set(v['file'] for v in self.warnings))) / len(self.includes) * 100) if self.includes else 0
        }

if __name__ == '__main__':
    auditor = IncludeAuditor('src')
    auditor.audit_directory()
    passed = auditor.print_report()
    sys.exit(0 if passed else 1)
