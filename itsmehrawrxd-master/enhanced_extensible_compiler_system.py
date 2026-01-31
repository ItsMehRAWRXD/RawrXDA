#!/usr/bin/env python3
"""
Enhanced Extensible Compiler System
Implements advanced dependency management, IR handling, dynamic pipelines, 
cross-compilation, error reporting, optimization levels, and caching
"""

import tkinter as tk
from tkinter import ttk, filedialog, messagebox, scrolledtext
import os
import sys
import json
import hashlib
import threading
import time
from pathlib import Path
from typing import Dict, List, Optional, Any, Callable, Tuple
from dataclasses import dataclass, field
from abc import ABC, abstractmethod
from collections import defaultdict, deque
import yaml

# Custom exception classes
class CompilerException(Exception):
    """Custom exception for compiler-specific errors"""
    
    def __init__(self, message: str, stage: str = "unknown", line: int = 0, column: int = 0):
        super().__init__(message)
        self.stage = stage
        self.line = line
        self.column = column

class CircularDependencyError(CompilerException):
    """Exception raised when circular dependencies are detected"""
    pass

# Enhanced data structures
@dataclass
class CompilationResult:
    """Structured result of compilation process"""
    ast: Any = None
    ir: Any = None
    target_code: str = ""
    metadata: Dict[str, Any] = field(default_factory=dict)
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)
    compilation_time: float = 0.0
    optimization_level: int = 0

@dataclass
class IRPassInfo:
    """Information about an IR pass"""
    name: str
    description: str
    dependencies: List[str] = field(default_factory=list)
    optimization_levels: List[int] = field(default_factory=lambda: [0, 1, 2, 3])
    platform_specific: bool = False
    target_platforms: List[str] = field(default_factory=list)

@dataclass
class BackendTargetInfo:
    """Information about backend targets"""
    name: str
    description: str
    platform: str
    architecture: str
    file_extension: str
    cross_compile_support: bool = False
    supported_platforms: List[str] = field(default_factory=list)

class BaseIRPass(ABC):
    """Base class for IR passes with enhanced functionality"""
    
    def __init__(self, optimization_level: int = 0):
        self.optimization_level = optimization_level
    
    @abstractmethod
    def apply(self, ir: Any, optimization_level: int = 0) -> Any:
        """Apply the pass to the IR"""
        pass
    
    def get_dependencies(self) -> List[str]:
        """Get list of pass dependencies"""
        return []
    
    def supports_optimization_level(self, level: int) -> bool:
        """Check if pass supports given optimization level"""
        return True
    
    def is_platform_specific(self) -> bool:
        """Check if pass is platform-specific"""
        return False
    
    def get_target_platforms(self) -> List[str]:
        """Get supported target platforms"""
        return []

class BaseCodeGenerator(ABC):
    """Base class for code generators with cross-compilation support"""
    
    @abstractmethod
    def generate(self, ir: Any, target_platform: str = None) -> str:
        """Generate code from IR"""
        pass
    
    def cross_generate(self, ir: Any, source_platform: str, target_platform: str) -> str:
        """Generate code for a different platform than the source"""
        # Default implementation - can be overridden by specific generators
        if source_platform == target_platform:
            return self.generate(ir, target_platform)
        else:
            # Implement cross-compilation logic
            return self._cross_compile_implementation(ir, source_platform, target_platform)
    
    def _cross_compile_implementation(self, ir: Any, source_platform: str, target_platform: str) -> str:
        """Default cross-compilation implementation"""
        # This would be implemented by specific generators
        raise NotImplementedError(f"Cross-compilation from {source_platform} to {target_platform} not supported")

