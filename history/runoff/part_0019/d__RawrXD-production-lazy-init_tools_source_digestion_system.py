#!/usr/bin/env python3
"""
RawrXD IDE Source Digestion System
===================================
A comprehensive self-auditing system that analyzes the entire IDE source codebase
to calculate:
- What is done (complete implementations)
- What isn't done (stubs, placeholders)
- What needs to be done (missing features)
- What needs to be fully created (new files needed)

Health Points System:
- Each component has health points (HP) based on implementation completeness
- Full implementation = 100 HP
- Partial implementation = 10-90 HP based on coverage
- Stub only = 1-10 HP
- Missing = 0 HP

Usage:
    python source_digestion_system.py --src-dir "D:\RawrXD-production-lazy-init\src"
    python source_digestion_system.py --full-audit
    python source_digestion_system.py --self-check

Author: RawrXD IDE Team
Version: 1.0.0
"""

import os
import sys
import re
import json
import hashlib
import argparse
from pathlib import Path
from dataclasses import dataclass, field, asdict
from typing import List, Dict, Set, Optional, Tuple, Any
from datetime import datetime
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed
import threading

# ============================================================================
# CONFIGURATION
# ============================================================================

class Config:
    """Configuration for the digestion system"""
    
    # File extensions to analyze
    SOURCE_EXTENSIONS = {
        '.cpp', '.c', '.h', '.hpp', '.asm', '.inc',
        '.py', '.ps1', '.bat', '.sh', '.cmake'
    }
    
    # Documentation extensions
    DOC_EXTENSIONS = {'.md', '.txt', '.rst', '.doc'}
    
    # Stub indicators - patterns that indicate incomplete implementation
    STUB_PATTERNS = [
        r'//\s*TODO',
        r'//\s*FIXME',
        r'//\s*HACK',
        r'//\s*XXX',
        r'//\s*STUB',
        r'//\s*PLACEHOLDER',
        r'//\s*NOT\s+IMPLEMENTED',
        r'//\s*coming\s+soon',
        r'//\s*No-op',
        r'//\s*Simple\s+stub',
        r'//\s*Minimal\s+implementation',
        r'//\s*Will\s+be\s+implemented',
        r'//\s*To\s+be\s+implemented',
        r'return\s+(nullptr|NULL|0|false|"")\s*;\s*//.*stub',
        r'throw\s+std::runtime_error\s*\(\s*"Not\s+implemented',
        r'qWarning\s*\(\s*\)\s*<<\s*"Not\s+implemented',
        r'qDebug\s*\(\s*\)\s*<<\s*".*stub',
        r';\s*//\s*stub',
        r'{\s*}\s*//\s*stub',
        r'{\s*//\s*TODO',
        r'#\s*TODO',
        r';\s*#\s*STUB',
    ]
    
    # Complete implementation indicators
    COMPLETE_PATTERNS = [
        r'//\s*COMPLETE',
        r'//\s*IMPLEMENTED',
        r'//\s*PRODUCTION',
        r'//\s*DONE',
        r'//\s*FULLY\s+IMPLEMENTED',
    ]
    
    # Critical components that must exist
    CRITICAL_COMPONENTS = {
        'MainWindow': 'src/qtapp/MainWindow.cpp',
        'AgenticEngine': 'src/agentic_engine.cpp',
        'InferenceEngine': 'src/qtapp/inference_engine.cpp',
        'ModelLoader': 'src/auto_model_loader.cpp',
        'ChatInterface': 'src/chat_interface.cpp',
        'TerminalManager': 'src/qtapp/TerminalManager.cpp',
        'FileBrowser': 'src/file_browser.cpp',
        'LSPClient': 'src/lsp_client.cpp',
        'GGUFLoader': 'src/gguf_loader.cpp',
        'StreamingEngine': 'src/streaming_engine.cpp',
        'RefactoringEngine': 'src/refactoring_engine.cpp',
        'SecurityManager': 'src/security_manager.cpp',
        'TelemetrySystem': 'src/telemetry.cpp',
        'ErrorHandler': 'src/error_handler.cpp',
        'ConfigManager': 'src/config_manager.cpp',
    }
    
    # Feature categories for organization
    FEATURE_CATEGORIES = {
        'core': ['main', 'ide', 'window', 'app'],
        'ai': ['ai_', 'agentic', 'model_', 'inference', 'llm', 'gguf', 'streaming'],
        'editor': ['editor', 'syntax', 'highlight', 'completion', 'minimap'],
        'terminal': ['terminal', 'shell', 'powershell', 'console'],
        'git': ['git_', 'git/', 'github'],
        'lsp': ['lsp_', 'language_server'],
        'ui': ['ui_', 'widget', 'panel', 'dialog', 'menu', 'toolbar'],
        'network': ['http', 'websocket', 'api_', 'server'],
        'masm': ['masm', '.asm', 'asm_'],
        'testing': ['test_', 'test/', 'testing', 'benchmark'],
        'config': ['config', 'settings', 'preferences'],
        'security': ['security', 'auth', 'crypto', 'jwt'],
        'monitoring': ['telemetry', 'metrics', 'monitor', 'observability'],
    }


