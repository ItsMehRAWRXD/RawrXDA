#!/usr/bin/env python3
"""
Visual Studio-Style Language Selection Dialog
"What Language Are You Working In Today?" - Multi-language workspace configuration
"""

import tkinter as tk
from tkinter import ttk, messagebox, filedialog
import os
import sys
import json
import subprocess
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple
import threading
import time

class LanguageInfo:
    """Information about a programming language"""
    
    def __init__(self, name: str, extensions: List[str], icon: str = "📄", 
                 compiler: str = None, interpreter: str = None, 
                 toolchain: str = None, description: str = ""):
        self.name = name
        self.extensions = extensions
        self.icon = icon
        self.compiler = compiler
        self.interpreter = interpreter
        self.toolchain = toolchain
        self.description = description
        self.available = False
        self.detected_path = None
        self.version = None

class ToolchainDetector:
    """Detects available toolchains and compilers"""
    
    def __init__(self):
        self.detected_toolchains = {}
        self.detect_toolchains()
    
    def detect_toolchains(self):
        """Detect all available toolchains on the system"""
        
        print("🔍 Detecting available toolchains...")
        
        # Common toolchain commands to check
        toolchains = {
            'gcc': ['gcc', '--version'],
            'g++': ['g++', '--version'],
            'clang': ['clang', '--version'],
            'clang++': ['clang++', '--version'],
            'python': ['python', '--version'],
            'python3': ['python3', '--version'],
            'node': ['node', '--version'],
            'npm': ['npm', '--version'],
            'java': ['java', '-version'],
            'javac': ['javac', '-version'],
            'dotnet': ['dotnet', '--version'],
            'go': ['go', 'version'],
            'rustc': ['rustc', '--version'],
            'cargo': ['cargo', '--version'],
            'php': ['php', '--version'],
            'ruby': ['ruby', '--version'],
            'swift': ['swift', '--version'],
            'kotlin': ['kotlin', '-version'],
            'scala': ['scala', '-version'],
            'ghc': ['ghc', '--version'],
            'lua': ['lua', '-v'],
            'perl': ['perl', '-v'],
            'bash': ['bash', '--version'],
            'powershell': ['powershell', '-Command', '$PSVersionTable.PSVersion'],
            'cmd': ['cmd', '/c', 'ver']
        }
        
        for toolchain, command in toolchains.items():
            try:
                result = subprocess.run(command, capture_output=True, text=True, timeout=5)
                if result.returncode == 0:
                    version_info = result.stdout.strip() or result.stderr.strip()
                    self.detected_toolchains[toolchain] = {
                        'path': shutil.which(command[0]),
                        'version': version_info,
                        'available': True
                    }
                    print(f"✅ Found {toolchain}: {version_info[:50]}...")
                else:
                    self.detected_toolchains[toolchain] = {
                        'path': None,
                        'version': None,
                        'available': False
                    }
            except (subprocess.TimeoutExpired, FileNotFoundError, Exception) as e:
                self.detected_toolchains[toolchain] = {
                    'path': None,
                    'version': None,
                    'available': False
                }
        
        print(f"🔍 Toolchain detection complete. Found {len([t for t in self.detected_toolchains.values() if t['available']])} available toolchains.")
    
    def is_toolchain_available(self, toolchain: str) -> bool:
        """Check if a specific toolchain is available"""
        return self.detected_toolchains.get(toolchain, {}).get('available', False)
    
    def get_toolchain_info(self, toolchain: str) -> Dict:
        """Get toolchain information"""
        return self.detected_toolchains.get(toolchain, {'available': False})