class EnhancedExtensibleCompilerSystem:
    """Enhanced extensible compiler system with advanced features"""
    
    def __init__(self):
        self.languages = {}
        self.ir_passes = {}
        self.backend_targets = {}
        self.cache = {}
        self.cache_enabled = True
        self.optimization_level = 0
        
        # Initialize default components
        self._initialize_default_components()
        
        print("Enhanced Extensible Compiler System initialized")
    
    def _initialize_default_components(self):
        """Initialize default language, passes, and targets"""
        
        # Default languages
        self.languages = {
            'python': {'name': 'Python', 'extensions': ['.py']},
            'cpp': {'name': 'C++', 'extensions': ['.cpp', '.cxx', '.cc', '.c']},
            'javascript': {'name': 'JavaScript', 'extensions': ['.js', '.ts']},
            'java': {'name': 'Java', 'extensions': ['.java']},
            'rust': {'name': 'Rust', 'extensions': ['.rs']},
            'go': {'name': 'Go', 'extensions': ['.go']}
        }
        
        # Default IR passes
        self.ir_passes = {
            'lexical_analysis': IRPassInfo(
                name='Lexical Analysis',
                description='Tokenize source code',
                dependencies=[],
                optimization_levels=[0, 1, 2, 3]
            ),
            'syntax_analysis': IRPassInfo(
                name='Syntax Analysis',
                description='Parse tokens into AST',
                dependencies=['lexical_analysis'],
                optimization_levels=[0, 1, 2, 3]
            ),
            'semantic_analysis': IRPassInfo(
                name='Semantic Analysis',
                description='Type checking and symbol resolution',
                dependencies=['syntax_analysis'],
                optimization_levels=[0, 1, 2, 3]
            ),
            'constant_folding': IRPassInfo(
                name='Constant Folding',
                description='Fold constant expressions',
                dependencies=['semantic_analysis'],
                optimization_levels=[1, 2, 3]
            ),
            'dead_code_elimination': IRPassInfo(
                name='Dead Code Elimination',
                description='Remove unreachable code',
                dependencies=['constant_folding'],
                optimization_levels=[2, 3]
            ),
            'register_allocation': IRPassInfo(
                name='Register Allocation',
                description='Allocate registers for variables',
                dependencies=['dead_code_elimination'],
                optimization_levels=[1, 2, 3],
                platform_specific=True,
                target_platforms=['x86_64', 'arm64', 'riscv']
            )
        }
        
        # Default backend targets
        self.backend_targets = {
            'x86_64_elf': BackendTargetInfo(
                name='x86-64 ELF',
                description='Linux x86-64 executable',
                platform='linux',
                architecture='x86_64',
                file_extension='.elf',
                cross_compile_support=True,
                supported_platforms=['linux', 'windows', 'macos']
            ),
            'x86_64_pe': BackendTargetInfo(
                name='x86-64 PE',
                description='Windows x86-64 executable',
                platform='windows',
                architecture='x86_64',
                file_extension='.exe',
                cross_compile_support=True,
                supported_platforms=['windows', 'linux', 'macos']
            ),
            'arm64_macho': BackendTargetInfo(
                name='ARM64 Mach-O',
                description='macOS ARM64 executable',
                platform='macos',
                architecture='arm64',
                file_extension='.macho',
                cross_compile_support=True,
                supported_platforms=['macos', 'linux', 'windows']
            ),
            'wasm': BackendTargetInfo(
                name='WebAssembly',
                description='WebAssembly binary',
                platform='web',
                architecture='wasm',
                file_extension='.wasm',
                cross_compile_support=True,
                supported_platforms=['web', 'linux', 'windows', 'macos']
            )
        }
    
    def get_ordered_passes(self, selected_passes: List[str]) -> List[BaseIRPass]:
        """Get passes in dependency order using topological sorting"""
        
        try:
            # Build dependency graph
            graph = defaultdict(list)
            in_degree = defaultdict(int)
            
            # Initialize graph
            for pass_name in selected_passes:
                if pass_name not in self.ir_passes:
                    raise CompilerException(f"Unknown pass: {pass_name}", "pass_selection")
                
                pass_info = self.ir_passes[pass_name]
                in_degree[pass_name] = len(pass_info.dependencies)
                
                for dep in pass_info.dependencies:
                    if dep in selected_passes:
                        graph[dep].append(pass_name)
            
            # Topological sort using Kahn's algorithm
            queue = deque([pass_name for pass_name in selected_passes if in_degree[pass_name] == 0])
            ordered_passes = []
            
            while queue:
                current_pass = queue.popleft()
                ordered_passes.append(current_pass)
                
                for dependent in graph[current_pass]:
                    in_degree[dependent] -= 1
                    if in_degree[dependent] == 0:
                        queue.append(dependent)
            
            # Check for circular dependencies
            if len(ordered_passes) != len(selected_passes):
                remaining = set(selected_passes) - set(ordered_passes)
                raise CircularDependencyError(f"Circular dependency detected in passes: {remaining}")
            
            return ordered_passes
            
        except Exception as e:
            if isinstance(e, (CompilerException, CircularDependencyError)):
                raise
            else:
                raise CompilerException(f"Error ordering passes: {e}", "pass_ordering")
    
    def compile(self, source_code: str, language_name: str, passes: List[str], target_name: str) -> CompilationResult:
        """Enhanced compile method with structured results"""
        
        start_time = time.time()
        result = CompilationResult()
        result.optimization_level = self.optimization_level
        
        try:
            # Check cache first
            cache_key = self._generate_cache_key(source_code, language_name, passes, target_name)
            if self.cache_enabled and cache_key in self.cache:
                cached_result = self.cache[cache_key]
                result.target_code = cached_result['target_code']
                result.metadata['cached'] = True
                result.compilation_time = time.time() - start_time
                return result
            
            # Stage 1: Lexical Analysis
            try:
                result.ast = self._lexical_analysis(source_code, language_name)
                result.metadata['lexical_analysis'] = 'success'
            except Exception as e:
                raise CompilerException(f"Lexical analysis failed: {e}", "lexing")
            
            # Stage 2: Syntax Analysis
            try:
                result.ast = self._syntax_analysis(result.ast, language_name)
                result.metadata['syntax_analysis'] = 'success'
            except Exception as e:
                raise CompilerException(f"Syntax analysis failed: {e}", "parsing")
            
            # Stage 3: Apply IR passes in order
            try:
                ordered_passes = self.get_ordered_passes(passes)
                result.ir = result.ast  # Start with AST
                
                for pass_name in ordered_passes:
                    pass_instance = self._get_pass_instance(pass_name)
                    result.ir = pass_instance.apply(result.ir, self.optimization_level)
                    result.metadata[f'pass_{pass_name}'] = 'success'
                
            except Exception as e:
                raise CompilerException(f"IR pass execution failed: {e}", "ir_passes")
            
            # Stage 4: Code Generation
            try:
                result.target_code = self._code_generation(result.ir, target_name)
                result.metadata['code_generation'] = 'success'
            except Exception as e:
                raise CompilerException(f"Code generation failed: {e}", "codegen")
            
            # Cache result
            if self.cache_enabled:
                self.cache[cache_key] = {
                    'target_code': result.target_code,
                    'metadata': result.metadata,
                    'timestamp': time.time()
                }
            
            result.compilation_time = time.time() - start_time
            return result
            
        except CompilerException:
            raise
        except Exception as e:
            raise CompilerException(f"Compilation failed: {e}", "unknown")
    
    def create_pipeline(self, config_file: str) -> Callable:
        """Create a compilation pipeline from configuration file"""
        
        try:
            with open(config_file, 'r') as f:
                if config_file.endswith('.yaml') or config_file.endswith('.yml'):
                    config = yaml.safe_load(f)
                else:
                    config = json.load(f)
            
            # Extract pipeline configuration
            language = config.get('language', 'python')
            passes = config.get('passes', [])
            target = config.get('target', 'x86_64_elf')
            optimization_level = config.get('optimization_level', 0)
            
            # Create pipeline function
            def pipeline(source_code: str) -> CompilationResult:
                self.optimization_level = optimization_level
                return self.compile(source_code, language, passes, target)
            
            return pipeline
            
        except Exception as e:
            raise CompilerException(f"Failed to create pipeline from {config_file}: {e}", "pipeline_creation")
    
    def compile_with_cache(self, source_code: str, language_name: str, passes: List[str], target_name: str) -> str:
        """Compile with caching enabled"""
        
        result = self.compile(source_code, language_name, passes, target_name)
        return result.target_code
    
    def _generate_cache_key(self, source_code: str, language_name: str, passes: List[str], target_name: str) -> str:
        """Generate cache key for compilation"""
        
        key_data = {
            'source': source_code,
            'language': language_name,
            'passes': sorted(passes),
            'target': target_name,
            'optimization_level': self.optimization_level
        }
        
        key_string = json.dumps(key_data, sort_keys=True)
        return hashlib.md5(key_string.encode()).hexdigest()
    
    def _lexical_analysis(self, source_code: str, language_name: str) -> Any:
        """Perform lexical analysis"""
        # Simplified implementation
        return {'tokens': source_code.split(), 'language': language_name}
    
    def _syntax_analysis(self, ast: Any, language_name: str) -> Any:
        """Perform syntax analysis"""
        # Simplified implementation
        return {'ast': ast, 'language': language_name}
    
    def _get_pass_instance(self, pass_name: str) -> BaseIRPass:
        """Get instance of IR pass"""
        # This would return actual pass instances
        # For now, return a mock implementation
        class MockPass(BaseIRPass):
            def apply(self, ir: Any, optimization_level: int = 0) -> Any:
                return ir
        
        return MockPass()
    
    def _code_generation(self, ir: Any, target_name: str) -> str:
        """Generate target code from IR"""
        # Simplified implementation
        return f"Generated code for {target_name}"
    
    def set_optimization_level(self, level: int):
        """Set optimization level"""
        if 0 <= level <= 3:
            self.optimization_level = level
        else:
            raise ValueError("Optimization level must be between 0 and 3")
    
    def enable_caching(self, enabled: bool = True):
        """Enable or disable caching"""
        self.cache_enabled = enabled
    
    def clear_cache(self):
        """Clear compilation cache"""
        self.cache.clear()
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        return {
            'entries': len(self.cache),
            'enabled': self.cache_enabled,
            'memory_usage': sum(len(str(v)) for v in self.cache.values())
        }
    
    def show_enhanced_compiler_gui(self):
        """Show enhanced compiler GUI with all new features"""
        
        gui_window = tk.Toplevel()
        gui_window.title("Enhanced Extensible Compiler System")
        gui_window.geometry("1200x800")
        gui_window.configure(bg='#1e1e1e')
        
        # Create notebook for tabs
        notebook = ttk.Notebook(gui_window)
        notebook.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)
        
        # Compilation tab
        compilation_frame = ttk.Frame(notebook)
        notebook.add(compilation_frame, text="Compilation")
        self._create_compilation_tab(compilation_frame)
        
        # Pipeline tab
        pipeline_frame = ttk.Frame(notebook)
        notebook.add(pipeline_frame, text="Pipeline Configuration")
        self._create_pipeline_tab(pipeline_frame)
        
        # Cache tab
        cache_frame = ttk.Frame(notebook)
        notebook.add(cache_frame, text="Cache Management")
        self._create_cache_tab(cache_frame)
        
        # Optimization tab
        optimization_frame = ttk.Frame(notebook)
        notebook.add(optimization_frame, text="Optimization")
        self._create_optimization_tab(optimization_frame)
        
        # Cross-compilation tab
        cross_compile_frame = ttk.Frame(notebook)
        notebook.add(cross_compile_frame, text="Cross-Compilation")
        self._create_cross_compile_tab(cross_compile_frame)
    
    def _create_compilation_tab(self, parent):
        """Create compilation tab"""
        
        # Source code input
        source_frame = ttk.LabelFrame(parent, text="Source Code")
        source_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.source_text = scrolledtext.ScrolledText(source_frame, height=10)
        self.source_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Configuration frame
        config_frame = ttk.LabelFrame(parent, text="Configuration")
        config_frame.pack(fill=tk.X, padx=5, pady=5)
        
        # Language selection
        ttk.Label(config_frame, text="Language:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.language_var = tk.StringVar(value='python')
        language_combo = ttk.Combobox(config_frame, textvariable=self.language_var)
        language_combo['values'] = list(self.languages.keys())
        language_combo.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        # Target selection
        ttk.Label(config_frame, text="Target:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.target_var = tk.StringVar(value='x86_64_elf')
        target_combo = ttk.Combobox(config_frame, textvariable=self.target_var)
        target_combo['values'] = list(self.backend_targets.keys())
        target_combo.grid(row=0, column=3, sticky=tk.W, padx=5)
        
        # Optimization level
        ttk.Label(config_frame, text="Optimization:").grid(row=1, column=0, sticky=tk.W, padx=5)
        self.optimization_var = tk.IntVar(value=0)
        optimization_scale = ttk.Scale(config_frame, from_=0, to=3, variable=self.optimization_var, orient=tk.HORIZONTAL)
        optimization_scale.grid(row=1, column=1, sticky=tk.W, padx=5)
        
        # Pass selection
        pass_frame = ttk.LabelFrame(parent, text="IR Passes")
        pass_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.pass_vars = {}
        for i, (pass_name, pass_info) in enumerate(self.ir_passes.items()):
            var = tk.BooleanVar()
            self.pass_vars[pass_name] = var
            check = ttk.Checkbutton(pass_frame, text=f"{pass_name}: {pass_info.description}", variable=var)
            check.grid(row=i//2, column=i%2, sticky=tk.W, padx=5)
        
        # Control buttons
        control_frame = ttk.Frame(parent)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(control_frame, text="Compile", command=self._compile_code).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Load Pipeline", command=self._load_pipeline).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Save Pipeline", command=self._save_pipeline).pack(side=tk.LEFT, padx=5)
        
        # Output
        output_frame = ttk.LabelFrame(parent, text="Output")
        output_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.output_text = scrolledtext.ScrolledText(output_frame, height=8)
        self.output_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
    
    def _create_pipeline_tab(self, parent):
        """Create pipeline configuration tab"""
        
        # Pipeline editor
        editor_frame = ttk.LabelFrame(parent, text="Pipeline Configuration")
        editor_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.pipeline_text = scrolledtext.ScrolledText(editor_frame, height=15)
        self.pipeline_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Load default pipeline
        default_pipeline = {
            'language': 'python',
            'passes': ['lexical_analysis', 'syntax_analysis', 'semantic_analysis', 'constant_folding'],
            'target': 'x86_64_elf',
            'optimization_level': 2
        }
        self.pipeline_text.insert(1.0, json.dumps(default_pipeline, indent=2))
        
        # Pipeline controls
        control_frame = ttk.Frame(parent)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(control_frame, text="Load Pipeline", command=self._load_pipeline_config).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Save Pipeline", command=self._save_pipeline_config).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Validate Pipeline", command=self._validate_pipeline).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Test Pipeline", command=self._test_pipeline).pack(side=tk.LEFT, padx=5)
    
    def _create_cache_tab(self, parent):
        """Create cache management tab"""
        
        # Cache statistics
        stats_frame = ttk.LabelFrame(parent, text="Cache Statistics")
        stats_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.cache_stats_text = scrolledtext.ScrolledText(stats_frame, height=8)
        self.cache_stats_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Cache controls
        control_frame = ttk.Frame(parent)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(control_frame, text="Refresh Stats", command=self._refresh_cache_stats).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Clear Cache", command=self._clear_cache).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Enable Cache", command=lambda: self.enable_caching(True)).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Disable Cache", command=lambda: self.enable_caching(False)).pack(side=tk.LEFT, padx=5)
        
        # Initialize stats
        self._refresh_cache_stats()
    
    def _create_optimization_tab(self, parent):
        """Create optimization tab"""
        
        # Optimization level selector
        level_frame = ttk.LabelFrame(parent, text="Optimization Level")
        level_frame.pack(fill=tk.X, padx=5, pady=5)
        
        self.opt_level_var = tk.IntVar(value=0)
        level_scale = ttk.Scale(level_frame, from_=0, to=3, variable=self.opt_level_var, orient=tk.HORIZONTAL)
        level_scale.pack(fill=tk.X, padx=5, pady=5)
        
        level_label = ttk.Label(level_frame, text="Level 0: No optimization")
        level_label.pack(pady=5)
        
        # Optimization descriptions
        desc_frame = ttk.LabelFrame(parent, text="Optimization Descriptions")
        desc_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        descriptions = [
            "Level 0: No optimization - Fastest compilation",
            "Level 1: Basic optimization - Constant folding, simple optimizations",
            "Level 2: Standard optimization - Dead code elimination, register allocation",
            "Level 3: Aggressive optimization - All optimizations, may increase compilation time"
        ]
        
        for desc in descriptions:
            ttk.Label(desc_frame, text=desc).pack(anchor=tk.W, padx=5, pady=2)
    
    def _create_cross_compile_tab(self, parent):
        """Create cross-compilation tab"""
        
        # Platform selection
        platform_frame = ttk.LabelFrame(parent, text="Platform Selection")
        platform_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Label(platform_frame, text="Source Platform:").grid(row=0, column=0, sticky=tk.W, padx=5)
        self.source_platform_var = tk.StringVar(value='linux')
        source_combo = ttk.Combobox(platform_frame, textvariable=self.source_platform_var)
        source_combo['values'] = ['linux', 'windows', 'macos', 'web']
        source_combo.grid(row=0, column=1, sticky=tk.W, padx=5)
        
        ttk.Label(platform_frame, text="Target Platform:").grid(row=0, column=2, sticky=tk.W, padx=5)
        self.target_platform_var = tk.StringVar(value='windows')
        target_combo = ttk.Combobox(platform_frame, textvariable=self.target_platform_var)
        target_combo['values'] = ['linux', 'windows', 'macos', 'web']
        target_combo.grid(row=0, column=3, sticky=tk.W, padx=5)
        
        # Cross-compilation info
        info_frame = ttk.LabelFrame(parent, text="Cross-Compilation Information")
        info_frame.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        self.cross_compile_text = scrolledtext.ScrolledText(info_frame, height=10)
        self.cross_compile_text.pack(fill=tk.BOTH, expand=True, padx=5, pady=5)
        
        # Cross-compilation controls
        control_frame = ttk.Frame(parent)
        control_frame.pack(fill=tk.X, padx=5, pady=5)
        
        ttk.Button(control_frame, text="Check Compatibility", command=self._check_cross_compile_compatibility).pack(side=tk.LEFT, padx=5)
        ttk.Button(control_frame, text="Generate Cross-Compile", command=self._generate_cross_compile).pack(side=tk.LEFT, padx=5)
    
    def _compile_code(self):
        """Compile source code"""
        try:
            source_code = self.source_text.get(1.0, tk.END).strip()
            if not source_code:
                messagebox.showerror("Error", "Please enter source code")
                return
            
            language = self.language_var.get()
            target = self.target_var.get()
            optimization_level = self.optimization_var.get()
            
            # Get selected passes
            selected_passes = [name for name, var in self.pass_vars.items() if var.get()]
            
            # Set optimization level
            self.set_optimization_level(optimization_level)
            
            # Compile
            result = self.compile(source_code, language, selected_passes, target)
            
            # Display results
            self.output_text.delete(1.0, tk.END)
            self.output_text.insert(1.0, f"Compilation successful!\n")
            self.output_text.insert(tk.END, f"Target code:\n{result.target_code}\n")
            self.output_text.insert(tk.END, f"Compilation time: {result.compilation_time:.3f}s\n")
            self.output_text.insert(tk.END, f"Optimization level: {result.optimization_level}\n")
            
            if result.errors:
                self.output_text.insert(tk.END, f"Errors: {result.errors}\n")
            if result.warnings:
                self.output_text.insert(tk.END, f"Warnings: {result.warnings}\n")
                
        except Exception as e:
            messagebox.showerror("Compilation Error", str(e))
    
    def _load_pipeline(self):
        """Load pipeline configuration"""
        filename = filedialog.askopenfilename(
            title="Load Pipeline Configuration",
            filetypes=[("JSON files", "*.json"), ("YAML files", "*.yaml"), ("All files", "*.*")]
        )
        if filename:
            try:
                pipeline = self.create_pipeline(filename)
                messagebox.showinfo("Success", f"Pipeline loaded from {filename}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to load pipeline: {e}")
    
    def _save_pipeline(self):
        """Save current pipeline configuration"""
        filename = filedialog.asksaveasfilename(
            title="Save Pipeline Configuration",
            defaultextension=".json",
            filetypes=[("JSON files", "*.json"), ("YAML files", "*.yaml")]
        )
        if filename:
            try:
                config = {
                    'language': self.language_var.get(),
                    'passes': [name for name, var in self.pass_vars.items() if var.get()],
                    'target': self.target_var.get(),
                    'optimization_level': self.optimization_var.get()
                }
                
                with open(filename, 'w') as f:
                    json.dump(config, f, indent=2)
                
                messagebox.showinfo("Success", f"Pipeline saved to {filename}")
            except Exception as e:
                messagebox.showerror("Error", f"Failed to save pipeline: {e}")
    
    def _refresh_cache_stats(self):
        """Refresh cache statistics"""
        stats = self.get_cache_stats()
        self.cache_stats_text.delete(1.0, tk.END)
        self.cache_stats_text.insert(1.0, f"Cache Statistics:\n")
        self.cache_stats_text.insert(tk.END, f"Entries: {stats['entries']}\n")
        self.cache_stats_text.insert(tk.END, f"Enabled: {stats['enabled']}\n")
        self.cache_stats_text.insert(tk.END, f"Memory Usage: {stats['memory_usage']} bytes\n")
    
    def _clear_cache(self):
        """Clear cache"""
        self.clear_cache()
        self._refresh_cache_stats()
        messagebox.showinfo("Success", "Cache cleared")
    
    def _check_cross_compile_compatibility(self):
        """Check cross-compilation compatibility"""
        source_platform = self.source_platform_var.get()
        target_platform = self.target_platform_var.get()
        
        self.cross_compile_text.delete(1.0, tk.END)
        self.cross_compile_text.insert(1.0, f"Cross-compilation: {source_platform} -> {target_platform}\n")
        
        if source_platform == target_platform:
            self.cross_compile_text.insert(tk.END, "No cross-compilation needed\n")
        else:
            self.cross_compile_text.insert(tk.END, f"Cross-compilation required\n")
            self.cross_compile_text.insert(tk.END, f"Supported targets for {source_platform}:\n")
            # Add platform-specific information
            self.cross_compile_text.insert(tk.END, "- Windows: x86_64_pe\n")
            self.cross_compile_text.insert(tk.END, "- Linux: x86_64_elf\n")
            self.cross_compile_text.insert(tk.END, "- macOS: arm64_macho\n")
            self.cross_compile_text.insert(tk.END, "- Web: wasm\n")

