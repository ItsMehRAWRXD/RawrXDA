#!/usr/bin/env python3
"""
Fileless SDK/Toolchain Storage System
Store everything in memory and databases - never need another IDE again!
"""

import os
import sys
import json
import sqlite3
import pickle
import base64
import hashlib
import time
import threading
from typing import Dict, List, Any, Optional, Union
import zlib
import gzip
import lzma

class FilelessSDKStorage:
    """Fileless SDK and toolchain storage system"""
    
    def __init__(self):
        self.initialized = False
        self.memory_db = None
        self.sdk_registry = {}
        self.toolchain_registry = {}
        self.compiler_cache = {}
        self.library_cache = {}
        self.template_cache = {}
        
        # Compression settings
        self.compression_level = 9
        self.use_compression = True
        
        # Memory management
        self.max_memory_usage = 1024 * 1024 * 1024  # 1GB
        self.current_memory_usage = 0
        self.cleanup_threshold = 0.8  # 80% memory usage
        
        print("Fileless SDK Storage System created")
    
    def initialize(self):
        """Initialize the fileless storage system"""
        try:
            # Initialize in-memory database
            self.initialize_memory_database()
            
            # Load existing SDKs and toolchains
            self.load_existing_data()
            
            # Start memory management
            self.start_memory_management()
            
            self.initialized = True
            print("Fileless SDK Storage System initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Fileless SDK Storage: {e}")
            return False
    
    def initialize_memory_database(self):
        """Initialize in-memory SQLite database"""
        try:
            # Create in-memory database
            self.memory_db = sqlite3.connect(':memory:', check_same_thread=False)
            self.memory_db.row_factory = sqlite3.Row
            
            # Create tables
            self.create_storage_tables()
            
            print("In-memory database initialized")
            
        except Exception as e:
            print(f"Failed to initialize memory database: {e}")
            raise
    
    def create_storage_tables(self):
        """Create storage tables for SDKs and toolchains"""
        cursor = self.memory_db.cursor()
        
        # SDKs table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS sdks (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                version TEXT NOT NULL,
                platform TEXT NOT NULL,
                architecture TEXT NOT NULL,
                data BLOB NOT NULL,
                metadata TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                size_bytes INTEGER DEFAULT 0
            )
        ''')
        
        # Toolchains table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS toolchains (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                version TEXT NOT NULL,
                compiler_type TEXT NOT NULL,
                platform TEXT NOT NULL,
                data BLOB NOT NULL,
                metadata TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                size_bytes INTEGER DEFAULT 0
            )
        ''')
        
        # Libraries table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS libraries (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                version TEXT NOT NULL,
                language TEXT NOT NULL,
                data BLOB NOT NULL,
                metadata TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                size_bytes INTEGER DEFAULT 0
            )
        ''')
        
        # Templates table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS templates (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                type TEXT NOT NULL,
                language TEXT NOT NULL,
                data BLOB NOT NULL,
                metadata TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                size_bytes INTEGER DEFAULT 0
            )
        ''')
        
        # Compilers table
        cursor.execute('''
            CREATE TABLE IF NOT EXISTS compilers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                name TEXT UNIQUE NOT NULL,
                version TEXT NOT NULL,
                type TEXT NOT NULL,
                platform TEXT NOT NULL,
                data BLOB NOT NULL,
                metadata TEXT NOT NULL,
                created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
                last_accessed DATETIME DEFAULT CURRENT_TIMESTAMP,
                access_count INTEGER DEFAULT 0,
                size_bytes INTEGER DEFAULT 0
            )
        ''')
        
        # Indexes for performance
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_sdks_name ON sdks(name)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_sdks_platform ON sdks(platform)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_toolchains_name ON toolchains(name)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_toolchains_type ON toolchains(compiler_type)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_libraries_name ON libraries(name)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_libraries_language ON libraries(language)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_templates_type ON templates(type)')
        cursor.execute('CREATE INDEX IF NOT EXISTS idx_compilers_name ON compilers(name)')
        
        self.memory_db.commit()
        print("Storage tables created")
    
    def load_existing_data(self):
        """Load existing SDKs and toolchains from persistent storage"""
        try:
            # Load from persistent database if it exists
            persistent_db_path = "fileless_storage.db"
            if os.path.exists(persistent_db_path):
                self.load_from_persistent_storage(persistent_db_path)
            
            # Load from memory cache files
            self.load_from_memory_cache()
            
            print("Existing data loaded")
            
        except Exception as e:
            print(f"Error loading existing data: {e}")
    
    def load_from_persistent_storage(self, db_path):
        """Load data from persistent database"""
        try:
            persistent_db = sqlite3.connect(db_path)
            persistent_db.row_factory = sqlite3.Row
            
            # Copy data from persistent to memory database
            tables = ['sdks', 'toolchains', 'libraries', 'templates', 'compilers']
            
            for table in tables:
                cursor = persistent_db.cursor()
                cursor.execute(f'SELECT * FROM {table}')
                rows = cursor.fetchall()
                
                if rows:
                    # Insert into memory database
                    memory_cursor = self.memory_db.cursor()
                    for row in rows:
                        columns = ', '.join(row.keys())
                        placeholders = ', '.join(['?' for _ in row])
                        query = f'INSERT OR REPLACE INTO {table} ({columns}) VALUES ({placeholders})'
                        memory_cursor.execute(query, list(row))
            
            self.memory_db.commit()
            persistent_db.close()
            
            print(f"Data loaded from persistent storage: {db_path}")
            
        except Exception as e:
            print(f"Error loading from persistent storage: {e}")
    
    def load_from_memory_cache(self):
        """Load data from memory cache files"""
        try:
            cache_dir = "fileless_cache"
            if not os.path.exists(cache_dir):
                return
            
            # Load SDKs
            sdk_cache_file = os.path.join(cache_dir, "sdks.pkl")
            if os.path.exists(sdk_cache_file):
                with open(sdk_cache_file, 'rb') as f:
                    self.sdk_registry = pickle.load(f)
            
            # Load toolchains
            toolchain_cache_file = os.path.join(cache_dir, "toolchains.pkl")
            if os.path.exists(toolchain_cache_file):
                with open(toolchain_cache_file, 'rb') as f:
                    self.toolchain_registry = pickle.load(f)
            
            # Load compilers
            compiler_cache_file = os.path.join(cache_dir, "compilers.pkl")
            if os.path.exists(compiler_cache_file):
                with open(compiler_cache_file, 'rb') as f:
                    self.compiler_cache = pickle.load(f)
            
            # Load libraries
            library_cache_file = os.path.join(cache_dir, "libraries.pkl")
            if os.path.exists(library_cache_file):
                with open(library_cache_file, 'rb') as f:
                    self.library_cache = pickle.load(f)
            
            # Load templates
            template_cache_file = os.path.join(cache_dir, "templates.pkl")
            if os.path.exists(template_cache_file):
                with open(template_cache_file, 'rb') as f:
                    self.template_cache = pickle.load(f)
            
            print("Memory cache loaded")
            
        except Exception as e:
            print(f"Error loading memory cache: {e}")
    
    def start_memory_management(self):
        """Start memory management thread"""
        memory_thread = threading.Thread(target=self.manage_memory)
        memory_thread.daemon = True
        memory_thread.start()
        
        print("Memory management started")
    
    def manage_memory(self):
        """Manage memory usage and cleanup"""
        while True:
            try:
                # Check memory usage
                if self.current_memory_usage > self.max_memory_usage * self.cleanup_threshold:
                    self.cleanup_old_data()
                
                # Update memory usage
                self.update_memory_usage()
                
                time.sleep(30)  # Check every 30 seconds
                
            except Exception as e:
                print(f"Memory management error: {e}")
                time.sleep(30)
    
    def update_memory_usage(self):
        """Update current memory usage"""
        try:
            # Calculate memory usage from database
            cursor = self.memory_db.cursor()
            
            # Get total size from all tables
            tables = ['sdks', 'toolchains', 'libraries', 'templates', 'compilers']
            total_size = 0
            
            for table in tables:
                cursor.execute(f'SELECT SUM(size_bytes) FROM {table}')
                result = cursor.fetchone()
                if result[0]:
                    total_size += result[0]
            
            self.current_memory_usage = total_size
            
        except Exception as e:
            print(f"Error updating memory usage: {e}")
    
    def cleanup_old_data(self):
        """Clean up old, unused data"""
        try:
            cursor = self.memory_db.cursor()
            
            # Clean up old SDKs (older than 30 days, accessed less than 5 times)
            cursor.execute('''
                DELETE FROM sdks 
                WHERE last_accessed < datetime('now', '-30 days') 
                AND access_count < 5
            ''')
            
            # Clean up old toolchains
            cursor.execute('''
                DELETE FROM toolchains 
                WHERE last_accessed < datetime('now', '-30 days') 
                AND access_count < 5
            ''')
            
            # Clean up old libraries
            cursor.execute('''
                DELETE FROM libraries 
                WHERE last_accessed < datetime('now', '-30 days') 
                AND access_count < 5
            ''')
            
            # Clean up old templates
            cursor.execute('''
                DELETE FROM templates 
                WHERE last_accessed < datetime('now', '-30 days') 
                AND access_count < 5
            ''')
            
            # Clean up old compilers
            cursor.execute('''
                DELETE FROM compilers 
                WHERE last_accessed < datetime('now', '-30 days') 
                AND access_count < 5
            ''')
            
            self.memory_db.commit()
            
            print("Old data cleaned up")
            
        except Exception as e:
            print(f"Error cleaning up old data: {e}")
    
    def store_sdk(self, name: str, version: str, platform: str, architecture: str, 
                  data: bytes, metadata: Dict[str, Any]) -> bool:
        """Store SDK in fileless storage"""
        try:
            # Compress data if enabled
            if self.use_compression:
                data = self.compress_data(data)
            
            # Calculate size
            size_bytes = len(data)
            
            # Store in database
            cursor = self.memory_db.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO sdks 
                (name, version, platform, architecture, data, metadata, size_bytes)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (name, version, platform, architecture, data, json.dumps(metadata), size_bytes))
            
            self.memory_db.commit()
            
            # Update memory usage
            self.current_memory_usage += size_bytes
            
            print(f"SDK stored: {name} v{version} ({size_bytes} bytes)")
            return True
            
        except Exception as e:
            print(f"Error storing SDK: {e}")
            return False
    
    def store_toolchain(self, name: str, version: str, compiler_type: str, platform: str,
                       data: bytes, metadata: Dict[str, Any]) -> bool:
        """Store toolchain in fileless storage"""
        try:
            # Compress data if enabled
            if self.use_compression:
                data = self.compress_data(data)
            
            # Calculate size
            size_bytes = len(data)
            
            # Store in database
            cursor = self.memory_db.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO toolchains 
                (name, version, compiler_type, platform, data, metadata, size_bytes)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (name, version, compiler_type, platform, data, json.dumps(metadata), size_bytes))
            
            self.memory_db.commit()
            
            # Update memory usage
            self.current_memory_usage += size_bytes
            
            print(f"Toolchain stored: {name} v{version} ({size_bytes} bytes)")
            return True
            
        except Exception as e:
            print(f"Error storing toolchain: {e}")
            return False
    
    def store_library(self, name: str, version: str, language: str, 
                     data: bytes, metadata: Dict[str, Any]) -> bool:
        """Store library in fileless storage"""
        try:
            # Compress data if enabled
            if self.use_compression:
                data = self.compress_data(data)
            
            # Calculate size
            size_bytes = len(data)
            
            # Store in database
            cursor = self.memory_db.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO libraries 
                (name, version, language, data, metadata, size_bytes)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (name, version, language, data, json.dumps(metadata), size_bytes))
            
            self.memory_db.commit()
            
            # Update memory usage
            self.current_memory_usage += size_bytes
            
            print(f"Library stored: {name} v{version} ({size_bytes} bytes)")
            return True
            
        except Exception as e:
            print(f"Error storing library: {e}")
            return False
    
    def store_template(self, name: str, type: str, language: str, 
                      data: bytes, metadata: Dict[str, Any]) -> bool:
        """Store template in fileless storage"""
        try:
            # Compress data if enabled
            if self.use_compression:
                data = self.compress_data(data)
            
            # Calculate size
            size_bytes = len(data)
            
            # Store in database
            cursor = self.memory_db.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO templates 
                (name, type, language, data, metadata, size_bytes)
                VALUES (?, ?, ?, ?, ?, ?)
            ''', (name, type, language, data, json.dumps(metadata), size_bytes))
            
            self.memory_db.commit()
            
            # Update memory usage
            self.current_memory_usage += size_bytes
            
            print(f"Template stored: {name} ({size_bytes} bytes)")
            return True
            
        except Exception as e:
            print(f"Error storing template: {e}")
            return False
    
    def store_compiler(self, name: str, version: str, type: str, platform: str,
                      data: bytes, metadata: Dict[str, Any]) -> bool:
        """Store compiler in fileless storage"""
        try:
            # Compress data if enabled
            if self.use_compression:
                data = self.compress_data(data)
            
            # Calculate size
            size_bytes = len(data)
            
            # Store in database
            cursor = self.memory_db.cursor()
            cursor.execute('''
                INSERT OR REPLACE INTO compilers 
                (name, version, type, platform, data, metadata, size_bytes)
                VALUES (?, ?, ?, ?, ?, ?, ?)
            ''', (name, version, type, platform, data, json.dumps(metadata), size_bytes))
            
            self.memory_db.commit()
            
            # Update memory usage
            self.current_memory_usage += size_bytes
            
            print(f"Compiler stored: {name} v{version} ({size_bytes} bytes)")
            return True
            
        except Exception as e:
            print(f"Error storing compiler: {e}")
            return False
    
    def get_sdk(self, name: str, version: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Get SDK from fileless storage"""
        try:
            cursor = self.memory_db.cursor()
            
            if version:
                cursor.execute('''
                    SELECT * FROM sdks 
                    WHERE name = ? AND version = ?
                ''', (name, version))
            else:
                cursor.execute('''
                    SELECT * FROM sdks 
                    WHERE name = ? 
                    ORDER BY last_accessed DESC 
                    LIMIT 1
                ''', (name,))
            
            row = cursor.fetchone()
            if row:
                # Update access count and last accessed
                cursor.execute('''
                    UPDATE sdks 
                    SET access_count = access_count + 1, 
                        last_accessed = CURRENT_TIMESTAMP 
                    WHERE id = ?
                ''', (row['id'],))
                self.memory_db.commit()
                
                # Decompress data if needed
                data = row['data']
                if self.use_compression:
                    data = self.decompress_data(data)
                
                return {
                    'name': row['name'],
                    'version': row['version'],
                    'platform': row['platform'],
                    'architecture': row['architecture'],
                    'data': data,
                    'metadata': json.loads(row['metadata']),
                    'size_bytes': row['size_bytes']
                }
            
            return None
            
        except Exception as e:
            print(f"Error getting SDK: {e}")
            return None
    
    def get_toolchain(self, name: str, version: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Get toolchain from fileless storage"""
        try:
            cursor = self.memory_db.cursor()
            
            if version:
                cursor.execute('''
                    SELECT * FROM toolchains 
                    WHERE name = ? AND version = ?
                ''', (name, version))
            else:
                cursor.execute('''
                    SELECT * FROM toolchains 
                    WHERE name = ? 
                    ORDER BY last_accessed DESC 
                    LIMIT 1
                ''', (name,))
            
            row = cursor.fetchone()
            if row:
                # Update access count and last accessed
                cursor.execute('''
                    UPDATE toolchains 
                    SET access_count = access_count + 1, 
                        last_accessed = CURRENT_TIMESTAMP 
                    WHERE id = ?
                ''', (row['id'],))
                self.memory_db.commit()
                
                # Decompress data if needed
                data = row['data']
                if self.use_compression:
                    data = self.decompress_data(data)
                
                return {
                    'name': row['name'],
                    'version': row['version'],
                    'compiler_type': row['compiler_type'],
                    'platform': row['platform'],
                    'data': data,
                    'metadata': json.loads(row['metadata']),
                    'size_bytes': row['size_bytes']
                }
            
            return None
            
        except Exception as e:
            print(f"Error getting toolchain: {e}")
            return None
    
    def get_library(self, name: str, version: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Get library from fileless storage"""
        try:
            cursor = self.memory_db.cursor()
            
            if version:
                cursor.execute('''
                    SELECT * FROM libraries 
                    WHERE name = ? AND version = ?
                ''', (name, version))
            else:
                cursor.execute('''
                    SELECT * FROM libraries 
                    WHERE name = ? 
                    ORDER BY last_accessed DESC 
                    LIMIT 1
                ''', (name,))
            
            row = cursor.fetchone()
            if row:
                # Update access count and last accessed
                cursor.execute('''
                    UPDATE libraries 
                    SET access_count = access_count + 1, 
                        last_accessed = CURRENT_TIMESTAMP 
                    WHERE id = ?
                ''', (row['id'],))
                self.memory_db.commit()
                
                # Decompress data if needed
                data = row['data']
                if self.use_compression:
                    data = self.decompress_data(data)
                
                return {
                    'name': row['name'],
                    'version': row['version'],
                    'language': row['language'],
                    'data': data,
                    'metadata': json.loads(row['metadata']),
                    'size_bytes': row['size_bytes']
                }
            
            return None
            
        except Exception as e:
            print(f"Error getting library: {e}")
            return None
    
    def get_template(self, name: str) -> Optional[Dict[str, Any]]:
        """Get template from fileless storage"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT * FROM templates 
                WHERE name = ?
            ''', (name,))
            
            row = cursor.fetchone()
            if row:
                # Update access count and last accessed
                cursor.execute('''
                    UPDATE templates 
                    SET access_count = access_count + 1, 
                        last_accessed = CURRENT_TIMESTAMP 
                    WHERE id = ?
                ''', (row['id'],))
                self.memory_db.commit()
                
                # Decompress data if needed
                data = row['data']
                if self.use_compression:
                    data = self.decompress_data(data)
                
                return {
                    'name': row['name'],
                    'type': row['type'],
                    'language': row['language'],
                    'data': data,
                    'metadata': json.loads(row['metadata']),
                    'size_bytes': row['size_bytes']
                }
            
            return None
            
        except Exception as e:
            print(f"Error getting template: {e}")
            return None
    
    def get_compiler(self, name: str, version: Optional[str] = None) -> Optional[Dict[str, Any]]:
        """Get compiler from fileless storage"""
        try:
            cursor = self.memory_db.cursor()
            
            if version:
                cursor.execute('''
                    SELECT * FROM compilers 
                    WHERE name = ? AND version = ?
                ''', (name, version))
            else:
                cursor.execute('''
                    SELECT * FROM compilers 
                    WHERE name = ? 
                    ORDER BY last_accessed DESC 
                    LIMIT 1
                ''', (name,))
            
            row = cursor.fetchone()
            if row:
                # Update access count and last accessed
                cursor.execute('''
                    UPDATE compilers 
                    SET access_count = access_count + 1, 
                        last_accessed = CURRENT_TIMESTAMP 
                    WHERE id = ?
                ''', (row['id'],))
                self.memory_db.commit()
                
                # Decompress data if needed
                data = row['data']
                if self.use_compression:
                    data = self.decompress_data(data)
                
                return {
                    'name': row['name'],
                    'version': row['version'],
                    'type': row['type'],
                    'platform': row['platform'],
                    'data': data,
                    'metadata': json.loads(row['metadata']),
                    'size_bytes': row['size_bytes']
                }
            
            return None
            
        except Exception as e:
            print(f"Error getting compiler: {e}")
            return None
    
    def list_sdks(self) -> List[Dict[str, Any]]:
        """List all stored SDKs"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT name, version, platform, architecture, 
                       size_bytes, access_count, last_accessed
                FROM sdks 
                ORDER BY last_accessed DESC
            ''')
            
            rows = cursor.fetchall()
            return [dict(row) for row in rows]
            
        except Exception as e:
            print(f"Error listing SDKs: {e}")
            return []
    
    def list_toolchains(self) -> List[Dict[str, Any]]:
        """List all stored toolchains"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT name, version, compiler_type, platform, 
                       size_bytes, access_count, last_accessed
                FROM toolchains 
                ORDER BY last_accessed DESC
            ''')
            
            rows = cursor.fetchall()
            return [dict(row) for row in rows]
            
        except Exception as e:
            print(f"Error listing toolchains: {e}")
            return []
    
    def list_libraries(self) -> List[Dict[str, Any]]:
        """List all stored libraries"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT name, version, language, 
                       size_bytes, access_count, last_accessed
                FROM libraries 
                ORDER BY last_accessed DESC
            ''')
            
            rows = cursor.fetchall()
            return [dict(row) for row in rows]
            
        except Exception as e:
            print(f"Error listing libraries: {e}")
            return []
    
    def list_templates(self) -> List[Dict[str, Any]]:
        """List all stored templates"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT name, type, language, 
                       size_bytes, access_count, last_accessed
                FROM templates 
                ORDER BY last_accessed DESC
            ''')
            
            rows = cursor.fetchall()
            return [dict(row) for row in rows]
            
        except Exception as e:
            print(f"Error listing templates: {e}")
            return []
    
    def list_compilers(self) -> List[Dict[str, Any]]:
        """List all stored compilers"""
        try:
            cursor = self.memory_db.cursor()
            cursor.execute('''
                SELECT name, version, type, platform, 
                       size_bytes, access_count, last_accessed
                FROM compilers 
                ORDER BY last_accessed DESC
            ''')
            
            rows = cursor.fetchall()
            return [dict(row) for row in rows]
            
        except Exception as e:
            print(f"Error listing compilers: {e}")
            return []
    
    def compress_data(self, data: bytes) -> bytes:
        """Compress data using LZMA compression"""
        try:
            return lzma.compress(data, preset=self.compression_level)
        except Exception as e:
            print(f"Compression error: {e}")
            return data
    
    def decompress_data(self, data: bytes) -> bytes:
        """Decompress data using LZMA decompression"""
        try:
            return lzma.decompress(data)
        except Exception as e:
            print(f"Decompression error: {e}")
            return data
    
    def save_to_persistent_storage(self):
        """Save data to persistent storage"""
        try:
            persistent_db_path = "fileless_storage.db"
            persistent_db = sqlite3.connect(persistent_db_path)
            
            # Copy data from memory to persistent database
            tables = ['sdks', 'toolchains', 'libraries', 'templates', 'compilers']
            
            for table in tables:
                cursor = self.memory_db.cursor()
                cursor.execute(f'SELECT * FROM {table}')
                rows = cursor.fetchall()
                
                if rows:
                    # Create table in persistent database
                    cursor.execute(f'SELECT sql FROM sqlite_master WHERE type="table" AND name="{table}"')
                    create_sql = cursor.fetchone()[0]
                    persistent_db.execute(create_sql)
                    
                    # Insert data
                    for row in rows:
                        columns = ', '.join(row.keys())
                        placeholders = ', '.join(['?' for _ in row])
                        query = f'INSERT OR REPLACE INTO {table} ({columns}) VALUES ({placeholders})'
                        persistent_db.execute(query, list(row))
            
            persistent_db.commit()
            persistent_db.close()
            
            print(f"Data saved to persistent storage: {persistent_db_path}")
            
        except Exception as e:
            print(f"Error saving to persistent storage: {e}")
    
    def save_to_memory_cache(self):
        """Save data to memory cache files"""
        try:
            cache_dir = "fileless_cache"
            os.makedirs(cache_dir, exist_ok=True)
            
            # Save SDKs
            with open(os.path.join(cache_dir, "sdks.pkl"), 'wb') as f:
                pickle.dump(self.sdk_registry, f)
            
            # Save toolchains
            with open(os.path.join(cache_dir, "toolchains.pkl"), 'wb') as f:
                pickle.dump(self.toolchain_registry, f)
            
            # Save compilers
            with open(os.path.join(cache_dir, "compilers.pkl"), 'wb') as f:
                pickle.dump(self.compiler_cache, f)
            
            # Save libraries
            with open(os.path.join(cache_dir, "libraries.pkl"), 'wb') as f:
                pickle.dump(self.library_cache, f)
            
            # Save templates
            with open(os.path.join(cache_dir, "templates.pkl"), 'wb') as f:
                pickle.dump(self.template_cache, f)
            
            print("Memory cache saved")
            
        except Exception as e:
            print(f"Error saving memory cache: {e}")
    
    def get_storage_statistics(self) -> Dict[str, Any]:
        """Get storage statistics"""
        try:
            cursor = self.memory_db.cursor()
            
            stats = {
                'total_items': 0,
                'total_size': 0,
                'memory_usage': self.current_memory_usage,
                'max_memory': self.max_memory_usage,
                'memory_usage_percent': (self.current_memory_usage / self.max_memory_usage) * 100,
                'tables': {}
            }
            
            tables = ['sdks', 'toolchains', 'libraries', 'templates', 'compilers']
            
            for table in tables:
                cursor.execute(f'SELECT COUNT(*) FROM {table}')
                count = cursor.fetchone()[0]
                
                cursor.execute(f'SELECT SUM(size_bytes) FROM {table}')
                size = cursor.fetchone()[0] or 0
                
                stats['tables'][table] = {
                    'count': count,
                    'size_bytes': size
                }
                
                stats['total_items'] += count
                stats['total_size'] += size
            
            return stats
            
        except Exception as e:
            print(f"Error getting storage statistics: {e}")
            return {}
    
    def search_items(self, query: str, item_type: str = 'all') -> List[Dict[str, Any]]:
        """Search for items in storage"""
        try:
            cursor = self.memory_db.cursor()
            results = []
            
            if item_type == 'all' or item_type == 'sdks':
                cursor.execute('''
                    SELECT name, version, platform, architecture, 
                           size_bytes, access_count, last_accessed
                    FROM sdks 
                    WHERE name LIKE ? OR version LIKE ? OR platform LIKE ?
                ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
                results.extend([dict(row) for row in cursor.fetchall()])
            
            if item_type == 'all' or item_type == 'toolchains':
                cursor.execute('''
                    SELECT name, version, compiler_type, platform, 
                           size_bytes, access_count, last_accessed
                    FROM toolchains 
                    WHERE name LIKE ? OR version LIKE ? OR compiler_type LIKE ?
                ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
                results.extend([dict(row) for row in cursor.fetchall()])
            
            if item_type == 'all' or item_type == 'libraries':
                cursor.execute('''
                    SELECT name, version, language, 
                           size_bytes, access_count, last_accessed
                    FROM libraries 
                    WHERE name LIKE ? OR version LIKE ? OR language LIKE ?
                ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
                results.extend([dict(row) for row in cursor.fetchall()])
            
            if item_type == 'all' or item_type == 'templates':
                cursor.execute('''
                    SELECT name, type, language, 
                           size_bytes, access_count, last_accessed
                    FROM templates 
                    WHERE name LIKE ? OR type LIKE ? OR language LIKE ?
                ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
                results.extend([dict(row) for row in cursor.fetchall()])
            
            if item_type == 'all' or item_type == 'compilers':
                cursor.execute('''
                    SELECT name, version, type, platform, 
                           size_bytes, access_count, last_accessed
                    FROM compilers 
                    WHERE name LIKE ? OR version LIKE ? OR type LIKE ?
                ''', (f'%{query}%', f'%{query}%', f'%{query}%'))
                results.extend([dict(row) for row in cursor.fetchall()])
            
            return results
            
        except Exception as e:
            print(f"Error searching items: {e}")
            return []
    
    def shutdown(self):
        """Shutdown the fileless storage system"""
        try:
            # Save to persistent storage
            self.save_to_persistent_storage()
            
            # Save to memory cache
            self.save_to_memory_cache()
            
            # Close database connection
            if self.memory_db:
                self.memory_db.close()
            
            self.initialized = False
            print("Fileless SDK Storage System shutdown complete")
            
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the fileless SDK storage system"""
    print("Testing Fileless SDK Storage System...")
    
    storage = FilelessSDKStorage()
    
    if storage.initialize():
        print("✅ Fileless SDK Storage initialized")
        
        # Test storing SDK
        sdk_data = b"Mock SDK data for testing"
        metadata = {"description": "Test SDK", "author": "Test Author"}
        
        if storage.store_sdk("test_sdk", "1.0.0", "windows", "x64", sdk_data, metadata):
            print("✅ SDK stored successfully")
        
        # Test storing toolchain
        toolchain_data = b"Mock toolchain data for testing"
        toolchain_metadata = {"description": "Test Toolchain", "compiler": "gcc"}
        
        if storage.store_toolchain("test_toolchain", "1.0.0", "gcc", "windows", 
                                 toolchain_data, toolchain_metadata):
            print("✅ Toolchain stored successfully")
        
        # Test storing library
        library_data = b"Mock library data for testing"
        library_metadata = {"description": "Test Library", "language": "python"}
        
        if storage.store_library("test_library", "1.0.0", "python", 
                               library_data, library_metadata):
            print("✅ Library stored successfully")
        
        # Test storing template
        template_data = b"Mock template data for testing"
        template_metadata = {"description": "Test Template", "type": "web_app"}
        
        if storage.store_template("test_template", "web_app", "python", 
                                template_data, template_metadata):
            print("✅ Template stored successfully")
        
        # Test storing compiler
        compiler_data = b"Mock compiler data for testing"
        compiler_metadata = {"description": "Test Compiler", "type": "gcc"}
        
        if storage.store_compiler("test_compiler", "1.0.0", "gcc", "windows", 
                                  compiler_data, compiler_metadata):
            print("✅ Compiler stored successfully")
        
        # Test retrieving data
        sdk = storage.get_sdk("test_sdk")
        if sdk:
            print(f"✅ SDK retrieved: {sdk['name']} v{sdk['version']}")
        
        toolchain = storage.get_toolchain("test_toolchain")
        if toolchain:
            print(f"✅ Toolchain retrieved: {toolchain['name']} v{toolchain['version']}")
        
        library = storage.get_library("test_library")
        if library:
            print(f"✅ Library retrieved: {library['name']} v{library['version']}")
        
        template = storage.get_template("test_template")
        if template:
            print(f"✅ Template retrieved: {template['name']}")
        
        compiler = storage.get_compiler("test_compiler")
        if compiler:
            print(f"✅ Compiler retrieved: {compiler['name']} v{compiler['version']}")
        
        # Test listing items
        sdks = storage.list_sdks()
        print(f"✅ SDKs in storage: {len(sdks)}")
        
        toolchains = storage.list_toolchains()
        print(f"✅ Toolchains in storage: {len(toolchains)}")
        
        libraries = storage.list_libraries()
        print(f"✅ Libraries in storage: {len(libraries)}")
        
        templates = storage.list_templates()
        print(f"✅ Templates in storage: {len(templates)}")
        
        compilers = storage.list_compilers()
        print(f"✅ Compilers in storage: {len(compilers)}")
        
        # Test search
        search_results = storage.search_items("test")
        print(f"✅ Search results: {len(search_results)} items found")
        
        # Test statistics
        stats = storage.get_storage_statistics()
        print(f"✅ Storage statistics: {stats['total_items']} items, {stats['total_size']} bytes")
        
        # Shutdown
        storage.shutdown()
        print("✅ Fileless SDK Storage test complete")
    else:
        print("❌ Failed to initialize Fileless SDK Storage")

if __name__ == "__main__":
    main()