# ============================================================================
# DATA STRUCTURES
# ============================================================================

@dataclass
class StubInfo:
    """Information about a stub/placeholder"""
    line_number: int
    line_content: str
    pattern_matched: str
    severity: str  # 'critical', 'high', 'medium', 'low'

@dataclass 
class FunctionInfo:
    """Information about a function/method"""
    name: str
    line_start: int
    line_end: int
    return_type: str
    is_stub: bool
    stub_indicators: List[str] = field(default_factory=list)
    implementation_score: int = 0  # 0-100

@dataclass
class FileAnalysis:
    """Analysis results for a single file"""
    path: str
    relative_path: str
    extension: str
    size_bytes: int
    line_count: int
    code_lines: int
    comment_lines: int
    blank_lines: int
    stubs: List[StubInfo] = field(default_factory=list)
    todos: List[Tuple[int, str]] = field(default_factory=list)
    functions: List[FunctionInfo] = field(default_factory=list)
    imports: List[str] = field(default_factory=list)
    exports: List[str] = field(default_factory=list)
    health_points: int = 100
    category: str = 'other'
    is_complete: bool = True
    completion_percentage: float = 100.0
    hash: str = ''
    last_modified: str = ''

@dataclass
class ComponentStatus:
    """Status of a major component"""
    name: str
    files: List[str] = field(default_factory=list)
    total_lines: int = 0
    stub_count: int = 0
    todo_count: int = 0
    health_points: int = 100
    completion_percentage: float = 100.0
    missing_dependencies: List[str] = field(default_factory=list)
    status: str = 'unknown'  # 'complete', 'partial', 'stub', 'missing'

@dataclass
class ProjectManifest:
    """Complete project manifest"""
    project_name: str
    src_root: str
    analysis_timestamp: str
    total_files: int = 0
    total_lines: int = 0
    total_code_lines: int = 0
    total_stubs: int = 0
    total_todos: int = 0
    overall_health: int = 100
    overall_completion: float = 100.0
    files_by_category: Dict[str, List[str]] = field(default_factory=dict)
    components: Dict[str, ComponentStatus] = field(default_factory=dict)
    file_analyses: Dict[str, FileAnalysis] = field(default_factory=dict)
    missing_features: List[str] = field(default_factory=list)
    recommendations: List[str] = field(default_factory=list)
    critical_issues: List[str] = field(default_factory=list)


# ============================================================================
# ANALYZERS
# ============================================================================