def main():
    """Test the enhanced compiler system"""
    
    print("Testing Enhanced Extensible Compiler System...")
    
    # Create compiler system
    compiler = EnhancedExtensibleCompilerSystem()
    
    # Test dependency ordering
    print("\n1. Testing dependency ordering...")
    try:
        ordered_passes = compiler.get_ordered_passes(['semantic_analysis', 'lexical_analysis', 'syntax_analysis'])
        print(f"   Ordered passes: {ordered_passes}")
    except Exception as e:
        print(f"   Error: {e}")
    
    # Test compilation
    print("\n2. Testing compilation...")
    try:
        result = compiler.compile("print('hello')", "python", ["lexical_analysis", "syntax_analysis"], "x86_64_elf")
        print(f"   Compilation successful: {result.target_code}")
        print(f"   Compilation time: {result.compilation_time:.3f}s")
    except Exception as e:
        print(f"   Error: {e}")
    
    # Test caching
    print("\n3. Testing caching...")
    try:
        result1 = compiler.compile_with_cache("print('hello')", "python", ["lexical_analysis"], "x86_64_elf")
        result2 = compiler.compile_with_cache("print('hello')", "python", ["lexical_analysis"], "x86_64_elf")
        print(f"   Cache enabled: {compiler.cache_enabled}")
        print(f"   Cache entries: {len(compiler.cache)}")
    except Exception as e:
        print(f"   Error: {e}")
    
    print("\n✅ Enhanced compiler system test complete!")

if __name__ == "__main__":
    main()