class VisualStudioLanguageSelector:
    """
    Visual Studio-style language selection dialog
    Allows multi-language workspace configuration with toolchain detection
    """
    
    def __init__(self, ide_instance=None):
        self.ide = ide_instance
        self.selected_languages = set()
        self.workspace_config = {}
        self.toolchain_detector = ToolchainDetector()
        
        # Define supported languages with their toolchains
        self.languages = self._initialize_languages()
        
        print("🎯 Visual Studio Language Selector initialized")
    
    def _initialize_languages(self) -> Dict[str, LanguageInfo]:
        """Initialize supported languages"""
        
        languages = {}
        
        # System Languages (always available)
        languages['EON'] = LanguageInfo(
            name='EON',
            extensions=['.eon', '.eonasm'],
            icon='⚡',
            toolchain='eon-compiler',
            description='Proprietary assembly-like language with advanced features'
        )
        
        languages['Assembly'] = LanguageInfo(
            name='Assembly',
            extensions=['.asm', '.s', '.S'],
            icon='🔧',
            compiler='nasm',
            description='Low-level assembly programming'
        )
        
        # C/C++ Family
        languages['C'] = LanguageInfo(
            name='C',
            extensions=['.c', '.h'],
            icon='🔵',
            compiler='gcc',
            toolchain='gcc',
            description='System programming and embedded development'
        )
        
        languages['C++'] = LanguageInfo(
            name='C++',
            extensions=['.cpp', '.cc', '.cxx', '.hpp', '.h'],
            icon='🟣',
            compiler='g++',
            toolchain='g++',
            description='Object-oriented system programming'
        )
        
        # Interpreted Languages
        languages['Python'] = LanguageInfo(
            name='Python',
            extensions=['.py', '.pyw', '.pyc'],
            icon='🐍',
            interpreter='python',
            description='High-level scripting and data science'
        )
        
        languages['JavaScript'] = LanguageInfo(
            name='JavaScript',
            extensions=['.js', '.mjs', '.jsx'],
            icon='🟨',
            interpreter='node',
            toolchain='node',
            description='Web development and server-side scripting'
        )
        
        languages['TypeScript'] = LanguageInfo(
            name='TypeScript',
            extensions=['.ts', '.tsx'],
            icon='🔷',
            compiler='tsc',
            toolchain='typescript',
            description='Typed JavaScript for large applications'
        )
        
        # JVM Languages
        languages['Java'] = LanguageInfo(
            name='Java',
            extensions=['.java'],
            icon='☕',
            compiler='javac',
            interpreter='java',
            toolchain='java',
            description='Enterprise application development'
        )
        
        languages['Kotlin'] = LanguageInfo(
            name='Kotlin',
            extensions=['.kt', '.kts'],
            icon='🟢',
            compiler='kotlinc',
            toolchain='kotlin',
            description='Modern JVM language for Android and server'
        )
        
        languages['Scala'] = LanguageInfo(
            name='Scala',
            extensions=['.scala'],
            icon='🔴',
            compiler='scalac',
            interpreter='scala',
            toolchain='scala',
            description='Functional programming on the JVM'
        )
        
        # .NET Languages
        languages['C#'] = LanguageInfo(
            name='C#',
            extensions=['.cs'],
            icon='🟦',
            compiler='csc',
            toolchain='dotnet',
            description='Microsoft .NET application development'
        )
        
        languages['VB.NET'] = LanguageInfo(
            name='VB.NET',
            extensions=['.vb'],
            icon='🟫',
            compiler='vbc',
            toolchain='dotnet',
            description='Visual Basic .NET development'
        )
        
        # Modern Languages
        languages['Go'] = LanguageInfo(
            name='Go',
            extensions=['.go'],
            icon='🐹',
            compiler='go',
            toolchain='go',
            description='Google\'s systems programming language'
        )
        
        languages['Rust'] = LanguageInfo(
            name='Rust',
            extensions=['.rs'],
            icon='🦀',
            compiler='rustc',
            toolchain='rust',
            description='Memory-safe systems programming'
        )
        
        languages['Swift'] = LanguageInfo(
            name='Swift',
            extensions=['.swift'],
            icon='🦉',
            compiler='swiftc',
            toolchain='swift',
            description='Apple\'s modern programming language'
        )
        
        # Web Technologies
        languages['HTML'] = LanguageInfo(
            name='HTML',
            extensions=['.html', '.htm'],
            icon='🌐',
            description='Web page markup'
        )
        
        languages['CSS'] = LanguageInfo(
            name='CSS',
            extensions=['.css', '.scss', '.sass'],
            icon='🎨',
            description='Web styling and layout'
        )
        
        languages['PHP'] = LanguageInfo(
            name='PHP',
            extensions=['.php', '.phtml'],
            icon='🐘',
            interpreter='php',
            toolchain='php',
            description='Server-side web development'
        )
        
        languages['Ruby'] = LanguageInfo(
            name='Ruby',
            extensions=['.rb'],
            icon='💎',
            interpreter='ruby',
            toolchain='ruby',
            description='Dynamic object-oriented programming'
        )
        
        # Functional Languages
        languages['Haskell'] = LanguageInfo(
            name='Haskell',
            extensions=['.hs', '.lhs'],
            icon='🔷',
            compiler='ghc',
            toolchain='haskell',
            description='Pure functional programming'
        )
        
        languages['Lua'] = LanguageInfo(
            name='Lua',
            extensions=['.lua'],
            icon='🌙',
            interpreter='lua',
            toolchain='lua',
            description='Lightweight scripting language'
        )
        
        # Shell Scripting
        languages['Bash'] = LanguageInfo(
            name='Bash',
            extensions=['.sh', '.bash'],
            icon='🐚',
            interpreter='bash',
            toolchain='bash',
            description='Unix shell scripting'
        )
        
        languages['PowerShell'] = LanguageInfo(
            name='PowerShell',
            extensions=['.ps1', '.psm1'],
            icon='⚡',
            interpreter='powershell',
            toolchain='powershell',
            description='Windows automation and scripting'
        )
        
        # Check availability for each language
        for lang_name, lang_info in languages.items():
            lang_info.available = self._check_language_availability(lang_info)
        
        return languages
    
    def _check_language_availability(self, lang_info: LanguageInfo) -> bool:
        """Check if a language's toolchain is available"""
        
        if lang_info.toolchain:
            if self.toolchain_detector.is_toolchain_available(lang_info.toolchain):
                lang_info.detected_path = self.toolchain_detector.get_toolchain_info(lang_info.toolchain)['path']
                lang_info.version = self.toolchain_detector.get_toolchain_info(lang_info.toolchain)['version']
                return True
        
        if lang_info.compiler:
            if self.toolchain_detector.is_toolchain_available(lang_info.compiler):
                lang_info.detected_path = self.toolchain_detector.get_toolchain_info(lang_info.compiler)['path']
                lang_info.version = self.toolchain_detector.get_toolchain_info(lang_info.compiler)['version']
                return True
        
        if lang_info.interpreter:
            if self.toolchain_detector.is_toolchain_available(lang_info.interpreter):
                lang_info.detected_path = self.toolchain_detector.get_toolchain_info(lang_info.interpreter)['path']
                lang_info.version = self.toolchain_detector.get_toolchain_info(lang_info.interpreter)['version']
                return True
        
        # Some languages are always available (HTML, CSS, etc.)
        if lang_info.name in ['HTML', 'CSS', 'EON']:
            return True
        
        return False
    
    def show_language_selection_dialog(self) -> bool:
        """Show the Visual Studio-style language selection dialog"""
        
        # Create main dialog
        dialog = tk.Toplevel()
        dialog.title("What Language Are You Working In Today?")
        dialog.geometry("900x700")
        dialog.resizable(True, True)
        
        # Center dialog on screen
        dialog.update_idletasks()
        x = (dialog.winfo_screenwidth() // 2) - (900 // 2)
        y = (dialog.winfo_screenheight() // 2) - (700 // 2)
        dialog.geometry(f"900x700+{x}+{y}")
        
        # Make it modal
        if self.ide and hasattr(self.ide, 'root'):
            dialog.transient(self.ide.root)
            dialog.grab_set()
        
        # Main frame
        main_frame = ttk.Frame(dialog, padding="20")
        main_frame.pack(fill='both', expand=True)
        
        # Header
        self._create_header(main_frame)
        
        # Language selection area
        self._create_language_selection(main_frame)
        
        # Workspace options
        self._create_workspace_options(main_frame)
        
        # Toolchain information
        self._create_toolchain_info(main_frame)
        
        # Action buttons
        self._create_action_buttons(main_frame, dialog)
        
        # Configure grid weights
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(2, weight=1)
        
        # Wait for user response
        dialog.wait_window()
        
        return len(self.selected_languages) > 0
    
    def _create_header(self, parent):
        """Create dialog header"""
        
        header_frame = ttk.Frame(parent)
        header_frame.pack(fill='x', pady=(0, 20))
        
        # Title
        title_label = ttk.Label(header_frame, 
                               text="What Language Are You Working In Today?",
                               font=("Segoe UI", 18, "bold"))
        title_label.pack(pady=(0, 10))
        
        # Subtitle
        subtitle_label = ttk.Label(header_frame,
                                  text="Select the languages you'll be working with in this workspace.\nYou can select multiple languages for mixed-language projects.",
                                  font=("Segoe UI", 10),
                                  foreground="gray")
        subtitle_label.pack()
    
    def _create_language_selection(self, parent):
        """Create language selection area"""
        
        # Language selection frame
        lang_frame = ttk.LabelFrame(parent, text="Programming Languages", padding="15")
        lang_frame.pack(fill='both', expand=True, pady=(0, 15))
        
        # Create scrollable frame for languages
        canvas = tk.Canvas(lang_frame, height=400)
        scrollbar = ttk.Scrollbar(lang_frame, orient="vertical", command=canvas.yview)
        scrollable_frame = ttk.Frame(canvas)
        
        scrollable_frame.bind(
            "<Configure>",
            lambda e: canvas.configure(scrollregion=canvas.bbox("all"))
        )
        
        canvas.create_window((0, 0), window=scrollable_frame, anchor="nw")
        canvas.configure(yscrollcommand=scrollbar.set)
        
        # Language checkboxes
        self.language_vars = {}
        row = 0
        
        for lang_name, lang_info in self.languages.items():
            # Create language frame
            lang_item_frame = ttk.Frame(scrollable_frame)
            lang_item_frame.grid(row=row, column=0, sticky='ew', padx=5, pady=5)
            
            # Checkbox
            var = tk.BooleanVar()
            self.language_vars[lang_name] = var
            
            checkbox = ttk.Checkbutton(lang_item_frame, variable=var, 
                                     command=lambda name=lang_name: self._on_language_selected(name))
            checkbox.grid(row=0, column=0, sticky='w', padx=(0, 10))
            
            # Language icon and name
            lang_label = ttk.Label(lang_item_frame, 
                                  text=f"{lang_info.icon} {lang_name}",
                                  font=("Segoe UI", 11, "bold"))
            lang_label.grid(row=0, column=1, sticky='w', padx=(0, 15))
            
            # Availability indicator
            if lang_info.available:
                status_label = ttk.Label(lang_item_frame, text="✅ Available", 
                                       foreground="green", font=("Segoe UI", 9))
            else:
                status_label = ttk.Label(lang_item_frame, text="❌ Not Available", 
                                       foreground="red", font=("Segoe UI", 9))
            status_label.grid(row=0, column=2, sticky='w', padx=(0, 15))
            
            # Description
            desc_label = ttk.Label(lang_item_frame, 
                                  text=lang_info.description,
                                  font=("Segoe UI", 9),
                                  foreground="gray")
            desc_label.grid(row=1, column=1, columnspan=2, sticky='w', padx=(20, 0))
            
            # Extensions
            ext_text = ", ".join(lang_info.extensions)
            ext_label = ttk.Label(lang_item_frame,
                                 text=f"Extensions: {ext_text}",
                                 font=("Segoe UI", 8),
                                 foreground="darkgray")
            ext_label.grid(row=2, column=1, columnspan=2, sticky='w', padx=(20, 0))
            
            row += 1
        
        # Configure grid
        scrollable_frame.columnconfigure(0, weight=1)
        
        # Pack canvas and scrollbar
        canvas.pack(side="left", fill="both", expand=True)
        scrollbar.pack(side="right", fill="y")
        
        # Bind mousewheel to canvas
        def _on_mousewheel(event):
            canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        canvas.bind_all("<MouseWheel>", _on_mousewheel)
    
    def _create_workspace_options(self, parent):
        """Create workspace configuration options"""
        
        workspace_frame = ttk.LabelFrame(parent, text="Workspace Configuration", padding="10")
        workspace_frame.pack(fill='x', pady=(0, 15))
        
        # Project name
        ttk.Label(workspace_frame, text="Project Name:").grid(row=0, column=0, sticky='w', padx=(0, 10))
        self.project_name_var = tk.StringVar(value="MultiLanguageProject")
        project_entry = ttk.Entry(workspace_frame, textvariable=self.project_name_var, width=30)
        project_entry.grid(row=0, column=1, sticky='w', padx=(0, 20))
        
        # Project location
        ttk.Label(workspace_frame, text="Location:").grid(row=0, column=2, sticky='w', padx=(0, 10))
        self.project_location_var = tk.StringVar(value=os.getcwd())
        location_entry = ttk.Entry(workspace_frame, textvariable=self.project_location_var, width=30)
        location_entry.grid(row=0, column=3, sticky='w', padx=(0, 10))
        
        browse_button = ttk.Button(workspace_frame, text="Browse...", 
                                  command=self._browse_location)
        browse_button.grid(row=0, column=4, sticky='w')
        
        # Create solution checkbox
        self.create_solution_var = tk.BooleanVar(value=True)
        solution_check = ttk.Checkbutton(workspace_frame, 
                                        text="Create solution structure",
                                        variable=self.create_solution_var)
        solution_check.grid(row=1, column=0, columnspan=2, sticky='w', pady=(10, 0))
        
        # Initialize with templates
        self.init_templates_var = tk.BooleanVar(value=True)
        templates_check = ttk.Checkbutton(workspace_frame,
                                         text="Initialize with language templates",
                                         variable=self.init_templates_var)
        templates_check.grid(row=1, column=2, columnspan=2, sticky='w', pady=(10, 0))
    
    def _create_toolchain_info(self, parent):
        """Create toolchain information display"""
        
        toolchain_frame = ttk.LabelFrame(parent, text="Detected Toolchains", padding="10")
        toolchain_frame.pack(fill='x', pady=(0, 15))
        
        # Toolchain info text
        self.toolchain_text = tk.Text(toolchain_frame, height=6, width=80, 
                                     font=("Consolas", 9), bg="#f8f9fa")
        self.toolchain_text.pack(fill='both', expand=True)
        
        # Populate toolchain info
        self._update_toolchain_info()
    
    def _create_action_buttons(self, parent, dialog):
        """Create action buttons"""
        
        button_frame = ttk.Frame(parent)
        button_frame.pack(fill='x', pady=(10, 0))
        
        # Select All button
        select_all_button = ttk.Button(button_frame, text="Select All Available",
                                      command=self._select_all_available)
        select_all_button.pack(side='left', padx=(0, 10))
        
        # Clear All button
        clear_all_button = ttk.Button(button_frame, text="Clear All",
                                     command=self._clear_all)
        clear_all_button.pack(side='left', padx=(0, 10))
        
        # Refresh toolchains button
        refresh_button = ttk.Button(button_frame, text="Refresh Toolchains",
                                   command=self._refresh_toolchains)
        refresh_button.pack(side='left', padx=(0, 10))
        
        # Spacer
        ttk.Frame(button_frame).pack(side='left', expand=True, fill='x')
        
        # Create Workspace button
        create_button = ttk.Button(button_frame, text="Create Workspace",
                                  command=lambda: self._create_workspace(dialog),
                                  style="Accent.TButton")
        create_button.pack(side='right', padx=(10, 0))
        
        # Cancel button
        cancel_button = ttk.Button(button_frame, text="Cancel",
                                  command=dialog.destroy)
        cancel_button.pack(side='right')
    
    def _on_language_selected(self, lang_name: str):
        """Handle language selection"""
        
        if self.language_vars[lang_name].get():
            self.selected_languages.add(lang_name)
        else:
            self.selected_languages.discard(lang_name)
        
        print(f"🎯 Language selection updated: {len(self.selected_languages)} languages selected")
    
    def _browse_location(self):
        """Browse for project location"""
        
        location = filedialog.askdirectory(title="Select Project Location")
        if location:
            self.project_location_var.set(location)
    
    def _update_toolchain_info(self):
        """Update toolchain information display"""
        
        self.toolchain_text.delete(1.0, tk.END)
        
        available_count = 0
        for toolchain, info in self.toolchain_detector.detected_toolchains.items():
            if info['available']:
                available_count += 1
                self.toolchain_text.insert(tk.END, f"✅ {toolchain}: {info['version'][:60]}...\n")
                self.toolchain_text.insert(tk.END, f"   Path: {info['path']}\n\n")
            else:
                self.toolchain_text.insert(tk.END, f"❌ {toolchain}: Not available\n")
        
        self.toolchain_text.insert(tk.END, f"\n📊 Summary: {available_count} toolchains detected")
    
    def _select_all_available(self):
        """Select all available languages"""
        
        for lang_name, lang_info in self.languages.items():
            if lang_info.available:
                self.language_vars[lang_name].set(True)
                self.selected_languages.add(lang_name)
        
        print(f"✅ Selected all available languages: {len(self.selected_languages)}")
    
    def _clear_all(self):
        """Clear all language selections"""
        
        for var in self.language_vars.values():
            var.set(False)
        
        self.selected_languages.clear()
        print("🧹 Cleared all language selections")
    
    def _refresh_toolchains(self):
        """Refresh toolchain detection"""
        
        print("🔄 Refreshing toolchain detection...")
        self.toolchain_detector = ToolchainDetector()
        
        # Update language availability
        for lang_name, lang_info in self.languages.items():
            lang_info.available = self._check_language_availability(lang_info)
        
        self._update_toolchain_info()
        print("✅ Toolchain detection refreshed")
    
    def _create_workspace(self, dialog):
        """Create the multi-language workspace"""
        
        if not self.selected_languages:
            messagebox.showwarning("No Languages Selected", 
                                 "Please select at least one language to create a workspace.")
            return
        
        try:
            project_name = self.project_name_var.get()
            project_location = self.project_location_var.get()
            create_solution = self.create_solution_var.get()
            init_templates = self.init_templates_var.get()
            
            # Create workspace
            self._create_project_structure(project_name, project_location, 
                                         self.selected_languages, create_solution, init_templates)
            
            # Show success message
            messagebox.showinfo("Workspace Created", 
                              f"Multi-language workspace '{project_name}' created successfully!\n\n"
                              f"Languages: {', '.join(sorted(self.selected_languages))}\n"
                              f"Location: {project_location}")
            
            dialog.destroy()
            
        except Exception as e:
            messagebox.showerror("Error", f"Failed to create workspace: {str(e)}")
    
    def _create_project_structure(self, project_name: str, location: str, 
                                languages: Set[str], create_solution: bool, 
                                init_templates: bool):
        """Create project structure for selected languages"""
        
        project_path = Path(location) / project_name
        project_path.mkdir(parents=True, exist_ok=True)
        
        print(f"🏗️ Creating workspace structure at: {project_path}")
        
        # Create main project files
        self._create_workspace_config(project_path, languages)
        
        # Create solution structure if requested
        if create_solution:
            self._create_solution_structure(project_path, languages)
        
        # Create language-specific directories and files
        for lang_name in languages:
            self._create_language_structure(project_path, lang_name, init_templates)
        
        # Create shared resources
        self._create_shared_resources(project_path, languages)
        
        print(f"✅ Workspace structure created successfully")
    
    def _create_workspace_config(self, project_path: Path, languages: Set[str]):
        """Create workspace configuration file"""
        
        config = {
            'name': project_path.name,
            'languages': list(languages),
            'created': time.time(),
            'version': '1.0.0',
            'settings': {
                'auto_detect_toolchains': True,
                'multi_language_build': True,
                'shared_include_paths': True
            }
        }
        
        config_file = project_path / 'workspace.json'
        with open(config_file, 'w') as f:
            json.dump(config, f, indent=2)
        
        print(f"📄 Created workspace config: {config_file}")
    
    def _create_solution_structure(self, project_path: Path, languages: Set[str]):
        """Create solution/project structure"""
        
        # Create solution directory
        solution_dir = project_path / 'Solution'
        solution_dir.mkdir(exist_ok=True)
        
        # Create language-specific project directories
        for lang_name in languages:
            lang_dir = solution_dir / f"{lang_name}Project"
            lang_dir.mkdir(exist_ok=True)
            
            # Create project file
            project_file = lang_dir / f"{lang_name}Project.{self._get_project_extension(lang_name)}"
            self._create_project_file(project_file, lang_name)
    
    def _create_language_structure(self, project_path: Path, lang_name: str, init_templates: bool):
        """Create language-specific structure"""
        
        lang_info = self.languages[lang_name]
        
        # Create language directory
        lang_dir = project_path / lang_name
        lang_dir.mkdir(exist_ok=True)
        
        # Create subdirectories
        subdirs = ['src', 'include', 'lib', 'bin', 'tests', 'docs']
        for subdir in subdirs:
            (lang_dir / subdir).mkdir(exist_ok=True)
        
        # Create templates if requested
        if init_templates:
            self._create_language_templates(lang_dir, lang_name, lang_info)
        
        # Create build configuration
        self._create_build_config(lang_dir, lang_name, lang_info)
    
    def _create_language_templates(self, lang_dir: Path, lang_name: str, lang_info: LanguageInfo):
        """Create language-specific templates"""
        
        templates = {
            'C': {
                'main.c': '''#include <stdio.h>
#include <stdlib.h>

int main() {
    printf("Hello from C!\\n");
    return 0;
}'''
            },
            'C++': {
                'main.cpp': '''#include <iostream>

int main() {
    std::cout << "Hello from C++!" << std::endl;
    return 0;
}'''
            },
            'Python': {
                'main.py': '''#!/usr/bin/env python3
"""
Main Python application
"""

def main():
    print("Hello from Python!")

if __name__ == "__main__":
    main()'''
            },
            'JavaScript': {
                'main.js': '''// Main JavaScript application
console.log("Hello from JavaScript!");

function main() {
    console.log("Application started");
}

main();'''
            },
            'Java': {
                'Main.java': '''public class Main {
    public static void main(String[] args) {
        System.out.println("Hello from Java!");
    }
}'''
            }
        }
        
        if lang_name in templates:
            for filename, content in templates[lang_name].items():
                template_file = lang_dir / 'src' / filename
                with open(template_file, 'w') as f:
                    f.write(content)
                print(f"📝 Created template: {template_file}")
    
    def _create_build_config(self, lang_dir: Path, lang_name: str, lang_info: LanguageInfo):
        """Create build configuration for language"""
        
        build_config = {
            'language': lang_name,
            'extensions': lang_info.extensions,
            'toolchain': lang_info.toolchain,
            'compiler': lang_info.compiler,
            'interpreter': lang_info.interpreter,
            'build_command': self._get_build_command(lang_name),
            'run_command': self._get_run_command(lang_name)
        }
        
        config_file = lang_dir / 'build.json'
        with open(config_file, 'w') as f:
            json.dump(build_config, f, indent=2)
    
    def _get_project_extension(self, lang_name: str) -> str:
        """Get project file extension for language"""
        
        extensions = {
            'C': 'vcxproj',
            'C++': 'vcxproj',
            'C#': 'csproj',
            'Java': 'iml',
            'Python': 'pyproj',
            'JavaScript': 'jsproj'
        }
        
        return extensions.get(lang_name, 'proj')
    
    def _get_build_command(self, lang_name: str) -> str:
        """Get build command for language"""
        
        commands = {
            'C': 'gcc -o {output} {sources}',
            'C++': 'g++ -o {output} {sources}',
            'Java': 'javac {sources}',
            'C#': 'dotnet build',
            'Python': 'python -m py_compile {sources}',
            'JavaScript': 'node {sources}'
        }
        
        return commands.get(lang_name, 'echo "No build command configured"')
    
    def _get_run_command(self, lang_name: str) -> str:
        """Get run command for language"""
        
        commands = {
            'C': './{executable}',
            'C++': './{executable}',
            'Java': 'java {class}',
            'C#': 'dotnet run',
            'Python': 'python {script}',
            'JavaScript': 'node {script}'
        }
        
        return commands.get(lang_name, 'echo "No run command configured"')
    
    def _create_shared_resources(self, project_path: Path, languages: Set[str]):
        """Create shared resources for multi-language project"""
        
        # Create shared directory
        shared_dir = project_path / 'Shared'
        shared_dir.mkdir(exist_ok=True)
        
        # Create common subdirectories
        subdirs = ['includes', 'libraries', 'resources', 'scripts', 'config']
        for subdir in subdirs:
            (shared_dir / subdir).mkdir(exist_ok=True)
        
        # Create build script
        build_script = shared_dir / 'scripts' / 'build_all.py'
        self._create_build_script(build_script, languages)
        
        # Create README
        readme_file = project_path / 'README.md'
        self._create_readme(readme_file, languages)
    
    def _create_build_script(self, script_path: Path, languages: Set[str]):
        """Create multi-language build script"""
        
        script_content = f'''#!/usr/bin/env python3
"""
Multi-language build script
Builds all languages in the workspace
"""

import os
import subprocess
import json
from pathlib import Path

def build_language(lang_name):
    """Build a specific language"""
    lang_dir = Path(lang_name)
    config_file = lang_dir / 'build.json'
    
    if not config_file.exists():
        print(f"⚠️ No build config for {{lang_name}}")
        return False
    
    with open(config_file) as f:
        config = json.load(f)
    
    build_cmd = config.get('build_command', '')
    if not build_cmd:
        print(f"⚠️ No build command for {{lang_name}}")
        return False
    
    print(f"🔨 Building {{lang_name}}...")
    try:
        result = subprocess.run(build_cmd, shell=True, cwd=lang_dir, 
                              capture_output=True, text=True)
        if result.returncode == 0:
            print(f"✅ {{lang_name}} built successfully")
            return True
        else:
            print(f"❌ {{lang_name}} build failed: {{result.stderr}}")
            return False
    except Exception as e:
        print(f"❌ {{lang_name}} build error: {{e}}")
        return False

def main():
    """Build all languages"""
    languages = {list(languages)}
    
    print("🏗️ Multi-language workspace build starting...")
    print(f"Languages: {{', '.join(languages)}}")
    
    success_count = 0
    for lang_name in languages:
        if build_language(lang_name):
            success_count += 1
    
    print(f"\\n📊 Build complete: {{success_count}}/{{len(languages)}} languages built successfully")

if __name__ == "__main__":
    main()
'''
        
        with open(script_path, 'w') as f:
            f.write(script_content)
        
        # Make executable on Unix systems
        try:
            os.chmod(script_path, 0o755)
        except:
            pass
    
    def _create_readme(self, readme_path: Path, languages: Set[str]):
        """Create project README"""
        
        readme_content = f'''# Multi-Language Workspace

This workspace contains projects in multiple programming languages.

## Languages

{chr(10).join(f"- **{lang}**: {self.languages[lang].description}" for lang in sorted(languages))}

## Project Structure

```
{readme_path.parent.name}/
├── workspace.json          # Workspace configuration
├── README.md              # This file
├── Shared/                # Shared resources
│   ├── includes/          # Common headers
│   ├── libraries/         # Shared libraries
│   ├── resources/         # Shared assets
│   └── scripts/           # Build scripts
└── [Language]/            # Language-specific projects
    ├── src/               # Source files
    ├── include/           # Headers
    ├── lib/               # Libraries
    ├── bin/               # Binaries
    ├── tests/             # Test files
    └── build.json         # Build configuration
```

## Building

### Build All Languages
```bash
python Shared/scripts/build_all.py
```

### Build Individual Language
```bash
cd [Language]
# Follow language-specific build instructions in build.json
```

## Getting Started

1. Ensure required toolchains are installed
2. Run the build script to compile all languages
3. Check individual language directories for specific instructions

## Toolchain Requirements

{chr(10).join(f"- **{lang}**: {self.languages[lang].toolchain or 'Built-in'}" for lang in sorted(languages) if self.languages[lang].available)}

Generated on: {time.ctime()}
'''
        
        with open(readme_path, 'w') as f:
            f.write(readme_content)

# Integration function
def integrate_language_selector(ide_instance):
    """Integrate Visual Studio-style language selector with IDE"""
    
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("File", "New Multi-Language Workspace...", 
                                 lambda: VisualStudioLanguageSelector(ide_instance).show_language_selection_dialog())
    
    # Add startup language selection
    ide_instance.language_selector = VisualStudioLanguageSelector(ide_instance)
    
    # Show on startup
    ide_instance.after(1000, lambda: ide_instance.language_selector.show_language_selection_dialog())
    
    print("🎯 Visual Studio-style language selector integrated")

if __name__ == "__main__":
    print("🎯 Visual Studio Language Selector")
    print("=" * 50)
    print("Creates multi-language workspaces with proper toolchain detection")
    
    # Create test dialog
    root = tk.Tk()
    root.withdraw()  # Hide main window
    
    selector = VisualStudioLanguageSelector()
    selector.show_language_selection_dialog()
    
    print(f"✅ Selected languages: {selector.selected_languages}")
