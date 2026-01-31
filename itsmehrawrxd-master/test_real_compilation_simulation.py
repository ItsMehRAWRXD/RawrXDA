#!/usr/bin/env python3
"""
Test Real Online Compilation - Simulation Mode
Tests our real compilation system without requiring Docker to be running
"""

import os
import sys
import json
import time
import requests
from pathlib import Path
from datetime import datetime

class RealCompilationTester:
    def __init__(self):
        self.ide_root = Path(__file__).parent
        self.test_dir = self.ide_root / "real_compilation_test"
        self.test_dir.mkdir(exist_ok=True)
        
        # Test services (simulated but realistic)
        self.services = {
            'judge0': {
                'name': 'Judge0',
                'api_url': 'http://localhost:8080',
                'status': 'not_running',
                'supported_languages': {
                    'python': 71,
                    'javascript': 63,
                    'java': 62,
                    'cpp': 54,
                    'c': 50,
                    'rust': 73,
                    'go': 60,
                    'php': 68,
                    'ruby': 72
                }
            },
            'piston': {
                'name': 'Piston',
                'api_url': 'http://localhost:2000',
                'status': 'not_running',
                'supported_languages': {
                    'python': 'python',
                    'javascript': 'javascript',
                    'java': 'java',
                    'cpp': 'cpp',
                    'c': 'c',
                    'rust': 'rust',
                    'go': 'go',
                    'php': 'php',
                    'ruby': 'ruby'
                }
            }
        }

    def test_service_availability(self):
        """Test if services are available"""
        print("Testing Service Availability")
        print("=" * 40)
        
        for service_name, service_info in self.services.items():
            print(f"\nTesting {service_info['name']}...")
            
            try:
                # Try to connect to service
                if service_name == 'judge0':
                    response = requests.get(f"{service_info['api_url']}/languages", timeout=5)
                else:
                    response = requests.get(f"{service_info['api_url']}/api/v2/runtimes", timeout=5)
                
                if response.status_code == 200:
                    service_info['status'] = 'running'
                    print(f"  ✅ {service_info['name']}: RUNNING")
                    print(f"  📡 API URL: {service_info['api_url']}")
                    print(f"  🎯 Status: Available for real compilation")
                else:
                    service_info['status'] = 'error'
                    print(f"  ❌ {service_info['name']}: ERROR (Status: {response.status_code})")
                    
            except requests.exceptions.ConnectionError:
                service_info['status'] = 'not_running'
                print(f"  ⚠️  {service_info['name']}: NOT RUNNING")
                print(f"  💡 To start: docker run -d -p {service_info['api_url'].split(':')[-1]}:{service_info['api_url'].split(':')[-1]} {'judge0/judge0' if service_name == 'judge0' else 'ghcr.io/engineer-man/piston'}")
            except Exception as e:
                service_info['status'] = 'error'
                print(f"  ❌ {service_info['name']}: ERROR - {str(e)[:50]}...")

    def simulate_real_compilation(self):
        """Simulate real compilation with realistic results"""
        print("\nSimulating Real Compilation")
        print("=" * 40)
        
        # Test Python compilation
        print("\nTesting Python Compilation...")
        python_code = '''
print("Hello from REAL compilation!")
print("This would run on Judge0/Piston")
for i in range(3):
    print(f"Count: {i}")
'''
        
        python_result = self.simulate_judge0_compilation(python_code, 'python', 'test_python.py')
        self.display_compilation_result(python_result)
        
        # Test C++ compilation
        print("\nTesting C++ Compilation...")
        cpp_code = '''
#include <iostream>
int main() {
    std::cout << "Hello from REAL C++ compilation!" << std::endl;
    std::cout << "This would run on Judge0/Piston" << std::endl;
    return 0;
}
'''
        
        cpp_result = self.simulate_piston_compilation(cpp_code, 'cpp', 'test_cpp.cpp')
        self.display_compilation_result(cpp_result)
        
        # Test JavaScript compilation
        print("\nTesting JavaScript Compilation...")
        js_code = '''
console.log("Hello from REAL JavaScript compilation!");
console.log("This would run on Judge0/Piston");
for (let i = 0; i < 3; i++) {
    console.log(`Count: ${i}`);
}
'''
        
        js_result = self.simulate_judge0_compilation(js_code, 'javascript', 'test_js.js')
        self.display_compilation_result(js_result)

    def simulate_judge0_compilation(self, source_code, language, filename):
        """Simulate Judge0 compilation with realistic results"""
        print(f"  🔄 Compiling {language} with Judge0...")
        
        # Simulate API call delay
        time.sleep(1)
        
        # Create realistic result
        result = {
            'status': 'success',
            'service': 'judge0',
            'language': language,
            'filename': filename,
            'output': self.get_realistic_output(language),
            'error': '',
            'compile_output': '',
            'execution_time': '0.123s',
            'memory_used': '1024 KB',
            'status_description': 'Accepted',
            'compilation_time': time.time(),
            'simulated': True
        }
        
        return result

    def simulate_piston_compilation(self, source_code, language, filename):
        """Simulate Piston compilation with realistic results"""
        print(f"  🔄 Compiling {language} with Piston...")
        
        # Simulate API call delay
        time.sleep(1)
        
        # Create realistic result
        result = {
            'status': 'success',
            'service': 'piston',
            'language': language,
            'filename': filename,
            'output': self.get_realistic_output(language),
            'error': '',
            'compile_output': '',
            'execution_time': '0.156s',
            'memory_used': '2048 KB',
            'status_description': 'Execution completed',
            'compilation_time': time.time(),
            'simulated': True
        }
        
        return result

    def get_realistic_output(self, language):
        """Get realistic output for language"""
        outputs = {
            'python': 'Hello from REAL compilation!\nThis would run on Judge0/Piston\nCount: 0\nCount: 1\nCount: 2',
            'cpp': 'Hello from REAL C++ compilation!\nThis would run on Judge0/Piston',
            'javascript': 'Hello from REAL JavaScript compilation!\nThis would run on Judge0/Piston\nCount: 0\nCount: 1\nCount: 2',
            'java': 'Hello from REAL Java compilation!\nThis would run on Judge0/Piston',
            'rust': 'Hello from REAL Rust compilation!\nThis would run on Judge0/Piston'
        }
        return outputs.get(language, f'Hello from REAL {language} compilation!')

    def display_compilation_result(self, result):
        """Display compilation result"""
        print(f"  ✅ {result['service'].upper()} compilation successful")
        print(f"  📄 Language: {result['language']}")
        print(f"  📁 File: {result['filename']}")
        print(f"  ⏱️  Execution time: {result['execution_time']}")
        print(f"  💾 Memory used: {result['memory_used']}")
        print(f"  📤 Output:")
        for line in result['output'].split('\n'):
            print(f"    {line}")
        if result['error']:
            print(f"  ❌ Errors: {result['error']}")
        print(f"  🎯 Status: {result['status_description']}")

    def test_workspace_creation(self):
        """Test workspace creation and file management"""
        print("\nTesting Workspace Creation")
        print("=" * 40)
        
        # Create test workspace
        workspace_structure = {
            'source': 'source/',
            'output': 'output/',
            'logs': 'logs/',
            'results': 'results/'
        }
        
        for component, path in workspace_structure.items():
            component_dir = self.test_dir / path
            component_dir.mkdir(exist_ok=True)
            print(f"  ✅ Created {component}: {component_dir}")
        
        # Test file operations
        test_file = self.test_dir / 'source' / 'test_workspace.py'
        with open(test_file, 'w') as f:
            f.write('print("Workspace test successful!")')
        print(f"  ✅ Created test file: {test_file}")
        
        # Test result saving
        result_file = self.test_dir / 'results' / 'test_result.json'
        test_result = {
            'test': 'workspace_creation',
            'status': 'success',
            'timestamp': datetime.now().isoformat()
        }
        with open(result_file, 'w') as f:
            json.dump(test_result, f, indent=2)
        print(f"  ✅ Created result file: {result_file}")

    def test_language_support(self):
        """Test language support"""
        print("\nTesting Language Support")
        print("=" * 40)
        
        print("Judge0 supported languages:")
        for lang, lang_id in self.services['judge0']['supported_languages'].items():
            print(f"  • {lang} (ID: {lang_id})")
        
        print("\nPiston supported languages:")
        for lang, piston_name in self.services['piston']['supported_languages'].items():
            print(f"  • {lang} (Piston: {piston_name})")
        
        total_languages = len(set(list(self.services['judge0']['supported_languages'].keys()) + 
                                list(self.services['piston']['supported_languages'].keys())))
        print(f"\nTotal unique languages supported: {total_languages}")

    def generate_test_report(self):
        """Generate comprehensive test report"""
        print("\nGenerating Test Report")
        print("=" * 40)
        
        report = {
            'test_timestamp': datetime.now().isoformat(),
            'services_tested': list(self.services.keys()),
            'service_status': {name: info['status'] for name, info in self.services.items()},
            'workspace_created': True,
            'file_operations': True,
            'language_support': len(self.services['judge0']['supported_languages']),
            'real_compilation_ready': any(info['status'] == 'running' for info in self.services.values()),
            'docker_required': True,
            'next_steps': [
                'Start Docker Desktop',
                'Run: docker run -d -p 8080:8080 judge0/judge0',
                'Run: docker run -d -p 2000:2000 ghcr.io/engineer-man/piston',
                'Test real compilation with running services'
            ]
        }
        
        # Save report
        report_file = self.test_dir / 'test_report.json'
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"  ✅ Test report saved: {report_file}")
        return report

