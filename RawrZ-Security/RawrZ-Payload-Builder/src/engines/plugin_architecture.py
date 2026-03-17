#!/usr/bin/env python3
"""
Plugin Architecture System
Sandbox isolation for plugins
"""

import os
import sys
import json
import importlib
import threading
import time
from typing import Dict, List, Any, Optional
import subprocess
import tempfile
import shutil

class PluginArchitecture:
    """Plugin architecture system with sandbox isolation"""
    
    def __init__(self):
        self.initialized = False
        self.plugins = {}
        self.plugin_sandboxes = {}
        self.plugin_metadata = {}
        self.plugin_threads = {}
        self.sandbox_base_dir = tempfile.mkdtemp(prefix="plugin_sandbox_")
        
        print("Plugin Architecture System created")
    
    def initialize(self):
        """Initialize the plugin architecture"""
        try:
            # Create sandbox directories
            self.create_sandbox_environment()
            
            # Load existing plugins
            self.load_plugins()
            
            # Start plugin monitoring
            self.start_plugin_monitoring()
            
            self.initialized = True
            print("Plugin Architecture System initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Plugin Architecture: {e}")
            return False
    
    def create_sandbox_environment(self):
        """Create sandbox environment for plugins"""
        # Create base sandbox directory
        os.makedirs(self.sandbox_base_dir, exist_ok=True)
        
        # Create subdirectories
        subdirs = ['plugins', 'temp', 'logs', 'data', 'cache']
        for subdir in subdirs:
            os.makedirs(os.path.join(self.sandbox_base_dir, subdir), exist_ok=True)
        
        print(f"Sandbox environment created at: {self.sandbox_base_dir}")
    
    def load_plugins(self):
        """Load existing plugins"""
        plugins_dir = os.path.join(self.sandbox_base_dir, 'plugins')
        
        if not os.path.exists(plugins_dir):
            return
        
        for plugin_file in os.listdir(plugins_dir):
            if plugin_file.endswith('.py'):
                plugin_name = plugin_file[:-3]
                try:
                    self.load_plugin(plugin_name, os.path.join(plugins_dir, plugin_file))
                except Exception as e:
                    print(f"Failed to load plugin {plugin_name}: {e}")
    
    def load_plugin(self, plugin_name: str, plugin_path: str):
        """Load a plugin with sandbox isolation"""
        try:
            # Create sandbox for this plugin
            sandbox_dir = os.path.join(self.sandbox_base_dir, 'temp', plugin_name)
            os.makedirs(sandbox_dir, exist_ok=True)
            
            # Copy plugin to sandbox
            sandbox_plugin_path = os.path.join(sandbox_dir, f"{plugin_name}.py")
            shutil.copy2(plugin_path, sandbox_plugin_path)
            
            # Load plugin metadata
            metadata = self.extract_plugin_metadata(sandbox_plugin_path)
            
            # Initialize plugin in sandbox
            plugin_instance = self.initialize_plugin_in_sandbox(plugin_name, sandbox_plugin_path, sandbox_dir)
            
            if plugin_instance:
                self.plugins[plugin_name] = plugin_instance
                self.plugin_sandboxes[plugin_name] = sandbox_dir
                self.plugin_metadata[plugin_name] = metadata
                
                print(f"Plugin {plugin_name} loaded successfully")
                return True
            
        except Exception as e:
            print(f"Failed to load plugin {plugin_name}: {e}")
            return False
    
    def extract_plugin_metadata(self, plugin_path: str) -> Dict[str, Any]:
        """Extract metadata from plugin file"""
        metadata = {
            'name': '',
            'version': '1.0.0',
            'description': '',
            'author': '',
            'dependencies': [],
            'permissions': [],
            'sandbox_level': 'restricted'
        }
        
        try:
            with open(plugin_path, 'r') as f:
                content = f.read()
                
                # Extract metadata from comments
                lines = content.split('\n')
                for line in lines:
                    if line.strip().startswith('# @'):
                        key_value = line.strip()[3:].split(':', 1)
                        if len(key_value) == 2:
                            key = key_value[0].strip()
                            value = key_value[1].strip()
                            if key in metadata:
                                metadata[key] = value
                
        except Exception as e:
            print(f"Failed to extract metadata: {e}")
        
        return metadata
    
    def initialize_plugin_in_sandbox(self, plugin_name: str, plugin_path: str, sandbox_dir: str):
        """Initialize plugin in sandbox environment"""
        try:
            # Create isolated environment
            sys.path.insert(0, sandbox_dir)
            
            # Import plugin module
            spec = importlib.util.spec_from_file_location(plugin_name, plugin_path)
            module = importlib.util.module_from_spec(spec)
            
            # Execute in sandbox
            spec.loader.exec_module(module)
            
            # Create plugin instance
            if hasattr(module, 'Plugin'):
                plugin_instance = module.Plugin()
                plugin_instance.sandbox_dir = sandbox_dir
                plugin_instance.plugin_name = plugin_name
                return plugin_instance
            else:
                print(f"Plugin {plugin_name} does not have a Plugin class")
                return None
                
        except Exception as e:
            print(f"Failed to initialize plugin {plugin_name}: {e}")
            return None
        finally:
            # Remove from sys.path
            if sandbox_dir in sys.path:
                sys.path.remove(sandbox_dir)
    
    def start_plugin_monitoring(self):
        """Start monitoring plugin health"""
        monitoring_thread = threading.Thread(target=self.monitor_plugins)
        monitoring_thread.daemon = True
        monitoring_thread.start()
        
        print("Plugin monitoring started")
    
    def monitor_plugins(self):
        """Monitor plugin health and performance"""
        while True:
            try:
                for plugin_name, plugin_instance in self.plugins.items():
                    # Check plugin health
                    if hasattr(plugin_instance, 'health_check'):
                        try:
                            health = plugin_instance.health_check()
                            if not health:
                                print(f"Plugin {plugin_name} health check failed")
                                self.restart_plugin(plugin_name)
                        except Exception as e:
                            print(f"Plugin {plugin_name} health check error: {e}")
                            self.restart_plugin(plugin_name)
                
                time.sleep(30)  # Check every 30 seconds
                
            except Exception as e:
                print(f"Plugin monitoring error: {e}")
                time.sleep(30)
    
    def restart_plugin(self, plugin_name: str):
        """Restart a plugin"""
        try:
            if plugin_name in self.plugins:
                # Stop plugin
                self.stop_plugin(plugin_name)
                
                # Reload plugin
                plugin_path = os.path.join(self.plugin_sandboxes[plugin_name], f"{plugin_name}.py")
                self.load_plugin(plugin_name, plugin_path)
                
                print(f"Plugin {plugin_name} restarted")
                
        except Exception as e:
            print(f"Failed to restart plugin {plugin_name}: {e}")
    
    def stop_plugin(self, plugin_name: str):
        """Stop a plugin"""
        try:
            if plugin_name in self.plugins:
                plugin_instance = self.plugins[plugin_name]
                
                # Call plugin cleanup
                if hasattr(plugin_instance, 'cleanup'):
                    plugin_instance.cleanup()
                
                # Remove from registry
                del self.plugins[plugin_name]
                del self.plugin_sandboxes[plugin_name]
                del self.plugin_metadata[plugin_name]
                
                print(f"Plugin {plugin_name} stopped")
                
        except Exception as e:
            print(f"Failed to stop plugin {plugin_name}: {e}")
    
    def execute_plugin_function(self, plugin_name: str, function_name: str, *args, **kwargs):
        """Execute a function in a plugin with sandbox isolation"""
        try:
            if plugin_name not in self.plugins:
                raise ValueError(f"Plugin {plugin_name} not found")
            
            plugin_instance = self.plugins[plugin_name]
            
            if not hasattr(plugin_instance, function_name):
                raise ValueError(f"Function {function_name} not found in plugin {plugin_name}")
            
            # Execute in sandbox
            function = getattr(plugin_instance, function_name)
            result = function(*args, **kwargs)
            
            return result
            
        except Exception as e:
            print(f"Failed to execute {function_name} in plugin {plugin_name}: {e}")
            return None
    
    def get_plugin_list(self) -> List[Dict[str, Any]]:
        """Get list of loaded plugins"""
        plugin_list = []
        
        for plugin_name, metadata in self.plugin_metadata.items():
            plugin_info = {
                'name': plugin_name,
                'metadata': metadata,
                'status': 'active' if plugin_name in self.plugins else 'inactive',
                'sandbox_dir': self.plugin_sandboxes.get(plugin_name, '')
            }
            plugin_list.append(plugin_info)
        
        return plugin_list
    
    def create_plugin_template(self, plugin_name: str, plugin_type: str = 'basic'):
        """Create a plugin template"""
        template_dir = os.path.join(self.sandbox_base_dir, 'plugins')
        os.makedirs(template_dir, exist_ok=True)
        
        plugin_path = os.path.join(template_dir, f"{plugin_name}.py")
        
        if plugin_type == 'basic':
            template_content = f'''#!/usr/bin/env python3
"""
{plugin_name} Plugin
Generated plugin template
"""

# @name: {plugin_name}
# @version: 1.0.0
# @description: Basic plugin template
# @author: Plugin Developer
# @dependencies: []
# @permissions: []
# @sandbox_level: restricted

class Plugin:
    """Basic plugin class"""
    
    def __init__(self):
        self.plugin_name = "{plugin_name}"
        self.sandbox_dir = ""
        self.initialized = False
    
    def initialize(self):
        """Initialize the plugin"""
        self.initialized = True
        print(f"Plugin {{self.plugin_name}} initialized")
        return True
    
    def cleanup(self):
        """Cleanup plugin resources"""
        self.initialized = False
        print(f"Plugin {{self.plugin_name}} cleaned up")
    
    def health_check(self):
        """Check plugin health"""
        return self.initialized
    
    def execute(self, *args, **kwargs):
        """Execute plugin function"""
        print(f"Plugin {{self.plugin_name}} executed with args: {{args}}, kwargs: {{kwargs}}")
        return "Plugin executed successfully"
'''
        
        elif plugin_type == 'advanced':
            template_content = f'''#!/usr/bin/env python3
"""
{plugin_name} Plugin
Advanced plugin template with sandbox isolation
"""

# @name: {plugin_name}
# @version: 1.0.0
# @description: Advanced plugin template
# @author: Plugin Developer
# @dependencies: []
# @permissions: [file_read, file_write, network_access]
# @sandbox_level: restricted

import os
import sys
import json
import threading
import time

class Plugin:
    """Advanced plugin class with sandbox isolation"""
    
    def __init__(self):
        self.plugin_name = "{plugin_name}"
        self.sandbox_dir = ""
        self.initialized = False
        self.threads = []
        self.data = {{}}
    
    def initialize(self):
        """Initialize the plugin"""
        try:
            # Initialize plugin data
            self.data = {{
                'start_time': time.time(),
                'execution_count': 0,
                'last_execution': None
            }}
            
            self.initialized = True
            print(f"Plugin {{self.plugin_name}} initialized")
            return True
            
        except Exception as e:
            print(f"Failed to initialize plugin {{self.plugin_name}}: {{e}}")
            return False
    
    def cleanup(self):
        """Cleanup plugin resources"""
        try:
            # Stop all threads
            for thread in self.threads:
                if thread.is_alive():
                    thread.join(timeout=1)
            
            # Save plugin data
            self.save_plugin_data()
            
            self.initialized = False
            print(f"Plugin {{self.plugin_name}} cleaned up")
            
        except Exception as e:
            print(f"Error during plugin cleanup: {{e}}")
    
    def health_check(self):
        """Check plugin health"""
        try:
            # Check if plugin is responsive
            current_time = time.time()
            if current_time - self.data.get('start_time', 0) > 3600:  # 1 hour
                return False
            
            return self.initialized
            
        except Exception as e:
            print(f"Health check failed: {{e}}")
            return False
    
    def execute(self, *args, **kwargs):
        """Execute plugin function"""
        try:
            self.data['execution_count'] += 1
            self.data['last_execution'] = time.time()
            
            print(f"Plugin {{self.plugin_name}} executed (count: {{self.data['execution_count']}})")
            
            # Process arguments
            result = {{
                'plugin_name': self.plugin_name,
                'execution_count': self.data['execution_count'],
                'args': args,
                'kwargs': kwargs,
                'timestamp': time.time()
            }}
            
            return result
            
        except Exception as e:
            print(f"Plugin execution error: {{e}}")
            return None
    
    def save_plugin_data(self):
        """Save plugin data to sandbox"""
        try:
            if self.sandbox_dir:
                data_file = os.path.join(self.sandbox_dir, 'plugin_data.json')
                with open(data_file, 'w') as f:
                    json.dump(self.data, f, indent=2)
                    
        except Exception as e:
            print(f"Failed to save plugin data: {{e}}")
    
    def load_plugin_data(self):
        """Load plugin data from sandbox"""
        try:
            if self.sandbox_dir:
                data_file = os.path.join(self.sandbox_dir, 'plugin_data.json')
                if os.path.exists(data_file):
                    with open(data_file, 'r') as f:
                        self.data = json.load(f)
                        
        except Exception as e:
            print(f"Failed to load plugin data: {{e}}")
'''
        
        try:
            with open(plugin_path, 'w') as f:
                f.write(template_content)
            
            print(f"Plugin template created: {plugin_path}")
            return plugin_path
            
        except Exception as e:
            print(f"Failed to create plugin template: {e}")
            return None
    
    def validate_plugin(self, plugin_path: str) -> bool:
        """Validate a plugin before loading"""
        try:
            # Check if file exists
            if not os.path.exists(plugin_path):
                return False
            
            # Check if it's a Python file
            if not plugin_path.endswith('.py'):
                return False
            
            # Check for required Plugin class
            with open(plugin_path, 'r') as f:
                content = f.read()
                
                if 'class Plugin:' not in content:
                    return False
                
                if 'def initialize(self):' not in content:
                    return False
                
                if 'def cleanup(self):' not in content:
                    return False
            
            return True
            
        except Exception as e:
            print(f"Plugin validation error: {e}")
            return False
    
    def get_sandbox_info(self) -> Dict[str, Any]:
        """Get sandbox information"""
        return {
            'sandbox_base_dir': self.sandbox_base_dir,
            'plugin_count': len(self.plugins),
            'sandbox_dirs': list(self.plugin_sandboxes.keys()),
            'total_size': self.get_sandbox_size()
        }
    
    def get_sandbox_size(self) -> int:
        """Get total size of sandbox directory"""
        total_size = 0
        try:
            for dirpath, dirnames, filenames in os.walk(self.sandbox_base_dir):
                for filename in filenames:
                    filepath = os.path.join(dirpath, filename)
                    if os.path.exists(filepath):
                        total_size += os.path.getsize(filepath)
        except Exception as e:
            print(f"Error calculating sandbox size: {e}")
        
        return total_size
    
    def cleanup_sandbox(self):
        """Clean up sandbox environment"""
        try:
            if os.path.exists(self.sandbox_base_dir):
                shutil.rmtree(self.sandbox_base_dir)
                print("Sandbox environment cleaned up")
        except Exception as e:
            print(f"Failed to cleanup sandbox: {e}")
    
    def shutdown(self):
        """Shutdown the plugin architecture"""
        try:
            # Stop all plugins
            for plugin_name in list(self.plugins.keys()):
                self.stop_plugin(plugin_name)
            
            # Cleanup sandbox
            self.cleanup_sandbox()
            
            self.initialized = False
            print("Plugin Architecture System shutdown complete")
            
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the plugin architecture"""
    print("Testing Plugin Architecture System...")
    
    plugin_system = PluginArchitecture()
    
    if plugin_system.initialize():
        print("✅ Plugin Architecture initialized")
        
        # Create plugin templates
        basic_plugin = plugin_system.create_plugin_template("test_basic", "basic")
        advanced_plugin = plugin_system.create_plugin_template("test_advanced", "advanced")
        
        if basic_plugin and advanced_plugin:
            print("✅ Plugin templates created")
            
            # Load plugins
            plugin_system.load_plugin("test_basic", basic_plugin)
            plugin_system.load_plugin("test_advanced", advanced_plugin)
            
            # Get plugin list
            plugins = plugin_system.get_plugin_list()
            print(f"Loaded plugins: {len(plugins)}")
            
            # Execute plugin functions
            result1 = plugin_system.execute_plugin_function("test_basic", "execute", "test", "args")
            result2 = plugin_system.execute_plugin_function("test_advanced", "execute", "test", "args")
            
            print(f"Basic plugin result: {result1}")
            print(f"Advanced plugin result: {result2}")
            
            # Get sandbox info
            sandbox_info = plugin_system.get_sandbox_info()
            print(f"Sandbox info: {sandbox_info}")
            
            # Shutdown
            plugin_system.shutdown()
            print("✅ Plugin Architecture test complete")
        else:
            print("❌ Failed to create plugin templates")
    else:
        print("❌ Failed to initialize Plugin Architecture")

if __name__ == "__main__":
    main()
