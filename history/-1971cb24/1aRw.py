#!/usr/bin/env python3
"""
RawrXD Menu & Breadcrumb Validator
Validates all menu connections, slot wiring, and breadcrumb paths
Ensures UI completeness and feature accessibility
"""

import os
import re
import sys
import json
from pathlib import Path
from collections import defaultdict
from typing import List, Dict, Set, Tuple

class MenuValidator:
    def __init__(self, project_root: str = "D:/RawrXD-production-lazy-init"):
        self.project_root = project_root
        self.src_dir = os.path.join(project_root, "src")
        self.results = {
            "menus": {},
            "slots": {},
            "breadcrumbs": {},
            "coverage": {},
            "issues": []
        }

    def extract_menu_definitions(self) -> Dict[str, List[str]]:
        """Extract menu definitions from UI files or code"""
        menus = defaultdict(list)
        
        ui_files = self._find_files("*.ui")
        for ui_file in ui_files:
            menus.update(self._parse_ui_file(ui_file))

        # Also check MainWindow.cpp for programmatic menus
        mainwindow_cpp = os.path.join(self.src_dir, "qtapp", "MainWindow.cpp")
        if os.path.exists(mainwindow_cpp):
            menus.update(self._parse_programmatic_menus(mainwindow_cpp))

        return dict(menus)

    def _find_files(self, pattern: str) -> List[str]:
        """Find files matching pattern"""
        results = []
        for root, dirs, files in os.walk(self.src_dir):
            for file in files:
                if self._match_pattern(file, pattern):
                    results.append(os.path.join(root, file))
        return results

    def _match_pattern(self, filename: str, pattern: str) -> bool:
        """Simple glob pattern matching"""
        pattern = pattern.replace("*", ".*").replace("?", ".")
        return re.match(f"^{pattern}$", filename) is not None

    def _parse_ui_file(self, filepath: str) -> Dict[str, List[str]]:
        """Parse Qt UI file for menu actions"""
        menus = defaultdict(list)
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
                # Find menu names
                menu_pattern = r'<property name="title">.*?<string>(\w+.*?)</string>'
                menus_list = re.findall(menu_pattern, content)
                
                # Find actions
                action_pattern = r'<action name="(\w+)">'
                actions = re.findall(action_pattern, content)
                
                for menu in menus_list:
                    menus[menu].extend(actions)
        except Exception as e:
            print(f"Warning: Could not parse {filepath}: {e}")
        
        return dict(menus)

    def _parse_programmatic_menus(self, filepath: str) -> Dict[str, List[str]]:
        """Parse programmatic menu creation in C++"""
        menus = defaultdict(list)
        try:
            with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
                
                # Find menu creation patterns
                menu_pattern = r'new QMenu\("(\w+.*?)"\)'
                menus_list = re.findall(menu_pattern, content)
                
                # Find addAction patterns
                action_pattern = r'->addAction\("(\w+.*?)"\s*(?:,\s*this)?'
                actions = re.findall(action_pattern, content)
                
                for menu in menus_list:
                    menus[menu] = actions
        except Exception as e:
            print(f"Warning: Could not parse {filepath}: {e}")
        
        return dict(menus)

    def extract_slot_declarations(self) -> Dict[str, Dict]:
        """Extract all slot declarations from headers"""
        slots = {}
        
        header_files = self._find_files("*.h")
        for header in header_files:
            try:
                with open(header, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    
                    # Find slot declarations
                    slot_pattern = r'(public\s+)?slots?:.*?void\s+(\w+)\s*\('
                    slot_matches = re.findall(slot_pattern, content, re.DOTALL)
                    
                    for match in slot_matches:
                        slot_name = match[1] if isinstance(match, tuple) else match
                        slots[slot_name] = {
                            "file": header,
                            "declared": True,
                            "implemented": False
                        }
            except Exception as e:
                print(f"Warning: Could not parse {header}: {e}")
        
        return slots

    def extract_slot_implementations(self) -> Dict[str, Dict]:
        """Extract slot implementations"""
        implementations = {}
        
        cpp_files = self._find_files("*.cpp")
        for cpp_file in cpp_files:
            try:
                with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    
                    # Find function implementations
                    impl_pattern = r'void\s+\w+::(\w+)\s*\([^)]*\)\s*\{'
                    matches = re.findall(impl_pattern, content)
                    
                    for func_name in matches:
                        if func_name not in implementations:
                            implementations[func_name] = {
                                "file": cpp_file,
                                "implemented": True
                            }
            except Exception as e:
                print(f"Warning: Could not parse {cpp_file}: {e}")
        
        return implementations

    def validate_menu_to_slot_wiring(self, menus: Dict, slots: Dict) -> Dict:
        """Validate that all menu items are wired to slots"""
        wiring_status = {
            "total_menu_items": 0,
            "wired_items": 0,
            "unwired_items": [],
            "missing_slots": [],
            "disconnected_slots": list(slots.keys())
        }
        
        for menu_name, actions in menus.items():
            for action in actions:
                wiring_status["total_menu_items"] += 1
                
                # Convert action name to slot name (onActionName)
                slot_name = f"on{action[0].upper()}{action[1:]}"
                
                if slot_name in slots:
                    wiring_status["wired_items"] += 1
                    if slot_name in wiring_status["disconnected_slots"]:
                        wiring_status["disconnected_slots"].remove(slot_name)
                else:
                    wiring_status["missing_slots"].append({
                        "action": action,
                        "expected_slot": slot_name
                    })
                    wiring_status["unwired_items"].append(action)
        
        wiring_status["wiring_coverage"] = (
            100 * wiring_status["wired_items"] / 
            max(wiring_status["total_menu_items"], 1)
        )
        
        return wiring_status

    def validate_breadcrumbs(self) -> Dict:
        """Validate breadcrumb navigation paths"""
        breadcrumbs = {}
        
        # Find breadcrumb implementations
        breadcrumb_pattern = re.compile(r'breadcrumb.*?path\s*=\s*["\'](.+?)["\']', re.IGNORECASE)
        
        cpp_files = self._find_files("*.cpp")
        for cpp_file in cpp_files:
            try:
                with open(cpp_file, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                    matches = breadcrumb_pattern.findall(content)
                    if matches:
                        breadcrumbs[cpp_file] = matches
            except Exception:
                pass
        
        return {
            "files_with_breadcrumbs": len(breadcrumbs),
            "total_breadcrumb_paths": sum(len(v) for v in breadcrumbs.values()),
            "breadcrumbs": breadcrumbs
        }

    def generate_coverage_report(self) -> Dict:
        """Generate comprehensive coverage report"""
        menus = self.extract_menu_definitions()
        slots = self.extract_slot_declarations()
        implementations = self.extract_slot_implementations()
        wiring = self.validate_menu_to_slot_wiring(menus, slots)
        breadcrumbs = self.validate_breadcrumbs()
        
        # Merge implementation status
        for slot_name, impl_info in implementations.items():
            if slot_name in slots:
                slots[slot_name]["implemented"] = True
                slots[slot_name]["file"] = impl_info["file"]
        
        implemented_slots = sum(1 for s in slots.values() if s["implemented"])
        total_slots = len(slots)
        
        return {
            "menus": menus,
            "slots": slots,
            "implementations": implementations,
            "wiring": wiring,
            "breadcrumbs": breadcrumbs,
            "summary": {
                "total_slots": total_slots,
                "implemented_slots": implemented_slots,
                "implementation_coverage": 100 * implemented_slots / max(total_slots, 1),
                "menu_wiring_coverage": wiring["wiring_coverage"],
                "total_menu_items": wiring["total_menu_items"],
                "wired_menu_items": wiring["wired_items"],
                "missing_slots": len(wiring["missing_slots"]),
                "disconnected_slots": len(wiring["disconnected_slots"])
            }
        }

    def print_report(self, report: Dict):
        """Print human-readable report"""
        print("\n" + "="*70)
        print("🎯 RawrXD MENU & BREADCRUMB VALIDATION REPORT")
        print("="*70)
        
        summary = report["summary"]
        print("\n📊 COVERAGE SUMMARY:")
        print(f"  Implementation Coverage:   {summary['implementation_coverage']:.1f}% ({summary['implemented_slots']}/{summary['total_slots']})")
        print(f"  Menu Wiring Coverage:      {summary['menu_wiring_coverage']:.1f}% ({summary['wired_menu_items']}/{summary['total_menu_items']})")
        print(f"  Disconnected Slots:        {summary['disconnected_slots']}")
        print(f"  Missing Slot Definitions:  {summary['missing_slots']}")
        
        if report["wiring"]["missing_slots"]:
            print("\n⚠️  MISSING SLOT DEFINITIONS:")
            for missing in report["wiring"]["missing_slots"]:
                print(f"    - Action: {missing['action']} → Expected slot: {missing['expected_slot']}")
        
        if report["wiring"]["disconnected_slots"]:
            print("\n⚠️  DISCONNECTED SLOTS (declared but unused):")
            for slot in report["wiring"]["disconnected_slots"][:5]:
                print(f"    - {slot}")
            if len(report["wiring"]["disconnected_slots"]) > 5:
                print(f"    ... and {len(report['wiring']['disconnected_slots']) - 5} more")
        
        print(f"\n📍 BREADCRUMBS: {report['breadcrumbs']['total_breadcrumb_paths']} paths in {report['breadcrumbs']['files_with_breadcrumbs']} files")
        
        print("\n" + "="*70)
        
        return report

    def export_json(self, report: Dict, output_file: str = None):
        """Export report as JSON"""
        if output_file is None:
            output_file = os.path.join(self.project_root, "menu_validation_report.json")
        
        # Remove file paths to reduce JSON size
        simplified = {
            "summary": report["summary"],
            "menus": report["menus"],
            "wiring_details": report["wiring"],
            "breadcrumbs_count": report["breadcrumbs"]["total_breadcrumb_paths"]
        }
        
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(simplified, f, indent=2)
        
        print(f"\n✓ JSON report saved to: {output_file}")
        return output_file


def main():
    import argparse
    parser = argparse.ArgumentParser(description="RawrXD Menu & Breadcrumb Validator")
    parser.add_argument("--project-root", default="D:/RawrXD-production-lazy-init", help="Project root")
    parser.add_argument("--json", action="store_true", help="Output as JSON")
    parser.add_argument("--output", help="Output file for JSON report")
    
    args = parser.parse_args()
    
    validator = MenuValidator(args.project_root)
    report = validator.generate_coverage_report()
    
    if args.json:
        validator.export_json(report, args.output)
    else:
        validator.print_report(report)
        print(f"\n💾 To save JSON report: python menu_validator.py --json --output report.json")


if __name__ == "__main__":
    main()
