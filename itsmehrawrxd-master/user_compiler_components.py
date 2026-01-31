#!/usr/bin/env python3
"""
User Compiler Components System
Allows users to create and manage:
- Custom Language Parsers
- Custom IR Passes  
- Custom Backend Targets
"""

import os
import sys
import json
import importlib
import inspect
from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional, Callable, Type
from dataclasses import dataclass, asdict
from enum import Enum
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import subprocess

class ComponentType(Enum):
    LANGUAGE_PARSER = "language_parser"
    IR_PASS = "ir_pass"
    BACKEND_TARGET = "backend_target"

class LanguageType(Enum):
    COMPILED = "compiled"
    INTERPRETED = "interpreted"
    TRANSPILED = "transpiled"
    BYTECODE = "bytecode"

class TargetType(Enum):
    EXECUTABLE = "executable"
    LIBRARY = "library"
    BYTECODE = "bytecode"
    WEB = "web"
    MOBILE = "mobile"
    EMBEDDED = "embedded"

@dataclass
class UserLanguageInfo:
    name: str
    extension: str
    language_type: LanguageType
    description: str
    keywords: List[str]
    operators: List[str]
    delimiters: List[str]
    parser_class: str
    lexer_class: str
    author: str
    version: str
    file_path: str

@dataclass
class UserIRPassInfo:
    name: str
    description: str
    pass_class: str
    dependencies: List[str]
    optimization_level: int
    author: str
    version: str
    file_path: str

@dataclass
class UserBackendTargetInfo:
    name: str
    target_type: TargetType
    description: str
    codegen_class: str
    file_extension: str
    platform: str
    author: str
    version: str
    file_path: str

class BaseUserLexer(ABC):
    """Base class for user-defined lexers"""
    
    @abstractmethod
    def tokenize(self, source: str) -> List[Any]:
        """Tokenize source code"""
        pass
    
    @abstractmethod
    def get_keywords(self) -> List[str]:
        """Get language keywords"""
        pass
    
    @abstractmethod
    def get_operators(self) -> List[str]:
        """Get language operators"""
        pass

class BaseUserParser(ABC):
    """Base class for user-defined parsers"""
    
    @abstractmethod
    def parse(self, tokens: List[Any]) -> Any:
        """Parse tokens into AST"""
        pass
    
    @abstractmethod
    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        pass

class BaseUserIRPass(ABC):
    """Base class for user-defined IR passes"""
    
    @abstractmethod
    def apply(self, ir: Any) -> Any:
        """Apply IR transformation"""
        pass
    
    @abstractmethod
    def get_name(self) -> str:
        """Get pass name"""
        pass
    
    @abstractmethod
    def get_description(self) -> str:
        """Get pass description"""
        pass

class BaseUserCodeGenerator(ABC):
    """Base class for user-defined code generators"""
    
    @abstractmethod
    def generate(self, ir: Any, target_info: UserBackendTargetInfo) -> str:
        """Generate code for target"""
        pass
    
    @abstractmethod
    def get_supported_targets(self) -> List[TargetType]:
        """Get supported target types"""
        pass