class FileAnalyzer:
    """Analyzes individual source files"""
    
    def __init__(self, config: Config):
        self.config = config
        self.stub_patterns = [re.compile(p, re.IGNORECASE) for p in config.STUB_PATTERNS]
        self.complete_patterns = [re.compile(p, re.IGNORECASE) for p in config.COMPLETE_PATTERNS]
    
    def analyze_file(self, file_path: Path, src_root: Path) -> Optional[FileAnalysis]:
        """Analyze a single file"""
        try:
            if not file_path.exists():
                return None
            
            relative_path = str(file_path.relative_to(src_root))
            extension = file_path.suffix.lower()
            
            # Read file content
            try:
                with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
                    content = f.read()
                    lines = content.split('\n')
            except Exception as e:
                print(f"Warning: Could not read {file_path}: {e}")
                return None
            
            # Basic stats
            size_bytes = file_path.stat().st_size
            line_count = len(lines)
            last_modified = datetime.fromtimestamp(file_path.stat().st_mtime).isoformat()
            
            # Calculate hash
            file_hash = hashlib.md5(content.encode()).hexdigest()
            
            # Count line types
            code_lines = 0
            comment_lines = 0
            blank_lines = 0
            in_multiline_comment = False
            
            for line in lines:
                stripped = line.strip()
                if not stripped:
                    blank_lines += 1
                elif in_multiline_comment:
                    comment_lines += 1
                    if '*/' in stripped:
                        in_multiline_comment = False
                elif stripped.startswith('/*'):
                    comment_lines += 1
                    if '*/' not in stripped:
                        in_multiline_comment = True
                elif stripped.startswith('//') or stripped.startswith('#'):
                    comment_lines += 1
                else:
                    code_lines += 1
            
            # Find stubs
            stubs = self._find_stubs(lines)
            
            # Find TODOs
            todos = self._find_todos(lines)
            
            # Find functions (for C/C++)
            functions = []
            if extension in ['.cpp', '.c', '.h', '.hpp']:
                functions = self._find_functions(content, lines)
            
            # Calculate health points
            stub_penalty = len(stubs) * 5
            todo_penalty = len(todos) * 2
            health_points = max(0, 100 - stub_penalty - todo_penalty)
            
            # Calculate completion percentage
            completion_pct = self._calculate_completion(content, stubs, todos, functions)
            
            # Determine category
            category = self._determine_category(relative_path)
            
            # Check if complete
            is_complete = len(stubs) == 0 and len(todos) == 0 and completion_pct >= 90
            
            return FileAnalysis(
                path=str(file_path),
                relative_path=relative_path,
                extension=extension,
                size_bytes=size_bytes,
                line_count=line_count,
                code_lines=code_lines,
                comment_lines=comment_lines,
                blank_lines=blank_lines,
                stubs=stubs,
                todos=todos,
                functions=functions,
                health_points=health_points,
                category=category,
                is_complete=is_complete,
                completion_percentage=completion_pct,
                hash=file_hash,
                last_modified=last_modified
            )
            
        except Exception as e:
            print(f"Error analyzing {file_path}: {e}")
            return None
    
    def _find_stubs(self, lines: List[str]) -> List[StubInfo]:
        """Find all stub indicators in the file"""
        stubs = []
        
        for i, line in enumerate(lines, 1):
            for pattern in self.stub_patterns:
                if pattern.search(line):
                    severity = self._determine_severity(line)
                    stubs.append(StubInfo(
                        line_number=i,
                        line_content=line.strip()[:100],
                        pattern_matched=pattern.pattern,
                        severity=severity
                    ))
                    break  # Only count once per line
        
        return stubs
    
    def _find_todos(self, lines: List[str]) -> List[Tuple[int, str]]:
        """Find all TODO/FIXME comments"""
        todos = []
        todo_pattern = re.compile(r'(TODO|FIXME|HACK|XXX)\s*:?\s*(.*)', re.IGNORECASE)
        
        for i, line in enumerate(lines, 1):
            match = todo_pattern.search(line)
            if match:
                todos.append((i, match.group(0).strip()[:100]))
        
        return todos
    
    def _find_functions(self, content: str, lines: List[str]) -> List[FunctionInfo]:
        """Find function definitions in C/C++ code"""
        functions = []
        
        # Pattern for function definitions
        func_pattern = re.compile(
            r'^(?:(?:inline|static|virtual|explicit|constexpr|extern)\s+)*'
            r'([\w:*&<>\s]+?)\s+'
            r'([\w:]+)\s*\('
            r'([^)]*)\)\s*'
            r'(?:const|noexcept|override|final|\s)*'
            r'\s*\{',
            re.MULTILINE
        )
        
        for match in func_pattern.finditer(content):
            start_pos = match.start()
            start_line = content[:start_pos].count('\n') + 1
            
            # Find the matching closing brace
            brace_count = 1
            end_line = start_line
            search_start = match.end()
            
            for i, char in enumerate(content[search_start:], search_start):
                if char == '{':
                    brace_count += 1
                elif char == '}':
                    brace_count -= 1
                    if brace_count == 0:
                        end_line = content[:i].count('\n') + 1
                        break
            
            # Get function body
            func_body = '\n'.join(lines[start_line-1:end_line])
            
            # Check if stub
            is_stub = any(p.search(func_body) for p in self.stub_patterns)
            stub_indicators = []
            if is_stub:
                for p in self.stub_patterns:
                    if p.search(func_body):
                        stub_indicators.append(p.pattern)
            
            # Calculate implementation score
            impl_score = self._calculate_function_score(func_body)
            
            functions.append(FunctionInfo(
                name=match.group(2),
                line_start=start_line,
                line_end=end_line,
                return_type=match.group(1).strip(),
                is_stub=is_stub,
                stub_indicators=stub_indicators,
                implementation_score=impl_score
            ))
        
        return functions
    
    def _determine_severity(self, line: str) -> str:
        """Determine the severity of a stub"""
        line_lower = line.lower()
        if 'critical' in line_lower or 'important' in line_lower:
            return 'critical'
        elif 'fixme' in line_lower or 'bug' in line_lower:
            return 'high'
        elif 'todo' in line_lower:
            return 'medium'
        else:
            return 'low'
    
    def _calculate_completion(self, content: str, stubs: List[StubInfo], 
                              todos: List[Tuple[int, str]], 
                              functions: List[FunctionInfo]) -> float:
        """Calculate overall completion percentage for a file"""
        if not content.strip():
            return 0.0
        
        # Base score
        score = 100.0
        
        # Deduct for stubs
        for stub in stubs:
            if stub.severity == 'critical':
                score -= 20
            elif stub.severity == 'high':
                score -= 10
            elif stub.severity == 'medium':
                score -= 5
            else:
                score -= 2
        
        # Deduct for TODOs
        score -= len(todos) * 2
        
        # Check function implementations
        if functions:
            stub_funcs = sum(1 for f in functions if f.is_stub)
            total_funcs = len(functions)
            func_completion = ((total_funcs - stub_funcs) / total_funcs) * 100
            score = (score + func_completion) / 2
        
        return max(0.0, min(100.0, score))
    
    def _calculate_function_score(self, body: str) -> int:
        """Calculate implementation score for a function body"""
        lines = [l.strip() for l in body.split('\n') if l.strip()]
        
        if len(lines) <= 2:  # Empty or near-empty
            return 5
        
        # Check for stub patterns
        stub_count = sum(1 for p in self.stub_patterns if p.search(body))
        if stub_count > 0:
            return max(5, 50 - (stub_count * 10))
        
        # Check for proper implementation
        has_logic = any(kw in body for kw in ['if', 'for', 'while', 'switch', 'try'])
        has_return = 'return ' in body and 'return nullptr' not in body and 'return 0' not in body
        
        score = 50
        if has_logic:
            score += 25
        if has_return:
            score += 15
        if len(lines) > 10:
            score += 10
        
        return min(100, score)
    
    def _determine_category(self, relative_path: str) -> str:
        """Determine which category a file belongs to"""
        path_lower = relative_path.lower()
        
        for category, patterns in Config.FEATURE_CATEGORIES.items():
            if any(p in path_lower for p in patterns):
                return category
        
        return 'other'


