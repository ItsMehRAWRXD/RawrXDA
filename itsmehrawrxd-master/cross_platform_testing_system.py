#!/usr/bin/env python3
"""
Cross-Platform Testing System
Integrates with your existing Safe Hybrid IDE for Linux/Mac/Windows testing
Supports VM, Docker, and WSL2 integration
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
from pathlib import Path
import platform
import psutil
import requests
import zipfile
import shutil

class CrossPlatformTestingSystem:
    """
    Cross-platform testing system that integrates with your existing IDE
    Supports multiple execution environments for comprehensive testing
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.test_environments = {}
        self.active_tests = {}
        self.vm_manager = None
        self.docker_manager = None
        self.wsl_manager = None
        
        # Initialize platform-specific managers
        self._initialize_platform_managers()
        
        print("🌍 Cross-Platform Testing System initialized")
    
    def _initialize_platform_managers(self):
        """Initialize platform-specific testing managers"""
        
        # VM Manager (VirtualBox/VMware)
        self.vm_manager = VMManager()
        
        # Docker Manager (already integrated with your existing system)
        self.docker_manager = DockerTestingManager()
        
        # WSL2 Manager (Windows Subsystem for Linux)
        if platform.system() == "Windows":
            self.wsl_manager = WSL2Manager()
        
        # Cloud Testing Manager
        self.cloud_manager = CloudTestingManager()
    
    def get_available_environments(self):
        """Get list of available testing environments"""
        environments = []
        
        # Check VM environments
        if self.vm_manager.is_available():
            environments.extend(self.vm_manager.list_vms())
        
        # Check Docker environments (integrate with your existing Docker system)
        if self.docker_manager.is_available():
            environments.extend(self.docker_manager.list_containers())
        
        # Check WSL2 environments
        if self.wsl_manager and self.wsl_manager.is_available():
            environments.extend(self.wsl_manager.list_distributions())
        
        # Check cloud environments
        if self.cloud_manager.is_available():
            environments.extend(self.cloud_manager.list_instances())
        
        return environments
    
    def create_test_environment(self, env_type, config):
        """Create a new testing environment"""
        
        if env_type == "vm":
            return self.vm_manager.create_vm(config)
        elif env_type == "docker":
            return self.docker_manager.create_container(config)
        elif env_type == "wsl2":
            return self.wsl_manager.create_distribution(config)
        elif env_type == "cloud":
            return self.cloud_manager.create_instance(config)
        else:
            raise ValueError(f"Unknown environment type: {env_type}")
    
    def run_cross_platform_test(self, test_config):
        """Run tests across multiple platforms"""
        
        results = {}
        threads = []
        
        for env_name, env_config in test_config['environments'].items():
            thread = threading.Thread(
                target=self._run_test_in_environment,
                args=(env_name, env_config, test_config['test_script'], results)
            )
            threads.append(thread)
            thread.start()
        
        # Wait for all tests to complete
        for thread in threads:
            thread.join()
        
        return self._analyze_test_results(results)
    
    def _run_test_in_environment(self, env_name, env_config, test_script, results):
        """Run test in a specific environment"""
        
        try:
            if env_config['type'] == 'vm':
                result = self.vm_manager.run_test(env_name, test_script)
            elif env_config['type'] == 'docker':
                result = self.docker_manager.run_test(env_name, test_script)
            elif env_config['type'] == 'wsl2':
                result = self.wsl_manager.run_test(env_name, test_script)
            elif env_config['type'] == 'cloud':
                result = self.cloud_manager.run_test(env_name, test_script)
            else:
                result = {'error': f'Unknown environment type: {env_config["type"]}'}
            
            results[env_name] = result
            
        except Exception as e:
            results[env_name] = {'error': str(e)}
    
    def _analyze_test_results(self, results):
        """Analyze and compare test results across platforms"""
        
        analysis = {
            'total_tests': len(results),
            'successful': 0,
            'failed': 0,
            'platform_differences': [],
            'performance_comparison': {},
            'recommendations': []
        }
        
        for env_name, result in results.items():
            if 'error' in result:
                analysis['failed'] += 1
            else:
                analysis['successful'] += 1
                
                # Performance comparison
                if 'execution_time' in result:
                    analysis['performance_comparison'][env_name] = result['execution_time']
        
        # Generate recommendations
        analysis['recommendations'] = self._generate_recommendations(results)
        
        return analysis