class UserComponentManager:
    """Manages user-defined compiler components"""
    
    def __init__(self):
        self.user_components_dir = os.path.join(os.getcwd(), 'user_components')
        self.languages = {}
        self.ir_passes = {}
        self.backend_targets = {}
        self.component_registry = {}
        
        # Create user components directory if it doesn't exist
        os.makedirs(self.user_components_dir, exist_ok=True)
        
        # Load existing components
        self.load_all_components()
    
    def load_all_components(self):
        """Load all user components from the directory"""
        print("🔧 Loading user compiler components...")
        
        for root, dirs, files in os.walk(self.user_components_dir):
            for file in files:
                if file.endswith('.py'):
                    self.load_component_file(os.path.join(root, file))
        
        print(f"✅ Loaded {len(self.languages)} languages, {len(self.ir_passes)} IR passes, {len(self.backend_targets)} backend targets")
    
    def load_component_file(self, file_path: str):
        """Load a component from a Python file"""
        try:
            spec = importlib.util.spec_from_file_location("user_component", file_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            
            # Check for language component
            if hasattr(module, 'LanguageInfo'):
                lang_info = module.LanguageInfo
                self.languages[lang_info.name.lower()] = lang_info
                self.component_registry[lang_info.name.lower()] = {
                    'type': ComponentType.LANGUAGE_PARSER,
                    'info': lang_info,
                    'module': module
                }
                print(f"✅ Loaded language: {lang_info.name}")
            
            # Check for IR pass component
            if hasattr(module, 'IRPassInfo'):
                pass_info = module.IRPassInfo
                self.ir_passes[pass_info.name.lower()] = pass_info
                self.component_registry[pass_info.name.lower()] = {
                    'type': ComponentType.IR_PASS,
                    'info': pass_info,
                    'module': module
                }
                print(f"✅ Loaded IR pass: {pass_info.name}")
            
            # Check for backend target component
            if hasattr(module, 'BackendTargetInfo'):
                target_info = module.BackendTargetInfo
                self.backend_targets[target_info.name.lower()] = target_info
                self.component_registry[target_info.name.lower()] = {
                    'type': ComponentType.BACKEND_TARGET,
                    'info': target_info,
                    'module': module
                }
                print(f"✅ Loaded backend target: {target_info.name}")
                
        except Exception as e:
            print(f"❌ Error loading component {file_path}: {e}")
    
    def create_language_component(self, name: str, extension: str, language_type: LanguageType, 
                                description: str, author: str, version: str = "1.0.0") -> str:
        """Create a new language component"""
        # Generate template
        template = self.generate_language_template(name, extension, language_type, description, author, version)
        
        # Save to file
        filename = f"{name.lower()}_parser.py"
        file_path = os.path.join(self.user_components_dir, filename)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(template)
        
        # Load the new component
        self.load_component_file(file_path)
        
        return file_path
    
    def create_ir_pass_component(self, name: str, description: str, optimization_level: int,
                               author: str, version: str = "1.0.0") -> str:
        """Create a new IR pass component"""
        # Generate template
        template = self.generate_ir_pass_template(name, description, optimization_level, author, version)
        
        # Save to file
        filename = f"{name.lower()}_pass.py"
        file_path = os.path.join(self.user_components_dir, filename)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(template)
        
        # Load the new component
        self.load_component_file(file_path)
        
        return file_path
    
    def create_backend_target_component(self, name: str, target_type: TargetType, description: str,
                                      file_extension: str, platform: str, author: str, 
                                      version: str = "1.0.0") -> str:
        """Create a new backend target component"""
        # Generate template
        template = self.generate_backend_target_template(name, target_type, description, 
                                                       file_extension, platform, author, version)
        
        # Save to file
        filename = f"{name.lower()}_target.py"
        file_path = os.path.join(self.user_components_dir, filename)
        
        with open(file_path, 'w', encoding='utf-8') as f:
            f.write(template)
        
        # Load the new component
        self.load_component_file(file_path)
        
        return file_path
    
    def generate_language_template(self, name: str, extension: str, language_type: LanguageType,
                                 description: str, author: str, version: str) -> str:
        """Generate a language parser template"""
        template = f'''#!/usr/bin/env python3
"""
Custom Language Parser: {name}
Author: {author}
Version: {version}
Description: {description}
"""

from user_compiler_components import BaseUserLexer, BaseUserParser, UserLanguageInfo, LanguageType
from typing import List, Any

class {name}Lexer(BaseUserLexer):
    """Custom lexer for {name}"""
    
    def __init__(self):
        self.keywords = {self.get_keywords()}
        self.operators = {self.get_operators()}
        self.delimiters = {self.get_delimiters()}
    
    def tokenize(self, source: str) -> List[Any]:
        """Tokenize {name} source code"""
        # TODO: Implement tokenization logic
        tokens = []
        
        # Example tokenization logic:
        # 1. Split source into lines
        # 2. Process each line for tokens
        # 3. Handle comments, strings, numbers, identifiers
        # 4. Return list of tokens
        
        return tokens
    
    def get_keywords(self) -> List[str]:
        """Get {name} keywords"""
        return {self.get_keywords()}
    
    def get_operators(self) -> List[str]:
        """Get {name} operators"""
        return {self.get_operators()}
    
    def get_delimiters(self) -> List[str]:
        """Get {name} delimiters"""
        return {self.get_delimiters()}

class {name}Parser(BaseUserParser):
    """Custom parser for {name}"""
    
    def __init__(self, tokens):
        self.tokens = tokens
        self.current_token = 0
    
    def parse(self, tokens) -> Any:
        """Parse {name} tokens into AST"""
        # TODO: Implement parsing logic
        ast = None
        
        # Example parsing logic:
        # 1. Process tokens sequentially
        # 2. Build AST nodes based on grammar rules
        # 3. Handle expressions, statements, declarations
        # 4. Return complete AST
        
        return ast
    
    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        return ['ProgramNode', 'FunctionNode', 'VariableNode', 'ExpressionNode']

# Language information
LanguageInfo = UserLanguageInfo(
    name='{name}',
    extension='{extension}',
    language_type=LanguageType.{language_type.value.upper()},
    description='{description}',
    keywords={self.get_keywords()},
    operators={self.get_operators()},
    delimiters={self.get_delimiters()},
    parser_class='{name}Parser',
    lexer_class='{name}Lexer',
    author='{author}',
    version='{version}',
    file_path='{os.path.join(self.user_components_dir, f"{name.lower()}_parser.py")}'
)
'''
        return template
    
    def generate_ir_pass_template(self, name: str, description: str, optimization_level: int,
                                author: str, version: str) -> str:
        """Generate an IR pass template"""
        template = f'''#!/usr/bin/env python3
"""
Custom IR Pass: {name}
Author: {author}
Version: {version}
Description: {description}
"""

from user_compiler_components import BaseUserIRPass, UserIRPassInfo
from typing import Any

class {name}Pass(BaseUserIRPass):
    """Custom IR pass: {name}"""
    
    def __init__(self):
        self.name = "{name}"
        self.description = "{description}"
        self.optimization_level = {optimization_level}
    
    def apply(self, ir: Any) -> Any:
        """Apply {name} transformation to IR"""
        # TODO: Implement IR transformation logic
        
        # Example IR transformation:
        # 1. Analyze IR structure
        # 2. Apply optimization/transformation rules
        # 3. Return modified IR
        
        return ir
    
    def get_name(self) -> str:
        """Get pass name"""
        return self.name
    
    def get_description(self) -> str:
        """Get pass description"""
        return self.description

# IR pass information
IRPassInfo = UserIRPassInfo(
    name="{name}",
    description="{description}",
    pass_class="{name}Pass",
    dependencies=[],
    optimization_level={optimization_level},
    author="{author}",
    version="{version}",
    file_path="{os.path.join(self.user_components_dir, f"{name.lower()}_pass.py")}"
)
'''
        return template
    
    def generate_backend_target_template(self, name: str, target_type: TargetType, description: str,
                                       file_extension: str, platform: str, author: str, version: str) -> str:
        """Generate a backend target template"""
        template = f'''#!/usr/bin/env python3
"""
Custom Backend Target: {name}
Author: {author}
Version: {version}
Description: {description}
"""

from user_compiler_components import BaseUserCodeGenerator, UserBackendTargetInfo, TargetType
from typing import Any

class {name}CodeGenerator(BaseUserCodeGenerator):
    """Custom code generator for {name}"""
    
    def __init__(self):
        self.name = "{name}"
        self.description = "{description}"
        self.target_type = TargetType.{target_type.value.upper()}
    
    def generate(self, ir: Any, target_info: UserBackendTargetInfo) -> str:
        """Generate code for {name} target"""
        # TODO: Implement code generation logic
        
        # Example code generation:
        # 1. Analyze IR structure
        # 2. Generate target-specific code
        # 3. Handle platform-specific features
        # 4. Return generated code
        
        return generated_code
    
    def get_supported_targets(self) -> List[TargetType]:
        """Get supported target types"""
        return [TargetType.{target_type.value.upper()}]

# Backend target information
BackendTargetInfo = UserBackendTargetInfo(
    name="{name}",
    target_type=TargetType.{target_type.value.upper()},
    description="{description}",
    codegen_class="{name}CodeGenerator",
    file_extension="{file_extension}",
    platform="{platform}",
    author="{author}",
    version="{version}",
    file_path="{os.path.join(self.user_components_dir, f"{name.lower()}_target.py")}"
)
'''
        return template
    
    def get_component_info(self, component_name: str) -> Optional[Dict]:
        """Get information about a component"""
        return self.component_registry.get(component_name.lower())
    
    def list_components(self, component_type: Optional[ComponentType] = None) -> List[str]:
        """List all components or components of a specific type"""
        if component_type is None:
            return list(self.component_registry.keys())
        
        return [name for name, info in self.component_registry.items() 
                if info['type'] == component_type]
    
    def remove_component(self, component_name: str) -> bool:
        """Remove a component"""
        if component_name.lower() in self.component_registry:
            info = self.component_registry[component_name.lower()]
            file_path = info['info'].file_path
            
            try:
                os.remove(file_path)
                del self.component_registry[component_name.lower()]
                
                # Remove from specific registries
                if info['type'] == ComponentType.LANGUAGE_PARSER:
                    del self.languages[component_name.lower()]
                elif info['type'] == ComponentType.IR_PASS:
                    del self.ir_passes[component_name.lower()]
                elif info['type'] == ComponentType.BACKEND_TARGET:
                    del self.backend_targets[component_name.lower()]
                
                print(f"✅ Removed component: {component_name}")
                return True
            except Exception as e:
                print(f"❌ Error removing component {component_name}: {e}")
                return False
        
        return False

class UserComponentGUI:
    """GUI for managing user compiler components"""
    
    def __init__(self, component_manager: UserComponentManager):
        self.component_manager = component_manager
        self.root = tk.Tk()
        self.root.title("🔧 User Compiler Components")
        self.root.geometry("1200x800")
        
        self.setup_ui()
    
    def setup_ui(self):
        """Setup the GUI"""
        # Main notebook
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Overview tab
        overview_frame = ttk.Frame(notebook)
        notebook.add(overview_frame, text="📊 Overview")
        self.create_overview_tab(overview_frame)
        
        # Language Parsers tab
        languages_frame = ttk.Frame(notebook)
        notebook.add(languages_frame, text="🌐 Language Parsers")
        self.create_languages_tab(languages_frame)
        
        # IR Passes tab
        passes_frame = ttk.Frame(notebook)
        notebook.add(passes_frame, text="⚙️ IR Passes")
        self.create_ir_passes_tab(passes_frame)
        
        # Backend Targets tab
        targets_frame = ttk.Frame(notebook)
        notebook.add(targets_frame, text="🎯 Backend Targets")
        self.create_backend_targets_tab(targets_frame)
        
        # Component Creator tab
        creator_frame = ttk.Frame(notebook)
        notebook.add(creator_frame, text="🔧 Component Creator")
        self.create_component_creator_tab(creator_frame)
    
    def create_overview_tab(self, parent):
        """Create overview tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="📊 User Compiler Components Overview", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="🔄 Refresh", 
                  command=self.refresh_overview).pack(side=tk.RIGHT)
        
        # Statistics
        stats_frame = ttk.LabelFrame(parent, text="Component Statistics", padding="10")
        stats_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.languages_count_label = ttk.Label(stats_frame, text="Languages: 0")
        self.languages_count_label.pack(anchor=tk.W)
        
        self.passes_count_label = ttk.Label(stats_frame, text="IR Passes: 0")
        self.passes_count_label.pack(anchor=tk.W)
        
        self.targets_count_label = ttk.Label(stats_frame, text="Backend Targets: 0")
        self.targets_count_label.pack(anchor=tk.W)
        
        # Recent components
        recent_frame = ttk.LabelFrame(parent, text="Recent Components", padding="10")
        recent_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.recent_tree = ttk.Treeview(recent_frame, columns=('Name', 'Type', 'Author', 'Version'), show='headings')
        self.recent_tree.heading('Name', text='Name')
        self.recent_tree.heading('Type', text='Type')
        self.recent_tree.heading('Author', text='Author')
        self.recent_tree.heading('Version', text='Version')
        
        scrollbar = ttk.Scrollbar(recent_frame, orient=tk.VERTICAL, command=self.recent_tree.yview)
        self.recent_tree.configure(yscrollcommand=scrollbar.set)
        
        self.recent_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.refresh_overview()
    
    def create_languages_tab(self, parent):
        """Create language parsers tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🌐 Language Parsers", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Create New", 
                  command=self.create_language_dialog).pack(side=tk.RIGHT, padx=(0, 10))
        ttk.Button(header_frame, text="🔄 Refresh", 
                  command=self.refresh_languages).pack(side=tk.RIGHT)
        
        # Languages list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.languages_tree = ttk.Treeview(list_frame, 
                                         columns=('Name', 'Extension', 'Type', 'Author', 'Version', 'Description'), 
                                         show='headings', height=15)
        
        for col in ('Name', 'Extension', 'Type', 'Author', 'Version', 'Description'):
            self.languages_tree.heading(col, text=col)
            self.languages_tree.column(col, width=120)
        
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.languages_tree.yview)
        self.languages_tree.configure(yscrollcommand=scrollbar.set)
        
        self.languages_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Context menu
        self.languages_tree.bind("<Button-3>", self.show_language_context_menu)
        
        self.refresh_languages()
    
    def create_ir_passes_tab(self, parent):
        """Create IR passes tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="⚙️ IR Optimization Passes", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Create New", 
                  command=self.create_ir_pass_dialog).pack(side=tk.RIGHT, padx=(0, 10))
        ttk.Button(header_frame, text="🔄 Refresh", 
                  command=self.refresh_ir_passes).pack(side=tk.RIGHT)
        
        # IR passes list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.passes_tree = ttk.Treeview(list_frame, 
                                      columns=('Name', 'Description', 'Level', 'Author', 'Version'), 
                                      show='headings', height=15)
        
        for col in ('Name', 'Description', 'Level', 'Author', 'Version'):
            self.passes_tree.heading(col, text=col)
            self.passes_tree.column(col, width=150)
        
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.passes_tree.yview)
        self.passes_tree.configure(yscrollcommand=scrollbar.set)
        
        self.passes_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.refresh_ir_passes()
    
    def create_backend_targets_tab(self, parent):
        """Create backend targets tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🎯 Backend Targets", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Create New", 
                  command=self.create_backend_target_dialog).pack(side=tk.RIGHT, padx=(0, 10))
        ttk.Button(header_frame, text="🔄 Refresh", 
                  command=self.refresh_backend_targets).pack(side=tk.RIGHT)
        
        # Backend targets list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        self.targets_tree = ttk.Treeview(list_frame, 
                                       columns=('Name', 'Type', 'Platform', 'Extension', 'Author', 'Version'), 
                                       show='headings', height=15)
        
        for col in ('Name', 'Type', 'Platform', 'Extension', 'Author', 'Version'):
            self.targets_tree.heading(col, text=col)
            self.targets_tree.column(col, width=120)
        
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.targets_tree.yview)
        self.targets_tree.configure(yscrollcommand=scrollbar.set)
        
        self.targets_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        self.refresh_backend_targets()
    
    def create_component_creator_tab(self, parent):
        """Create component creator tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🔧 Component Creator", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        # Component type selection
        type_frame = ttk.LabelFrame(parent, text="Component Type", padding="10")
        type_frame.pack(fill=tk.X, padx=10, pady=10)
        
        self.component_type_var = tk.StringVar(value="language_parser")
        
        ttk.Radiobutton(type_frame, text="🌐 Language Parser", 
                       variable=self.component_type_var, value="language_parser").pack(anchor=tk.W)
        ttk.Radiobutton(type_frame, text="⚙️ IR Pass", 
                       variable=self.component_type_var, value="ir_pass").pack(anchor=tk.W)
        ttk.Radiobutton(type_frame, text="🎯 Backend Target", 
                       variable=self.component_type_var, value="backend_target").pack(anchor=tk.W)
        
        # Component form
        self.form_frame = ttk.LabelFrame(parent, text="Component Details", padding="10")
        self.form_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Create form based on component type
        self.create_component_form()
        
        # Bind type change
        self.component_type_var.trace('w', self.on_component_type_change)
    
    def create_component_form(self):
        """Create the component form based on selected type"""
        # Clear existing form
        for widget in self.form_frame.winfo_children():
            widget.destroy()
        
        component_type = self.component_type_var.get()
        
        if component_type == "language_parser":
            self.create_language_form()
        elif component_type == "ir_pass":
            self.create_ir_pass_form()
        elif component_type == "backend_target":
            self.create_backend_target_form()
    
    def create_language_form(self):
        """Create language parser form"""
        # Name
        ttk.Label(self.form_frame, text="Language Name:").pack(anchor=tk.W)
        self.name_entry = ttk.Entry(self.form_frame, width=30)
        self.name_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Extension
        ttk.Label(self.form_frame, text="File Extension:").pack(anchor=tk.W)
        self.extension_entry = ttk.Entry(self.form_frame, width=30)
        self.extension_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Language Type
        ttk.Label(self.form_frame, text="Language Type:").pack(anchor=tk.W)
        self.language_type_combo = ttk.Combobox(self.form_frame, 
                                               values=[t.value for t in LanguageType], 
                                               state="readonly")
        self.language_type_combo.pack(fill=tk.X, pady=(0, 10))
        
        # Description
        ttk.Label(self.form_frame, text="Description:").pack(anchor=tk.W)
        self.description_text = scrolledtext.ScrolledText(self.form_frame, height=3)
        self.description_text.pack(fill=tk.X, pady=(0, 10))
        
        # Author
        ttk.Label(self.form_frame, text="Author:").pack(anchor=tk.W)
        self.author_entry = ttk.Entry(self.form_frame, width=30)
        self.author_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Version
        ttk.Label(self.form_frame, text="Version:").pack(anchor=tk.W)
        self.version_entry = ttk.Entry(self.form_frame, width=30)
        self.version_entry.insert(0, "1.0.0")
        self.version_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Create button
        ttk.Button(self.form_frame, text="🔧 Create Language Parser", 
                  command=self.create_language_component).pack(pady=10)
    
    def create_ir_pass_form(self):
        """Create IR pass form"""
        # Name
        ttk.Label(self.form_frame, text="IR Pass Name:").pack(anchor=tk.W)
        self.name_entry = ttk.Entry(self.form_frame, width=30)
        self.name_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Description
        ttk.Label(self.form_frame, text="Description:").pack(anchor=tk.W)
        self.description_text = scrolledtext.ScrolledText(self.form_frame, height=3)
        self.description_text.pack(fill=tk.X, pady=(0, 10))
        
        # Optimization Level
        ttk.Label(self.form_frame, text="Optimization Level:").pack(anchor=tk.W)
        self.optimization_level_entry = ttk.Entry(self.form_frame, width=30)
        self.optimization_level_entry.insert(0, "1")
        self.optimization_level_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Author
        ttk.Label(self.form_frame, text="Author:").pack(anchor=tk.W)
        self.author_entry = ttk.Entry(self.form_frame, width=30)
        self.author_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Version
        ttk.Label(self.form_frame, text="Version:").pack(anchor=tk.W)
        self.version_entry = ttk.Entry(self.form_frame, width=30)
        self.version_entry.insert(0, "1.0.0")
        self.version_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Create button
        ttk.Button(self.form_frame, text="⚙️ Create IR Pass", 
                  command=self.create_ir_pass_component).pack(pady=10)
    
    def create_backend_target_form(self):
        """Create backend target form"""
        # Name
        ttk.Label(self.form_frame, text="Target Name:").pack(anchor=tk.W)
        self.name_entry = ttk.Entry(self.form_frame, width=30)
        self.name_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Target Type
        ttk.Label(self.form_frame, text="Target Type:").pack(anchor=tk.W)
        self.target_type_combo = ttk.Combobox(self.form_frame, 
                                             values=[t.value for t in TargetType], 
                                             state="readonly")
        self.target_type_combo.pack(fill=tk.X, pady=(0, 10))
        
        # Description
        ttk.Label(self.form_frame, text="Description:").pack(anchor=tk.W)
        self.description_text = scrolledtext.ScrolledText(self.form_frame, height=3)
        self.description_text.pack(fill=tk.X, pady=(0, 10))
        
        # File Extension
        ttk.Label(self.form_frame, text="File Extension:").pack(anchor=tk.W)
        self.extension_entry = ttk.Entry(self.form_frame, width=30)
        self.extension_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Platform
        ttk.Label(self.form_frame, text="Platform:").pack(anchor=tk.W)
        self.platform_entry = ttk.Entry(self.form_frame, width=30)
        self.platform_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Author
        ttk.Label(self.form_frame, text="Author:").pack(anchor=tk.W)
        self.author_entry = ttk.Entry(self.form_frame, width=30)
        self.author_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Version
        ttk.Label(self.form_frame, text="Version:").pack(anchor=tk.W)
        self.version_entry = ttk.Entry(self.form_frame, width=30)
        self.version_entry.insert(0, "1.0.0")
        self.version_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Create button
        ttk.Button(self.form_frame, text="🎯 Create Backend Target", 
                  command=self.create_backend_target_component).pack(pady=10)
    
    def on_component_type_change(self, *args):
        """Handle component type change"""
        self.create_component_form()
    
    def create_language_component(self):
        """Create a new language component"""
        name = self.name_entry.get()
        extension = self.extension_entry.get()
        language_type = LanguageType(self.language_type_combo.get())
        description = self.description_text.get("1.0", tk.END).strip()
        author = self.author_entry.get()
        version = self.version_entry.get()
        
        if name and extension and description and author:
            try:
                file_path = self.component_manager.create_language_component(
                    name, extension, language_type, description, author, version)
                messagebox.showinfo("Success", f"Language parser created: {file_path}")
                self.refresh_all()
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create language parser: {e}")
        else:
            messagebox.showerror("Error", "Please fill in all required fields")
    
    def create_ir_pass_component(self):
        """Create a new IR pass component"""
        name = self.name_entry.get()
        description = self.description_text.get("1.0", tk.END).strip()
        optimization_level = int(self.optimization_level_entry.get())
        author = self.author_entry.get()
        version = self.version_entry.get()
        
        if name and description and author:
            try:
                file_path = self.component_manager.create_ir_pass_component(
                    name, description, optimization_level, author, version)
                messagebox.showinfo("Success", f"IR pass created: {file_path}")
                self.refresh_all()
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create IR pass: {e}")
        else:
            messagebox.showerror("Error", "Please fill in all required fields")
    
    def create_backend_target_component(self):
        """Create a new backend target component"""
        name = self.name_entry.get()
        target_type = TargetType(self.target_type_combo.get())
        description = self.description_text.get("1.0", tk.END).strip()
        extension = self.extension_entry.get()
        platform = self.platform_entry.get()
        author = self.author_entry.get()
        version = self.version_entry.get()
        
        if name and description and extension and platform and author:
            try:
                file_path = self.component_manager.create_backend_target_component(
                    name, target_type, description, extension, platform, author, version)
                messagebox.showinfo("Success", f"Backend target created: {file_path}")
                self.refresh_all()
            except Exception as e:
                messagebox.showerror("Error", f"Failed to create backend target: {e}")
        else:
            messagebox.showerror("Error", "Please fill in all required fields")
    
    def refresh_overview(self):
        """Refresh overview tab"""
        self.languages_count_label.config(text=f"Languages: {len(self.component_manager.languages)}")
        self.passes_count_label.config(text=f"IR Passes: {len(self.component_manager.ir_passes)}")
        self.targets_count_label.config(text=f"Backend Targets: {len(self.component_manager.backend_targets)}")
        
        # Clear and repopulate recent components
        for item in self.recent_tree.get_children():
            self.recent_tree.delete(item)
        
        for name, info in self.component_manager.component_registry.items():
            self.recent_tree.insert('', 'end', values=(
                info['info'].name,
                info['type'].value.replace('_', ' ').title(),
                getattr(info['info'], 'author', 'Unknown'),
                getattr(info['info'], 'version', '1.0.0')
            ))
    
    def refresh_languages(self):
        """Refresh languages list"""
        for item in self.languages_tree.get_children():
            self.languages_tree.delete(item)
        
        for lang_name, lang_info in self.component_manager.languages.items():
            self.languages_tree.insert('', 'end', values=(
                lang_info.name,
                lang_info.extension,
                lang_info.language_type.value,
                lang_info.author,
                lang_info.version,
                lang_info.description
            ))
    
    def refresh_ir_passes(self):
        """Refresh IR passes list"""
        for item in self.passes_tree.get_children():
            self.passes_tree.delete(item)
        
        for pass_name, pass_info in self.component_manager.ir_passes.items():
            self.passes_tree.insert('', 'end', values=(
                pass_info.name,
                pass_info.description,
                pass_info.optimization_level,
                pass_info.author,
                pass_info.version
            ))
    
    def refresh_backend_targets(self):
        """Refresh backend targets list"""
        for item in self.targets_tree.get_children():
            self.targets_tree.delete(item)
        
        for target_name, target_info in self.component_manager.backend_targets.items():
            self.targets_tree.insert('', 'end', values=(
                target_info.name,
                target_info.target_type.value,
                target_info.platform,
                target_info.file_extension,
                target_info.author,
                target_info.version
            ))
    
    def refresh_all(self):
        """Refresh all tabs"""
        self.refresh_overview()
        self.refresh_languages()
        self.refresh_ir_passes()
        self.refresh_backend_targets()
    
    def show_language_context_menu(self, event):
        """Show context menu for language items"""
        # Implementation for context menu
        pass
    
    def create_language_dialog(self):
        """Create language dialog (placeholder)"""
        self.component_type_var.set("language_parser")
    
    def create_ir_pass_dialog(self):
        """Create IR pass dialog (placeholder)"""
        self.component_type_var.set("ir_pass")
    
    def create_backend_target_dialog(self):
        """Create backend target dialog (placeholder)"""
        self.component_type_var.set("backend_target")
    
    def run(self):
        """Run the GUI"""
        self.root.mainloop()

def test_user_component_system():
    """Test the user component system"""
    print("🧪 Testing User Component System...")
    
    # Create component manager
    manager = UserComponentManager()
    
    # Test creating components
    print("🔧 Creating test language parser...")
    lang_file = manager.create_language_component(
        "TestLang", ".test", LanguageType.COMPILED, 
        "A test programming language", "Test Author", "1.0.0")
    print(f"✅ Created: {lang_file}")
    
    print("⚙️ Creating test IR pass...")
    pass_file = manager.create_ir_pass_component(
        "TestPass", "A test optimization pass", 2, "Test Author", "1.0.0")
    print(f"✅ Created: {pass_file}")
    
    print("🎯 Creating test backend target...")
    target_file = manager.create_backend_target_component(
        "TestTarget", TargetType.EXECUTABLE, "A test backend target",
        ".exe", "test", "Test Author", "1.0.0")
    print(f"✅ Created: {target_file}")
    
    # List components
    print(f"\n📊 Total components: {len(manager.component_registry)}")
    print(f"🌐 Languages: {len(manager.languages)}")
    print(f"⚙️ IR Passes: {len(manager.ir_passes)}")
    print(f"🎯 Backend Targets: {len(manager.backend_targets)}")
    
    print("✅ User Component System test completed!")
    return True

if __name__ == "__main__":
    # Test the system
    test_user_component_system()
    
    # Create and run GUI
    manager = UserComponentManager()
    gui = UserComponentGUI(manager)
    gui.run()