class ComponentAnalyzer:
    """Analyzes project components and their relationships"""
    
    def __init__(self, config: Config):
        self.config = config
    
    def analyze_components(self, file_analyses: Dict[str, FileAnalysis]) -> Dict[str, ComponentStatus]:
        """Analyze all major components"""
        components = {}
        
        # Check critical components
        for name, expected_path in self.config.CRITICAL_COMPONENTS.items():
            status = self._check_component(name, expected_path, file_analyses)
            components[name] = status
        
        # Group by category and create component summaries
        for category in self.config.FEATURE_CATEGORIES.keys():
            category_files = [
                path for path, analysis in file_analyses.items()
                if analysis.category == category
            ]
            
            if category_files:
                status = self._create_category_component(category, category_files, file_analyses)
                components[f"Category_{category}"] = status
        
        return components
    
    def _check_component(self, name: str, expected_path: str, 
                         file_analyses: Dict[str, FileAnalysis]) -> ComponentStatus:
        """Check status of a critical component"""
        
        # Try to find the file
        matching_files = [
            path for path in file_analyses.keys()
            if expected_path in path or name.lower() in path.lower()
        ]
        
        if not matching_files:
            return ComponentStatus(
                name=name,
                status='missing',
                health_points=0,
                completion_percentage=0.0
            )
        
        # Aggregate stats from matching files
        total_lines = 0
        total_stubs = 0
        total_todos = 0
        files = []
        
        for path in matching_files:
            analysis = file_analyses[path]
            files.append(path)
            total_lines += analysis.line_count
            total_stubs += len(analysis.stubs)
            total_todos += len(analysis.todos)
        
        # Calculate health
        health = 100 - (total_stubs * 5) - (total_todos * 2)
        health = max(0, min(100, health))
        
        # Determine status
        if total_stubs == 0 and total_todos == 0:
            status = 'complete'
        elif total_stubs < 5:
            status = 'partial'
        else:
            status = 'stub'
        
        # Calculate completion
        completion = 100 - (total_stubs * 3) - (total_todos * 1)
        completion = max(0.0, min(100.0, completion))
        
        return ComponentStatus(
            name=name,
            files=files,
            total_lines=total_lines,
            stub_count=total_stubs,
            todo_count=total_todos,
            health_points=health,
            completion_percentage=completion,
            status=status
        )
    
    def _create_category_component(self, category: str, files: List[str],
                                    file_analyses: Dict[str, FileAnalysis]) -> ComponentStatus:
        """Create component status for a category"""
        total_lines = 0
        total_stubs = 0
        total_todos = 0
        
        for path in files:
            if path in file_analyses:
                analysis = file_analyses[path]
                total_lines += analysis.line_count
                total_stubs += len(analysis.stubs)
                total_todos += len(analysis.todos)
        
        health = 100 - (total_stubs * 2) - (total_todos * 1)
        health = max(0, min(100, health))
        
        completion = 100 - (total_stubs * 2) - (total_todos * 0.5)
        completion = max(0.0, min(100.0, completion))
        
        if total_stubs == 0 and total_todos == 0:
            status = 'complete'
        elif total_stubs < 10:
            status = 'partial'
        else:
            status = 'stub'
        
        return ComponentStatus(
            name=f"Category: {category.title()}",
            files=files,
            total_lines=total_lines,
            stub_count=total_stubs,
            todo_count=total_todos,
            health_points=health,
            completion_percentage=completion,
            status=status
        )


# ============================================================================
# MAIN DIGESTION ENGINE
# ============================================================================

