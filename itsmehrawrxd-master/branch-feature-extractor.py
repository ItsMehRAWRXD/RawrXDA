#!/usr/bin/env python3
"""
RawrZ Branch Feature Extractor
Systematically scans all branches and extracts missing features for local integration
"""

import os
import subprocess
import json
import re
from pathlib import Path
from typing import Dict, List, Set, Tuple
import hashlib

class BranchFeatureExtractor:
    def __init__(self, repo_path: str = "../temp-repo"):
        self.repo_path = Path(repo_path)
        self.local_engines = set()
        self.branch_features = {}
        self.missing_features = {}
        self.extracted_features = {}
        
    def scan_local_engines(self):
        """Scan local RawrZApp for existing engines"""
        engines_path = Path("src/engines")
        if engines_path.exists():
            for engine_file in engines_path.glob("*.js"):
                engine_name = engine_file.stem
                self.local_engines.add(engine_name)
                print(f"[LOCAL] Found engine: {engine_name}")
        
        # Also check for other component types
        self._scan_local_components()
        
    def _scan_local_components(self):
        """Scan for other local components (templates, public files, etc.)"""
        components = {
            'templates': Path("src/templates"),
            'public': Path("public"),
            'utils': Path("src/utils"),
            'configs': Path("config")
        }
        
        for comp_type, comp_path in components.items():
            if comp_path.exists():
                for item in comp_path.rglob("*"):
                    if item.is_file():
                        rel_path = item.relative_to(comp_path)
                        self.local_engines.add(f"{comp_type}/{rel_path}")
                        
    def get_all_branches(self) -> List[str]:
        """Get list of all branches in the repository"""
        try:
            if not self.repo_path.exists():
                print(f"Repository path {self.repo_path} does not exist!")
                return []
                
            result = subprocess.run(
                ["git", "branch", "-r"], 
                cwd=self.repo_path,
                capture_output=True,
                text=True
            )
            branches = []
            for line in result.stdout.split('\n'):
                if 'origin/' in line and not 'HEAD' in line:
                    branch = line.strip().replace('origin/', '')
                    branches.append(branch)
            print(f"Found {len(branches)} remote branches")
            return branches
        except Exception as e:
            print(f"Error getting branches: {e}")
            print(f"Repository path: {self.repo_path}")
            print(f"Path exists: {self.repo_path.exists()}")
            return []
    
    def scan_branch_features(self, branch: str) -> Dict:
        """Scan a specific branch for features"""
        print(f"\n[SCANNING] Branch: {branch}")
        features = {
            'engines': set(),
            'templates': set(),
            'public_files': set(),
            'configs': set(),
            'scripts': set(),
            'assemblies': set(),
            'loaders': set(),
            'encryptors': set()
        }
        
        try:
            # Get file tree for the branch
            result = subprocess.run(
                ["git", "ls-tree", "-r", "--name-only", f"origin/{branch}"],
                cwd=self.repo_path,
                capture_output=True,
                text=True
            )
            
            files = result.stdout.split('\n')
            
            for file_path in files:
                if not file_path.strip():
                    continue
                    
                file_lower = file_path.lower()
                
                # Categorize files
                if 'src/engines/' in file_path and file_path.endswith('.js'):
                    engine_name = Path(file_path).stem
                    features['engines'].add(engine_name)
                    
                elif 'templates/' in file_path:
                    features['templates'].add(file_path)
                    
                elif 'public/' in file_path:
                    features['public_files'].add(file_path)
                    
                elif file_path.endswith('.config') or file_path.endswith('.json'):
                    features['configs'].add(file_path)
                    
                elif file_path.endswith('.py') or file_path.endswith('.sh') or file_path.endswith('.bat'):
                    features['scripts'].add(file_path)
                    
                elif file_path.endswith('.asm') or file_path.endswith('.s'):
                    features['assemblies'].add(file_path)
                    
                elif 'loader' in file_lower:
                    features['loaders'].add(file_path)
                    
                elif 'encrypt' in file_lower or 'crypt' in file_lower:
                    features['encryptors'].add(file_path)
            
            # Convert sets to lists for JSON serialization
            for key in features:
                features[key] = list(features[key])
                
            self.branch_features[branch] = features
            return features
            
        except Exception as e:
            print(f"Error scanning branch {branch}: {e}")
            return {}
    
    def identify_missing_features(self):
        """Compare branch features with local features to identify missing components"""
        print("\n[ANALYZING] Identifying missing features...")
        
        for branch, features in self.branch_features.items():
            missing = {}
            
            # Check engines
            branch_engines = set(features.get('engines', []))
            missing_engines = branch_engines - self.local_engines
            if missing_engines:
                missing['engines'] = list(missing_engines)
            
            # Check other feature types
            for feature_type in ['templates', 'assemblies', 'loaders', 'encryptors', 'scripts']:
                branch_features_set = set(features.get(feature_type, []))
                if branch_features_set:
                    missing[feature_type] = list(branch_features_set)
            
            if missing:
                self.missing_features[branch] = missing
                
    def extract_simple_feature(self, branch: str, file_path: str) -> str:
        """Extract content of a specific file from a branch"""
        try:
            result = subprocess.run(
                ["git", "show", f"origin/{branch}:{file_path}"],
                cwd=self.repo_path,
                capture_output=True,
                text=True
            )
            
            if result.returncode == 0:
                return result.stdout
            else:
                print(f"Failed to extract {file_path} from {branch}: {result.stderr}")
                return ""
        except Exception as e:
            print(f"Error extracting {file_path} from {branch}: {e}")
            return ""
    
    def create_simple_engine_template(self, engine_name: str, branch: str, source_file: str = None):
        """Create a simplified engine template based on branch content"""
        template = f'''// RawrZ {engine_name.replace('-', ' ').title()} Engine - Simple Implementation
const {{ logger }} = require('../utils/logger');

class {self._to_class_name(engine_name)} {{
    constructor() {{
        this.name = '{engine_name}';
        this.version = '1.0.0';
        this.initialized = false;
        logger.info('{self._to_class_name(engine_name)} initialized: Simple implementation from branch.');
    }}

    async initialize(config = {{}}) {{
        this.config = config;
        this.initialized = true;
        logger.info(`${{this.name}} initialized with config: ${{JSON.stringify(config)}}`);
    }}

    async getPanelConfig() {{
        return {{
            name: this.name,
            version: this.version,
            description: 'Simple implementation extracted from branch',
            endpoints: this.getAvailableEndpoints(),
            status: {{
                initialized: this.initialized,
                source: '{branch}'
            }}
        }};
    }}

    getAvailableEndpoints() {{
        return [
            {{ method: 'GET', path: '/api/{engine_name}/status', description: 'Get engine status' }},
            {{ method: 'POST', path: '/api/{engine_name}/execute', description: 'Execute operation' }}
        ];
    }}
}}

module.exports = new {self._to_class_name(engine_name)}();'''
        
        return template
    
    def _to_class_name(self, engine_name: str) -> str:
        """Convert engine name to class name"""
        return ''.join(word.capitalize() for word in engine_name.split('-'))
    
    def generate_extraction_report(self):
        """Generate a comprehensive report of missing features"""
        report = {
            'summary': {
                'total_branches_scanned': len(self.branch_features),
                'branches_with_missing_features': len(self.missing_features),
                'total_local_engines': len(self.local_engines)
            },
            'missing_features': self.missing_features,
            'extraction_recommendations': []
        }
        
        # Generate recommendations
        for branch, missing in self.missing_features.items():
            for feature_type, features in missing.items():
                if feature_type == 'engines':
                    for engine in features:
                        report['extraction_recommendations'].append({
                            'type': 'engine',
                            'name': engine,
                            'branch': branch,
                            'priority': 'high' if 'simple' in engine.lower() else 'medium',
                            'action': f'Extract and create simplified version of {engine}'
                        })
                elif feature_type in ['assemblies', 'loaders', 'encryptors']:
                    for feature in features:
                        report['extraction_recommendations'].append({
                            'type': feature_type,
                            'name': feature,
                            'branch': branch,
                            'priority': 'high',
                            'action': f'Extract {feature_type}: {feature}'
                        })
        
        return report
    
    def save_report(self, report: dict, filename: str = "branch-analysis-report.json"):
        """Save the analysis report to file"""
        with open(filename, 'w') as f:
            json.dump(report, f, indent=2)
        print(f"[REPORT] Saved analysis report to {filename}")
    
    def run_full_analysis(self):
        """Run complete branch analysis"""
        print("[STARTING] RawrZ Branch Feature Extraction Analysis")
        print("=" * 60)
        
        # Step 1: Scan local engines
        print("\n[STEP 1] Scanning local engines...")
        self.scan_local_engines()
        
        # Step 2: Get all branches
        print("\n[STEP 2] Getting all branches...")
        branches = self.get_all_branches()
        print(f"Found {len(branches)} branches to analyze")
        
        # Step 3: Scan each branch
        print("\n[STEP 3] Scanning branches for features...")
        for branch in branches:
            self.scan_branch_features(branch)
        
        # Step 4: Identify missing features
        print("\n[STEP 4] Identifying missing features...")
        self.identify_missing_features()
        
        # Step 5: Generate report
        print("\n[STEP 5] Generating extraction report...")
        report = self.generate_extraction_report()
        self.save_report(report)
        
        # Step 6: Display summary
        print("\n[SUMMARY] Analysis Complete!")
        print("=" * 60)
        print(f"Branches scanned: {report['summary']['total_branches_scanned']}")
        print(f"Branches with missing features: {report['summary']['branches_with_missing_features']}")
        print(f"Local engines found: {report['summary']['total_local_engines']}")
        print(f"Extraction recommendations: {len(report['extraction_recommendations'])}")
        
        return report

def main():
    """Main execution function"""
    extractor = BranchFeatureExtractor()
    report = extractor.run_full_analysis()
    
    # Display top recommendations
    print("\n[TOP RECOMMENDATIONS]")
    print("-" * 40)
    high_priority = [rec for rec in report['extraction_recommendations'] if rec['priority'] == 'high']
    for i, rec in enumerate(high_priority[:10], 1):
        print(f"{i}. {rec['action']} (from {rec['branch']})")

if __name__ == "__main__":
    main()
