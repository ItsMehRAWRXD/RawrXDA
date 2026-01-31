#!/usr/bin/env python3
"""
RawrZ Universal IDE - Real Online Compilation with Judge0 and Piston
Implements actual online compilation using real code execution engines
"""

import os
import sys
import json
import requests
import time
import subprocess
from pathlib import Path
from datetime import datetime

class RealOnlineCompiler:
    def __init__(self):
        self.ide_root = Path(__file__).parent
        self.workspace_dir = self.ide_root / "real_compilation_workspace"
        self.workspace_dir.mkdir(exist_ok=True)
        
        # Real online compilation services
        self.services = {
            'judge0': {
                'name': 'Judge0',
                'api_url': 'http://localhost:8080',
                'docker_command': 'docker run -d -p 8080:8080 judge0/judge0',
                'supported_languages': {
                    'python': 71,
                    'javascript': 63,
                    'java': 62,
                    'cpp': 54,
                    'c': 50,
                    'rust': 73,
                    'go': 60,
                    'php': 68,
                    'ruby': 72,
                    'swift': 83,
                    'kotlin': 78
                }
            },
            'piston': {
                'name': 'Piston',
                'api_url': 'http://localhost:2000',
                'docker_command': 'docker run -d -p 2000:2000 ghcr.io/engineer-man/piston',
                'supported_languages': {
                    'python': 'python',
                    'javascript': 'javascript',
                    'java': 'java',
                    'cpp': 'cpp',
                    'c': 'c',
                    'rust': 'rust',
                    'go': 'go',
                    'php': 'php',
                    'ruby': 'ruby',
                    'swift': 'swift',
                    'kotlin': 'kotlin'
                }
            }
        }
        
        # Setup workspace
        self.setup_workspace()

    def setup_workspace(self):
        """Setup workspace for real compilation"""
        print("Setting up Real Online Compilation Workspace...")
        
        workspace_structure = {
            'source': 'source/',
            'output': 'output/',
            'logs': 'logs/',
            'results': 'results/',
            'config': 'config/'
        }
        
        for component, path in workspace_structure.items():
            component_dir = self.workspace_dir / path
            component_dir.mkdir(exist_ok=True)
            print(f"  Created {component}: {component_dir}")
        
        # Create configuration
        config = {
            'services': self.services,
            'workspace_created': datetime.now().isoformat(),
            'settings': {
                'timeout': 30,
                'retry_attempts': 3,
                'auto_setup_docker': True
            }
        }
        
        config_file = self.workspace_dir / 'config' / 'real_compilation_config.json'
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)
        
        print("Real compilation workspace ready!")

    def setup_judge0(self):
        """Setup Judge0 Docker container"""
        print("Setting up Judge0...")
        
        try:
            # Check if Judge0 is already running
            response = requests.get(f"{self.services['judge0']['api_url']}/languages", timeout=5)
            if response.status_code == 200:
                print("  Judge0 already running")
                return True
        except:
            pass
        
        # Start Judge0 Docker container
        print("  Starting Judge0 Docker container...")
        try:
            result = subprocess.run(
                self.services['judge0']['docker_command'].split(),
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode == 0:
                print("  Judge0 started successfully")
                # Wait for service to be ready
                time.sleep(10)
                return True
            else:
                print(f"  Judge0 startup failed: {result.stderr}")
                return False
        except Exception as e:
            print(f"  Judge0 setup failed: {e}")
            return False

    def setup_piston(self):
        """Setup Piston Docker container"""
        print("Setting up Piston...")
        
        try:
            # Check if Piston is already running
            response = requests.get(f"{self.services['piston']['api_url']}/api/v2/runtimes", timeout=5)
            if response.status_code == 200:
                print("  Piston already running")
                return True
        except:
            pass
        
        # Start Piston Docker container
        print("  Starting Piston Docker container...")
        try:
            result = subprocess.run(
                self.services['piston']['docker_command'].split(),
                capture_output=True,
                text=True,
                timeout=30
            )
            
            if result.returncode == 0:
                print("  Piston started successfully")
                # Wait for service to be ready
                time.sleep(10)
                return True
            else:
                print(f"  Piston startup failed: {result.stderr}")
                return False
        except Exception as e:
            print(f"  Piston setup failed: {e}")
            return False

    def compile_with_judge0(self, source_code, language, filename=None):
        """Compile code using Judge0"""
        print(f"Compiling {language} with Judge0...")
        
        if language not in self.services['judge0']['supported_languages']:
            return {
                'status': 'error',
                'message': f'Language {language} not supported by Judge0',
                'service': 'judge0'
            }
        
        language_id = self.services['judge0']['supported_languages'][language]
        
        # Prepare request
        payload = {
            'source_code': source_code,
            'language_id': language_id,
            'stdin': ''
        }
        
        try:
            # Submit code for compilation
            response = requests.post(
                f"{self.services['judge0']['api_url']}/submissions",
                json=payload,
                timeout=30
            )
            
            if response.status_code == 201:
                submission_data = response.json()
                token = submission_data['token']
                
                # Wait for compilation to complete
                time.sleep(2)
                
                # Get results
                result_response = requests.get(
                    f"{self.services['judge0']['api_url']}/submissions/{token}",
                    timeout=30
                )
                
                if result_response.status_code == 200:
                    result = result_response.json()
                    
                    return {
                        'status': 'success',
                        'service': 'judge0',
                        'language': language,
                        'filename': filename,
                        'output': result.get('stdout', ''),
                        'error': result.get('stderr', ''),
                        'compile_output': result.get('compile_output', ''),
                        'execution_time': result.get('time', ''),
                        'memory_used': result.get('memory', ''),
                        'status_description': result.get('status', {}).get('description', ''),
                        'compilation_time': time.time()
                    }
                else:
                    return {
                        'status': 'error',
                        'message': 'Failed to get compilation results',
                        'service': 'judge0'
                    }
            else:
                return {
                    'status': 'error',
                    'message': f'Judge0 API error: {response.status_code}',
                    'service': 'judge0'
                }
                
        except Exception as e:
            return {
                'status': 'error',
                'message': f'Judge0 compilation failed: {str(e)}',
                'service': 'judge0'
            }

    def compile_with_piston(self, source_code, language, filename=None):
        """Compile code using Piston"""
        print(f"Compiling {language} with Piston...")
        
        if language not in self.services['piston']['supported_languages']:
            return {
                'status': 'error',
                'message': f'Language {language} not supported by Piston',
                'service': 'piston'
            }
        
        piston_language = self.services['piston']['supported_languages'][language]
        
        # Prepare request
        payload = {
            'language': piston_language,
            'version': '*',
            'files': [
                {
                    'name': filename or f'main.{language}',
                    'content': source_code
                }
            ]
        }
        
        try:
            # Execute code
            response = requests.post(
                f"{self.services['piston']['api_url']}/api/v2/execute",
                json=payload,
                timeout=30
            )
            
            if response.status_code == 200:
                result = response.json()
                
                return {
                    'status': 'success',
                    'service': 'piston',
                    'language': language,
                    'filename': filename,
                    'output': result.get('run', {}).get('stdout', ''),
                    'error': result.get('run', {}).get('stderr', ''),
                    'compile_output': result.get('compile', {}).get('stdout', ''),
                    'execution_time': result.get('run', {}).get('runtime', ''),
                    'memory_used': result.get('run', {}).get('memory', ''),
                    'status_description': 'Execution completed',
                    'compilation_time': time.time()
                }
            else:
                return {
                    'status': 'error',
                    'message': f'Piston API error: {response.status_code}',
                    'service': 'piston'
                }
                
        except Exception as e:
            return {
                'status': 'error',
                'message': f'Piston compilation failed: {str(e)}',
                'service': 'piston'
            }

    def compile_code(self, source_code, language, filename=None, service='judge0'):
        """Compile code using specified service"""
        print(f"Real Online Compilation with {service.upper()}")
        print("=" * 50)
        
        # Save source code
        if filename:
            source_file = self.workspace_dir / 'source' / filename
            with open(source_file, 'w', encoding='utf-8') as f:
                f.write(source_code)
            print(f"Source code saved: {source_file}")
        
        # Compile with selected service
        if service == 'judge0':
            result = self.compile_with_judge0(source_code, language, filename)
        elif service == 'piston':
            result = self.compile_with_piston(source_code, language, filename)
        else:
            result = {
                'status': 'error',
                'message': f'Unknown service: {service}',
                'service': service
            }
        
        # Save result
        if result['status'] == 'success':
            self.save_compilation_result(result, filename)
            print(f"✅ Real compilation successful with {result['service']}")
            print(f"Output: {result['output']}")
            if result['error']:
                print(f"Errors: {result['error']}")
        else:
            print(f"❌ Real compilation failed: {result['message']}")
        
        return result

    def save_compilation_result(self, result, filename):
        """Save compilation result"""
        timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
        
        # Save result JSON
        result_file = self.workspace_dir / 'results' / f"{filename}_result_{timestamp}.json"
        with open(result_file, 'w') as f:
            json.dump(result, f, indent=2)
        
        # Save output
        if result['output']:
            output_file = self.workspace_dir / 'output' / f"{filename}_output_{timestamp}.txt"
            with open(output_file, 'w') as f:
                f.write(result['output'])
        
        # Save logs
        log_file = self.workspace_dir / 'logs' / f"{filename}_log_{timestamp}.txt"
        with open(log_file, 'w') as f:
            f.write(f"Real Compilation Log for {filename}\n")
            f.write(f"Service: {result['service']}\n")
            f.write(f"Language: {result['language']}\n")
            f.write(f"Status: {result['status']}\n")
            f.write(f"Output: {result['output']}\n")
            if result['error']:
                f.write(f"Error: {result['error']}\n")
            f.write(f"Timestamp: {datetime.now().isoformat()}\n")
        
        print(f"Result saved: {result_file}")

    def setup_services(self):
        """Setup all compilation services"""
        print("Setting up Real Online Compilation Services...")
        print("=" * 50)
        
        # Setup Judge0
        judge0_ready = self.setup_judge0()
        
        # Setup Piston
        piston_ready = self.setup_piston()
        
        print(f"\nService Status:")
        print(f"  Judge0: {'✅ Ready' if judge0_ready else '❌ Failed'}")
        print(f"  Piston: {'✅ Ready' if piston_ready else '❌ Failed'}")
        
        return judge0_ready or piston_ready

def main():
    """Main function to test real online compilation"""
    print("RawrZ Universal IDE - Real Online Compilation")
    print("Using Judge0 and Piston for actual compilation")
    print("=" * 60)
    
    compiler = RealOnlineCompiler()
    
    # Setup services
    if compiler.setup_services():
        print("\nTesting Real Compilation...")
        
        # Test Python compilation
        python_code = '''
print("Hello from REAL online compilation!")
print("This is actually running on Judge0/Piston")
for i in range(3):
    print(f"Count: {i}")
'''
        
        result = compiler.compile_code(python_code, 'python', 'test_real.py', 'judge0')
        if result['status'] == 'success':
            print("✅ Real Python compilation successful!")
        else:
            print("❌ Real Python compilation failed")
        
        # Test C++ compilation
        cpp_code = '''
#include <iostream>
int main() {
    std::cout << "Hello from REAL C++ compilation!" << std::endl;
    std::cout << "This is actually running on Judge0/Piston" << std::endl;
    return 0;
}
'''
        
        result = compiler.compile_code(cpp_code, 'cpp', 'test_real.cpp', 'piston')
        if result['status'] == 'success':
            print("✅ Real C++ compilation successful!")
        else:
            print("❌ Real C++ compilation failed")
    else:
        print("❌ No compilation services available")
        print("Please ensure Docker is installed and running")

if __name__ == "__main__":
    main()
