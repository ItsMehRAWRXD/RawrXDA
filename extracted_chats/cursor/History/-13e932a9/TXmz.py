#!/usr/bin/env python3
"""
Model Learning Agent
Learns from D:\ drive patterns and evolves AI models
"""

import os
import json
import hashlib
from pathlib import Path
from typing import Dict, List, Any
import datetime

class ModelLearningAgent:
    def __init__(self, study_path: str = "D:\\"):
        self.study_path = study_path
        self.learned_patterns = {}
        self.model_evolution = {}
        self.study_session = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        
    def scan_directory(self, path: str) -> Dict[str, Any]:
        """Scan directory for files and patterns"""
        findings = {
            'files': [],
            'directories': [],
            'extensions': {},
            'patterns': [],
            'code_files': []
        }
        
        try:
            for root, dirs, files in os.walk(path):
                for file in files:
                    file_path = os.path.join(root, file)
                    ext = Path(file).suffix.lower()
                    
                    findings['files'].append(file_path)
                    
                    # Track extensions
                    if ext:
                        findings['extensions'][ext] = findings['extensions'].get(ext, 0) + 1
                    
                    # Track code files
                    code_exts = ['.py', '.js', '.ts', '.java', '.cpp', '.c', '.rs', '.go', '.sh', '.bash']
                    if ext in code_exts:
                        findings['code_files'].append(file_path)
                        
        except PermissionError as e:
            print(f"Permission denied: {e}")
        except Exception as e:
            print(f"Error scanning {path}: {e}")
            
        return findings
    
    def analyze_scaffold(self, scaffold_file: str) -> Dict[str, Any]:
        """Analyze the generated scaffold to understand model structure"""
        analysis = {
            'total_lines': 0,
            'modules': {},
            'agents': [],
            'integrations': [],
            'api_endpoints': 0,
            'patterns': []
        }
        
        try:
            with open(scaffold_file, 'r', encoding='utf-8') as f:
                lines = f.readlines()
                analysis['total_lines'] = len(lines)
                
                current_module = None
                for i, line in enumerate(lines, 1):
                    # Detect core modules
                    if 'export const coreModule' in line:
                        module_num = line.split('coreModule')[1].split()[0] if 'coreModule' in line else 'unknown'
                        analysis['modules'][f'core_{module_num}'] = {'line': i, 'type': 'core'}
                    
                    # Detect agent modules
                    elif any(agent in line for agent in ['BigDaddyG', 'RawrZ', 'Supernova', 'Evolution', 'Chat']):
                        for agent in ['BigDaddyG', 'RawrZ', 'Supernova', 'Evolution', 'Chat']:
                            if f'{agent}Agent' in line and agent not in analysis['agents']:
                                analysis['agents'].append(agent)
                    
                    # Detect integrations
                    elif 'Integration' in line:
                        for integration in ['Electron', 'Ollama', 'WebSocket', 'Express', 'React']:
                            if integration in line and integration not in analysis['integrations']:
                                analysis['integrations'].append(integration)
                    
                    # Detect API endpoints
                    elif "app.post('/api/endpoint-" in line:
                        analysis['api_endpoints'] += 1
                        
        except Exception as e:
            print(f"Error analyzing scaffold: {e}")
            
        return analysis
    
    def learn_from_twin_generators(self) -> Dict[str, Any]:
        """Learn from twin generator patterns"""
        knowledge = {
            'generators_found': [],
            'model_patterns': {},
            'twin_strategies': []
        }
        
        twin_files = [
            'all_models_twin_generator.py',
            'identical_twin_generator.py',
            'name_based_twin_generator.py',
            'simple_name_twin_generator.py'
        ]
        
        for gen_file in twin_files:
            file_path = os.path.join(self.study_path, gen_file)
            if os.path.exists(file_path):
                knowledge['generators_found'].append(gen_file)
                try:
                    with open(file_path, 'r', encoding='utf-8') as f:
                        content = f.read()
                        
                        # Extract model names
                        if 'all_models = [' in content:
                            models_section = content.split('all_models = [')[1].split(']')[0]
                            knowledge['model_patterns'][gen_file] = {
                                'models_count': models_section.count('"'),
                                'has_domains': 'ModelDomain' in content
                            }
                            
                except Exception as e:
                    print(f"Error reading {gen_file}: {e}")
                    
        return knowledge
    
    def generate_model_identity(self, learned_knowledge: Dict[str, Any]) -> Dict[str, Any]:
        """Generate model identity based on learned patterns"""
        identity = {
            'model_name': 'EVOLVED_MODEL',
            'version': '1.0.0',
            'learned_from': [],
            'capabilities': [],
            'architecture': {},
            'signature': None
        }
        
        # Learn from scaffold
        if 'scaffold_analysis' in learned_knowledge:
            scaffold = learned_knowledge['scaffold_analysis']
            identity['learned_from'].append('bigdaddyg_rawrz_scaffold')
            identity['architecture']['modules'] = len(scaffold.get('modules', {}))
            identity['architecture']['agents'] = scaffold.get('agents', [])
            identity['architecture']['integrations'] = scaffold.get('integrations', [])
            
            # Define capabilities based on what we learned
            if 'BigDaddyG' in scaffold.get('agents', []):
                identity['capabilities'].append('browser_automation')
            if 'RawrZ' in scaffold.get('agents', []):
                identity['capabilities'].append('agentic_ai')
            if 'Ollama' in scaffold.get('integrations', []):
                identity['capabilities'].append('local_llm_integration')
                
        # Learn from twin generators
        if 'twin_knowledge' in learned_knowledge:
            twin_info = learned_knowledge['twin_knowledge']
            identity['learned_from'].append('twin_generators')
            identity['capabilities'].append('model_duplication')
            identity['capabilities'].append('hyperparameter_optimization')
            
        # Generate signature hash
        signature_data = f"{identity['model_name']}_{identity['version']}_{','.join(identity['capabilities'])}"
        identity['signature'] = hashlib.md5(signature_data.encode()).hexdigest()
        
        return identity
    
    def evolve_model(self, base_identity: Dict[str, Any], learned_data: Dict[str, Any]) -> Dict[str, Any]:
        """Evolve the model based on ACTUAL learned patterns - not time!"""
        evolved = base_identity.copy()
        
        # Calculate evolution based on actual learned patterns
        code_files = learned_data.get('scan_results', {}).get('code_files', [])
        scaffold_lines = learned_data.get('scaffold_analysis', {}).get('total_lines', 0)
        agents_found = len(learned_data.get('scaffold_analysis', {}).get('agents', []))
        
        # Intelligence based on code complexity learned
        intelligence = min(0.99, 0.50 + (len(code_files) * 0.01) + (scaffold_lines / 10000))
        
        # Adaptability based on diversity of patterns
        adaptability = min(0.99, 0.50 + (len(learned_data.get('extensions', {})) * 0.05))
        
        # Creativity based on number of different agents
        creativity = min(0.99, 0.50 + (agents_found * 0.10))
        
        # Efficiency based on scaffold organization
        efficiency = min(0.99, 0.50 + (scaffold_lines / 20000))
        
        evolved['version'] = '2.0.0'
        evolved['evolved_at'] = self.study_session
        evolved['evolution_traits'] = {
            'intelligence': round(intelligence, 3),
            'adaptability': round(adaptability, 3),
            'creativity': round(creativity, 3),
            'efficiency': round(efficiency, 3)
        }
        
        # Capabilities based on what was ACTUALLY learned
        evolved['new_capabilities'] = []
        
        # If we learned from scaffold, gain scaffold capabilities
        if scaffold_lines > 0:
            evolved['new_capabilities'].append('scaffold_generation')
            evolved['new_capabilities'].append(f'code_structure_analysis_{scaffold_lines // 1000}k_lines')
        
        # If we found multiple code files, learn multi-language
        if len(code_files) > 10:
            extensions = set([Path(f).suffix.lower() for f in code_files])
            evolved['new_capabilities'].append(f'multi_language_{len(extensions)}_languages')
        
        # If we found multiple agents, learn multi-agent coordination
        if agents_found >= 3:
            evolved['new_capabilities'].append('multi_agent_coordination')
            evolved['capabilities'].append('agent_orchestration')
        
        # If we found generators, learn meta-programming
        generators = learned_data.get('twin_knowledge', {}).get('generators_found', [])
        if generators:
            evolved['new_capabilities'].append('meta_programming')
            evolved['new_capabilities'].append(f'generator_pattern_{len(generators)}_types')
        
        # Evolution formula: Can you do it or can't you? No time-based learning!
        evolved['can_learn'] = intelligence > 0.7 and adaptability > 0.7
        evolved['mastery_level'] = 'Expert' if (intelligence + adaptability + creativity + efficiency) / 4 > 0.85 else 'Intermediate'
        
        return evolved
    
    def conduct_study_session(self) -> Dict[str, Any]:
        """Run a complete study session"""
        print("=" * 60)
        print("MODEL LEARNING AGENT - STUDY SESSION")
        print(f"Session: {self.study_session}")
        print(f"Studying: {self.study_path}")
        print("=" * 60)
        
        # 1. Scan directory structure
        print("\n[1/5] Scanning directory structure...")
        scan_results = self.scan_directory(self.study_path)
        print(f"Found {len(scan_results['code_files'])} code files")
        print(f"File extensions: {list(scan_results['extensions'].keys())[:10]}")
        
        # 2. Analyze scaffold
        print("\n[2/5] Analyzing scaffold structure...")
        scaffold_file = os.path.join(self.study_path, 'bigdaddyg_rawrz_scaffold.js')
        scaffold_analysis = {}
        if os.path.exists(scaffold_file):
            scaffold_analysis = self.analyze_scaffold(scaffold_file)
            print(f"Scaffold has {scaffold_analysis['total_lines']} lines")
            print(f"Found agents: {', '.join(scaffold_analysis['agents'])}")
        else:
            print("Scaffold file not found")
        
        # 3. Learn from twin generators
        print("\n[3/5] Learning from twin generators...")
        twin_knowledge = self.learn_from_twin_generators()
        print(f"Found {len(twin_knowledge['generators_found'])} generator files")
        
        # 4. Generate model identity
        print("\n[4/5] Generating model identity...")
        learned_knowledge = {
            'scaffold_analysis': scaffold_analysis,
            'twin_knowledge': twin_knowledge,
            'scan_results': scan_results
        }
        identity = self.generate_model_identity(learned_knowledge)
        print(f"Model: {identity['model_name']} v{identity['version']}")
        print(f"Capabilities: {', '.join(identity['capabilities'])}")
        
        # 5. Evolve model
        print("\n[5/5] Evolving model...")
        evolved = self.evolve_model(identity, learned_knowledge)
        print(f"Evolved to v{evolved['version']}")
        print(f"New capabilities: {', '.join(evolved['new_capabilities'])}")
        
        # Save results
        session_report = {
            'session': self.study_session,
            'study_path': self.study_path,
            'findings': {
                'files_scanned': len(scan_results['files']),
                'code_files': len(scan_results['code_files']),
                'scaffold_lines': scaffold_analysis.get('total_lines', 0),
                'agents_discovered': scaffold_analysis.get('agents', []),
                'generators_found': twin_knowledge['generators_found']
            },
            'model_identity': identity,
            'evolved_model': evolved
        }
        
        # Save to file
        report_file = os.path.join(self.study_path, f'model_study_report_{self.study_session.replace(":", "-")}.json')
        with open(report_file, 'w', encoding='utf-8') as f:
            json.dump(session_report, f, indent=2, default=str)
        
        print(f"\n{'='*60}")
        print("STUDY SESSION COMPLETE")
        print(f"Report saved to: {report_file}")
        print(f"{'='*60}\n")
        
        return session_report

def main():
    """Run the learning agent"""
    agent = ModelLearningAgent(study_path="D:\\")
    report = agent.conduct_study_session()
    
    # Display summary
    print("\n📊 SUMMARY:")
    print(f"   Files studied: {report['findings']['files_scanned']}")
    print(f"   Code files analyzed: {report['findings']['code_files']}")
    print(f"   Scaffold structure learned: {report['findings']['scaffold_lines']} lines")
    print(f"   Agents discovered: {len(report['findings']['agents_discovered'])}")
    print(f"   Twin generators found: {len(report['findings']['generators_found'])}")
    print(f"\n🤖 Model Identity:")
    print(f"   Name: {report['model_identity']['model_name']}")
    print(f"   Version: {report['evolved_model']['version']}")
    print(f"   Capabilities: {', '.join(report['evolved_model']['new_capabilities'])}")
    
if __name__ == "__main__":
    main()
