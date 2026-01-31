#!/usr/bin/env python3
"""
Sandboxed Sandboxie - Minecraft-Style Procedural Sandbox System
Every sandbox is procedurally generated and completely different!
Self-modifying, self-destructing - permanently locks on exit!
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import subprocess
import threading
import time
import hashlib
import random
import string
import tempfile
import shutil
import json
from pathlib import Path
import platform
import psutil

class SandboxedSandboxieSystem:
    """
    Minecraft-style procedural sandbox system
    Every sandbox is procedurally generated and completely different!
    Permanently locks and self-destructs on exit - irreversible!
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.active_sandboxes = {}
        self.mutation_seed = self._generate_mutation_seed()
        self.lock_mechanisms = []
        
        print("🎮 Sandboxed Sandboxie System initialized")
        print(f"🎲 World seed: {self.mutation_seed}")
        print("🌍 Like Minecraft - every sandbox is procedurally generated!")
    
    def _generate_mutation_seed(self):
        """Generate unique mutation seed for this session"""
        timestamp = str(time.time())
        random_data = ''.join(random.choices(string.ascii_letters + string.digits, k=32))
        return hashlib.sha256((timestamp + random_data).encode()).hexdigest()[:16]
    
    def create_procedural_sandbox(self, config):
        """Create a procedurally generated sandbox like Minecraft worlds"""
        
        sandbox_name = config['name']
        mutation_level = config.get('mutation_level', 'high')
        
        # Generate unique world ID like Minecraft
        sandbox_id = self._generate_world_id(sandbox_name)
        
        sandbox_info = {
            'id': sandbox_id,
            'name': sandbox_name,
            'mutation_level': mutation_level,
            'created_at': time.time(),
            'mutation_count': 0,
            'lock_mechanisms': [],
            'self_destruct_timer': config.get('destruct_timer', 300),  # 5 minutes default
            'status': 'creating'
        }
        
        try:
            # Create mutating environment
            self._create_mutating_environment(sandbox_info)
            
            # Apply mutation layers
            self._apply_mutation_layers(sandbox_info, mutation_level)
            
            # Setup self-destruct mechanism
            self._setup_self_destruct(sandbox_info)
            
            sandbox_info['status'] = 'active'
            self.active_sandboxes[sandbox_id] = sandbox_info
            
            print(f"✅ Procedural sandbox world '{sandbox_id}' created")
            print(f"🌍 Minecraft-style world generated!")
            print(f"⏰ Will self-destruct in {sandbox_info['self_destruct_timer']} seconds")
            
            return sandbox_info
            
        except Exception as e:
            sandbox_info['status'] = 'error'
            sandbox_info['error'] = str(e)
            print(f"❌ Failed to create mutating sandbox: {e}")
            return sandbox_info
    
    def _generate_world_id(self, base_name):
        """Generate procedural world ID like Minecraft - different every time"""
        
        mutation_data = f"{base_name}_{self.mutation_seed}_{time.time()}_{random.random()}"
        mutated_id = hashlib.sha256(mutation_data.encode()).hexdigest()[:12]
        return f"world_{mutated_id}"
    
    def _create_mutating_environment(self, sandbox_info):
        """Create the mutating environment structure"""
        
        # Create base world directory like Minecraft
        base_dir = Path(tempfile.gettempdir()) / f"sandboxed_sandboxie_{sandbox_info['id']}"
        base_dir.mkdir(exist_ok=True)
        
        sandbox_info['base_dir'] = str(base_dir)
        
        # Create procedural world chunks like Minecraft
        world_chunks = ['overworld', 'nether', 'end', 'dimensions', 'biomes', 'structures']
        for chunk in world_chunks:
            chunk_path = base_dir / self._generate_chunk_name(chunk)
            chunk_path.mkdir(exist_ok=True)
            sandbox_info[f'{chunk}_dir'] = str(chunk_path)
        
        # Create world generation log like Minecraft
        world_log = base_dir / f"world_gen_{random.randint(1000, 9999)}.log"
        sandbox_info['world_log'] = str(world_log)
        
        # Write initial world generation data
        with open(world_log, 'w') as f:
            f.write(f"Sandboxed Sandboxie World Generation Log\n")
            f.write(f"World ID: {sandbox_info['id']}\n")
            f.write(f"Generated: {time.ctime()}\n")
            f.write(f"World Seed: {self.mutation_seed}\n")
            f.write(f"Generation Level: {sandbox_info['mutation_level']}\n")
            f.write(f"Like Minecraft - every world is different!\n")
    
    def _generate_chunk_name(self, base_name):
        """Generate procedural chunk name like Minecraft biomes"""
        
        mutation_chars = random.choices(string.ascii_letters + string.digits, k=4)
        mutated_suffix = ''.join(mutation_chars)
        return f"{base_name}_{mutated_suffix}"
    
    def _apply_mutation_layers(self, sandbox_info, mutation_level):
        """Apply mutation layers to the sandbox"""
        
        if mutation_level == 'maximum':
            layers = ['filesystem', 'process', 'network', 'memory', 'crypto', 'registry', 'kernel']
        elif mutation_level == 'high':
            layers = ['filesystem', 'process', 'network', 'memory']
        elif mutation_level == 'medium':
            layers = ['filesystem', 'process']
        else:
            layers = ['filesystem']
        
        for layer in layers:
            self._apply_mutation_layer(sandbox_info, layer)
    
    def _apply_mutation_layer(self, sandbox_info, layer_type):
        """Apply specific mutation layer"""
        
        if layer_type == 'filesystem':
            self._mutate_filesystem(sandbox_info)
        elif layer_type == 'process':
            self._mutate_process_environment(sandbox_info)
        elif layer_type == 'network':
            self._mutate_network_environment(sandbox_info)
        elif layer_type == 'memory':
            self._mutate_memory_environment(sandbox_info)
        elif layer_type == 'crypto':
            self._mutate_crypto_environment(sandbox_info)
        elif layer_type == 'registry':
            self._mutate_registry_environment(sandbox_info)
        elif layer_type == 'kernel':
            self._mutate_kernel_environment(sandbox_info)
    
    def _mutate_filesystem(self, sandbox_info):
        """Create mutating filesystem layer"""
        
        files_dir = Path(sandbox_info['files_dir'])
        
        # Create fake system directories with mutated names
        fake_dirs = ['system32', 'program_files', 'windows', 'users', 'temp']
        for fake_dir in fake_dirs:
            mutated_name = self._mutate_name(fake_dir)
            fake_path = files_dir / mutated_name
            fake_path.mkdir(exist_ok=True)
            
            # Create fake files
            for i in range(random.randint(5, 15)):
                fake_file = fake_path / f"{self._mutate_name('file')}.{random.choice(['dll', 'exe', 'sys', 'dat'])}"
                fake_file.write_text(f"Fake {fake_dir} file - {time.time()}")
        
        # Create mutation marker
        marker_file = files_dir / f"mutation_marker_{random.randint(10000, 99999)}"
        marker_file.write_text(f"Filesystem mutation active - {self.mutation_seed}")
    
    def _mutate_process_environment(self, sandbox_info):
        """Create mutating process environment"""
        
        process_dir = Path(sandbox_info['processes_dir'])
        
        # Create fake process files
        fake_processes = ['explorer.exe', 'svchost.exe', 'winlogon.exe', 'csrss.exe', 'smss.exe']
        for process in fake_processes:
            mutated_name = self._mutate_name(process.replace('.exe', '')) + '.exe'
            process_file = process_dir / mutated_name
            process_file.write_text(f"Fake process: {process}")
        
        # Create process mutation script
        process_script = process_dir / f"process_mutator_{random.randint(1000, 9999)}.py"
        script_content = f'''#!/usr/bin/env python3
import os
import time
import random
import string

# Process mutation script - changes every execution
mutation_id = "{self.mutation_seed}_{time.time()}"
print(f"Process mutation active: {{mutation_id}}")

# Fake process environment
os.environ['FAKE_PROCESS_ID'] = str(random.randint(1000, 99999))
os.environ['MUTATION_ACTIVE'] = 'true'
'''
        process_script.write_text(script_content)
    
    def _mutate_network_environment(self, sandbox_info):
        """Create mutating network environment"""
        
        network_dir = Path(sandbox_info['network_dir'])
        
        # Create fake network interfaces
        fake_interfaces = ['eth0', 'wlan0', 'lo', 'tun0', 'vpn0']
        for interface in fake_interfaces:
            mutated_name = self._mutate_name(interface)
            interface_file = network_dir / f"{mutated_name}.conf"
            interface_file.write_text(f"Fake network interface: {interface}")
        
        # Create network mutation script
        network_script = network_dir / f"network_mutator_{random.randint(1000, 9999)}.py"
        script_content = f'''#!/usr/bin/env python3
import socket
import random

# Network mutation - fake network stack
def fake_network_call():
    return "Network mutation active: {self.mutation_seed}"
'''
        network_script.write_text(script_content)
    
    def _mutate_memory_environment(self, sandbox_info):
        """Create mutating memory environment"""
        
        memory_dir = Path(sandbox_info['memory_dir'])
        
        # Create fake memory regions
        for i in range(random.randint(10, 20)):
            region_name = f"memory_region_{random.randint(1000, 9999)}"
            region_file = memory_dir / f"{region_name}.mem"
            region_file.write_text(f"Fake memory region - {time.time()}")
        
        # Create memory mutation script
        memory_script = memory_dir / f"memory_mutator_{random.randint(1000, 9999)}.py"
        script_content = f'''#!/usr/bin/env python3
import random
import time

# Memory mutation - fake memory operations
class FakeMemory:
    def __init__(self):
        self.mutation_id = "{self.mutation_seed}"
    
    def allocate(self, size):
        return f"Fake allocation: {{size}} bytes"
    
    def deallocate(self, ptr):
        return "Fake deallocation"
'''
        memory_script.write_text(script_content)
    
    def _mutate_crypto_environment(self, sandbox_info):
        """Create mutating cryptographic environment"""
        
        crypto_dir = Path(sandbox_info['crypto_dir'])
        
        # Create fake crypto keys
        for i in range(random.randint(5, 10)):
            key_name = f"crypto_key_{random.randint(1000, 9999)}"
            key_file = crypto_dir / f"{key_name}.key"
            key_file.write_text(f"Fake crypto key - {time.time()}")
        
        # Create crypto mutation script
        crypto_script = crypto_dir / f"crypto_mutator_{random.randint(1000, 9999)}.py"
        script_content = f'''#!/usr/bin/env python3
import hashlib
import random

# Crypto mutation - fake cryptographic operations
def fake_encrypt(data):
    return hashlib.sha256(f"{{data}}_{{time.time()}}".encode()).hexdigest()

def fake_decrypt(encrypted):
    return "Fake decryption result"
'''
        crypto_script.write_text(script_content)
    
    def _mutate_registry_environment(self, sandbox_info):
        """Create mutating registry environment (Windows)"""
        
        if platform.system() != "Windows":
            return
        
        registry_dir = Path(sandbox_info['registry_dir'])
        
        # Create fake registry entries
        fake_keys = ['HKEY_LOCAL_MACHINE', 'HKEY_CURRENT_USER', 'HKEY_CLASSES_ROOT']
        for key in fake_keys:
            mutated_name = self._mutate_name(key.replace('HKEY_', ''))
            key_file = registry_dir / f"{mutated_name}.reg"
            key_file.write_text(f"Fake registry key: {key}")
    
    def _mutate_kernel_environment(self, sandbox_info):
        """Create mutating kernel environment"""
        
        kernel_dir = Path(sandbox_info.get('kernel_dir', sandbox_info['base_dir']))
        
        # Create fake kernel modules
        fake_modules = ['ntoskrnl.exe', 'hal.dll', 'kdcom.dll', 'bootvid.dll']
        for module in fake_modules:
            mutated_name = self._mutate_name(module.replace('.exe', '').replace('.dll', ''))
            module_file = kernel_dir / f"{mutated_name}.sys"
            module_file.write_text(f"Fake kernel module: {module}")
    
    def _setup_self_destruct(self, sandbox_info):
        """Setup self-destruct mechanism that can't be undone"""
        
        destruct_timer = sandbox_info['self_destruct_timer']
        
        # Create self-destruct script
        destruct_script = Path(sandbox_info['base_dir']) / f"self_destruct_{random.randint(10000, 99999)}.py"
        
        script_content = f'''#!/usr/bin/env python3
import os
import time
import shutil
import random
import string
import hashlib

# Self-destruct script - PERMANENT LOCK
class SelfDestructSystem:
    def __init__(self):
        self.sandbox_id = "{sandbox_info['id']}"
        self.mutation_seed = "{self.mutation_seed}"
        self.base_dir = r"{sandbox_info['base_dir']}"
        self.destruct_timer = {destruct_timer}
        self.locked = False
    
    def start_destruct_sequence(self):
        """Start irreversible self-destruct sequence"""
        print(f"🧬 Self-destruct sequence initiated for {{self.sandbox_id}}")
        print(f"⏰ Will lock permanently in {{self.destruct_timer}} seconds")
        
        time.sleep(self.destruct_timer)
        self.permanent_lock()
    
    def permanent_lock(self):
        """Permanent lock - CANNOT BE UNDONE"""
        print("🔒 PERMANENT LOCK ACTIVATED - CANNOT BE UNDONE!")
        
        # Create permanent lock files
        lock_files = []
        for i in range(100):  # Create 100 lock files
            lock_name = f"permanent_lock_{{random.randint(100000, 999999)}}.lock"
            lock_file = os.path.join(self.base_dir, lock_name)
            
            with open(lock_file, 'w') as f:
                f.write(f"PERMANENT LOCK - {{self.sandbox_id}}\\n")
                f.write(f"Locked at: {{time.ctime()}}\\n")
                f.write(f"Mutation seed: {{self.mutation_seed}}\\n")
                f.write("THIS CANNOT BE UNDONE!\\n")
                f.write(f"Lock ID: {{hashlib.sha256(f'{{self.sandbox_id}}_{{time.time()}}_{{random.random()}}'.encode()).hexdigest()}}")
            
            lock_files.append(lock_file)
        
        # Encrypt all files with random keys
        self.encrypt_all_files()
        
        # Create destruction marker
        destruction_marker = os.path.join(self.base_dir, f"DESTRUCTION_COMPLETE_{{random.randint(100000, 999999)}}.marker")
        with open(destruction_marker, 'w') as f:
            f.write("SANDBOX PERMANENTLY DESTROYED\\n")
            f.write(f"Sandbox ID: {{self.sandbox_id}}\\n")
            f.write(f"Destruction time: {{time.ctime()}}\\n")
            f.write("IRREVERSIBLE - CANNOT BE RECOVERED\\n")
        
        self.locked = True
        print("💀 Sandbox permanently locked and destroyed!")
    
    def encrypt_all_files(self):
        """Encrypt all files with random keys (makes them unrecoverable)"""
        for root, dirs, files in os.walk(self.base_dir):
            for file in files:
                file_path = os.path.join(root, file)
                try:
                    # Generate random encryption key
                    key = ''.join(random.choices(string.ascii_letters + string.digits, k=32))
                    
                    # Read original content
                    with open(file_path, 'rb') as f:
                        content = f.read()
                    
                    # Create encrypted content (simple XOR for demo)
                    encrypted = bytearray()
                    key_bytes = key.encode()
                    for i, byte in enumerate(content):
                        encrypted.append(byte ^ key_bytes[i % len(key_bytes)])
                    
                    # Write encrypted content
                    with open(file_path, 'wb') as f:
                        f.write(encrypted)
                    
                    # Write key to separate file (but we'll delete it)
                    key_file = file_path + f".key_{{random.randint(10000, 99999)}}"
                    with open(key_file, 'w') as f:
                        f.write(key)
                    
                    # Delete key file immediately
                    os.remove(key_file)
                    
                except Exception as e:
                    pass  # Continue even if some files fail

# Start self-destruct sequence
destruct_system = SelfDestructSystem()
destruct_system.start_destruct_sequence()
'''
        
        destruct_script.write_text(script_content)
        sandbox_info['destruct_script'] = str(destruct_script)
        
        # Start self-destruct timer in background
        def start_destruct_timer():
            time.sleep(destruct_timer)
            try:
                subprocess.run([sys.executable, str(destruct_script)], timeout=10)
            except:
                pass
        
        destruct_thread = threading.Thread(target=start_destruct_timer, daemon=True)
        destruct_thread.start()
        
        sandbox_info['destruct_thread'] = destruct_thread
    
    def run_in_mutating_sandbox(self, sandbox_id, command):
        """Run command in mutating sandbox"""
        
        if sandbox_id not in self.active_sandboxes:
            return {'error': f'Sandbox {sandbox_id} not found'}
        
        sandbox_info = self.active_sandboxes[sandbox_id]
        
        # Increment mutation count
        sandbox_info['mutation_count'] += 1
        
        # Apply runtime mutation
        self._apply_runtime_mutation(sandbox_info)
        
        try:
            # Run command in sandbox environment
            base_dir = Path(sandbox_info['base_dir'])
            
            result = subprocess.run(
                command,
                shell=True,
                cwd=base_dir,
                capture_output=True,
                text=True,
                timeout=30,
                env=self._create_mutated_environment()
            )
            
            return {
                'success': result.returncode == 0,
                'stdout': result.stdout,
                'stderr': result.stderr,
                'sandbox_id': sandbox_id,
                'mutation_count': sandbox_info['mutation_count']
            }
            
        except subprocess.TimeoutExpired:
            return {'error': 'Command timed out', 'sandbox_id': sandbox_id}
        except Exception as e:
            return {'error': str(e), 'sandbox_id': sandbox_id}
    
    def _apply_runtime_mutation(self, sandbox_info):
        """Apply runtime mutations to the sandbox"""
        
        # Create new mutation files
        mutation_dir = Path(sandbox_info['base_dir']) / f"runtime_mutation_{sandbox_info['mutation_count']}"
        mutation_dir.mkdir(exist_ok=True)
        
        # Generate mutation data
        mutation_data = {
            'mutation_count': sandbox_info['mutation_count'],
            'timestamp': time.time(),
            'random_data': ''.join(random.choices(string.ascii_letters + string.digits, k=100))
        }
        
        mutation_file = mutation_dir / f"mutation_{random.randint(10000, 99999)}.json"
        with open(mutation_file, 'w') as f:
            json.dump(mutation_data, f, indent=2)
        
        # Log mutation
        with open(sandbox_info['mutation_log'], 'a') as f:
            f.write(f"Runtime mutation {sandbox_info['mutation_count']}: {time.ctime()}\n")
    
    def _create_mutated_environment(self):
        """Create mutated environment variables"""
        
        env = os.environ.copy()
        
        # Add mutation-specific environment variables
        env['MUTATION_ACTIVE'] = 'true'
        env['MUTATION_SEED'] = self.mutation_seed
        env['SANDBOX_MUTATING'] = 'true'
        env['PERMANENT_LOCK'] = 'enabled'
        
        # Add fake environment variables
        fake_vars = ['FAKE_PROCESS_ID', 'FAKE_MEMORY_SIZE', 'FAKE_NETWORK_ID']
        for var in fake_vars:
            env[var] = str(random.randint(10000, 99999))
        
        return env
    
    def get_mutation_status(self, sandbox_id):
        """Get current mutation status of sandbox"""
        
        if sandbox_id not in self.active_sandboxes:
            return {'error': f'Sandbox {sandbox_id} not found'}
        
        sandbox_info = self.active_sandboxes[sandbox_id]
        
        return {
            'sandbox_id': sandbox_id,
            'mutation_count': sandbox_info['mutation_count'],
            'mutation_seed': self.mutation_seed,
            'status': sandbox_info['status'],
            'created_at': sandbox_info['created_at'],
            'destruct_timer': sandbox_info['self_destruct_timer'],
            'base_dir': sandbox_info['base_dir']
        }

