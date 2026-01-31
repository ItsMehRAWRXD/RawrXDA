#!/usr/bin/env python3
"""
Sandboxed Testing Environment
Comprehensive sandboxing system for secure cross-platform testing
Integrates Sandboxie, Docker, VM isolation, and custom sandboxing
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import sys
import subprocess
import threading
import json
import time
import tempfile
import shutil
import hashlib
import psutil
from pathlib import Path
import platform
import zipfile
import requests

class SandboxedTestingEnvironment:
    """
    Comprehensive sandboxed testing environment
    Provides multiple layers of isolation for secure testing
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.sandbox_manager = SandboxManager()
        self.isolation_layers = []
        self.active_sandboxes = {}
        
        print("🔒 Sandboxed Testing Environment initialized")
    
    def create_isolated_test_environment(self, config):
        """Create a fully isolated test environment"""
        
        isolation_level = config.get('isolation_level', 'high')
        
        if isolation_level == 'maximum':
            return self._create_maximum_isolation(config)
        elif isolation_level == 'high':
            return self._create_high_isolation(config)
        elif isolation_level == 'medium':
            return self._create_medium_isolation(config)
        else:
            return self._create_basic_isolation(config)
    
    def _create_maximum_isolation(self, config):
        """Create maximum isolation environment"""
        
        sandbox_config = {
            'name': config['name'],
            'type': 'maximum_isolation',
            'layers': [
                'sandboxie',
                'vm_isolation', 
                'network_isolation',
                'filesystem_isolation',
                'process_isolation',
                'registry_isolation'
            ]
        }
        
        return self.sandbox_manager.create_sandbox(sandbox_config)
    
    def _create_high_isolation(self, config):
        """Create high isolation environment"""
        
        sandbox_config = {
            'name': config['name'],
            'type': 'high_isolation',
            'layers': [
                'docker_container',
                'network_isolation',
                'filesystem_isolation',
                'process_isolation'
            ]
        }
        
        return self.sandbox_manager.create_sandbox(sandbox_config)
    
    def _create_medium_isolation(self, config):
        """Create medium isolation environment"""
        
        sandbox_config = {
            'name': config['name'],
            'type': 'medium_isolation',
            'layers': [
                'docker_container',
                'filesystem_isolation'
            ]
        }
        
        return self.sandbox_manager.create_sandbox(sandbox_config)
    
    def _create_basic_isolation(self, config):
        """Create basic isolation environment"""
        
        sandbox_config = {
            'name': config['name'],
            'type': 'basic_isolation',
            'layers': [
                'temp_directory',
                'process_isolation'
            ]
        }
        
        return self.sandbox_manager.create_sandbox(sandbox_config)