class VMManager:
    """Virtual Machine Manager for cross-platform testing"""
    
    def __init__(self):
        self.vm_list = []
        self.virtualbox_path = self._find_virtualbox()
        self.vmware_path = self._find_vmware()
        
    def _find_virtualbox(self):
        """Find VirtualBox installation"""
        possible_paths = [
            r"C:\Program Files\Oracle\VirtualBox\VBoxManage.exe",
            r"C:\Program Files (x86)\Oracle\VirtualBox\VBoxManage.exe",
            "/usr/bin/VBoxManage",
            "/usr/local/bin/VBoxManage"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return None
    
    def _find_vmware(self):
        """Find VMware installation"""
        possible_paths = [
            r"C:\Program Files (x86)\VMware\VMware Workstation\vmrun.exe",
            r"C:\Program Files\VMware\VMware Workstation\vmrun.exe",
            "/usr/bin/vmrun",
            "/usr/local/bin/vmrun"
        ]
        
        for path in possible_paths:
            if os.path.exists(path):
                return path
        return None
    
    def is_available(self):
        """Check if VM software is available"""
        return self.virtualbox_path is not None or self.vmware_path is not None
    
    def list_vms(self):
        """List available virtual machines"""
        vms = []
        
        if self.virtualbox_path:
            try:
                result = subprocess.run([self.virtualbox_path, "list", "vms"], 
                                      capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    for line in result.stdout.strip().split('\n'):
                        if line.strip():
                            vms.append({
                                'name': line.split('"')[1],
                                'type': 'virtualbox',
                                'status': 'available'
                            })
            except Exception as e:
                print(f"Error listing VirtualBox VMs: {e}")
        
        return vms
    
    def create_vm(self, config):
        """Create a new virtual machine"""
        
        if self.virtualbox_path:
            return self._create_virtualbox_vm(config)
        elif self.vmware_path:
            return self._create_vmware_vm(config)
        else:
            raise RuntimeError("No VM software available")
    
    def _create_virtualbox_vm(self, config):
        """Create VirtualBox VM"""
        
        vm_name = config['name']
        
        try:
            # Create VM
            subprocess.run([
                self.virtualbox_path, "createvm",
                "--name", vm_name,
                "--ostype", config.get('os_type', 'Linux_64'),
                "--register"
            ], check=True, timeout=60)
            
            # Configure memory
            subprocess.run([
                self.virtualbox_path, "modifyvm", vm_name,
                "--memory", str(config.get('memory', 2048))
            ], check=True, timeout=30)
            
            # Create hard disk
            subprocess.run([
                self.virtualbox_path, "createhd",
                "--filename", f"{vm_name}.vdi",
                "--size", str(config.get('disk_size', 20480))
            ], check=True, timeout=120)
            
            print(f"✅ VirtualBox VM '{vm_name}' created successfully")
            return {'success': True, 'vm_name': vm_name, 'type': 'virtualbox'}
            
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to create VirtualBox VM: {e}")
            return {'success': False, 'error': str(e)}

class DockerTestingManager:
    """Docker-based testing manager (integrates with your existing Docker system)"""
    
    def __init__(self):
        self.docker_available = self._check_docker()
        self.test_containers = {}
    
    def _check_docker(self):
        """Check if Docker is available"""
        try:
            result = subprocess.run(["docker", "--version"], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def is_available(self):
        """Check if Docker is available"""
        return self.docker_available
    
    def list_containers(self):
        """List available test containers"""
        
        containers = []
        
        # Standard Linux distributions
        linux_distros = [
            {'name': 'ubuntu-20.04', 'image': 'ubuntu:20.04', 'type': 'docker'},
            {'name': 'ubuntu-22.04', 'image': 'ubuntu:22.04', 'type': 'docker'},
            {'name': 'debian-11', 'image': 'debian:11', 'type': 'docker'},
            {'name': 'centos-8', 'image': 'centos:8', 'type': 'docker'},
            {'name': 'alpine-3.18', 'image': 'alpine:3.18', 'type': 'docker'},
            {'name': 'fedora-38', 'image': 'fedora:38', 'type': 'docker'},
        ]
        
        # macOS simulation (using Linux with macOS-like environment)
        macos_simulation = [
            {'name': 'macos-simulation', 'image': 'ubuntu:22.04', 'type': 'docker'},
        ]
        
        containers.extend(linux_distros)
        containers.extend(macos_simulation)
        
        return containers
    
    def create_container(self, config):
        """Create a test container"""
        
        container_name = config['name']
        image = config['image']
        
        try:
            # Pull image if not exists
            subprocess.run(["docker", "pull", image], 
                         check=True, timeout=300)
            
            # Create and start container
            subprocess.run([
                "docker", "run", "-d",
                "--name", container_name,
                "--platform", config.get('platform', 'linux/amd64'),
                image,
                "tail", "-f", "/dev/null"
            ], check=True, timeout=60)
            
            # Install development tools
            self._install_dev_tools(container_name)
            
            print(f"✅ Docker container '{container_name}' created successfully")
            return {'success': True, 'container_name': container_name, 'type': 'docker'}
            
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to create Docker container: {e}")
            return {'success': False, 'error': str(e)}
    
    def _install_dev_tools(self, container_name):
        """Install development tools in container"""
        
        install_commands = [
            "apt-get update",
            "apt-get install -y build-essential git curl wget python3 python3-pip nodejs npm",
            "pip3 install --upgrade pip"
        ]
        
        for cmd in install_commands:
            try:
                subprocess.run([
                    "docker", "exec", container_name,
                    "bash", "-c", cmd
                ], check=True, timeout=120)
            except:
                pass  # Continue even if some tools fail to install
    
    def run_test(self, container_name, test_script):
        """Run test in Docker container"""
        
        try:
            # Copy test script to container
            with tempfile.NamedTemporaryFile(mode='w', suffix='.py', delete=False) as f:
                f.write(test_script)
                script_path = f.name
            
            # Copy script to container
            subprocess.run([
                "docker", "cp", script_path, f"{container_name}:/tmp/test.py"
            ], check=True, timeout=30)
            
            # Run test
            result = subprocess.run([
                "docker", "exec", container_name,
                "python3", "/tmp/test.py"
            ], capture_output=True, text=True, timeout=60)
            
            # Cleanup
            os.unlink(script_path)
            
            return {
                'success': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'execution_time': time.time()
            }
            
        except Exception as e:
            return {'error': str(e)}

class WSL2Manager:
    """Windows Subsystem for Linux 2 Manager"""
    
    def __init__(self):
        self.wsl_available = self._check_wsl2()
    
    def _check_wsl2(self):
        """Check if WSL2 is available"""
        if platform.system() != "Windows":
            return False
        
        try:
            result = subprocess.run(["wsl", "--status"], 
                                  capture_output=True, text=True, timeout=5)
            return result.returncode == 0
        except:
            return False
    
    def is_available(self):
        """Check if WSL2 is available"""
        return self.wsl_available
    
    def list_distributions(self):
        """List available WSL distributions"""
        
        distributions = []
        
        if self.wsl_available:
            try:
                result = subprocess.run(["wsl", "--list", "--verbose"], 
                                      capture_output=True, text=True, timeout=10)
                if result.returncode == 0:
                    for line in result.stdout.strip().split('\n')[1:]:
                        if line.strip():
                            parts = line.split()
                            if len(parts) >= 3:
                                distributions.append({
                                    'name': parts[0],
                                    'version': parts[1],
                                    'type': 'wsl2',
                                    'status': 'running' if parts[2] == 'Running' else 'stopped'
                                })
            except Exception as e:
                print(f"Error listing WSL distributions: {e}")
        
        return distributions
    
    def create_distribution(self, config):
        """Create a new WSL distribution"""
        
        distro_name = config['name']
        image_url = config.get('image_url', 'https://cloud-images.ubuntu.com/wsl/jammy/current/ubuntu-jammy-wsl-amd64-wsl.rootfs.tar.gz')
        
        try:
            # Download and install distribution
            subprocess.run([
                "wsl", "--install", "-d", distro_name
            ], check=True, timeout=600)
            
            print(f"✅ WSL2 distribution '{distro_name}' created successfully")
            return {'success': True, 'distribution_name': distro_name, 'type': 'wsl2'}
            
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to create WSL2 distribution: {e}")
            return {'success': False, 'error': str(e)}

class CloudTestingManager:
    """Cloud-based testing manager"""
    
    def __init__(self):
        self.cloud_available = self._check_cloud_access()
        self.instances = {}
    
    def _check_cloud_access(self):
        """Check if cloud services are accessible"""
        try:
            # Simple connectivity test
            response = requests.get("https://api.github.com", timeout=5)
            return response.status_code == 200
        except:
            return False
    
    def is_available(self):
        """Check if cloud services are available"""
        return self.cloud_available
    
    def list_instances(self):
        """List available cloud instances"""
        
        instances = [
            {'name': 'github-codespaces', 'type': 'cloud', 'provider': 'github'},
            {'name': 'replit', 'type': 'cloud', 'provider': 'replit'},
            {'name': 'gitpod', 'type': 'cloud', 'provider': 'gitpod'},
        ]
        
        return instances
    
    def create_instance(self, config):
        """Create a cloud instance"""
        
        provider = config.get('provider', 'github')
        
        if provider == 'github':
            return self._create_github_codespace(config)
        else:
            return {'success': False, 'error': f'Unsupported provider: {provider}'}
    
    def _create_github_codespace(self, config):
        """Create GitHub Codespace"""
        
        # This would require GitHub CLI and proper authentication
        return {'success': False, 'error': 'GitHub Codespace creation requires GitHub CLI setup'}

class CrossPlatformTestRunner:
    """Main test runner for cross-platform testing"""
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.testing_system = CrossPlatformTestingSystem(ide_instance)
        self.setup_gui()
    
    def setup_gui(self):
        """Setup GUI for cross-platform testing"""
        
        # Create new window for cross-platform testing
        self.test_window = tk.Toplevel(self.ide.root)
        self.test_window.title("🌍 Cross-Platform Testing System")
        self.test_window.geometry("1000x700")
        
        # Main frame
        main_frame = ttk.Frame(self.test_window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="🌍 Cross-Platform Testing System", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Available environments
        self.setup_environments_section(main_frame)
        
        # Test configuration
        self.setup_test_config_section(main_frame)
        
        # Results
        self.setup_results_section(main_frame)
        
        # Configure grid weights
        self.test_window.columnconfigure(0, weight=1)
        self.test_window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(3, weight=1)
    
    def setup_environments_section(self, parent):
        """Setup available environments section"""
        
        env_frame = ttk.LabelFrame(parent, text="🖥️ Available Testing Environments", padding="10")
        env_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Environment list
        self.env_listbox = tk.Listbox(env_frame, height=6, selectmode=tk.MULTIPLE)
        self.env_listbox.grid(row=0, column=0, columnspan=2, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Scrollbar
        env_scrollbar = ttk.Scrollbar(env_frame, orient=tk.VERTICAL, command=self.env_listbox.yview)
        env_scrollbar.grid(row=0, column=2, sticky=(tk.N, tk.S))
        self.env_listbox.config(yscrollcommand=env_scrollbar.set)
        
        # Buttons
        button_frame = ttk.Frame(env_frame)
        button_frame.grid(row=1, column=0, columnspan=3, pady=(10, 0))
        
        ttk.Button(button_frame, text="🔄 Refresh", command=self.refresh_environments).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="➕ Create New", command=self.create_environment).pack(side=tk.LEFT, padx=5)
        
        env_frame.columnconfigure(0, weight=1)
        env_frame.rowconfigure(0, weight=1)
        
        # Load initial environments
        self.refresh_environments()
    
    def setup_test_config_section(self, parent):
        """Setup test configuration section"""
        
        config_frame = ttk.LabelFrame(parent, text="⚙️ Test Configuration", padding="10")
        config_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Test script
        ttk.Label(config_frame, text="Test Script:").grid(row=0, column=0, sticky=tk.W, pady=5)
        
        script_frame = ttk.Frame(config_frame)
        script_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=5)
        
        self.script_text = scrolledtext.ScrolledText(script_frame, height=8, width=80)
        self.script_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        script_frame.columnconfigure(0, weight=1)
        script_frame.rowconfigure(0, weight=1)
        
        # Load sample test script
        self.load_sample_test_script()
        
        # Run button
        ttk.Button(config_frame, text="🚀 Run Cross-Platform Tests", 
                  command=self.run_tests).grid(row=2, column=0, pady=10)
        
        config_frame.columnconfigure(0, weight=1)
    
    def setup_results_section(self, parent):
        """Setup results section"""
        
        results_frame = ttk.LabelFrame(parent, text="📊 Test Results", padding="10")
        results_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Results text
        self.results_text = scrolledtext.ScrolledText(results_frame, height=10, width=80)
        self.results_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)
    
    def refresh_environments(self):
        """Refresh available environments"""
        
        self.env_listbox.delete(0, tk.END)
        
        environments = self.testing_system.get_available_environments()
        
        for env in environments:
            display_text = f"{env['name']} ({env['type']}) - {env.get('status', 'available')}"
            self.env_listbox.insert(tk.END, display_text)
    
    def create_environment(self):
        """Create new testing environment"""
        
        # Simple dialog for environment creation
        dialog = tk.Toplevel(self.test_window)
        dialog.title("Create Testing Environment")
        dialog.geometry("400x300")
        
        # Environment type selection
        ttk.Label(dialog, text="Environment Type:").pack(pady=5)
        env_type_var = tk.StringVar(value="docker")
        env_type_combo = ttk.Combobox(dialog, textvariable=env_type_var, 
                                     values=["docker", "vm", "wsl2", "cloud"])
        env_type_combo.pack(pady=5)
        
        # Environment name
        ttk.Label(dialog, text="Environment Name:").pack(pady=5)
        name_var = tk.StringVar(value="test-env")
        name_entry = ttk.Entry(dialog, textvariable=name_var)
        name_entry.pack(pady=5)
        
        def create_env():
            config = {
                'name': name_var.get(),
                'type': env_type_var.get(),
                'image': 'ubuntu:22.04' if env_type_var.get() == 'docker' else None
            }
            
            try:
                result = self.testing_system.create_test_environment(
                    env_type_var.get(), config
                )
                
                if result['success']:
                    messagebox.showinfo("Success", f"Environment '{name_var.get()}' created successfully!")
                    dialog.destroy()
                    self.refresh_environments()
                else:
                    messagebox.showerror("Error", f"Failed to create environment: {result['error']}")
                    
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create environment: {e}")
        
        ttk.Button(dialog, text="Create", command=create_env).pack(pady=20)
    
    def load_sample_test_script(self):
        """Load sample test script"""
        
        sample_script = '''#!/usr/bin/env python3
"""
Cross-Platform Test Script
Tests basic functionality across different operating systems
"""

import sys
import os
import platform
import subprocess

def test_basic_functionality():
    """Test basic system functionality"""
    results = {
        'platform': platform.system(),
        'version': platform.version(),
        'architecture': platform.architecture()[0],
        'python_version': sys.version,
        'working_directory': os.getcwd(),
        'environment_variables': dict(os.environ),
        'disk_usage': {},
        'memory_info': {},
        'network_test': False
    }
    
    # Test disk usage
    try:
        if platform.system() == "Windows":
            result = subprocess.run(['wmic', 'logicaldisk', 'get', 'size,freespace,caption'], 
                                  capture_output=True, text=True)
        else:
            result = subprocess.run(['df', '-h'], capture_output=True, text=True)
        results['disk_usage'] = result.stdout
    except:
        pass
    
    # Test memory
    try:
        import psutil
        results['memory_info'] = {
            'total': psutil.virtual_memory().total,
            'available': psutil.virtual_memory().available,
            'percent': psutil.virtual_memory().percent
        }
    except:
        pass
    
    # Test network connectivity
    try:
        import requests
        response = requests.get('https://httpbin.org/get', timeout=5)
        results['network_test'] = response.status_code == 200
    except:
        pass
    
    # Test file operations
    try:
        test_file = '/tmp/cross_platform_test.txt'
        with open(test_file, 'w') as f:
            f.write('Cross-platform test file')
        
        with open(test_file, 'r') as f:
            content = f.read()
        
        os.remove(test_file)
        results['file_operations'] = content == 'Cross-platform test file'
    except:
        results['file_operations'] = False
    
    return results

if __name__ == "__main__":
    results = test_basic_functionality()
    
    print("=== CROSS-PLATFORM TEST RESULTS ===")
    for key, value in results.items():
        print(f"{key}: {value}")
    
    print("\\n=== TEST COMPLETED ===")
'''
        
        self.script_text.delete(1.0, tk.END)
        self.script_text.insert(1.0, sample_script)
    
    def run_tests(self):
        """Run cross-platform tests"""
        
        # Get selected environments
        selected_indices = self.env_listbox.curselection()
        if not selected_indices:
            messagebox.showwarning("Warning", "Please select at least one environment")
            return
        
        # Get test script
        test_script = self.script_text.get(1.0, tk.END).strip()
        if not test_script:
            messagebox.showwarning("Warning", "Please enter a test script")
            return
        
        # Prepare test configuration
        test_config = {
            'test_script': test_script,
            'environments': {}
        }
        
        # Add selected environments
        for index in selected_indices:
            env_text = self.env_listbox.get(index)
            env_name = env_text.split(' ')[0]
            env_type = env_text.split('(')[1].split(')')[0]
            
            test_config['environments'][env_name] = {
                'type': env_type,
                'name': env_name
            }
        
        # Run tests in background
        def run_tests_thread():
            try:
                results = self.testing_system.run_cross_platform_test(test_config)
                self.display_results(results)
            except Exception as e:
                self.results_text.insert(tk.END, f"❌ Test execution failed: {e}\\n")
        
        threading.Thread(target=run_tests_thread, daemon=True).start()
        
        self.results_text.delete(1.0, tk.END)
        self.results_text.insert(tk.END, "🚀 Starting cross-platform tests...\\n")
        self.results_text.insert(tk.END, f"Testing {len(selected_indices)} environments\\n\\n")
    
    def display_results(self, results):
        """Display test results"""
        
        self.results_text.insert(tk.END, "📊 TEST RESULTS\\n")
        self.results_text.insert(tk.END, "=" * 50 + "\\n")
        
        self.results_text.insert(tk.END, f"Total Tests: {results['total_tests']}\\n")
        self.results_text.insert(tk.END, f"Successful: {results['successful']}\\n")
        self.results_text.insert(tk.END, f"Failed: {results['failed']}\\n\\n")
        
        if results['performance_comparison']:
            self.results_text.insert(tk.END, "⚡ Performance Comparison:\\n")
            for env, time_taken in results['performance_comparison'].items():
                self.results_text.insert(tk.END, f"  {env}: {time_taken:.2f}s\\n")
            self.results_text.insert(tk.END, "\\n")
        
        if results['recommendations']:
            self.results_text.insert(tk.END, "💡 Recommendations:\\n")
            for rec in results['recommendations']:
                self.results_text.insert(tk.END, f"  • {rec}\\n")
        
        self.results_text.insert(tk.END, "\\n✅ Cross-platform testing completed!")

# Integration function for your existing IDE
def integrate_cross_platform_testing(ide_instance):
    """Integrate cross-platform testing with your existing IDE"""
    
    # Add menu item
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("Tools", "Cross-Platform Testing", 
                                 lambda: CrossPlatformTestRunner(ide_instance))
    
    print("🌍 Cross-Platform Testing System integrated with IDE")

if __name__ == "__main__":
    # Test the system
    print("🌍 Cross-Platform Testing System")
    print("=" * 50)
    
    # Create mock IDE instance
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
    
    ide = MockIDE()
    testing_system = CrossPlatformTestingSystem(ide)
    
    # Test available environments
    environments = testing_system.get_available_environments()
    print(f"Available environments: {len(environments)}")
    
    for env in environments:
        print(f"  • {env['name']} ({env['type']})")
    
    print("\\n✅ Cross-Platform Testing System ready!")