class SandboxedSandboxieGUI:
    """GUI for the Sandboxed Sandboxie system"""
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.sandboxed_system = SandboxedSandboxieSystem(ide_instance)
        self.setup_gui()
    
    def setup_gui(self):
        """Setup GUI for mutating sandbox"""
        
        # Create new window
        self.sandbox_window = tk.Toplevel(self.ide.root)
        self.sandbox_window.title("🎮 Sandboxed Sandboxie - Minecraft-Style Procedural Sandbox")
        self.sandbox_window.geometry("1000x700")
        
        # Main frame
        main_frame = ttk.Frame(self.sandbox_window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="🎮 Sandboxed Sandboxie", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 5))
        
        subtitle_label = ttk.Label(main_frame, text="Minecraft-Style Procedural Sandbox System", 
                                  font=("Arial", 12))
        subtitle_label.grid(row=1, column=0, columnspan=3, pady=(0, 20))
        
        # Warning
        warning_label = ttk.Label(main_frame, 
                                 text="🌍 Like Minecraft - every sandbox is procedurally generated and different!", 
                                 foreground="blue", font=("Arial", 10, "bold"))
        warning_label.grid(row=2, column=0, columnspan=3, pady=(0, 5))
        
        warning_label2 = ttk.Label(main_frame, 
                                  text="⚠️ WARNING: Permanently LOCKS on exit - can't be undone!", 
                                  foreground="red", font=("Arial", 10, "bold"))
        warning_label2.grid(row=3, column=0, columnspan=3, pady=(0, 10))
        
        # World generation
        self.setup_creation_section(main_frame)
        
        # Test execution
        self.setup_execution_section(main_frame)
        
        # Results
        self.setup_results_section(main_frame)
        
        # Configure grid weights
        self.sandbox_window.columnconfigure(0, weight=1)
        self.sandbox_window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(6, weight=1)
    
    def setup_creation_section(self, parent):
        """Setup world generation section"""
        
        creation_frame = ttk.LabelFrame(parent, text="🌍 Generate Procedural World", padding="10")
        creation_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # World name
        ttk.Label(creation_frame, text="World Name:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.sandbox_name_var = tk.StringVar(value="my_minecraft_world")
        sandbox_name_entry = ttk.Entry(creation_frame, textvariable=self.sandbox_name_var, width=30)
        sandbox_name_entry.grid(row=0, column=1, sticky=tk.W, pady=5, padx=(10, 0))
        
        # Generation level
        ttk.Label(creation_frame, text="Generation Level:").grid(row=1, column=0, sticky=tk.W, pady=5)
        self.mutation_level_var = tk.StringVar(value="high")
        mutation_combo = ttk.Combobox(creation_frame, textvariable=self.mutation_level_var, 
                                     values=["medium", "high", "maximum"])
        mutation_combo.grid(row=1, column=1, sticky=tk.W, pady=5, padx=(10, 0))
        
        # Destruct timer
        ttk.Label(creation_frame, text="Self-Destruct Timer (seconds):").grid(row=2, column=0, sticky=tk.W, pady=5)
        self.destruct_timer_var = tk.StringVar(value="300")
        timer_entry = ttk.Entry(creation_frame, textvariable=self.destruct_timer_var, width=10)
        timer_entry.grid(row=2, column=1, sticky=tk.W, pady=5, padx=(10, 0))
        
        # Create button
        ttk.Button(creation_frame, text="🌍 Generate Procedural World", 
                  command=self.create_sandbox).grid(row=3, column=0, columnspan=2, pady=10)
        
        creation_frame.columnconfigure(1, weight=1)
    
    def setup_execution_section(self, parent):
        """Setup test execution section"""
        
        execution_frame = ttk.LabelFrame(parent, text="🚀 Execute in Mutating Sandbox", padding="10")
        execution_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Command input
        ttk.Label(execution_frame, text="Command:").grid(row=0, column=0, sticky=tk.W, pady=5)
        self.command_var = tk.StringVar(value="python --version")
        command_entry = ttk.Entry(execution_frame, textvariable=self.command_var, width=50)
        command_entry.grid(row=0, column=1, sticky=(tk.W, tk.E), pady=5, padx=(10, 0))
        
        # Execute button
        ttk.Button(execution_frame, text="🚀 Execute in Mutating Sandbox", 
                  command=self.execute_command).grid(row=1, column=0, columnspan=2, pady=10)
        
        execution_frame.columnconfigure(1, weight=1)
    
    def setup_results_section(self, parent):
        """Setup results section"""
        
        results_frame = ttk.LabelFrame(parent, text="📊 Results", padding="10")
        results_frame.grid(row=4, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Results text
        self.results_text = scrolledtext.ScrolledText(results_frame, height=15, width=80)
        self.results_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        results_frame.columnconfigure(0, weight=1)
        results_frame.rowconfigure(0, weight=1)
    
    def create_sandbox(self):
        """Create mutating sandbox"""
        
        sandbox_name = self.sandbox_name_var.get().strip()
        if not sandbox_name:
            messagebox.showwarning("Warning", "Please enter a sandbox name")
            return
        
        mutation_level = self.mutation_level_var.get()
        destruct_timer = int(self.destruct_timer_var.get())
        
        config = {
            'name': sandbox_name,
            'mutation_level': mutation_level,
            'destruct_timer': destruct_timer
        }
        
        try:
            result = self.mutating_system.create_mutating_sandbox(config)
            
            if result['status'] == 'active':
                messagebox.showinfo("Success", f"Mutating sandbox created!\nID: {result['id']}\nWill self-destruct in {destruct_timer} seconds")
                self.results_text.insert(tk.END, f"✅ Mutating sandbox created: {result['id']}\n")
                self.results_text.insert(tk.END, f"🧬 Mutation seed: {self.mutating_system.mutation_seed}\n")
                self.results_text.insert(tk.END, f"⏰ Self-destruct timer: {destruct_timer} seconds\n\n")
            else:
                messagebox.showerror("Error", f"Failed to create sandbox: {result.get('error', 'Unknown error')}")
                self.results_text.insert(tk.END, f"❌ Failed to create sandbox: {result.get('error', 'Unknown error')}\n")
                
        except Exception as e:
            messagebox.showerror("Error", f"Failed to create sandbox: {e}")
            self.results_text.insert(tk.END, f"❌ Error creating sandbox: {e}\n")
    
    def execute_command(self):
        """Execute command in mutating sandbox"""
        
        if not self.mutating_system.active_sandboxes:
            messagebox.showwarning("Warning", "No active sandboxes. Create one first.")
            return
        
        command = self.command_var.get().strip()
        if not command:
            messagebox.showwarning("Warning", "Please enter a command")
            return
        
        # Get first active sandbox
        sandbox_id = list(self.mutating_system.active_sandboxes.keys())[0]
        
        try:
            result = self.mutating_system.run_in_mutating_sandbox(sandbox_id, command)
            
            self.results_text.insert(tk.END, f"🚀 Executing in sandbox: {sandbox_id}\n")
            self.results_text.insert(tk.END, f"Command: {command}\n")
            self.results_text.insert(tk.END, "=" * 50 + "\n")
            
            if 'error' in result:
                self.results_text.insert(tk.END, f"❌ Error: {result['error']}\n")
            else:
                if result['success']:
                    self.results_text.insert(tk.END, "✅ Command executed successfully\n")
                else:
                    self.results_text.insert(tk.END, "❌ Command failed\n")
                
                if result.get('stdout'):
                    self.results_text.insert(tk.END, f"\n📤 Output:\n{result['stdout']}\n")
                
                if result.get('stderr'):
                    self.results_text.insert(tk.END, f"\n⚠️ Errors:\n{result['stderr']}\n")
                
                self.results_text.insert(tk.END, f"\n🧬 Mutation count: {result.get('mutation_count', 0)}\n")
            
            self.results_text.insert(tk.END, "\n" + "=" * 50 + "\n\n")
            
        except Exception as e:
            self.results_text.insert(tk.END, f"❌ Execution error: {e}\n")

# Integration function
def integrate_sandboxed_sandboxie(ide_instance):
    """Integrate Sandboxed Sandboxie with IDE"""
    
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("Tools", "Sandboxed Sandboxie", 
                                 lambda: SandboxedSandboxieGUI(ide_instance))
    
    print("🎮 Sandboxed Sandboxie integrated with IDE")

if __name__ == "__main__":
    print("🎮 Sandboxed Sandboxie - Minecraft-Style Procedural Sandbox")
    print("=" * 60)
    
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
    
    ide = MockIDE()
    sandboxed_system = SandboxedSandboxieSystem(ide)
    
    print("✅ Sandboxed Sandboxie ready!")
    print("🌍 Like Minecraft - every world is procedurally generated!")
    print("⚠️ Remember: Worlds LOCK PERMANENTLY on exit!")
