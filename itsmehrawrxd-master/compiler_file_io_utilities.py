#!/usr/bin/env python3
"""
Compiler File I/O Utilities for n0mn0m IDE
Complete file input/output operations for the compiler system
"""

import os
import sys
import json
import pickle
import zipfile
import tarfile
import tempfile
import shutil
import mmap
import hashlib
from pathlib import Path
from typing import Dict, List, Optional, Union, Any, BinaryIO, TextIO
import threading
import time
from datetime import datetime
import logging

class CompilerFileIOUtilities:
    """
    Comprehensive file I/O utilities for the n0mn0m IDE compiler
    Handles all file operations with advanced features
    """
    
    def __init__(self):
        self.file_cache = {}
        self.file_locks = {}
        self.file_watchers = {}
        self.compression_enabled = True
        self.encryption_enabled = False
        self.backup_enabled = True
        
        # Setup logging
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.INFO)
        
        print("📁 Compiler File I/O Utilities initialized")
    
    def read_text_file(self, file_path: Union[str, Path], 
                      encoding: str = 'utf-8', 
                      buffer_size: int = 8192) -> str:
        """Read text file with advanced options"""
        
        file_path = Path(file_path)
        
        try:
            # Check cache first
            cache_key = f"{file_path}_{file_path.stat().st_mtime}"
            if cache_key in self.file_cache:
                self.logger.debug(f"Reading from cache: {file_path}")
                return self.file_cache[cache_key]
            
            # Acquire file lock
            with self._get_file_lock(file_path):
                with open(file_path, 'r', encoding=encoding, buffering=buffer_size) as f:
                    content = f.read()
                
                # Cache the content
                self.file_cache[cache_key] = content
                
                self.logger.info(f"Read text file: {file_path} ({len(content)} chars)")
                return content
                
        except Exception as e:
            self.logger.error(f"Error reading text file {file_path}: {e}")
            raise
    
    def write_text_file(self, file_path: Union[str, Path], 
                       content: str, 
                       encoding: str = 'utf-8',
                       create_backup: bool = True,
                       atomic_write: bool = True) -> bool:
        """Write text file with advanced options"""
        
        file_path = Path(file_path)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and file_path.exists():
                self._create_backup(file_path)
            
            # Acquire file lock
            with self._get_file_lock(file_path):
                
                if atomic_write:
                    # Atomic write using temporary file
                    temp_path = file_path.with_suffix(file_path.suffix + '.tmp')
                    with open(temp_path, 'w', encoding=encoding) as f:
                        f.write(content)
                    
                    # Atomic move
                    temp_path.replace(file_path)
                else:
                    # Direct write
                    with open(file_path, 'w', encoding=encoding) as f:
                        f.write(content)
                
                # Clear cache entry
                self._clear_cache_entry(file_path)
                
                self.logger.info(f"Wrote text file: {file_path} ({len(content)} chars)")
                return True
                
        except Exception as e:
            self.logger.error(f"Error writing text file {file_path}: {e}")
            return False
    
    def read_binary_file(self, file_path: Union[str, Path], 
                        chunk_size: int = 8192) -> bytes:
        """Read binary file with chunking support"""
        
        file_path = Path(file_path)
        
        try:
            # Check cache first
            cache_key = f"binary_{file_path}_{file_path.stat().st_mtime}"
            if cache_key in self.file_cache:
                self.logger.debug(f"Reading binary from cache: {file_path}")
                return self.file_cache[cache_key]
            
            # Acquire file lock
            with self._get_file_lock(file_path):
                with open(file_path, 'rb') as f:
                    content = f.read()
                
                # Cache the content
                self.file_cache[cache_key] = content
                
                self.logger.info(f"Read binary file: {file_path} ({len(content)} bytes)")
                return content
                
        except Exception as e:
            self.logger.error(f"Error reading binary file {file_path}: {e}")
            raise
    
    def write_binary_file(self, file_path: Union[str, Path], 
                         content: bytes,
                         create_backup: bool = True,
                         atomic_write: bool = True) -> bool:
        """Write binary file with advanced options"""
        
        file_path = Path(file_path)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and file_path.exists():
                self._create_backup(file_path)
            
            # Acquire file lock
            with self._get_file_lock(file_path):
                
                if atomic_write:
                    # Atomic write using temporary file
                    temp_path = file_path.with_suffix(file_path.suffix + '.tmp')
                    with open(temp_path, 'wb') as f:
                        f.write(content)
                    
                    # Atomic move
                    temp_path.replace(file_path)
                else:
                    # Direct write
                    with open(file_path, 'wb') as f:
                        f.write(content)
                
                # Clear cache entry
                self._clear_cache_entry(file_path)
                
                self.logger.info(f"Wrote binary file: {file_path} ({len(content)} bytes)")
                return True
                
        except Exception as e:
            self.logger.error(f"Error writing binary file {file_path}: {e}")
            return False
    
    def read_json_file(self, file_path: Union[str, Path], 
                      encoding: str = 'utf-8') -> Dict[str, Any]:
        """Read JSON file with error handling"""
        
        file_path = Path(file_path)
        
        try:
            content = self.read_text_file(file_path, encoding)
            data = json.loads(content)
            
            self.logger.info(f"Read JSON file: {file_path}")
            return data
            
        except json.JSONDecodeError as e:
            self.logger.error(f"JSON decode error in {file_path}: {e}")
            raise
        except Exception as e:
            self.logger.error(f"Error reading JSON file {file_path}: {e}")
            raise
    
    def write_json_file(self, file_path: Union[str, Path], 
                       data: Dict[str, Any],
                       indent: int = 2,
                       encoding: str = 'utf-8',
                       create_backup: bool = True) -> bool:
        """Write JSON file with formatting"""
        
        file_path = Path(file_path)
        
        try:
            content = json.dumps(data, indent=indent, ensure_ascii=False)
            success = self.write_text_file(file_path, content, encoding, create_backup)
            
            if success:
                self.logger.info(f"Wrote JSON file: {file_path}")
            
            return success
            
        except Exception as e:
            self.logger.error(f"Error writing JSON file {file_path}: {e}")
            return False
    
    def read_pickle_file(self, file_path: Union[str, Path]) -> Any:
        """Read pickle file"""
        
        file_path = Path(file_path)
        
        try:
            with open(file_path, 'rb') as f:
                data = pickle.load(f)
            
            self.logger.info(f"Read pickle file: {file_path}")
            return data
            
        except Exception as e:
            self.logger.error(f"Error reading pickle file {file_path}: {e}")
            raise
    
    def write_pickle_file(self, file_path: Union[str, Path], 
                         data: Any,
                         create_backup: bool = True) -> bool:
        """Write pickle file"""
        
        file_path = Path(file_path)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and file_path.exists():
                self._create_backup(file_path)
            
            with open(file_path, 'wb') as f:
                pickle.dump(data, f)
            
            self.logger.info(f"Wrote pickle file: {file_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error writing pickle file {file_path}: {e}")
            return False
    
    def create_directory(self, dir_path: Union[str, Path], 
                        parents: bool = True,
                        exist_ok: bool = True) -> bool:
        """Create directory with options"""
        
        dir_path = Path(dir_path)
        
        try:
            dir_path.mkdir(parents=parents, exist_ok=exist_ok)
            self.logger.info(f"Created directory: {dir_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error creating directory {dir_path}: {e}")
            return False
    
    def copy_file(self, src_path: Union[str, Path], 
                  dst_path: Union[str, Path],
                  preserve_metadata: bool = True,
                  create_backup: bool = True) -> bool:
        """Copy file with options"""
        
        src_path = Path(src_path)
        dst_path = Path(dst_path)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and dst_path.exists():
                self._create_backup(dst_path)
            
            # Copy file
            shutil.copy2(src_path, dst_path)
            
            self.logger.info(f"Copied file: {src_path} -> {dst_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error copying file {src_path} to {dst_path}: {e}")
            return False
    
    def move_file(self, src_path: Union[str, Path], 
                  dst_path: Union[str, Path],
                  create_backup: bool = True) -> bool:
        """Move file with backup option"""
        
        src_path = Path(src_path)
        dst_path = Path(dst_path)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and dst_path.exists():
                self._create_backup(dst_path)
            
            # Move file
            shutil.move(str(src_path), str(dst_path))
            
            self.logger.info(f"Moved file: {src_path} -> {dst_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error moving file {src_path} to {dst_path}: {e}")
            return False
    
    def delete_file(self, file_path: Union[str, Path], 
                   create_backup: bool = True,
                   permanent: bool = False) -> bool:
        """Delete file with backup option"""
        
        file_path = Path(file_path)
        
        try:
            if not file_path.exists():
                self.logger.warning(f"File does not exist: {file_path}")
                return False
            
            # Create backup if requested
            if create_backup and self.backup_enabled:
                self._create_backup(file_path)
            
            # Delete file
            if permanent:
                file_path.unlink()
            else:
                # Move to trash (if available)
                try:
                    import send2trash
                    send2trash.send2trash(str(file_path))
                except ImportError:
                    file_path.unlink()  # Fallback to permanent delete
            
            # Clear cache entry
            self._clear_cache_entry(file_path)
            
            self.logger.info(f"Deleted file: {file_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error deleting file {file_path}: {e}")
            return False
    
    def get_file_info(self, file_path: Union[str, Path]) -> Dict[str, Any]:
        """Get comprehensive file information"""
        
        file_path = Path(file_path)
        
        try:
            if not file_path.exists():
                return {'exists': False}
            
            stat = file_path.stat()
            
            info = {
                'exists': True,
                'path': str(file_path),
                'name': file_path.name,
                'stem': file_path.stem,
                'suffix': file_path.suffix,
                'size': stat.st_size,
                'size_human': self._format_file_size(stat.st_size),
                'created': datetime.fromtimestamp(stat.st_ctime),
                'modified': datetime.fromtimestamp(stat.st_mtime),
                'accessed': datetime.fromtimestamp(stat.st_atime),
                'is_file': file_path.is_file(),
                'is_dir': file_path.is_dir(),
                'is_symlink': file_path.is_symlink(),
                'permissions': oct(stat.st_mode)[-3:],
                'owner': stat.st_uid,
                'group': stat.st_gid
            }
            
            # Calculate file hash
            try:
                info['hash_md5'] = self._calculate_file_hash(file_path, 'md5')
                info['hash_sha256'] = self._calculate_file_hash(file_path, 'sha256')
            except Exception:
                pass
            
            return info
            
        except Exception as e:
            self.logger.error(f"Error getting file info for {file_path}: {e}")
            return {'exists': False, 'error': str(e)}
    
    def list_directory(self, dir_path: Union[str, Path], 
                      recursive: bool = False,
                      include_hidden: bool = False,
                      pattern: str = "*") -> List[Dict[str, Any]]:
        """List directory contents with options"""
        
        dir_path = Path(dir_path)
        
        try:
            if not dir_path.exists() or not dir_path.is_dir():
                return []
            
            items = []
            
            if recursive:
                # Recursive listing
                for item in dir_path.rglob(pattern):
                    if not include_hidden and item.name.startswith('.'):
                        continue
                    items.append(self.get_file_info(item))
            else:
                # Non-recursive listing
                for item in dir_path.iterdir():
                    if not include_hidden and item.name.startswith('.'):
                        continue
                    if pattern != "*" and not item.match(pattern):
                        continue
                    items.append(self.get_file_info(item))
            
            # Sort by name
            items.sort(key=lambda x: x['name'])
            
            self.logger.info(f"Listed directory: {dir_path} ({len(items)} items)")
            return items
            
        except Exception as e:
            self.logger.error(f"Error listing directory {dir_path}: {e}")
            return []
    
    def create_archive(self, archive_path: Union[str, Path], 
                      source_paths: List[Union[str, Path]],
                      format: str = 'zip',
                      compression: int = 6) -> bool:
        """Create archive from source paths"""
        
        archive_path = Path(archive_path)
        
        try:
            if format.lower() == 'zip':
                with zipfile.ZipFile(archive_path, 'w', 
                                   compression=zipfile.ZIP_DEFLATED,
                                   compresslevel=compression) as archive:
                    for source_path in source_paths:
                        source_path = Path(source_path)
                        if source_path.is_file():
                            archive.write(source_path, source_path.name)
                        elif source_path.is_dir():
                            for item in source_path.rglob('*'):
                                if item.is_file():
                                    arcname = item.relative_to(source_path)
                                    archive.write(item, arcname)
            
            elif format.lower() == 'tar':
                with tarfile.open(archive_path, 'w:gz') as archive:
                    for source_path in source_paths:
                        archive.add(source_path, arcname=Path(source_path).name)
            
            else:
                raise ValueError(f"Unsupported archive format: {format}")
            
            self.logger.info(f"Created archive: {archive_path}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error creating archive {archive_path}: {e}")
            return False
    
    def extract_archive(self, archive_path: Union[str, Path], 
                       extract_to: Union[str, Path],
                       create_backup: bool = True) -> bool:
        """Extract archive to destination"""
        
        archive_path = Path(archive_path)
        extract_to = Path(extract_to)
        
        try:
            # Create backup if requested
            if create_backup and self.backup_enabled and extract_to.exists():
                self._create_backup(extract_to)
            
            # Create extraction directory
            extract_to.mkdir(parents=True, exist_ok=True)
            
            # Extract based on file extension
            if archive_path.suffix.lower() == '.zip':
                with zipfile.ZipFile(archive_path, 'r') as archive:
                    archive.extractall(extract_to)
            
            elif archive_path.suffix.lower() in ['.tar', '.gz', '.bz2']:
                with tarfile.open(archive_path, 'r:*') as archive:
                    archive.extractall(extract_to)
            
            else:
                raise ValueError(f"Unsupported archive format: {archive_path.suffix}")
            
            self.logger.info(f"Extracted archive: {archive_path} -> {extract_to}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error extracting archive {archive_path}: {e}")
            return False
    
    def watch_file(self, file_path: Union[str, Path], 
                   callback: callable,
                   recursive: bool = False) -> str:
        """Watch file for changes"""
        
        file_path = Path(file_path)
        watch_id = f"watch_{file_path}_{int(time.time())}"
        
        try:
            # Simple file watching implementation
            def watch_thread():
                last_mtime = 0
                while watch_id in self.file_watchers:
                    try:
                        if file_path.exists():
                            current_mtime = file_path.stat().st_mtime
                            if current_mtime > last_mtime:
                                callback(str(file_path), 'modified')
                                last_mtime = current_mtime
                        time.sleep(1)
                    except Exception as e:
                        self.logger.error(f"Error in file watcher: {e}")
                        break
            
            self.file_watchers[watch_id] = {
                'path': file_path,
                'callback': callback,
                'thread': threading.Thread(target=watch_thread, daemon=True)
            }
            
            self.file_watchers[watch_id]['thread'].start()
            
            self.logger.info(f"Started watching file: {file_path}")
            return watch_id
            
        except Exception as e:
            self.logger.error(f"Error starting file watch for {file_path}: {e}")
            return ""
    
    def stop_watching(self, watch_id: str) -> bool:
        """Stop watching a file"""
        
        try:
            if watch_id in self.file_watchers:
                del self.file_watchers[watch_id]
                self.logger.info(f"Stopped watching: {watch_id}")
                return True
            return False
            
        except Exception as e:
            self.logger.error(f"Error stopping file watch {watch_id}: {e}")
            return False
    
    def clear_cache(self):
        """Clear file cache"""
        
        self.file_cache.clear()
        self.logger.info("Cleared file cache")
    
    def get_cache_stats(self) -> Dict[str, Any]:
        """Get cache statistics"""
        
        return {
            'cache_size': len(self.file_cache),
            'cache_keys': list(self.file_cache.keys()),
            'active_watchers': len(self.file_watchers),
            'watcher_ids': list(self.file_watchers.keys())
        }
    
    # Private helper methods
    
    def _get_file_lock(self, file_path: Path):
        """Get file lock for thread safety"""
        
        lock_key = str(file_path)
        if lock_key not in self.file_locks:
            self.file_locks[lock_key] = threading.Lock()
        
        return self.file_locks[lock_key]
    
    def _create_backup(self, file_path: Path):
        """Create backup of file"""
        
        try:
            backup_dir = file_path.parent / '.backups'
            backup_dir.mkdir(exist_ok=True)
            
            timestamp = datetime.now().strftime('%Y%m%d_%H%M%S')
            backup_name = f"{file_path.stem}_{timestamp}{file_path.suffix}"
            backup_path = backup_dir / backup_name
            
            shutil.copy2(file_path, backup_path)
            self.logger.debug(f"Created backup: {backup_path}")
            
        except Exception as e:
            self.logger.warning(f"Could not create backup for {file_path}: {e}")
    
    def _clear_cache_entry(self, file_path: Path):
        """Clear cache entry for file"""
        
        keys_to_remove = []
        for key in self.file_cache.keys():
            if str(file_path) in key:
                keys_to_remove.append(key)
        
        for key in keys_to_remove:
            del self.file_cache[key]
    
    def _format_file_size(self, size_bytes: int) -> str:
        """Format file size in human readable format"""
        
        if size_bytes == 0:
            return "0 B"
        
        size_names = ["B", "KB", "MB", "GB", "TB"]
        i = 0
        size = float(size_bytes)
        
        while size >= 1024.0 and i < len(size_names) - 1:
            size /= 1024.0
            i += 1
        
        return f"{size:.1f} {size_names[i]}"
    
    def _calculate_file_hash(self, file_path: Path, algorithm: str = 'md5') -> str:
        """Calculate file hash"""
        
        hash_obj = hashlib.new(algorithm)
        
        with open(file_path, 'rb') as f:
            for chunk in iter(lambda: f.read(4096), b""):
                hash_obj.update(chunk)
        
        return hash_obj.hexdigest()

# Integration function
def integrate_file_io_utilities(ide_instance):
    """Integrate file I/O utilities with IDE"""
    
    ide_instance.file_io_utils = CompilerFileIOUtilities()
    print("📁 File I/O utilities integrated with IDE")

if __name__ == "__main__":
    print("📁 Compiler File I/O Utilities")
    print("=" * 50)
    
    # Test the utilities
    file_io = CompilerFileIOUtilities()
    
    # Test file operations
    test_file = Path("test_file.txt")
    test_content = "Hello, n0mn0m IDE File I/O!"
    
    # Write and read test
    file_io.write_text_file(test_file, test_content)
    content = file_io.read_text_file(test_file)
    print(f"✅ File I/O test: {content}")
    
    # Get file info
    info = file_io.get_file_info(test_file)
    print(f"✅ File info: {info['size_human']}, {info['hash_md5']}")
    
    # Cleanup
    file_io.delete_file(test_file)
    
    print("✅ File I/O utilities ready!")