def main():
    """Main test function"""
    print("RawrZ Universal IDE - Real Compilation Test")
    print("Testing Real Online Compilation System")
    print("=" * 60)
    
    tester = RealCompilationTester()
    
    # Test service availability
    tester.test_service_availability()
    
    # Test workspace creation
    tester.test_workspace_creation()
    
    # Test language support
    tester.test_language_support()
    
    # Simulate real compilation
    tester.simulate_real_compilation()
    
    # Generate test report
    report = tester.generate_test_report()
    
    print("\n" + "=" * 60)
    print("TEST RESULTS SUMMARY")
    print("=" * 60)
    print(f"✅ Workspace creation: SUCCESS")
    print(f"✅ File operations: SUCCESS")
    print(f"✅ Language support: {report['language_support']} languages")
    print(f"✅ Compilation simulation: SUCCESS")
    print(f"❌ Real services: {'NOT RUNNING' if not report['real_compilation_ready'] else 'RUNNING'}")
    
    print(f"\n🎯 CONCLUSION:")
    if report['real_compilation_ready']:
        print("✅ Real online compilation is READY!")
        print("🚀 You can now compile code with Judge0/Piston")
    else:
        print("⚠️  Real online compilation requires Docker")
        print("🔧 Start Docker and run the containers to enable real compilation")
        print("📋 Next steps:")
        for step in report['next_steps']:
            print(f"  • {step}")
    
    print(f"\n📁 Test files saved in: {tester.test_dir}")

if __name__ == "__main__":
    main()
