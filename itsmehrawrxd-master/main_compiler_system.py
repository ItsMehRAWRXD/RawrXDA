#!/usr/bin/env python3
"""
Main Extensible Compiler System
Core system for managing language parsers, IR passes, and code generators
"""

import os
import sys
import json
import importlib
import inspect
from abc import ABC, abstractmethod
from typing import Dict, List, Any, Optional
from dataclasses import dataclass, field
from enum import Enum
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
import collections

# --- Base Classes and Data Structures ---

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
    def apply(self, ir: Any, optimization_level: int) -> Any:
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

# --- Custom Exception ---

class CompilerException(Exception):
    def __init__(self, message, stage="unknown"):
        super().__init__(f"[{stage.upper()}] {message}")
        self.stage = stage

# --- Main Compiler System ---

class ExtensibleCompilerSystem:
    """Main system for managing extensible compiler components"""
    
    def __init__(self):
        self.languages: Dict[str, LanguageInfo] = {}
        self.ir_passes: Dict[str, IRPassInfo] = {}
        self.backend_targets: Dict[str, BackendTargetInfo] = {}
        self.custom_parsers: Dict[str, type] = {}
        self.custom_passes: Dict[str, type] = {}
        self.custom_targets: Dict[str, type] = {}
        
        # Load components
        self.load_builtin_components()
        self.load_user_components()
        self.load_assembly_components()
        
    def load_builtin_components(self):
        """Load built-in language parsers, IR passes, and targets"""
        self.languages = {
            'cpp': LanguageInfo('C++', '.cpp', LanguageType.COMPILED, 'C++ programming language', 
                              ['int', 'float', 'return', 'if', 'else', 'while', 'for'], 
                              ['+', '-', '*', '/', '=', '==', '!=', '<', '>', '<=', '>='], 
                              [';', '(', ')', '{', '}'], 'CppParser', 'CppLexer'),
            'python': LanguageInfo('Python', '.py', LanguageType.INTERPRETED, 'Python programming language', 
                                  ['def', 'class', 'if', 'else', 'while', 'for', 'return', 'import'], 
                                  ['+', '-', '*', '/', '//', '%', '**', '=', '==', '!=', '<', '>', '<=', '>='], 
                                  [':', '(', ')', '[', ']', '{', '}'], 'PythonParser', 'PythonLexer'),
            'javascript': LanguageInfo('JavaScript', '.js', LanguageType.INTERPRETED, 'JavaScript programming language', 
                                      ['var', 'let', 'const', 'function', 'if', 'else', 'while', 'for', 'return'], 
                                      ['+', '-', '*', '/', '%', '=', '==', '===', '!=', '!==', '<', '>', '<=', '>='], 
                                      [';', '(', ')', '{', '}', '[', ']'], 'JavaScriptParser', 'JavaScriptLexer'),
            'rust': LanguageInfo('Rust', '.rs', LanguageType.COMPILED, 'Rust programming language', 
                                ['fn', 'let', 'mut', 'const', 'if', 'else', 'while', 'for', 'return'], 
                                ['+', '-', '*', '/', '%', '=', '==', '!=', '<', '>', '<=', '>='], 
                                [';', '(', ')', '{', '}'], 'RustParser', 'RustLexer')
        }
        self.ir_passes = {
            'constant_folding': IRPassInfo('Constant Folding', 'Fold constant expressions', 'ASTOptimizerPass', [], 1),
            'dead_code_elimination': IRPassInfo('Dead Code Elimination', 'Remove unreachable code', 'DeadCodeEliminationPass', ['constant_folding'], 2),
        }
        self.backend_targets = {
            'print_ast': BackendTargetInfo('Print AST', TargetType.EXECUTABLE, 'Print the AST to console', 'PrintASTGenerator', '.ast', 'all'),
            'print_ir': BackendTargetInfo('Print IR', TargetType.EXECUTABLE, 'Print the IR to console', 'PrintIRGenerator', '.ir', 'all'),
        }

    def load_user_components(self):
        """Discover and load user-defined components from the 'plugins' directory."""
        plugins_dir = os.path.join(os.path.dirname(__file__), 'plugins')
        if not os.path.isdir(plugins_dir):
            return

        for filename in os.listdir(plugins_dir):
            if filename.endswith(".py") and filename != "__init__.py":
                module_name = filename[:-3]
                try:
                    module = importlib.import_module(f"plugins.{module_name}")
                    self._register_module_components(module)
                except Exception as e:
                    print(f"Error loading plugin {module_name}: {e}", file=sys.stderr)

    def load_assembly_components(self):
        """Load assembly-based components"""
        try:
            from rust_assembly_compiler import RustAssemblyLexer, RustAssemblyParser, LANGUAGE_INFO
            
            # Register assembly-based Rust compiler
            for lang_name, lang_info in LANGUAGE_INFO.items():
                self.languages[lang_name] = lang_info
            
            self.custom_parsers['RustAssemblyLexer'] = RustAssemblyLexer
            self.custom_parsers['RustAssemblyParser'] = RustAssemblyParser
            
            print("✅ Assembly-based Rust compiler loaded successfully")
            
        except ImportError as e:
            print(f"⚠️  Assembly-based Rust compiler not available: {e}")
        except Exception as e:
            print(f"❌ Error loading assembly components: {e}")

    def _register_module_components(self, module):
        """Register components found within a plugin module."""
        for name, obj in inspect.getmembers(module):
            if inspect.isclass(obj):
                if issubclass(obj, BaseLexer) and obj is not BaseLexer:
                    lexer_name = obj.__name__
                    self.custom_parsers[lexer_name] = obj
                elif issubclass(obj, BaseParser) and obj is not BaseParser:
                    parser_name = obj.__name__
                    self.custom_parsers[parser_name] = obj
                elif issubclass(obj, BaseIRPass) and obj is not BaseIRPass:
                    self.custom_passes[name] = obj
                elif issubclass(obj, BaseCodeGenerator) and obj is not BaseCodeGenerator:
                    self.custom_targets[name] = obj

        if hasattr(module, 'LANGUAGE_INFO'):
            for lang_name, lang_info in module.LANGUAGE_INFO.items():
                self.languages[lang_name] = lang_info
        if hasattr(module, 'IR_PASS_INFO'):
            for pass_name, pass_info in module.IR_PASS_INFO.items():
                self.ir_passes[pass_name] = pass_info
        if hasattr(module, 'BACKEND_TARGET_INFO'):
            for target_name, target_info in module.BACKEND_TARGET_INFO.items():
                self.backend_targets[target_name] = target_info

    def get_ordered_passes(self, selected_passes: List[str]) -> List[BaseIRPass]:
        """Topologically sort the selected passes based on dependencies."""
        graph = {name: [] for name in selected_passes}
        in_degree = {name: 0 for name in selected_passes}

        for pass_name in selected_passes:
            info = self.ir_passes.get(pass_name)
            if not info:
                raise CompilerException(f"Unknown IR pass: {pass_name}", "Pass Resolution")
            for dep in info.dependencies:
                if dep in graph:
                    graph[dep].append(pass_name)
                    in_degree[pass_name] += 1
        
        queue = collections.deque([name for name in selected_passes if in_degree[name] == 0])
        ordered_passes = []
        
        while queue:
            node = queue.popleft()
            ordered_passes.append(node)
            for neighbor in graph.get(node, []):
                in_degree[neighbor] -= 1
                if in_degree[neighbor] == 0:
                    queue.append(neighbor)
        
        if len(ordered_passes) != len(selected_passes):
            raise CompilerException("Circular dependency detected in IR passes.", "Pass Resolution")

        return [self._create_pass_instance(name) for name in ordered_passes]

    def _create_pass_instance(self, pass_name: str) -> BaseIRPass:
        """Instantiate an IR pass object."""
        pass_info = self.ir_passes.get(pass_name)
        if not pass_info:
            raise CompilerException(f"IR Pass '{pass_name}' not found.", "Internal")
        pass_class = self.custom_passes.get(pass_info.pass_class)
        if not pass_class:
            raise CompilerException(f"Pass class '{pass_info.pass_class}' not found for '{pass_name}'.", "Internal")
        return pass_class()

    def compile(self, source_code: str, language_name: str, passes: List[str], target_name: str) -> str:
        """Compile source code using a selected pipeline."""
        lang_info = self.languages.get(language_name)
        if not lang_info:
            raise CompilerException(f"Language '{language_name}' not supported.", "Language")

        # Lexing
        try:
            lexer_class = self.custom_parsers[lang_info.lexer_class]
            lexer = lexer_class()
            tokens = lexer.tokenize(source_code)
        except Exception as e:
            raise CompilerException(f"Lexing failed: {e}", "Lexing")

        # Parsing (AST Generation)
        try:
            parser_class = self.custom_parsers[lang_info.parser_class]
            parser = parser_class()
            ast = parser.parse(tokens)
            ir = ast
        except Exception as e:
            raise CompilerException(f"Parsing failed: {e}", "Parsing")
        
        # Intermediate Representation (IR) Passes
        try:
            ordered_passes = self.get_ordered_passes(passes)
            for p in ordered_passes:
                ir = p.apply(ir, self.ir_passes[p.get_name()].optimization_level)
        except Exception as e:
            raise CompilerException(f"IR pass failed: {e}", "IR Pass")

        # Code Generation
        target_info = self.backend_targets.get(target_name)
        if not target_info:
            raise CompilerException(f"Backend target '{target_name}' not supported.", "Backend")
        
        try:
            codegen_class = self.custom_targets[target_info.codegen_class]
            codegen = codegen_class()
            final_code = codegen.generate(ir, target_info)
        except Exception as e:
            raise CompilerException(f"Code generation failed: {e}", "Code Generation")

        return final_code
