#!/usr/bin/env python3
"""
Extensible Compiler System
Allows users to add:
- New language parsers
- Custom IR passes
- Backend targets
"""

import os
import sys
import json
import importlib
import inspect
from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional, Callable
from dataclasses import dataclass
from enum import Enum
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

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
class LanguageInfo:
    name: str
    extension: str
    language_type: LanguageType
    description: str
    keywords: List[str]
    operators: List[str]
    delimiters: List[str]
    parser_class: str
    lexer_class: str

@dataclass
class IRPassInfo:
    name: str
    description: str
    pass_class: str
    dependencies: List[str]
    optimization_level: int

@dataclass
class BackendTargetInfo:
    name: str
    target_type: TargetType
    description: str
    codegen_class: str
    file_extension: str
    platform: str

class BaseLexer(ABC):
    """Base class for language lexers"""
    
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

class BaseParser(ABC):
    """Base class for language parsers"""
    
    @abstractmethod
    def parse(self, tokens: List[Any]) -> Any:
        """Parse tokens into AST"""
        pass
    
    @abstractmethod
    def get_ast_node_types(self) -> List[str]:
        """Get supported AST node types"""
        pass

class BaseIRPass(ABC):
    """Base class for IR optimization passes"""
    
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

class BaseCodeGenerator(ABC):
    """Base class for code generators"""
    
    @abstractmethod
    def generate(self, ir: Any, target_info: BackendTargetInfo) -> str:
        """Generate code for target"""
        pass
    
    @abstractmethod
    def get_supported_targets(self) -> List[TargetType]:
        """Get supported target types"""
        pass