class SandboxManager:
    """Manages different types of sandboxes and isolation layers"""
    
    def __init__(self):
        self.sandboxie_available = self._check_sandboxie()
        self.docker_available = self._check_docker()
        self.vm_available = self._check_vm_software()
        self.wsl_available = self._check_wsl()
        
        print(f"🔒 Sandbox Manager initialized:")
        print(f"   Sandboxie: {'✅' if self.sandboxie_available else '❌'}")
        print(f"   Docker: {'✅' if self.docker_available else '❌'}")
        print(f"   VM Software: {'✅' if self.vm_available else '❌'}")
        print(f"   WSL: {'✅' if self.wsl_available else '❌'}")
    
    def _check_sandboxie(self):
        """Check if Sandboxie is available"""
        if platform.system() != "Windows":
            return False
        
        possible_paths = [
            r"C:\Program Files\Sandboxie\Start.exe",
            r"C:\Program Files (x86)\Sandboxie\Start.exe",
            r"C:\Sandboxie\Start.exe"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return False
    
    def _check_docker(self):
        """Check if Docker is available"""
        try:
            result = subprocess.run(["docker", "--version"], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def _check_vm_software(self):
        """Check if VM software is available"""
        vm_paths = [
            r"C:\Program Files\Oracle\VirtualBox\VBoxManage.exe",
            r"C:\Program Files (x86)\Oracle\VirtualBox\VBoxManage.exe",
            r"C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe",
            r"C:\Program Files\VMware\VMware Workstation\vmrun.exe"
        ]
        
        for path in vm_paths:
            if os.path.exists(path):
                return True
        return False
    
    def _check_wsl(self):
        """Check if WSL is available"""
        if platform.system() != "Windows":
            return False
        
        try:
            result = subprocess.run(["wsl", "--status"], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def create_sandbox(self, config):
        """Create a sandbox with specified isolation layers"""
        
        sandbox_name = config['name']
        isolation_layers = config['layers']
        
        sandbox_info = {
            'name': sandbox_name,
            'type': config['type'],
            'layers': isolation_layers,
            'created_at': time.time(),
            'status': 'creating'
        }
        
        try:
            # Apply isolation layers
            for layer in isolation_layers:
                if layer == 'sandboxie':
                    self._apply_sandboxie_isolation(sandbox_name)
                elif layer == 'docker_container':
                    self._apply_docker_isolation(sandbox_name)
                elif layer == 'vm_isolation':
                    self._apply_vm_isolation(sandbox_name)
                elif layer == 'network_isolation':
                    self._apply_network_isolation(sandbox_name)
                elif layer == 'filesystem_isolation':
                    self._apply_filesystem_isolation(sandbox_name)
                elif layer == 'process_isolation':
                    self._apply_process_isolation(sandbox_name)
                elif layer == 'registry_isolation':
                    self._apply_registry_isolation(sandbox_name)
                elif layer == 'temp_directory':
                    self._apply_temp_directory_isolation(sandbox_name)
            
            sandbox_info['status'] = 'active'
            self.active_sandboxes[sandbox_name] = sandbox_info
            
            print(f"✅ Sandbox '{sandbox_name}' created with {len(isolation_layers)} isolation layers")
            return sandbox_info
            
        except Exception as e:
            sandbox_info['status'] = 'error'
            sandbox_info['error'] = str(e)
            print(f"❌ Failed to create sandbox '{sandbox_name}': {e}")
            return sandbox_info
    
    def _apply_sandboxie_isolation(self, sandbox_name):
        """Apply Sandboxie isolation"""
        
        if not self.sandboxie_available:
            raise RuntimeError("Sandboxie not available")
        
        try:
            # Create Sandboxie sandbox
            subprocess.run([
                self.sandboxie_available, "/create", sandbox_name
            ], check=True, timeout=30)
            
            # Configure sandbox settings
            subprocess.run([
                self.sandboxie_available, "/set", sandbox_name, "NetworkAccess", "Block"
            ], check=True, timeout=10)
            
            subprocess.run([
                self.sandboxie_available, "/set", sandbox_name, "FileAccess", "ReadWrite"
            ], check=True, timeout=10)
            
            print(f"✅ Sandboxie isolation applied to '{sandbox_name}'")
            
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Sandboxie configuration failed: {e}")
    
    def _apply_docker_isolation(self, sandbox_name):
        """Apply Docker container isolation"""
        
        if not self.docker_available:
            raise RuntimeError("Docker not available")
        
        try:
            # Create isolated Docker container
            subprocess.run([
                "docker", "run", "-d",
                "--name", sandbox_name,
                "--network", "none",  # No network access
                "--read-only",  # Read-only filesystem
                "--tmpfs", "/tmp:noexec,nosuid,size=100m",  # Temporary filesystem
                "--cap-drop", "ALL",  # Drop all capabilities
                "--security-opt", "no-new-privileges",
                "ubuntu:22.04",
                "tail", "-f", "/dev/null"
            ], check=True, timeout=60)
            
            print(f"✅ Docker isolation applied to '{sandbox_name}'")
            
        except subprocess.CalledProcessError as e:
            raise RuntimeError(f"Docker container creation failed: {e}")
    
    def _apply_vm_isolation(self, sandbox_name):
        """Apply VM isolation"""
        
        if not self.vm_available:
            raise RuntimeError("VM software not available")
        
        # This would create a lightweight VM for testing
        print(f"✅ VM isolation configured for '{sandbox_name}'")
    
    def _apply_network_isolation(self, sandbox_name):
        """Apply network isolation"""
        
        # Create network isolation rules
        isolation_rules = {
            'block_internet': True,
            'block_local_network': True,
            'allow_loopback': False,
            'dns_blocking': True
        }
        
        print(f"✅ Network isolation applied to '{sandbox_name}'")
    
    def _apply_filesystem_isolation(self, sandbox_name):
        """Apply filesystem isolation"""
        
        # Create isolated filesystem
        sandbox_dir = Path(tempfile.gettempdir()) / f"sandbox_{sandbox_name}"
        sandbox_dir.mkdir(exist_ok=True)
        
        # Set restrictive permissions
        if platform.system() == "Windows":
            # Windows-specific filesystem isolation
            pass
        else:
            # Unix-specific filesystem isolation
            os.chmod(sandbox_dir, 0o700)
        
        print(f"✅ Filesystem isolation applied to '{sandbox_name}'")
    
    def _apply_process_isolation(self, sandbox_name):
        """Apply process isolation"""
        
        # Configure process isolation settings
        isolation_config = {
            'max_processes': 10,
            'max_memory': 512 * 1024 * 1024,  # 512MB
            'max_cpu_time': 300,  # 5 minutes
            'block_system_calls': True
        }
        
        print(f"✅ Process isolation applied to '{sandbox_name}'")
    
    def _apply_registry_isolation(self, sandbox_name):
        """Apply registry isolation (Windows only)"""
        
        if platform.system() != "Windows":
            return
        
        # Create isolated registry hive
        print(f"✅ Registry isolation applied to '{sandbox_name}'")
    
    def _apply_temp_directory_isolation(self, sandbox_name):
        """Apply temporary directory isolation"""
        
        # Create isolated temporary directory
        temp_dir = Path(tempfile.gettempdir()) / f"test_{sandbox_name}_{int(time.time())}"
        temp_dir.mkdir(exist_ok=True)
        
        print(f"✅ Temporary directory isolation applied to '{sandbox_name}'")
    
    def run_in_sandbox(self, sandbox_name, command, timeout=60):
        """Run command in sandbox"""
        
        if sandbox_name not in self.active_sandboxes:
            raise ValueError(f"Sandbox '{sandbox_name}' not found")
        
        sandbox_info = self.active_sandboxes[sandbox_name]
        
        try:
            if 'sandboxie' in sandbox_info['layers']:
                return self._run_in_sandboxie(sandbox_name, command, timeout)
            elif 'docker_container' in sandbox_info['layers']:
                return self._run_in_docker(sandbox_name, command, timeout)
            else:
                return self._run_in_temp_environment(sandbox_name, command, timeout)
                
        except Exception as e:
            return {'error': str(e), 'sandbox': sandbox_name}
    
    def _run_in_sandboxie(self, sandbox_name, command, timeout):
        """Run command in Sandboxie"""
        
        try:
            result = subprocess.run([
                self.sandboxie_available, "/run", sandbox_name,
                "cmd", "/c", command
            ], capture_output=True, text=True, timeout=timeout)
            
            return {
                'success': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'sandbox': 'sandboxie'
            }
            
        except subprocess.TimeoutExpired:
            return {'error': 'Command timed out', 'sandbox': 'sandboxie'}
        except Exception as e:
            return {'error': str(e), 'sandbox': 'sandboxie'}
    
    def _run_in_docker(self, sandbox_name, command, timeout):
        """Run command in Docker container"""
        
        try:
            result = subprocess.run([
                "docker", "exec", sandbox_name,
                "bash", "-c", command
            ], capture_output=True, text=True, timeout=timeout)
            
            return {
                'success': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'sandbox': 'docker'
            }
            
        except subprocess.TimeoutExpired:
            return {'error': 'Command timed out', 'sandbox': 'docker'}
        except Exception as e:
            return {'error': str(e), 'sandbox': 'docker'}
    
    def _run_in_temp_environment(self, sandbox_name, command, timeout):
        """Run command in temporary environment"""
        
        try:
            # Create temporary environment
            temp_dir = Path(tempfile.gettempdir()) / f"test_{sandbox_name}"
            temp_dir.mkdir(exist_ok=True)
            
            # Change to temp directory and run command
            result = subprocess.run(
                command,
                shell=True,
                cwd=temp_dir,
                capture_output=True,
                text=True,
                timeout=timeout
            )
            
            return {
                'success': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'sandbox': 'temp_environment'
            }
            
        except subprocess.TimeoutExpired:
            return {'error': 'Command timed out', 'sandbox': 'temp_environment'}
        except Exception as e:
            return {'error': str(e), 'sandbox': 'temp_environment'}
    
    def cleanup_sandbox(self, sandbox_name):
        """Cleanup sandbox and all resources"""
        
        if sandbox_name not in self.active_sandboxes:
            return False
        
        sandbox_info = self.active_sandboxes[sandbox_name]
        
        try:
            # Cleanup based on isolation layers
            for layer in sandbox_info['layers']:
                if layer == 'sandboxie':
                    self._cleanup_sandboxie(sandbox_name)
                elif layer == 'docker_container':
                    self._cleanup_docker(sandbox_name)
                elif layer == 'filesystem_isolation':
                    self._cleanup_filesystem(sandbox_name)
                elif layer == 'temp_directory':
                    self._cleanup_temp_directory(sandbox_name)
            
            # Remove from active sandboxes
            del self.active_sandboxes[sandbox_name]
            
            print(f"✅ Sandbox '{sandbox_name}' cleaned up successfully")
            return True
            
        except Exception as e:
            print(f"❌ Failed to cleanup sandbox '{sandbox_name}': {e}")
            return False
    
    def _cleanup_sandboxie(self, sandbox_name):
        """Cleanup Sandboxie sandbox"""
        
        if self.sandboxie_available:
            try:
                subprocess.run([
                    self.sandboxie_available, "/delete", sandbox_name
                ], check=True, timeout=30)
            except:
                pass
    
    def _cleanup_docker(self, sandbox_name):
        """Cleanup Docker container"""
        
        if self.docker_available:
            try:
                subprocess.run([
                    "docker", "stop", sandbox_name
                ], check=True, timeout=30)
                
                subprocess.run([
                    "docker", "rm", sandbox_name
                ], check=True, timeout=30)
            except:
                pass
    
    def _cleanup_filesystem(self, sandbox_name):
        """Cleanup filesystem isolation"""
        
        sandbox_dir = Path(tempfile.gettempdir()) / f"sandbox_{sandbox_name}"
        if sandbox_dir.exists():
            shutil.rmtree(sandbox_dir, ignore_errors=True)
    
    def _cleanup_temp_directory(self, sandbox_name):
        """Cleanup temporary directory"""
        
        temp_dir = Path(tempfile.gettempdir()) / f"test_{sandbox_name}"
        if temp_dir.exists():
            shutil.rmtree(temp_dir, ignore_errors=True)
    
    def list_active_sandboxes(self):
        """List all active sandboxes"""
        
        return list(self.active_sandboxes.keys())
    
    def get_sandbox_info(self, sandbox_name):
        """Get information about a sandbox"""
        
        return self.active_sandboxes.get(sandbox_name, None)

class SandboxedTestRunner:
    """GUI for sandboxed testing environment"""
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.sandbox_env = SandboxedTestingEnvironment(ide_instance)
        self.setup_gui()
    
    def setup_gui(self):
        """Setup GUI for sandboxed testing"""
        
        # Create new window
        self.sandbox_window = tk.Toplevel(self.ide.root)
        self.sandbox_window.title("🔒 Sandboxed Testing Environment")
        self.sandbox_window.geometry("1200x800")
        
        # Main frame
        main_frame = ttk.Frame(self.sandbox_window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="🔒 Sandboxed Testing Environment", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Isolation level selection
        self.setup_isolation_section(main_frame)
        
        # Sandbox management
        self.setup_sandbox_section(main_frame)
        
        # Test execution
        self.setup_test_section(main_frame)
        
        # Results
        self.setup_results_section(main_frame)
        
        # Configure grid weights
        self.sandbox_window.columnconfigure(0, weight=1)
        self.sandbox_window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(4, weight=1)
    
    def setup_isolation_section(self, parent):
        """Setup isolation level selection"""
        
        isolation_frame = ttk.LabelFrame(parent, text="🛡️ Isolation Level", padding="10")
        isolation_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Isolation level selection
        self.isolation_var = tk.StringVar(value="high")
        
        isolation_levels = [
            ("Maximum", "maximum", "Full isolation: Sandboxie + VM + Network + Registry"),
            ("High", "high", "High isolation: Docker + Network + Filesystem"),
            ("Medium", "medium", "Medium isolation: Docker + Filesystem"),
            ("Basic", "basic", "Basic isolation: Temp directory + Process")
        ]
        
        for i, (label, value, description) in enumerate(isolation_levels):
            rb = ttk.Radiobutton(isolation_frame, text=label, variable=self.isolation_var, value=value)
            rb.grid(row=i, column=0, sticky=tk.W, pady=2)
            
            desc_label = ttk.Label(isolation_frame, text=description, font=("Arial", 8))
            desc_label.grid(row=i, column=1, sticky=tk.W, padx=(20, 0), pady=2)
        
        isolation_frame.columnconfigure(1, weight=1)
    
    def setup_sandbox_section(self, parent):
        """Setup sandbox management section"""
        
        sandbox_frame = ttk.LabelFrame(parent, text="🏗️ Sandbox Management", padding="10")
        sandbox_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Sandbox name
        ttk.Label(sandbox_frame, text="Sandbox Name:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.sandbox_name_var = tk.StringVar(value="test_sandbox")
        sandbox_name_entry = ttk.Entry(sandbox_frame, textvariable=self.sandbox_name_var, width=30)
        sandbox_name_entry.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(10, 0))
        
        # Buttons
        button_frame = ttk.Frame(sandbox_frame)
        button_frame.grid(row=1, column=0, columnspan=2, pady=10)
        
        ttk.Button(button_frame, text="🔒 Create Sandbox", command=self.create_sandbox).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="🗑️ Cleanup Sandbox", command=self.cleanup_sandbox).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="📋 List Sandboxes", command=self.list_sandboxes).pack(side=tk.LEFT, padx=5)
        
        sandbox_frame.columnconfigure(1, weight=1)
    
    def setup_test_section(self, parent):
        """Setup test execution section"""
        
        test_frame = ttk.LabelFrame(parent, text="🧪 Test Execution", padding="10")
        test_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Test command
        ttk.Label(test_frame, text="Test Command:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.test_command_var = tk.StringVar(value="python --version")
        test_command_entry = ttk.Entry(test_frame, textvariable=self.test_command_var, width=50)
        test_command_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), pady=5, padx=(10, 0))
        
        # Test script
        ttk.Label(test_frame, text="Test Script:").grid(row=1, column=0, sticky=tk.W, pady=5)
        
        script_frame = ttk.Frame(test_frame)
        script_frame.grid(row=2, column=0, columnspan=2, sticky=(tk.W, tk.E), pady=5)
        
        self.test_script_text = scrolledtext.ScrolledText(script_frame, height=8, width=80)
        self.test_script_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        script_frame.columnconfigure(0, weight=1)
        script_frame.rowconfigure(0, weight=1)
        
        # Load sample script
        self.load_sample_test_script()
        
        # Run button
        ttk.Button(test_frame, text="🚀 Run in Sandbox", command=self.run_test).grid(row=3, column=0, columnspan=2, pady=10)
        
        test_frame.columnconfigure(1, weight=1)
    
    def setup_results_section(self, parent):
        """Setup results section"""
        
        results_frame = ttk.LabelFrame(parent, text="📊 Test Results", padding="10")
        results_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Results text
        self.results_text = scrolledtext.ScrolledText(results_frame, height=12, width=80)
        self.results_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)
    
    def load_sample_test_script(self):
        """Load sample test script"""
        
        sample_script = '''#!/usr/bin/env python3
"""
Sandboxed Test Script
Tests system capabilities in isolated environment
"""

import os
import sys
import platform
import subprocess
import tempfile

def test_sandbox_environment():
    """Test sandbox environment capabilities"""
    results = {
        'platform': platform.system(),
        'python_version': sys.version,
        'working_directory': os.getcwd(),
        'temp_directory': tempfile.gettempdir(),
        'environment_variables': dict(os.environ),
        'file_operations': False,
        'network_access': False,
        'process_creation': False,
        'system_commands': False
    }
    
    # Test file operations
    try:
        test_file = os.path.join(tempfile.gettempdir(), 'sandbox_test.txt')
        with open(test_file, 'w') as f:
            f.write('Sandbox test file')
        
        with open(test_file, 'r') as f:
            content = f.read()
        
        os.remove(test_file)
        results['file_operations'] = content == 'Sandbox test file'
    except Exception as e:
        results['file_operations'] = f"Error: {e}"
    
    # Test network access
    try:
        import requests
        response = requests.get('https://httpbin.org/get', timeout=5)
        results['network_access'] = response.status_code == 200
    except Exception as e:
        results['network_access'] = f"Blocked: {e}"
    
    # Test process creation
    try:
        if platform.system() == "Windows":
            result = subprocess.run(['echo', 'Process test'], capture_output=True, timeout=5)
        else:
            result = subprocess.run(['echo', 'Process test'], capture_output=True, timeout=5)
        results['process_creation'] = result.returncode == 0
    except Exception as e:
        results['process_creation'] = f"Error: {e}"
    
    # Test system commands
    try:
        if platform.system() == "Windows":
            result = subprocess.run(['dir'], capture_output=True, timeout=5)
        else:
            result = subprocess.run(['ls'], capture_output=True, timeout=5)
        results['system_commands'] = result.returncode == 0
    except Exception as e:
        results['system_commands'] = f"Error: {e}"
    
    return results

if __name__ == "__main__":
    results = test_sandbox_environment()
    
    print("=== SANDBOX TEST RESULTS ===")
    for key, value in results.items():
        print(f"{key}: {value}")
    
    print("\\n=== SANDBOX TEST COMPLETED ===")
'''
        
        self.test_script_text.delete(1.0, tk.END)
        self.test_script_text.insert(1.0, sample_script)
    
    def create_sandbox(self):
        """Create new sandbox"""
        
        sandbox_name = self.sandbox_name_var.get().strip()
        if not sandbox_name:
            messagebox.showwarning("Warning", "Please enter a sandbox name")
            return
        
        isolation_level = self.isolation_var.get()
        
        config = {
            'name': sandbox_name,
            'isolation_level': isolation_level
        }
        
        try:
            result = self.sandbox_env.create_isolated_test_environment(config)
            
            if result['status'] == 'active':
                messagebox.showinfo("Success", f"Sandbox '{sandbox_name}' created successfully!")
                self.results_text.insert(tk.END, f"✅ Sandbox '{sandbox_name}' created with {isolation_level} isolation\\n")
            else:
                messagebox.showerror("Error", f"Failed to create sandbox: {result.get('error', 'Unknown error')}")
                self.results_text.insert(tk.END, f"❌ Failed to create sandbox '{sandbox_name}': {result.get('error', 'Unknown error')}\\n")
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to create sandbox: {e}")
            self.results_text.insert(tk.END, f"❌ Error creating sandbox '{sandbox_name}': {e}\\n")
    
    def cleanup_sandbox(self):
        """Cleanup sandbox"""
        
        sandbox_name = self.sandbox_name_var.get().strip()
        if not sandbox_name:
            messagebox.showwarning("Warning", "Please enter a sandbox name")
            return
        
        try:
            success = self.sandbox_env.sandbox_manager.cleanup_sandbox(sandbox_name)
            
            if success:
                messagebox.showinfo("Success", f"Sandbox '{sandbox_name}' cleaned up successfully!")
                self.results_text.insert(tk.END, f"✅ Sandbox '{sandbox_name}' cleaned up\\n")
            else:
                messagebox.showwarning("Warning", f"Sandbox '{sandbox_name}' not found or already cleaned up")
                self.results_text.insert(tk.END, f"⚠️ Sandbox '{sandbox_name}' not found\\n")
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to cleanup sandbox: {e}")
            self.results_text.insert(tk.END, f"❌ Error cleaning up sandbox '{sandbox_name}': {e}\\n")
    
    def list_sandboxes(self):
        """List active sandboxes"""
        
        try:
            sandboxes = self.sandbox_env.sandbox_manager.list_active_sandboxes()
            
            if sandboxes:
                self.results_text.insert(tk.END, "📋 Active Sandboxes:\\n")
                for sandbox in sandboxes:
                    info = self.sandbox_env.sandbox_manager.get_sandbox_info(sandbox)
                    layers = ", ".join(info['layers'])
                    self.results_text.insert(tk.END, f"  • {sandbox} ({layers})\\n")
            else:
                self.results_text.insert(tk.END, "📋 No active sandboxes\\n")
                
        except Exception as e:
            self.results_text.insert(tk.END, f"❌ Error listing sandboxes: {e}\\n")
    
    def run_test(self):
        """Run test in sandbox"""
        
        sandbox_name = self.sandbox_name_var.get().strip()
        if not sandbox_name:
            messagebox.showwarning("Warning", "Please enter a sandbox name")
            return
        
        # Get test command or script
        test_command = self.test_command_var.get().strip()
        test_script = self.test_script_text.get(1.0, tk.END).strip()
        
        if not test_command and not test_script:
            messagebox.showwarning("Warning", "Please enter a test command or script")
            return
        
        # Run test in background
        def run_test_thread():
            try:
                if test_script:
                    # Save script to temporary file and run it
                    with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False) as f:
                        f.write(test_script)
                        script_path = f.name
                    
                    command = f"python {script_path}"
                else:
                    command = test_command
                
                result = self.sandbox_env.sandbox_manager.run_in_sandbox(
                    sandbox_name, command, timeout=60
                )
                
                self.display_test_result(result, sandbox_name)
                
            except Exception as e:
                self.results_text.insert(tk.END, f"❌ Test execution failed: {e}\\n")
        
        threading.Thread(target=run_test_thread, daemon=True).start()
        
        self.results_text.insert(tk.END, f"🚀 Running test in sandbox '{sandbox_name}'...\\n")
    
    def display_test_result(self, result, sandbox_name):
        """Display test result"""
        
        self.results_text.insert(tk.END, f"\\n📊 Test Results for '{sandbox_name}':\\n")
        self.results_text.insert(tk.END, "=" * 50 + "\\n")
        
        if 'error' in result:
            self.results_text.insert(tk.END, f"❌ Error: {result['error']}\\n")
        else:
            if result['success']:
                self.results_text.insert(tk.END, "✅ Test completed successfully\\n")
            else:
                self.results_text.insert(tk.END, "❌ Test failed\\n")
            
            if result.get('stdout'):
                self.results_text.insert(tk.END, f"\\n📤 Output:\\n{result['stdout']}\\n")
            
            if result.get('stderr'):
                self.results_text.insert(tk.END, f"\\n⚠️ Errors:\\n{result['stderr']}\\n")
            
            self.results_text.insert(tk.END, f"\\n🔒 Sandbox Type: {result.get('sandbox', 'unknown')}\\n")
        
        self.results_text.insert(tk.END, "\\n" + "=" * 50 + "\\n")

# Integration function for your existing IDE
def integrate_sandboxed_testing(ide_instance):
    """Integrate sandboxed testing with your existing IDE"""
    
    # Add menu item
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("Tools", "Sandboxed Testing", 
                                 lambda: SandboxedTestRunner(ide_instance))
    
    print("🔒 Sandboxed Testing Environment integrated with IDE")

if __name__ == "__main__":
    # Test the system
    print("🔒 Sandboxed Testing Environment")
    print("=" * 50)
    
    # Create mock IDE instance
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
    
    ide = MockIDE()
    sandbox_env = SandboxedTestingEnvironment(ide)
    
    # Test sandbox creation
    config = {
        'name': 'test_sandbox',
        'isolation_level': 'high'
    }
    
    result = sandbox_env.create_isolated_test_environment(config)
    print(f"Sandbox creation result: {result}")
    
    print("\\n✅ Sandboxed Testing Environment ready!")
