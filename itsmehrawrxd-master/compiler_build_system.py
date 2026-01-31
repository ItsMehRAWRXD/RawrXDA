#!/usr/bin/env python3
"""
Compiler Build System for n0mn0m IDE
Complete build system with compilation, linking, and dependency management
"""

import os
import sys
import subprocess
import threading
import time
import json
import hashlib
import shutil
from pathlib import Path
from typing import Dict, List, Optional, Union, Any, Tuple, Callable
from enum import Enum
from dataclasses import dataclass
import logging
import tempfile
import queue

class BuildTarget(Enum):
    """Build targets"""
    DEBUG = "debug"
    RELEASE = "release"
    PROFILE = "profile"
    OPTIMIZED = "optimized"

class BuildStatus(Enum):
    """Build status"""
    IDLE = "idle"
    BUILDING = "building"
    SUCCESS = "success"
    FAILED = "failed"
    CANCELLED = "cancelled"

@dataclass
class BuildConfig:
    """Build configuration"""
    target: BuildTarget
    optimization_level: int = 2
    debug_info: bool = True
    warnings: bool = True
    static_analysis: bool = False
    parallel_build: bool = True
    max_parallel_jobs: int = 4
    output_directory: str = "build"
    intermediate_directory: str = "obj"
    dependencies_directory: str = "deps"

@dataclass
class SourceFile:
    """Source file information"""
    path: Path
    language: str
    compiler: str
    flags: List[str]
    dependencies: List[Path]
    output_file: Optional[Path] = None

@dataclass
class BuildResult:
    """Build result"""
    status: BuildStatus
    start_time: float
    end_time: float
    output_files: List[Path]
    errors: List[str]
    warnings: List[str]
    build_log: str