class ExtensibleCompilerSystem:
    """Main system for managing extensible compiler components"""
    
    def __init__(self):
        self.languages = {}
        self.ir_passes = {}
        self.backend_targets = {}
        self.custom_parsers = {}
        self.custom_passes = {}
        self.custom_targets = {}
        
        # Load built-in components
        self.load_builtin_components()
        
        # Load user components
        self.load_user_components()
    
    def load_builtin_components(self):
        """Load built-in language parsers, IR passes, and targets"""
        # Built-in languages
        self.languages = {
            'cpp': LanguageInfo(
                name='C++',
                extension='.cpp',
                language_type=LanguageType.COMPILED,
                description='C++ programming language',
                keywords=['int', 'float', 'double', 'char', 'void', 'if', 'else', 'while', 'for', 'return'],
                operators=['+', '-', '*', '/', '=', '==', '!=', '<', '>', '<=', '>=', '&&', '||', '!'],
                delimiters=[';', ',', '.', ':', '?', '(', ')', '{', '}', '[', ']', '<', '>'],
                parser_class='CppParser',
                lexer_class='CppLexer'
            ),
            'python': LanguageInfo(
                name='Python',
                extension='.py',
                language_type=LanguageType.INTERPRETED,
                description='Python programming language',
                keywords=['def', 'class', 'if', 'else', 'elif', 'while', 'for', 'in', 'return', 'import', 'from'],
                operators=['+', '-', '*', '/', '//', '%', '**', '=', '==', '!=', '<', '>', '<=', '>=', 'and', 'or', 'not'],
                delimiters=[':', '(', ')', '[', ']', '{', '}', ',', '.'],
                parser_class='PythonParser',
                lexer_class='PythonLexer'
            ),
            'javascript': LanguageInfo(
                name='JavaScript',
                extension='.js',
                language_type=LanguageType.INTERPRETED,
                description='JavaScript programming language',
                keywords=['var', 'let', 'const', 'function', 'if', 'else', 'while', 'for', 'return', 'class'],
                operators=['+', '-', '*', '/', '%', '=', '==', '===', '!=', '!==', '<', '>', '<=', '>=', '&&', '||', '!'],
                delimiters=[';', ',', '.', ':', '?', '(', ')', '{', '}', '[', ']'],
                parser_class='JavaScriptParser',
                lexer_class='JavaScriptLexer'
            ),
            'rust': LanguageInfo(
                name='Rust',
                extension='.rs',
                language_type=LanguageType.COMPILED,
                description='Rust programming language',
                keywords=['fn', 'let', 'mut', 'const', 'if', 'else', 'while', 'for', 'loop', 'return', 'struct', 'enum', 'impl'],
                operators=['+', '-', '*', '/', '%', '=', '==', '!=', '<', '>', '<=', '>=', '&&', '||', '!', '&', '|', '^'],
                delimiters=[';', ',', '.', ':', '?', '(', ')', '{', '}', '[', ']', '<', '>'],
                parser_class='RustParser',
                lexer_class='RustLexer'
            )
        }
        
        # Built-in IR passes
        self.ir_passes = {
            'constant_folding': IRPassInfo(
                name='Constant Folding',
                description='Fold constant expressions at compile time',
                pass_class='ConstantFoldingPass',
                dependencies=[],
                optimization_level=1
            ),
            'dead_code_elimination': IRPassInfo(
                name='Dead Code Elimination',
                description='Remove unreachable code',
                pass_class='DeadCodeEliminationPass',
                dependencies=[],
                optimization_level=2
            ),
            'loop_unrolling': IRPassInfo(
                name='Loop Unrolling',
                description='Unroll small loops for better performance',
                pass_class='LoopUnrollingPass',
                dependencies=[],
                optimization_level=3
            ),
            'function_inlining': IRPassInfo(
                name='Function Inlining',
                description='Inline small functions',
                pass_class='FunctionInliningPass',
                dependencies=[],
                optimization_level=2
            )
        }
        
        # Built-in backend targets
        self.backend_targets = {
            'x64_exe': BackendTargetInfo(
                name='x64 Executable',
                target_type=TargetType.EXECUTABLE,
                description='Windows x64 executable',
                codegen_class='X64CodeGenerator',
                file_extension='.exe',
                platform='windows'
            ),
            'x64_elf': BackendTargetInfo(
                name='x64 ELF',
                target_type=TargetType.EXECUTABLE,
                description='Linux x64 executable',
                codegen_class='X64ELFCodeGenerator',
                file_extension='',
                platform='linux'
            ),
            'arm64_exe': BackendTargetInfo(
                name='ARM64 Executable',
                target_type=TargetType.EXECUTABLE,
                description='ARM64 executable',
                codegen_class='ARM64CodeGenerator',
                file_extension='.exe',
                platform='windows'
            ),
            'python_bytecode': BackendTargetInfo(
                name='Python Bytecode',
                target_type=TargetType.BYTECODE,
                description='Python bytecode',
                codegen_class='PythonBytecodeGenerator',
                file_extension='.pyc',
                platform='python'
            ),
            'javascript': BackendTargetInfo(
                name='JavaScript',
                target_type=TargetType.WEB,
                description='JavaScript for web browsers',
                codegen_class='JavaScriptCodeGenerator',
                file_extension='.js',
                platform='web'
            ),
            'android_apk': BackendTargetInfo(
                name='Android APK',
                target_type=TargetType.MOBILE,
                description='Android application package',
                codegen_class='AndroidAPKGenerator',
                file_extension='.apk',
                platform='android'
            )
        }
    
    def load_user_components(self):
        """Load user-defined components"""
        user_dir = os.path.join(os.getcwd(), 'user_components')
        if os.path.exists(user_dir):
            self.scan_user_directory(user_dir)
    
    def scan_user_directory(self, directory: str):
        """Scan directory for user components"""
        for root, dirs, files in os.walk(directory):
            for file in files:
                if file.endswith('.py'):
                    self.load_user_component(os.path.join(root, file))
    
    def load_user_component(self, file_path: str):
        """Load a user component from file"""
        try:
            spec = importlib.util.spec_from_file_location("user_component", file_path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            
            # Check for language parsers
            if hasattr(module, 'LanguageInfo'):
                lang_info = module.LanguageInfo
                self.custom_parsers[lang_info.name.lower()] = lang_info
                self.languages[lang_info.name.lower()] = lang_info
            
            # Check for IR passes
            if hasattr(module, 'IRPassInfo'):
                pass_info = module.IRPassInfo
                self.custom_passes[pass_info.name.lower()] = pass_info
                self.ir_passes[pass_info.name.lower()] = pass_info
            
            # Check for backend targets
            if hasattr(module, 'BackendTargetInfo'):
                target_info = module.BackendTargetInfo
                self.custom_targets[target_info.name.lower()] = target_info
                self.backend_targets[target_info.name.lower()] = target_info
                
        except Exception as e:
            print(f"❌ Error loading user component {file_path}: {e}")
    
    def add_custom_language(self, language_info: LanguageInfo):
        """Add a custom language parser"""
        self.languages[language_info.name.lower()] = language_info
        self.custom_parsers[language_info.name.lower()] = language_info
        print(f"✅ Added custom language: {language_info.name}")
    
    def add_custom_ir_pass(self, pass_info: IRPassInfo):
        """Add a custom IR pass"""
        self.ir_passes[pass_info.name.lower()] = pass_info
        self.custom_passes[pass_info.name.lower()] = pass_info
        print(f"✅ Added custom IR pass: {pass_info.name}")
    
    def add_custom_backend_target(self, target_info: BackendTargetInfo):
        """Add a custom backend target"""
        self.backend_targets[target_info.name.lower()] = target_info
        self.custom_targets[target_info.name.lower()] = target_info
        print(f"✅ Added custom backend target: {target_info.name}")
    
    def get_language_info(self, language_name: str) -> Optional[LanguageInfo]:
        """Get language information"""
        return self.languages.get(language_name.lower())
    
    def get_ir_pass_info(self, pass_name: str) -> Optional[IRPassInfo]:
        """Get IR pass information"""
        return self.ir_passes.get(pass_name.lower())
    
    def get_backend_target_info(self, target_name: str) -> Optional[BackendTargetInfo]:
        """Get backend target information"""
        return self.backend_targets.get(target_name.lower())
    
    def list_languages(self) -> List[str]:
        """List all available languages"""
        return list(self.languages.keys())
    
    def list_ir_passes(self) -> List[str]:
        """List all available IR passes"""
        return list(self.ir_passes.keys())
    
    def list_backend_targets(self) -> List[str]:
        """List all available backend targets"""
        return list(self.backend_targets.keys())
    
    def create_language_template(self, language_name: str) -> str:
        """Create a template for a new language parser"""
        template = f'''#!/usr/bin/env python3
"""
Custom Language Parser: {language_name}
Generated by Extensible Compiler System
"""

from extensible_compiler_system import LanguageInfo, LanguageType, BaseLexer, BaseParser

class {language_name}Lexer(BaseLexer):
    """Custom lexer for {language_name}"""
    
    def __init__(self):
        self.keywords = {self.get_keywords()}
        self.operators = {self.get_operators()}
        self.delimiters = {self.get_delimiters()}
    
    def tokenize(self, source: str):
        """Tokenize {language_name} source code"""
        # TODO: Implement tokenization logic
        tokens = []
        # Your tokenization implementation here
        return tokens
    
    def get_keywords(self):
        """Get {language_name} keywords"""
        return {self.get_keywords()}
    
    def get_operators(self):
        """Get {language_name} operators"""
        return {self.get_operators()}

class {language_name}Parser(BaseParser):
    """Custom parser for {language_name}"""
    
    def __init__(self, tokens):
        self.tokens = tokens
        self.current_token = 0
    
    def parse(self, tokens):
        """Parse {language_name} tokens into AST"""
        # TODO: Implement parsing logic
        ast = None
        # Your parsing implementation here
        return ast
    
    def get_ast_node_types(self):
        """Get supported AST node types"""
        return ['ProgramNode', 'FunctionNode', 'VariableNode', 'ExpressionNode']

# Language information
LanguageInfo = LanguageInfo(
    name='{language_name}',
    extension='.{language_name.lower()}',
    language_type=LanguageType.COMPILED,
    description='{language_name} programming language',
    keywords={self.get_keywords()},
    operators={self.get_operators()},
    delimiters={self.get_delimiters()},
    parser_class='{language_name}Parser',
    lexer_class='{language_name}Lexer'
)
'''
        return template
    
    def create_ir_pass_template(self, pass_name: str) -> str:
        """Create a template for a new IR pass"""
        template = f'''#!/usr/bin/env python3
"""
Custom IR Pass: {pass_name}
Generated by Extensible Compiler System
"""

from extensible_compiler_system import IRPassInfo, BaseIRPass

class {pass_name}Pass(BaseIRPass):
    """Custom IR pass: {pass_name}"""
    
    def __init__(self):
        self.name = "{pass_name}"
        self.description = "Custom IR pass for {pass_name}"
    
    def apply(self, ir):
        """Apply {pass_name} transformation to IR"""
        # TODO: Implement IR transformation logic
        # Your IR transformation implementation here
        return ir
    
    def get_name(self):
        """Get pass name"""
        return self.name
    
    def get_description(self):
        """Get pass description"""
        return self.description

# IR pass information
IRPassInfo = IRPassInfo(
    name="{pass_name}",
    description="Custom IR pass for {pass_name}",
    pass_class="{pass_name}Pass",
    dependencies=[],
    optimization_level=1
)
'''
        return template
    
    def create_backend_target_template(self, target_name: str) -> str:
        """Create a template for a new backend target"""
        template = f'''#!/usr/bin/env python3
"""
Custom Backend Target: {target_name}
Generated by Extensible Compiler System
"""

from extensible_compiler_system import BackendTargetInfo, TargetType, BaseCodeGenerator

class {target_name}CodeGenerator(BaseCodeGenerator):
    """Custom code generator for {target_name}"""
    
    def __init__(self):
        self.name = "{target_name}"
        self.description = "Custom code generator for {target_name}"
    
    def generate(self, ir, target_info):
        """Generate code for {target_name} target"""
        # TODO: Implement code generation logic
        # Your code generation implementation here
        return generated_code
    
    def get_supported_targets(self):
        """Get supported target types"""
        return [TargetType.EXECUTABLE]

# Backend target information
BackendTargetInfo = BackendTargetInfo(
    name="{target_name}",
    target_type=TargetType.EXECUTABLE,
    description="Custom backend target for {target_name}",
    codegen_class="{target_name}CodeGenerator",
    file_extension=".exe",
    platform="custom"
)
'''
        return template

class ExtensibleCompilerGUI:
    """GUI for managing extensible compiler components"""
    
    def __init__(self, compiler_system: ExtensibleCompilerSystem):
        self.compiler_system = compiler_system
        self.root = tk.Tk()
        self.root.title("🔧 Extensible Compiler System")
        self.root.geometry("1000x700")
        
        self.setup_ui()
    
    def setup_ui(self):
        """Setup the GUI"""
        # Main notebook
        notebook = ttk.Notebook(self.root)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Languages tab
        languages_frame = ttk.Frame(notebook)
        notebook.add(languages_frame, text="🌐 Languages")
        self.create_languages_tab(languages_frame)
        
        # IR Passes tab
        passes_frame = ttk.Frame(notebook)
        notebook.add(passes_frame, text="⚙️ IR Passes")
        self.create_ir_passes_tab(passes_frame)
        
        # Backend Targets tab
        targets_frame = ttk.Frame(notebook)
        notebook.add(targets_frame, text="🎯 Backend Targets")
        self.create_backend_targets_tab(targets_frame)
        
        # Custom Components tab
        custom_frame = ttk.Frame(notebook)
        notebook.add(custom_frame, text="🔧 Custom Components")
        self.create_custom_components_tab(custom_frame)
    
    def create_languages_tab(self, parent):
        """Create languages management tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🌐 Language Parsers", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Add Custom Language", 
                  command=self.add_custom_language).pack(side=tk.RIGHT)
        
        # Languages list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Treeview for languages
        columns = ('Name', 'Extension', 'Type', 'Description')
        self.languages_tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=15)
        
        for col in columns:
            self.languages_tree.heading(col, text=col)
            self.languages_tree.column(col, width=150)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.languages_tree.yview)
        self.languages_tree.configure(yscrollcommand=scrollbar.set)
        
        self.languages_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Load languages
        self.refresh_languages_list()
    
    def create_ir_passes_tab(self, parent):
        """Create IR passes management tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="⚙️ IR Optimization Passes", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Add Custom IR Pass", 
                  command=self.add_custom_ir_pass).pack(side=tk.RIGHT)
        
        # IR passes list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Treeview for IR passes
        columns = ('Name', 'Description', 'Optimization Level', 'Dependencies')
        self.passes_tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=15)
        
        for col in columns:
            self.passes_tree.heading(col, text=col)
            self.passes_tree.column(col, width=150)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.passes_tree.yview)
        self.passes_tree.configure(yscrollcommand=scrollbar.set)
        
        self.passes_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Load IR passes
        self.refresh_ir_passes_list()
    
    def create_backend_targets_tab(self, parent):
        """Create backend targets management tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🎯 Backend Targets", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        ttk.Button(header_frame, text="➕ Add Custom Target", 
                  command=self.add_custom_backend_target).pack(side=tk.RIGHT)
        
        # Backend targets list
        list_frame = ttk.Frame(parent)
        list_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Treeview for backend targets
        columns = ('Name', 'Type', 'Platform', 'Extension', 'Description')
        self.targets_tree = ttk.Treeview(list_frame, columns=columns, show='headings', height=15)
        
        for col in columns:
            self.targets_tree.heading(col, text=col)
            self.targets_tree.column(col, width=150)
        
        # Scrollbar
        scrollbar = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.targets_tree.yview)
        self.targets_tree.configure(yscrollcommand=scrollbar.set)
        
        self.targets_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        scrollbar.pack(side=tk.RIGHT, fill=tk.Y)
        
        # Load backend targets
        self.refresh_backend_targets_list()
    
    def create_custom_components_tab(self, parent):
        """Create custom components management tab"""
        # Header
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(header_frame, text="🔧 Custom Components", 
                 font=('Segoe UI', 16, 'bold')).pack(side=tk.LEFT)
        
        # Custom components info
        info_frame = ttk.LabelFrame(parent, text="Custom Components Information", padding="10")
        info_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Label(info_frame, text="Custom Languages: " + str(len(self.compiler_system.custom_parsers))).pack(anchor=tk.W)
        ttk.Label(info_frame, text="Custom IR Passes: " + str(len(self.compiler_system.custom_passes))).pack(anchor=tk.W)
        ttk.Label(info_frame, text="Custom Backend Targets: " + str(len(self.compiler_system.custom_targets))).pack(anchor=tk.W)
        
        # Template generation
        template_frame = ttk.LabelFrame(parent, text="Generate Templates", padding="10")
        template_frame.pack(fill=tk.X, padx=10, pady=10)
        
        ttk.Button(template_frame, text="🌐 Generate Language Template", 
                  command=self.generate_language_template).pack(side=tk.LEFT, padx=5)
        ttk.Button(template_frame, text="⚙️ Generate IR Pass Template", 
                  command=self.generate_ir_pass_template).pack(side=tk.LEFT, padx=5)
        ttk.Button(template_frame, text="🎯 Generate Backend Target Template", 
                  command=self.generate_backend_target_template).pack(side=tk.LEFT, padx=5)
    
    def refresh_languages_list(self):
        """Refresh languages list"""
        for item in self.languages_tree.get_children():
            self.languages_tree.delete(item)
        
        for lang_name, lang_info in self.compiler_system.languages.items():
            self.languages_tree.insert('', 'end', values=(
                lang_info.name,
                lang_info.extension,
                lang_info.language_type.value,
                lang_info.description
            ))
    
    def refresh_ir_passes_list(self):
        """Refresh IR passes list"""
        for item in self.passes_tree.get_children():
            self.passes_tree.delete(item)
        
        for pass_name, pass_info in self.compiler_system.ir_passes.items():
            self.passes_tree.insert('', 'end', values=(
                pass_info.name,
                pass_info.description,
                pass_info.optimization_level,
                ', '.join(pass_info.dependencies)
            ))
    
    def refresh_backend_targets_list(self):
        """Refresh backend targets list"""
        for item in self.targets_tree.get_children():
            self.targets_tree.delete(item)
        
        for target_name, target_info in self.compiler_system.backend_targets.items():
            self.targets_tree.insert('', 'end', values=(
                target_info.name,
                target_info.target_type.value,
                target_info.platform,
                target_info.file_extension,
                target_info.description
            ))
    
    def add_custom_language(self):
        """Add custom language dialog"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Custom Language")
        dialog.geometry("500x400")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Language form
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="Language Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="File Extension:").pack(anchor=tk.W)
        extension_entry = ttk.Entry(form_frame, width=30)
        extension_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Description:").pack(anchor=tk.W)
        description_entry = ttk.Entry(form_frame, width=30)
        description_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Language Type:").pack(anchor=tk.W)
        type_combo = ttk.Combobox(form_frame, values=[t.value for t in LanguageType], state="readonly")
        type_combo.pack(fill=tk.X, pady=(0, 10))
        
        # Buttons
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        def save_language():
            name = name_entry.get()
            extension = extension_entry.get()
            description = description_entry.get()
            lang_type = LanguageType(type_combo.get())
            
            if name and extension and description:
                lang_info = LanguageInfo(
                    name=name,
                    extension=extension,
                    language_type=lang_type,
                    description=description,
                    keywords=[],
                    operators=[],
                    delimiters=[],
                    parser_class=f"{name}Parser",
                    lexer_class=f"{name}Lexer"
                )
                
                self.compiler_system.add_custom_language(lang_info)
                self.refresh_languages_list()
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please fill in all fields")
        
        ttk.Button(button_frame, text="💾 Save", command=save_language).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def add_custom_ir_pass(self):
        """Add custom IR pass dialog"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Custom IR Pass")
        dialog.geometry("500x400")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # IR pass form
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="IR Pass Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Description:").pack(anchor=tk.W)
        description_entry = ttk.Entry(form_frame, width=30)
        description_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Optimization Level:").pack(anchor=tk.W)
        level_entry = ttk.Entry(form_frame, width=30)
        level_entry.insert(0, "1")
        level_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Buttons
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        def save_ir_pass():
            name = name_entry.get()
            description = description_entry.get()
            level = int(level_entry.get())
            
            if name and description:
                pass_info = IRPassInfo(
                    name=name,
                    description=description,
                    pass_class=f"{name}Pass",
                    dependencies=[],
                    optimization_level=level
                )
                
                self.compiler_system.add_custom_ir_pass(pass_info)
                self.refresh_ir_passes_list()
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please fill in all fields")
        
        ttk.Button(button_frame, text="💾 Save", command=save_ir_pass).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def add_custom_backend_target(self):
        """Add custom backend target dialog"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Add Custom Backend Target")
        dialog.geometry("500x400")
        dialog.transient(self.root)
        dialog.grab_set()
        
        # Backend target form
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="Target Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Description:").pack(anchor=tk.W)
        description_entry = ttk.Entry(form_frame, width=30)
        description_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Target Type:").pack(anchor=tk.W)
        type_combo = ttk.Combobox(form_frame, values=[t.value for t in TargetType], state="readonly")
        type_combo.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="File Extension:").pack(anchor=tk.W)
        extension_entry = ttk.Entry(form_frame, width=30)
        extension_entry.pack(fill=tk.X, pady=(0, 10))
        
        ttk.Label(form_frame, text="Platform:").pack(anchor=tk.W)
        platform_entry = ttk.Entry(form_frame, width=30)
        platform_entry.pack(fill=tk.X, pady=(0, 10))
        
        # Buttons
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        def save_backend_target():
            name = name_entry.get()
            description = description_entry.get()
            target_type = TargetType(type_combo.get())
            extension = extension_entry.get()
            platform = platform_entry.get()
            
            if name and description and extension and platform:
                target_info = BackendTargetInfo(
                    name=name,
                    target_type=target_type,
                    description=description,
                    codegen_class=f"{name}CodeGenerator",
                    file_extension=extension,
                    platform=platform
                )
                
                self.compiler_system.add_custom_backend_target(target_info)
                self.refresh_backend_targets_list()
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please fill in all fields")
        
        ttk.Button(button_frame, text="💾 Save", command=save_backend_target).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def generate_language_template(self):
        """Generate language template"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Generate Language Template")
        dialog.geometry("400x200")
        dialog.transient(self.root)
        dialog.grab_set()
        
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="Language Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        def generate_template():
            name = name_entry.get()
            if name:
                template = self.compiler_system.create_language_template(name)
                
                # Save template to file
                filename = f"{name.lower()}_parser.py"
                with open(filename, 'w') as f:
                    f.write(template)
                
                messagebox.showinfo("Success", f"Template generated: {filename}")
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please enter a language name")
        
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        ttk.Button(button_frame, text="🔧 Generate", command=generate_template).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def generate_ir_pass_template(self):
        """Generate IR pass template"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Generate IR Pass Template")
        dialog.geometry("400x200")
        dialog.transient(self.root)
        dialog.grab_set()
        
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="IR Pass Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        def generate_template():
            name = name_entry.get()
            if name:
                template = self.compiler_system.create_ir_pass_template(name)
                
                # Save template to file
                filename = f"{name.lower()}_pass.py"
                with open(filename, 'w') as f:
                    f.write(template)
                
                messagebox.showinfo("Success", f"Template generated: {filename}")
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please enter an IR pass name")
        
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        ttk.Button(button_frame, text="🔧 Generate", command=generate_template).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def generate_backend_target_template(self):
        """Generate backend target template"""
        dialog = tk.Toplevel(self.root)
        dialog.title("Generate Backend Target Template")
        dialog.geometry("400x200")
        dialog.transient(self.root)
        dialog.grab_set()
        
        form_frame = ttk.Frame(dialog, padding="20")
        form_frame.pack(fill=tk.BOTH, expand=True)
        
        ttk.Label(form_frame, text="Target Name:").pack(anchor=tk.W)
        name_entry = ttk.Entry(form_frame, width=30)
        name_entry.pack(fill=tk.X, pady=(0, 10))
        
        def generate_template():
            name = name_entry.get()
            if name:
                template = self.compiler_system.create_backend_target_template(name)
                
                # Save template to file
                filename = f"{name.lower()}_target.py"
                with open(filename, 'w') as f:
                    f.write(template)
                
                messagebox.showinfo("Success", f"Template generated: {filename}")
                dialog.destroy()
            else:
                messagebox.showerror("Error", "Please enter a target name")
        
        button_frame = ttk.Frame(form_frame)
        button_frame.pack(fill=tk.X, pady=(20, 0))
        
        ttk.Button(button_frame, text="🔧 Generate", command=generate_template).pack(side=tk.LEFT, padx=(0, 10))
        ttk.Button(button_frame, text="❌ Cancel", command=dialog.destroy).pack(side=tk.RIGHT)
    
    def run(self):
        """Run the GUI"""
        self.root.mainloop()