class SourceDigestionEngine:
    """Main engine for digesting and analyzing the source codebase"""
    
    def __init__(self, src_root: str, max_workers: int = 8):
        self.src_root = Path(src_root)
        self.config = Config()
        self.file_analyzer = FileAnalyzer(self.config)
        self.component_analyzer = ComponentAnalyzer(self.config)
        self.max_workers = max_workers
        self.manifest: Optional[ProjectManifest] = None
        self._lock = threading.Lock()
    
    def digest(self) -> ProjectManifest:
        """Perform full source digestion"""
        print(f"\n{'='*60}")
        print(f"RawrXD IDE Source Digestion System")
        print(f"{'='*60}")
        print(f"Source Root: {self.src_root}")
        print(f"Started: {datetime.now().isoformat()}")
        print(f"{'='*60}\n")
        
        # Initialize manifest
        self.manifest = ProjectManifest(
            project_name="RawrXD IDE",
            src_root=str(self.src_root),
            analysis_timestamp=datetime.now().isoformat()
        )
        
        # Phase 1: Discover all source files
        print("Phase 1: Discovering source files...")
        source_files = self._discover_files()
        print(f"  Found {len(source_files)} source files")
        
        # Phase 2: Analyze files in parallel
        print("\nPhase 2: Analyzing files...")
        self._analyze_files_parallel(source_files)
        
        # Phase 3: Analyze components
        print("\nPhase 3: Analyzing components...")
        self.manifest.components = self.component_analyzer.analyze_components(
            self.manifest.file_analyses
        )
        
        # Phase 4: Calculate overall metrics
        print("\nPhase 4: Calculating metrics...")
        self._calculate_overall_metrics()
        
        # Phase 5: Generate recommendations
        print("\nPhase 5: Generating recommendations...")
        self._generate_recommendations()
        
        # Phase 6: Self-audit
        print("\nPhase 6: Running self-audit...")
        self._self_audit()
        
        print(f"\n{'='*60}")
        print("Digestion Complete!")
        print(f"{'='*60}\n")
        
        return self.manifest
    
    def _discover_files(self) -> List[Path]:
        """Discover all source files"""
        source_files = []
        
        for ext in self.config.SOURCE_EXTENSIONS:
            source_files.extend(self.src_root.rglob(f"*{ext}"))
        
        # Filter out build directories and generated files
        filtered = []
        exclude_patterns = ['build/', 'obj/', 'bin/', '.git/', 'CMakeFiles/', 
                          'autogen/', '.dir/', 'Debug/', 'Release/', 'x64/']
        
        for f in source_files:
            path_str = str(f)
            if not any(p in path_str for p in exclude_patterns):
                filtered.append(f)
        
        return filtered
    
    def _analyze_files_parallel(self, files: List[Path]):
        """Analyze files in parallel using thread pool"""
        total = len(files)
        completed = 0
        
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            futures = {
                executor.submit(self.file_analyzer.analyze_file, f, self.src_root): f
                for f in files
            }
            
            for future in as_completed(futures):
                file_path = futures[future]
                try:
                    analysis = future.result()
                    if analysis:
                        with self._lock:
                            self.manifest.file_analyses[analysis.relative_path] = analysis
                            
                            # Update category tracking
                            if analysis.category not in self.manifest.files_by_category:
                                self.manifest.files_by_category[analysis.category] = []
                            self.manifest.files_by_category[analysis.category].append(
                                analysis.relative_path
                            )
                except Exception as e:
                    print(f"  Error analyzing {file_path}: {e}")
                
                completed += 1
                if completed % 50 == 0:
                    print(f"  Analyzed {completed}/{total} files...")
        
        print(f"  Analyzed {completed} files total")
    
    def _calculate_overall_metrics(self):
        """Calculate overall project metrics"""
        total_files = 0
        total_lines = 0
        total_code_lines = 0
        total_stubs = 0
        total_todos = 0
        total_health = 0
        total_completion = 0.0
        
        for analysis in self.manifest.file_analyses.values():
            total_files += 1
            total_lines += analysis.line_count
            total_code_lines += analysis.code_lines
            total_stubs += len(analysis.stubs)
            total_todos += len(analysis.todos)
            total_health += analysis.health_points
            total_completion += analysis.completion_percentage
        
        self.manifest.total_files = total_files
        self.manifest.total_lines = total_lines
        self.manifest.total_code_lines = total_code_lines
        self.manifest.total_stubs = total_stubs
        self.manifest.total_todos = total_todos
        
        if total_files > 0:
            self.manifest.overall_health = int(total_health / total_files)
            self.manifest.overall_completion = total_completion / total_files
        
        print(f"  Total files: {total_files}")
        print(f"  Total lines: {total_lines:,}")
        print(f"  Total code lines: {total_code_lines:,}")
        print(f"  Total stubs found: {total_stubs}")
        print(f"  Total TODOs found: {total_todos}")
        print(f"  Overall health: {self.manifest.overall_health}%")
        print(f"  Overall completion: {self.manifest.overall_completion:.1f}%")
    
    def _generate_recommendations(self):
        """Generate recommendations based on analysis"""
        recommendations = []
        critical_issues = []
        missing_features = []
        
        # Check for critical missing components
        for name, status in self.manifest.components.items():
            if status.status == 'missing':
                critical_issues.append(f"CRITICAL: {name} component is missing")
                missing_features.append(name)
            elif status.status == 'stub':
                critical_issues.append(f"HIGH: {name} is mostly stubs ({status.stub_count} stubs)")
                recommendations.append(f"Implement {name} - currently at {status.completion_percentage:.1f}%")
        
        # Find files with most stubs
        stub_files = sorted(
            [(path, len(a.stubs)) for path, a in self.manifest.file_analyses.items() if a.stubs],
            key=lambda x: x[1],
            reverse=True
        )[:10]
        
        if stub_files:
            recommendations.append("Top files needing stub implementation:")
            for path, count in stub_files:
                recommendations.append(f"  - {path}: {count} stubs")
        
        # Check for categories with low completion
        for category, files in self.manifest.files_by_category.items():
            avg_completion = sum(
                self.manifest.file_analyses[f].completion_percentage
                for f in files if f in self.manifest.file_analyses
            ) / len(files) if files else 0
            
            if avg_completion < 70:
                recommendations.append(
                    f"Category '{category}' has low completion ({avg_completion:.1f}%)"
                )
        
        # Security recommendations
        if 'security' not in self.manifest.files_by_category or \
           len(self.manifest.files_by_category.get('security', [])) < 3:
            recommendations.append("Consider adding more security-related files")
        
        self.manifest.recommendations = recommendations
        self.manifest.critical_issues = critical_issues
        self.manifest.missing_features = missing_features
        
        print(f"  Generated {len(recommendations)} recommendations")
        print(f"  Found {len(critical_issues)} critical issues")
    
    def _self_audit(self):
        """Perform self-audit of the digestion system"""
        print("\n  Self-Audit Results:")
        
        # Check manifest integrity
        if self.manifest.total_files == 0:
            print("    ❌ No files analyzed - check source root path")
        else:
            print(f"    ✓ Analyzed {self.manifest.total_files} files")
        
        # Check for analysis coverage
        expected_categories = set(self.config.FEATURE_CATEGORIES.keys())
        found_categories = set(self.manifest.files_by_category.keys())
        missing_categories = expected_categories - found_categories
        
        if missing_categories:
            print(f"    ⚠ Missing categories: {missing_categories}")
        else:
            print(f"    ✓ All expected categories found")
        
        # Check component coverage
        total_components = len(self.config.CRITICAL_COMPONENTS)
        found_components = sum(
            1 for name, status in self.manifest.components.items()
            if status.status != 'missing' and not name.startswith('Category_')
        )
        print(f"    ✓ Component coverage: {found_components}/{total_components}")
        
        # Validate metrics
        if self.manifest.overall_health >= 0 and self.manifest.overall_health <= 100:
            print(f"    ✓ Health metrics valid: {self.manifest.overall_health}%")
        else:
            print(f"    ❌ Invalid health metric: {self.manifest.overall_health}")
        
        print("    ✓ Self-audit complete")
    
    def export_manifest(self, output_path: str, format: str = 'json'):
        """Export manifest to file"""
        if not self.manifest:
            raise ValueError("No manifest to export - run digest() first")
        
        if format == 'json':
            self._export_json(output_path)
        elif format == 'markdown':
            self._export_markdown(output_path)
        elif format == 'html':
            self._export_html(output_path)
        else:
            raise ValueError(f"Unknown format: {format}")
    
    def _export_json(self, output_path: str):
        """Export manifest as JSON"""
        # Convert dataclasses to dicts
        def convert(obj):
            if isinstance(obj, (FileAnalysis, ComponentStatus, ProjectManifest, StubInfo, FunctionInfo)):
                return asdict(obj)
            elif isinstance(obj, dict):
                return {k: convert(v) for k, v in obj.items()}
            elif isinstance(obj, list):
                return [convert(v) for v in obj]
            return obj
        
        data = convert(self.manifest)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, default=str)
        
        print(f"Exported JSON manifest to: {output_path}")
    
    def _export_markdown(self, output_path: str):
        """Export manifest as Markdown report"""
        lines = []
        
        # Header
        lines.append("# RawrXD IDE Source Digestion Report")
        lines.append("")
        lines.append(f"**Generated:** {self.manifest.analysis_timestamp}")
        lines.append(f"**Source Root:** `{self.manifest.src_root}`")
        lines.append("")
        
        # Overview
        lines.append("## Overview")
        lines.append("")
        lines.append("| Metric | Value |")
        lines.append("|--------|-------|")
        lines.append(f"| Total Files | {self.manifest.total_files:,} |")
        lines.append(f"| Total Lines | {self.manifest.total_lines:,} |")
        lines.append(f"| Code Lines | {self.manifest.total_code_lines:,} |")
        lines.append(f"| Total Stubs | {self.manifest.total_stubs} |")
        lines.append(f"| Total TODOs | {self.manifest.total_todos} |")
        lines.append(f"| Overall Health | {self.manifest.overall_health}% |")
        lines.append(f"| Overall Completion | {self.manifest.overall_completion:.1f}% |")
        lines.append("")
        
        # Health visualization
        lines.append("### Health Status")
        lines.append("")
        health = self.manifest.overall_health
        health_bar = "█" * (health // 5) + "░" * (20 - health // 5)
        if health >= 80:
            status = "🟢 Excellent"
        elif health >= 60:
            status = "🟡 Good"
        elif health >= 40:
            status = "🟠 Needs Work"
        else:
            status = "🔴 Critical"
        lines.append(f"```")
        lines.append(f"[{health_bar}] {health}% - {status}")
        lines.append(f"```")
        lines.append("")
        
        # Critical Issues
        if self.manifest.critical_issues:
            lines.append("## ⚠️ Critical Issues")
            lines.append("")
            for issue in self.manifest.critical_issues:
                lines.append(f"- {issue}")
            lines.append("")
        
        # Component Status
        lines.append("## Component Status")
        lines.append("")
        lines.append("| Component | Status | Health | Stubs | Completion |")
        lines.append("|-----------|--------|--------|-------|------------|")
        
        for name, status in sorted(self.manifest.components.items()):
            if not name.startswith('Category_'):
                status_emoji = {
                    'complete': '✅',
                    'partial': '🟡',
                    'stub': '🟠',
                    'missing': '❌'
                }.get(status.status, '❓')
                lines.append(
                    f"| {name} | {status_emoji} {status.status} | "
                    f"{status.health_points}% | {status.stub_count} | "
                    f"{status.completion_percentage:.1f}% |"
                )
        lines.append("")
        
        # Category Summary
        lines.append("## Category Summary")
        lines.append("")
        lines.append("| Category | Files | Status |")
        lines.append("|----------|-------|--------|")
        
        for category, files in sorted(self.manifest.files_by_category.items()):
            avg_completion = sum(
                self.manifest.file_analyses[f].completion_percentage
                for f in files if f in self.manifest.file_analyses
            ) / len(files) if files else 0
            
            if avg_completion >= 90:
                status = "✅ Complete"
            elif avg_completion >= 70:
                status = "🟡 Good"
            elif avg_completion >= 50:
                status = "🟠 Partial"
            else:
                status = "🔴 Needs Work"
            
            lines.append(f"| {category.title()} | {len(files)} | {status} ({avg_completion:.0f}%) |")
        lines.append("")
        
        # Files with most stubs
        lines.append("## Files Needing Attention")
        lines.append("")
        stub_files = sorted(
            [(path, len(a.stubs), a.completion_percentage) 
             for path, a in self.manifest.file_analyses.items() if a.stubs],
            key=lambda x: x[1],
            reverse=True
        )[:20]
        
        if stub_files:
            lines.append("| File | Stubs | Completion |")
            lines.append("|------|-------|------------|")
            for path, stub_count, completion in stub_files:
                lines.append(f"| `{path}` | {stub_count} | {completion:.1f}% |")
        else:
            lines.append("*No files with stubs found!*")
        lines.append("")
        
        # Recommendations
        lines.append("## Recommendations")
        lines.append("")
        if self.manifest.recommendations:
            for rec in self.manifest.recommendations:
                lines.append(f"- {rec}")
        else:
            lines.append("*No recommendations at this time.*")
        lines.append("")
        
        # Footer
        lines.append("---")
        lines.append("*Generated by RawrXD IDE Source Digestion System*")
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write('\n'.join(lines))
        
        print(f"Exported Markdown report to: {output_path}")
    
    def _export_html(self, output_path: str):
        """Export manifest as HTML report"""
        # Simple HTML export
        html = f"""<!DOCTYPE html>
<html>
<head>
    <title>RawrXD IDE Source Digestion Report</title>
    <style>
        body {{ font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; 
               max-width: 1200px; margin: 0 auto; padding: 20px; background: #1e1e1e; color: #d4d4d4; }}
        h1, h2 {{ color: #569cd6; }}
        table {{ border-collapse: collapse; width: 100%; margin: 20px 0; }}
        th, td {{ border: 1px solid #3c3c3c; padding: 8px; text-align: left; }}
        th {{ background: #252526; }}
        .health-bar {{ background: #3c3c3c; height: 20px; border-radius: 10px; overflow: hidden; }}
        .health-fill {{ height: 100%; transition: width 0.3s; }}
        .health-excellent {{ background: #4ec9b0; }}
        .health-good {{ background: #dcdcaa; }}
        .health-warning {{ background: #ce9178; }}
        .health-critical {{ background: #f44747; }}
        .metric-card {{ background: #252526; padding: 15px; border-radius: 8px; margin: 10px 0; }}
        .metric-value {{ font-size: 2em; color: #4ec9b0; }}
        .grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px; }}
    </style>
</head>
<body>
    <h1>🔍 RawrXD IDE Source Digestion Report</h1>
    <p>Generated: {self.manifest.analysis_timestamp}</p>
    
    <div class="grid">
        <div class="metric-card">
            <div>Total Files</div>
            <div class="metric-value">{self.manifest.total_files:,}</div>
        </div>
        <div class="metric-card">
            <div>Total Lines</div>
            <div class="metric-value">{self.manifest.total_lines:,}</div>
        </div>
        <div class="metric-card">
            <div>Total Stubs</div>
            <div class="metric-value">{self.manifest.total_stubs}</div>
        </div>
        <div class="metric-card">
            <div>Overall Health</div>
            <div class="metric-value">{self.manifest.overall_health}%</div>
        </div>
    </div>
    
    <h2>Health Status</h2>
    <div class="health-bar">
        <div class="health-fill health-{'excellent' if self.manifest.overall_health >= 80 else 'good' if self.manifest.overall_health >= 60 else 'warning' if self.manifest.overall_health >= 40 else 'critical'}" 
             style="width: {self.manifest.overall_health}%"></div>
    </div>
    
    <h2>Component Status</h2>
    <table>
        <tr><th>Component</th><th>Status</th><th>Health</th><th>Stubs</th><th>Completion</th></tr>
        {''.join(f"<tr><td>{name}</td><td>{status.status}</td><td>{status.health_points}%</td><td>{status.stub_count}</td><td>{status.completion_percentage:.1f}%</td></tr>" for name, status in self.manifest.components.items() if not name.startswith('Category_'))}
    </table>
    
    <h2>Recommendations</h2>
    <ul>
        {''.join(f"<li>{rec}</li>" for rec in self.manifest.recommendations)}
    </ul>
</body>
</html>"""
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html)
        
        print(f"Exported HTML report to: {output_path}")
    
    def print_summary(self):
        """Print a summary to console"""
        if not self.manifest:
            print("No manifest available - run digest() first")
            return
        
        print("\n" + "="*70)
        print("                    SOURCE DIGESTION SUMMARY")
        print("="*70)
        
        # Overall stats
        print(f"\n📊 OVERALL STATISTICS")
        print(f"   Total Files:      {self.manifest.total_files:,}")
        print(f"   Total Lines:      {self.manifest.total_lines:,}")
        print(f"   Code Lines:       {self.manifest.total_code_lines:,}")
        print(f"   Total Stubs:      {self.manifest.total_stubs}")
        print(f"   Total TODOs:      {self.manifest.total_todos}")
        
        # Health
        health = self.manifest.overall_health
        health_bar = "█" * (health // 5) + "░" * (20 - health // 5)
        print(f"\n💪 HEALTH: [{health_bar}] {health}%")
        
        # Completion
        completion = self.manifest.overall_completion
        comp_bar = "█" * int(completion // 5) + "░" * (20 - int(completion // 5))
        print(f"✅ COMPLETION: [{comp_bar}] {completion:.1f}%")
        
        # Critical issues
        if self.manifest.critical_issues:
            print(f"\n⚠️  CRITICAL ISSUES ({len(self.manifest.critical_issues)}):")
            for issue in self.manifest.critical_issues[:5]:
                print(f"   • {issue}")
        
        # Top recommendations
        if self.manifest.recommendations:
            print(f"\n📝 TOP RECOMMENDATIONS:")
            for rec in self.manifest.recommendations[:5]:
                print(f"   • {rec}")
        
        print("\n" + "="*70)


# ============================================================================
# CLI INTERFACE
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="RawrXD IDE Source Digestion System",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python source_digestion_system.py --src-dir "D:\\RawrXD-production-lazy-init\\src"
  python source_digestion_system.py --full-audit --output report.md
  python source_digestion_system.py --self-check
        """
    )
    
    parser.add_argument(
        '--src-dir',
        default='D:\\RawrXD-production-lazy-init\\src',
        help='Source directory to analyze'
    )
    
    parser.add_argument(
        '--output', '-o',
        help='Output file path for the report'
    )
    
    parser.add_argument(
        '--format', '-f',
        choices=['json', 'markdown', 'html'],
        default='markdown',
        help='Output format (default: markdown)'
    )
    
    parser.add_argument(
        '--full-audit',
        action='store_true',
        help='Run full audit with all checks'
    )
    
    parser.add_argument(
        '--self-check',
        action='store_true',
        help='Run self-check on the digestion system'
    )
    
    parser.add_argument(
        '--workers', '-w',
        type=int,
        default=8,
        help='Number of parallel workers (default: 8)'
    )
    
    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Verbose output'
    )
    
    args = parser.parse_args()
    
    # Initialize engine
    engine = SourceDigestionEngine(args.src_dir, max_workers=args.workers)
    
    # Run digestion
    manifest = engine.digest()
    
    # Print summary
    engine.print_summary()
    
    # Export if output specified
    if args.output:
        engine.export_manifest(args.output, args.format)
    else:
        # Default output
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        default_output = f"D:\\RawrXD-production-lazy-init\\SOURCE_DIGESTION_REPORT_{timestamp}.md"
        engine.export_manifest(default_output, 'markdown')
        
        # Also export JSON
        json_output = f"D:\\RawrXD-production-lazy-init\\source_digestion_manifest_{timestamp}.json"
        engine.export_manifest(json_output, 'json')


if __name__ == '__main__':
    main()