class CompilerBuildSystem:
    """
    Comprehensive build system for n0mn0m IDE
    Advanced compilation, linking, and dependency management
    """
    
    def __init__(self):
        self.build_config = BuildConfig(BuildTarget.DEBUG)
        self.source_files = {}
        self.dependencies = {}
        self.build_cache = {}
        self.build_queue = queue.Queue()
        self.build_thread = None
        self.build_status = BuildStatus.IDLE
        
        # Build tools
        self.compilers = {
            'c': 'gcc',
            'cpp': 'g++',
            'python': 'python',
            'javascript': 'node',
            'java': 'javac',
            'csharp': 'csc',
            'rust': 'rustc',
            'go': 'go',
            'eon': 'eon-compiler'
        }
        
        # Build statistics
        self.build_stats = {
            'total_builds': 0,
            'successful_builds': 0,
            'failed_builds': 0,
            'average_build_time': 0.0,
            'last_build_time': 0.0
        }
        
        # Event handlers
        self.event_handlers = {
            'build_started': [],
            'build_completed': [],
            'build_failed': [],
            'file_compiled': [],
            'dependency_resolved': []
        }
        
        # Setup logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)
        
        print("🔨 Build System initialized")
    
    def set_build_config(self, config: BuildConfig):
        """Set build configuration"""
        
        self.build_config = config
        self.logger.info(f"Build configuration set: {config.target.value}")
    
    def add_source_file(self, file_path: Union[str, Path], 
                       language: str,
                       compiler: Optional[str] = None,
                       flags: Optional[List[str]] = None) -> bool:
        """Add source file to build"""
        
        try:
            file_path = Path(file_path)
            
            if not file_path.exists():
                self.logger.error(f"Source file not found: {file_path}")
                return False
            
            # Auto-detect compiler if not specified
            if compiler is None:
                compiler = self.compilers.get(language, language)
            
            # Default flags
            if flags is None:
                flags = self._get_default_flags(language)
            
            source_file = SourceFile(
                path=file_path,
                language=language,
                compiler=compiler,
                flags=flags,
                dependencies=[]
            )
            
            self.source_files[str(file_path)] = source_file
            
            # Scan dependencies
            self._scan_dependencies(source_file)
            
            self.logger.info(f"Added source file: {file_path} ({language})")
            return True
            
        except Exception as e:
            self.logger.error(f"Error adding source file: {e}")
            return False
    
    def remove_source_file(self, file_path: Union[str, Path]) -> bool:
        """Remove source file from build"""
        
        try:
            file_path = str(Path(file_path))
            
            if file_path in self.source_files:
                del self.source_files[file_path]
                self.logger.info(f"Removed source file: {file_path}")
                return True
            else:
                self.logger.warning(f"Source file not found: {file_path}")
                return False
                
        except Exception as e:
            self.logger.error(f"Error removing source file: {e}")
            return False
    
    def scan_project(self, project_path: Union[str, Path]) -> int:
        """Scan project directory for source files"""
        
        try:
            project_path = Path(project_path)
            
            if not project_path.exists() or not project_path.is_dir():
                self.logger.error(f"Project path not found: {project_path}")
                return 0
            
            file_count = 0
            file_extensions = {
                '.c': 'c',
                '.cpp': 'cpp',
                '.cc': 'cpp',
                '.cxx': 'cpp',
                '.py': 'python',
                '.js': 'javascript',
                '.ts': 'typescript',
                '.java': 'java',
                '.cs': 'csharp',
                '.rs': 'rust',
                '.go': 'go',
                '.eon': 'eon'
            }
            
            # Recursively scan for source files
            for file_path in project_path.rglob('*'):
                if file_path.is_file():
                    ext = file_path.suffix.lower()
                    if ext in file_extensions:
                        language = file_extensions[ext]
                        if self.add_source_file(file_path, language):
                            file_count += 1
            
            self.logger.info(f"Scanned project: {file_count} source files found")
            return file_count
            
        except Exception as e:
            self.logger.error(f"Error scanning project: {e}")
            return 0
    
    def build_project(self, target: Optional[BuildTarget] = None) -> BuildResult:
        """Build the entire project"""
        
        try:
            if self.build_status == BuildStatus.BUILDING:
                self.logger.warning("Build already in progress")
                return BuildResult(
                    status=BuildStatus.FAILED,
                    start_time=time.time(),
                    end_time=time.time(),
                    output_files=[],
                    errors=["Build already in progress"],
                    warnings=[],
                    build_log=""
                )
            
            # Set target if specified
            if target:
                self.build_config.target = target
            
            start_time = time.time()
            self.build_status = BuildStatus.BUILDING
            
            self.logger.info(f"Starting build: {self.build_config.target.value}")
            self._notify_handlers('build_started', {'target': self.build_config.target.value})
            
            # Prepare build directories
            self._prepare_build_directories()
            
            # Build all source files
            build_result = self._build_all_files()
            
            end_time = time.time()
            build_result.start_time = start_time
            build_result.end_time = end_time
            
            # Update build statistics
            self._update_build_statistics(build_result)
            
            if build_result.status == BuildStatus.SUCCESS:
                self.logger.info(f"Build completed successfully in {end_time - start_time:.2f}s")
                self._notify_handlers('build_completed', build_result.__dict__)
            else:
                self.logger.error(f"Build failed: {len(build_result.errors)} errors")
                self._notify_handlers('build_failed', build_result.__dict__)
            
            return build_result
            
        except Exception as e:
            self.logger.error(f"Error building project: {e}")
            return BuildResult(
                status=BuildStatus.FAILED,
                start_time=time.time(),
                end_time=time.time(),
                output_files=[],
                errors=[str(e)],
                warnings=[],
                build_log=""
            )
        finally:
            self.build_status = BuildStatus.IDLE
    
    def build_file(self, file_path: Union[str, Path]) -> BuildResult:
        """Build a single file"""
        
        try:
            file_path = str(Path(file_path))
            
            if file_path not in self.source_files:
                return BuildResult(
                    status=BuildStatus.FAILED,
                    start_time=time.time(),
                    end_time=time.time(),
                    output_files=[],
                    errors=[f"Source file not found: {file_path}"],
                    warnings=[],
                    build_log=""
                )
            
            source_file = self.source_files[file_path]
            start_time = time.time()
            
            self.logger.info(f"Building file: {file_path}")
            
            # Check if rebuild is necessary
            if not self._needs_rebuild(source_file):
                self.logger.info(f"File up to date: {file_path}")
                return BuildResult(
                    status=BuildStatus.SUCCESS,
                    start_time=start_time,
                    end_time=time.time(),
                    output_files=[source_file.output_file] if source_file.output_file else [],
                    errors=[],
                    warnings=[],
                    build_log="File up to date"
                )
            
            # Compile the file
            result = self._compile_file(source_file)
            
            end_time = time.time()
            result.start_time = start_time
            result.end_time = end_time
            
            if result.status == BuildStatus.SUCCESS:
                self._notify_handlers('file_compiled', {
                    'file': file_path,
                    'output': source_file.output_file
                })
            
            return result
            
        except Exception as e:
            self.logger.error(f"Error building file: {e}")
            return BuildResult(
                status=BuildStatus.FAILED,
                start_time=time.time(),
                end_time=time.time(),
                output_files=[],
                errors=[str(e)],
                warnings=[],
                build_log=""
            )
    
    def clean_build(self) -> bool:
        """Clean build artifacts"""
        
        try:
            self.logger.info("Cleaning build artifacts")
            
            # Clean output directory
            output_dir = Path(self.build_config.output_directory)
            if output_dir.exists():
                shutil.rmtree(output_dir)
                self.logger.info(f"Cleaned output directory: {output_dir}")
            
            # Clean intermediate directory
            intermediate_dir = Path(self.build_config.intermediate_directory)
            if intermediate_dir.exists():
                shutil.rmtree(intermediate_dir)
                self.logger.info(f"Cleaned intermediate directory: {intermediate_dir}")
            
            # Clear build cache
            self.build_cache.clear()
            
            self.logger.info("Build cleanup completed")
            return True
            
        except Exception as e:
            self.logger.error(f"Error cleaning build: {e}")
            return False
    
    def get_dependencies(self, file_path: Union[str, Path]) -> List[Path]:
        """Get file dependencies"""
        
        file_path = str(Path(file_path))
        return self.dependencies.get(file_path, [])
    
    def resolve_dependencies(self, file_path: Union[str, Path]) -> bool:
        """Resolve dependencies for a file"""
        
        try:
            file_path = str(Path(file_path))
            
            if file_path not in self.source_files:
                return False
            
            source_file = self.source_files[file_path]
            dependencies = self._scan_dependencies(source_file)
            
            self.dependencies[file_path] = dependencies
            
            self._notify_handlers('dependency_resolved', {
                'file': file_path,
                'dependencies': [str(dep) for dep in dependencies]
            })
            
            return True
            
        except Exception as e:
            self.logger.error(f"Error resolving dependencies: {e}")
            return False
    
    def get_build_statistics(self) -> Dict[str, Any]:
        """Get build statistics"""
        
        return {
            'build_stats': self.build_stats,
            'source_files': len(self.source_files),
            'dependencies': len(self.dependencies),
            'build_cache_size': len(self.build_cache),
            'current_status': self.build_status.value,
            'build_config': {
                'target': self.build_config.target.value,
                'optimization_level': self.build_config.optimization_level,
                'debug_info': self.build_config.debug_info,
                'parallel_build': self.build_config.parallel_build
            }
        }
    
    def add_event_handler(self, event_type: str, handler: Callable):
        """Add event handler"""
        
        if event_type in self.event_handlers:
            self.event_handlers[event_type].append(handler)
            self.logger.info(f"Added event handler for: {event_type}")
    
    def remove_event_handler(self, event_type: str, handler: Callable):
        """Remove event handler"""
        
        if event_type in self.event_handlers and handler in self.event_handlers[event_type]:
            self.event_handlers[event_type].remove(handler)
            self.logger.info(f"Removed event handler for: {event_type}")
    
    def _prepare_build_directories(self):
        """Prepare build directories"""
        
        directories = [
            self.build_config.output_directory,
            self.build_config.intermediate_directory,
            self.build_config.dependencies_directory
        ]
        
        for directory in directories:
            Path(directory).mkdir(parents=True, exist_ok=True)
    
    def _build_all_files(self) -> BuildResult:
        """Build all source files"""
        
        errors = []
        warnings = []
        output_files = []
        build_log = ""
        
        try:
            if self.build_config.parallel_build:
                # Parallel build
                result = self._parallel_build()
            else:
                # Sequential build
                result = self._sequential_build()
            
            return result
            
        except Exception as e:
            self.logger.error(f"Error in build process: {e}")
            return BuildResult(
                status=BuildStatus.FAILED,
                start_time=time.time(),
                end_time=time.time(),
                output_files=[],
                errors=[str(e)],
                warnings=[],
                build_log=""
            )
    
    def _parallel_build(self) -> BuildResult:
        """Parallel build implementation"""
        
        errors = []
        warnings = []
        output_files = []
        
        # Create thread pool for parallel compilation
        max_workers = min(self.build_config.max_parallel_jobs, len(self.source_files))
        
        with threading.ThreadPoolExecutor(max_workers=max_workers) as executor:
            # Submit all compilation tasks
            future_to_file = {
                executor.submit(self.build_file, file_path): file_path
                for file_path in self.source_files.keys()
            }
            
            # Collect results
            for future in concurrent.futures.as_completed(future_to_file):
                file_path = future_to_file[future]
                try:
                    result = future.result()
                    if result.status == BuildStatus.SUCCESS:
                        output_files.extend(result.output_files)
                        warnings.extend(result.warnings)
                    else:
                        errors.extend(result.errors)
                except Exception as e:
                    errors.append(f"Error building {file_path}: {e}")
        
        status = BuildStatus.SUCCESS if not errors else BuildStatus.FAILED
        return BuildResult(
            status=status,
            start_time=time.time(),
            end_time=time.time(),
            output_files=output_files,
            errors=errors,
            warnings=warnings,
            build_log=f"Parallel build with {max_workers} workers"
        )
    
    def _sequential_build(self) -> BuildResult:
        """Sequential build implementation"""
        
        errors = []
        warnings = []
        output_files = []
        
        for file_path in self.source_files.keys():
            result = self.build_file(file_path)
            
            if result.status == BuildStatus.SUCCESS:
                output_files.extend(result.output_files)
                warnings.extend(result.warnings)
            else:
                errors.extend(result.errors)
                # Stop on first error in sequential build
                break
        
        status = BuildStatus.SUCCESS if not errors else BuildStatus.FAILED
        return BuildResult(
            status=status,
            start_time=time.time(),
            end_time=time.time(),
            output_files=output_files,
            errors=errors,
            warnings=warnings,
            build_log="Sequential build"
        )
    
    def _compile_file(self, source_file: SourceFile) -> BuildResult:
        """Compile a single source file"""
        
        try:
            # Generate output file path
            output_file = self._get_output_file_path(source_file)
            source_file.output_file = output_file
            
            # Build compilation command
            command = self._build_compilation_command(source_file, output_file)
            
            self.logger.debug(f"Compiling: {' '.join(command)}")
            
            # Execute compilation
            process = subprocess.run(
                command,
                capture_output=True,
                text=True,
                cwd=source_file.path.parent
            )
            
            if process.returncode == 0:
                return BuildResult(
                    status=BuildStatus.SUCCESS,
                    start_time=time.time(),
                    end_time=time.time(),
                    output_files=[output_file],
                    errors=[],
                    warnings=self._parse_warnings(process.stderr),
                    build_log=process.stdout
                )
            else:
                return BuildResult(
                    status=BuildStatus.FAILED,
                    start_time=time.time(),
                    end_time=time.time(),
                    output_files=[],
                    errors=self._parse_errors(process.stderr),
                    warnings=[],
                    build_log=process.stdout
                )
                
        except Exception as e:
            return BuildResult(
                status=BuildStatus.FAILED,
                start_time=time.time(),
                end_time=time.time(),
                output_files=[],
                errors=[str(e)],
                warnings=[],
                build_log=""
            )
    
    def _get_output_file_path(self, source_file: SourceFile) -> Path:
        """Get output file path for source file"""
        
        intermediate_dir = Path(self.build_config.intermediate_directory)
        output_name = source_file.path.stem
        
        # Determine output extension based on language
        output_extensions = {
            'c': '.o',
            'cpp': '.o',
            'python': '.pyc',
            'javascript': '.js',
            'java': '.class',
            'csharp': '.exe',
            'rust': '',
            'go': '',
            'eon': '.exe'
        }
        
        output_ext = output_extensions.get(source_file.language, '.o')
        return intermediate_dir / f"{output_name}{output_ext}"
    
    def _build_compilation_command(self, source_file: SourceFile, output_file: Path) -> List[str]:
        """Build compilation command"""
        
        command = [source_file.compiler]
        
        # Add flags
        command.extend(source_file.flags)
        
        # Add optimization flags
        if self.build_config.target == BuildTarget.RELEASE:
            command.extend(['-O3', '-DNDEBUG'])
        elif self.build_config.target == BuildTarget.DEBUG:
            command.extend(['-g', '-O0'])
        
        # Add output file
        command.extend(['-o', str(output_file)])
        
        # Add source file
        command.append(str(source_file.path))
        
        return command
    
    def _get_default_flags(self, language: str) -> List[str]:
        """Get default compilation flags for language"""
        
        default_flags = {
            'c': ['-Wall', '-Wextra', '-std=c99'],
            'cpp': ['-Wall', '-Wextra', '-std=c++17'],
            'python': ['-m', 'compileall'],
            'javascript': [],
            'java': ['-cp', '.'],
            'csharp': ['/target:exe'],
            'rust': ['--crate-type', 'bin'],
            'go': ['build'],
            'eon': ['--target', 'exe']
        }
        
        return default_flags.get(language, [])
    
    def _scan_dependencies(self, source_file: SourceFile) -> List[Path]:
        """Scan source file for dependencies"""
        
        dependencies = []
        
        try:
            # Simple dependency scanning based on file extension
            if source_file.language in ['c', 'cpp']:
                dependencies = self._scan_c_dependencies(source_file.path)
            elif source_file.language == 'python':
                dependencies = self._scan_python_dependencies(source_file.path)
            elif source_file.language == 'java':
                dependencies = self._scan_java_dependencies(source_file.path)
            
            source_file.dependencies = dependencies
            
        except Exception as e:
            self.logger.error(f"Error scanning dependencies for {source_file.path}: {e}")
        
        return dependencies
    
    def _scan_c_dependencies(self, file_path: Path) -> List[Path]:
        """Scan C/C++ file for #include dependencies"""
        
        dependencies = []
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if line.startswith('#include'):
                        # Extract include path
                        if '"' in line:
                            include_path = line.split('"')[1]
                        elif '<' in line and '>' in line:
                            include_path = line.split('<')[1].split('>')[0]
                        else:
                            continue
                        
                        # Convert to full path
                        if include_path.startswith('/'):
                            dep_path = Path(include_path)
                        else:
                            dep_path = file_path.parent / include_path
                        
                        if dep_path.exists():
                            dependencies.append(dep_path)
        
        except Exception as e:
            self.logger.error(f"Error scanning C dependencies: {e}")
        
        return dependencies
    
    def _scan_python_dependencies(self, file_path: Path) -> List[Path]:
        """Scan Python file for import dependencies"""
        
        dependencies = []
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if line.startswith('import ') or line.startswith('from '):
                        # Simple import parsing
                        module_name = line.split()[1].split('.')[0]
                        # This is simplified - real implementation would resolve module paths
                        pass
        
        except Exception as e:
            self.logger.error(f"Error scanning Python dependencies: {e}")
        
        return dependencies
    
    def _scan_java_dependencies(self, file_path: Path) -> List[Path]:
        """Scan Java file for import dependencies"""
        
        dependencies = []
        
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                for line_num, line in enumerate(f, 1):
                    line = line.strip()
                    if line.startswith('import '):
                        # Simple import parsing
                        package_name = line.split()[1].replace(';', '')
                        # This is simplified - real implementation would resolve package paths
                        pass
        
        except Exception as e:
            self.logger.error(f"Error scanning Java dependencies: {e}")
        
        return dependencies
    
    def _needs_rebuild(self, source_file: SourceFile) -> bool:
        """Check if file needs to be rebuilt"""
        
        if not source_file.output_file or not source_file.output_file.exists():
            return True
        
        source_mtime = source_file.path.stat().st_mtime
        output_mtime = source_file.output_file.stat().st_mtime
        
        if source_mtime > output_mtime:
            return True
        
        # Check dependencies
        for dep in source_file.dependencies:
            if dep.exists() and dep.stat().st_mtime > output_mtime:
                return True
        
        return False
    
    def _parse_errors(self, stderr: str) -> List[str]:
        """Parse compilation errors from stderr"""
        
        errors = []
        for line in stderr.split('\n'):
            line = line.strip()
            if line and ('error:' in line or 'Error:' in line):
                errors.append(line)
        
        return errors
    
    def _parse_warnings(self, stderr: str) -> List[str]:
        """Parse compilation warnings from stderr"""
        
        warnings = []
        for line in stderr.split('\n'):
            line = line.strip()
            if line and ('warning:' in line or 'Warning:' in line):
                warnings.append(line)
        
        return warnings
    
    def _update_build_statistics(self, result: BuildResult):
        """Update build statistics"""
        
        self.build_stats['total_builds'] += 1
        
        if result.status == BuildStatus.SUCCESS:
            self.build_stats['successful_builds'] += 1
        else:
            self.build_stats['failed_builds'] += 1
        
        build_time = result.end_time - result.start_time
        self.build_stats['last_build_time'] = build_time
        
        # Update average build time
        total_time = self.build_stats['average_build_time'] * (self.build_stats['total_builds'] - 1)
        self.build_stats['average_build_time'] = (total_time + build_time) / self.build_stats['total_builds']
    
    def _notify_handlers(self, event_type: str, data: Dict[str, Any]):
        """Notify event handlers"""
        
        if event_type in self.event_handlers:
            for handler in self.event_handlers[event_type]:
                try:
                    handler(data)
                except Exception as e:
                    self.logger.error(f"Event handler error: {e}")

# Integration function
def integrate_build_system(ide_instance):
    """Integrate build system with IDE"""
    
    ide_instance.build_system = CompilerBuildSystem()
    print("🔨 Build system integrated with IDE")

if __name__ == "__main__":
    print("🔨 Compiler Build System")
    print("=" * 50)
    
    # Test the build system
    build_system = CompilerBuildSystem()
    
    # Test adding source files
    test_file = Path("test.c")
    if test_file.exists():
        build_system.add_source_file(test_file, "c")
        print(f"✅ Added source file: {test_file}")
    
    # Test build statistics
    stats = build_system.get_build_statistics()
    print(f"✅ Build statistics: {stats}")
    
    print("✅ Build system ready!")