def test_extensible_compiler_system():
    """Test the extensible compiler system"""
    print("🧪 Testing Extensible Compiler System...")
    
    # Create compiler system
    compiler_system = ExtensibleCompilerSystem()
    
    # Test built-in components
    print(f"✅ Loaded {len(compiler_system.languages)} languages")
    print(f"✅ Loaded {len(compiler_system.ir_passes)} IR passes")
    print(f"✅ Loaded {len(compiler_system.backend_targets)} backend targets")
    
    # Test custom components
    print(f"✅ Loaded {len(compiler_system.custom_parsers)} custom parsers")
    print(f"✅ Loaded {len(compiler_system.custom_passes)} custom IR passes")
    print(f"✅ Loaded {len(compiler_system.custom_targets)} custom backend targets")
    
    # List components
    print("\n🌐 Available Languages:")
    for lang in compiler_system.list_languages():
        print(f"  - {lang}")
    
    print("\n⚙️ Available IR Passes:")
    for pass_name in compiler_system.list_ir_passes():
        print(f"  - {pass_name}")
    
    print("\n🎯 Available Backend Targets:")
    for target in compiler_system.list_backend_targets():
        print(f"  - {target}")
    
    print("\n✅ Extensible Compiler System test completed!")
    
    return True

if __name__ == "__main__":
    # Test the system
    test_extensible_compiler_system()
    
    # Create and run GUI
    compiler_system = ExtensibleCompilerSystem()
    gui = ExtensibleCompilerGUI(compiler_system)
    gui.run()
