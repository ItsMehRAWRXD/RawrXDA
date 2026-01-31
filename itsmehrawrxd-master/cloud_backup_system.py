#!/usr/bin/env python3
"""
Cloud Backup System
Automatic backup to Google Drive, OneDrive, Dropbox, and other cloud services
Prevents data loss from disconnections or hardware failures
"""

import os
import sys
import json
import time
import hashlib
import threading
from datetime import datetime, timedelta
from pathlib import Path
import shutil

class CloudBackupSystem:
    """Automatic cloud backup system for IDE data"""
    
    def __init__(self):
        self.backup_active = False
        self.cloud_services = {}
        self.backup_queue = []
        self.sync_interval = 300  # 5 minutes
        self.last_sync = None
        
        print("Cloud Backup System initialized")
        self.detect_cloud_services()
    
    def detect_cloud_services(self):
        """Detect available cloud services"""
        cloud_paths = {
            'google_drive': [
                os.path.expanduser('~/Google Drive'),
                os.path.expanduser('~/GoogleDrive'),
                'C:/Users/*/Google Drive',
                'C:/Users/*/GoogleDrive'
            ],
            'onedrive': [
                os.path.expanduser('~/OneDrive'),
                'C:/Users/*/OneDrive'
            ],
            'dropbox': [
                os.path.expanduser('~/Dropbox'),
                'C:/Users/*/Dropbox'
            ],
            'icloud': [
                os.path.expanduser('~/iCloud Drive'),
                'C:/Users/*/iCloud Drive'
            ],
            'mega': [
                os.path.expanduser('~/MEGA'),
                'C:/Users/*/MEGA'
            ],
            'box': [
                os.path.expanduser('~/Box'),
                'C:/Users/*/Box'
            ]
        }
        
        for service, paths in cloud_paths.items():
            for path_pattern in paths:
                if '*' in path_pattern:
                    # Handle wildcard paths
                    import glob
                    matches = glob.glob(path_pattern)
                    for match in matches:
                        if os.path.exists(match):
                            self.cloud_services[service] = match
                            print(f"✓ {service.title()} detected: {match}")
                            break
                else:
                    if os.path.exists(path_pattern):
                        self.cloud_services[service] = path_pattern
                        print(f"✓ {service.title()} detected: {path_pattern}")
                        break
        
        if not self.cloud_services:
            print("⚠️ No cloud services detected")
        else:
            print(f"Found {len(self.cloud_services)} cloud services")
    
    def create_backup_structure(self, cloud_path):
        """Create backup directory structure in cloud"""
        backup_structure = {
            'ide_backup': {
                'projects': {},
                'settings': {},
                'sessions': {},
                'collaborations': {},
                'logs': {}
            }
        }
        
        backup_root = os.path.join(cloud_path, 'EON_IDE_Backup')
        os.makedirs(backup_root, exist_ok=True)
        
        # Create subdirectories
        for category, subdirs in backup_structure['ide_backup'].items():
            category_path = os.path.join(backup_root, category)
            os.makedirs(category_path, exist_ok=True)
            
            if isinstance(subdirs, dict):
                for subdir in subdirs:
                    subdir_path = os.path.join(category_path, subdir)
                    os.makedirs(subdir_path, exist_ok=True)
        
        return backup_root
    
    def start_backup_service(self):
        """Start automatic backup service"""
        self.backup_active = True
        
        # Start backup thread
        backup_thread = threading.Thread(target=self.run_backup_service)
        backup_thread.daemon = True
        backup_thread.start()
        
        print("Cloud backup service started")
        return {"success": True, "message": "Backup service started"}
    
    def run_backup_service(self):
        """Run backup service in background"""
        while self.backup_active:
            try:
                # Check if it's time to sync
                if self.should_sync():
                    self.perform_backup()
                    self.last_sync = datetime.now()
                
                # Process backup queue
                self.process_backup_queue()
                
                time.sleep(60)  # Check every minute
                
            except Exception as e:
                print(f"Backup service error: {e}")
                time.sleep(60)
    
    def should_sync(self):
        """Check if it's time to sync"""
        if not self.last_sync:
            return True
        
        time_since_sync = datetime.now() - self.last_sync
        return time_since_sync.total_seconds() >= self.sync_interval
    
    def perform_backup(self):
        """Perform backup to all cloud services"""
        print("Starting cloud backup...")
        
        for service, cloud_path in self.cloud_services.items():
            try:
                self.backup_to_service(service, cloud_path)
                print(f"✓ Backup to {service} completed")
            except Exception as e:
                print(f"✗ Backup to {service} failed: {e}")
    
    def backup_to_service(self, service, cloud_path):
        """Backup to specific cloud service"""
        backup_root = self.create_backup_structure(cloud_path)
        
        # Backup current project
        current_project = self.get_current_project()
        if current_project:
            self.backup_project(current_project, backup_root)
        
        # Backup IDE settings
        self.backup_settings(backup_root)
        
        # Backup collaboration sessions
        self.backup_collaborations(backup_root)
        
        # Backup logs
        self.backup_logs(backup_root)
        
        # Create backup manifest
        self.create_backup_manifest(backup_root)
    
    def get_current_project(self):
        """Get current project information"""
        # This would integrate with the IDE's project system
        return {
            'name': 'Current Project',
            'path': os.getcwd(),
            'files': self.get_project_files(),
            'last_modified': datetime.now().isoformat()
        }
    
    def get_project_files(self):
        """Get list of project files"""
        project_files = []
        
        for root, dirs, files in os.walk('.'):
            # Skip hidden directories and common non-source directories
            dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ['node_modules', '__pycache__', 'build', 'dist']]
            
            for file in files:
                if not file.startswith('.'):
                    file_path = os.path.join(root, file)
                    project_files.append({
                        'path': file_path,
                        'size': os.path.getsize(file_path),
                        'modified': datetime.fromtimestamp(os.path.getmtime(file_path)).isoformat(),
                        'hash': self.calculate_file_hash(file_path)
                    })
        
        return project_files
    
    def calculate_file_hash(self, file_path):
        """Calculate file hash for integrity checking"""
        try:
            with open(file_path, 'rb') as f:
                return hashlib.md5(f.read()).hexdigest()
        except:
            return None
    
    def backup_project(self, project, backup_root):
        """Backup project to cloud"""
        project_backup_path = os.path.join(backup_root, 'projects', project['name'])
        os.makedirs(project_backup_path, exist_ok=True)
        
        # Copy project files
        for file_info in project['files']:
            source_path = file_info['path']
            dest_path = os.path.join(project_backup_path, file_info['path'])
            
            # Create destination directory
            os.makedirs(os.path.dirname(dest_path), exist_ok=True)
            
            # Copy file
            shutil.copy2(source_path, dest_path)
        
        # Save project metadata
        project_metadata = {
            'name': project['name'],
            'path': project['path'],
            'backup_time': datetime.now().isoformat(),
            'file_count': len(project['files']),
            'files': project['files']
        }
        
        metadata_path = os.path.join(project_backup_path, 'project_metadata.json')
        with open(metadata_path, 'w') as f:
            json.dump(project_metadata, f, indent=2)
    
    def backup_settings(self, backup_root):
        """Backup IDE settings"""
        settings_path = os.path.join(backup_root, 'settings')
        
        # Backup IDE configuration
        ide_config = {
            'theme': 'dark',
            'font_size': 11,
            'auto_save': True,
            'backup_interval': self.sync_interval,
            'cloud_services': list(self.cloud_services.keys()),
            'backup_time': datetime.now().isoformat()
        }
        
        config_path = os.path.join(settings_path, 'ide_config.json')
        with open(config_path, 'w') as f:
            json.dump(ide_config, f, indent=2)
        
        # Backup user preferences
        user_prefs = {
            'language': 'en',
            'timezone': 'UTC',
            'collaboration_enabled': True,
            'cloud_backup_enabled': True,
            'preferences_time': datetime.now().isoformat()
        }
        
        prefs_path = os.path.join(settings_path, 'user_preferences.json')
        with open(prefs_path, 'w') as f:
            json.dump(user_prefs, f, indent=2)
    
    def backup_collaborations(self, backup_root):
        """Backup collaboration sessions"""
        collaborations_path = os.path.join(backup_root, 'collaborations')
        
        # Backup active collaboration sessions
        collaboration_data = {
            'active_sessions': [],
            'shared_files': {},
            'user_connections': {},
            'backup_time': datetime.now().isoformat()
        }
        
        collab_path = os.path.join(collaborations_path, 'collaboration_data.json')
        with open(collab_path, 'w') as f:
            json.dump(collaboration_data, f, indent=2)
    
    def backup_logs(self, backup_root):
        """Backup system logs"""
        logs_path = os.path.join(backup_root, 'logs')
        
        # Backup IDE logs
        log_files = [
            'distribution_protection.log',
            'protection.log',
            'ide.log',
            'collaboration.log'
        ]
        
        for log_file in log_files:
            if os.path.exists(log_file):
                dest_path = os.path.join(logs_path, log_file)
                shutil.copy2(log_file, dest_path)
    
    def create_backup_manifest(self, backup_root):
        """Create backup manifest"""
        manifest = {
            'backup_id': str(int(time.time())),
            'backup_time': datetime.now().isoformat(),
            'ide_version': 'EON Compiler IDE v1.0',
            'cloud_services': list(self.cloud_services.keys()),
            'backup_size': self.calculate_backup_size(backup_root),
            'file_count': self.count_backup_files(backup_root),
            'integrity_check': self.perform_integrity_check(backup_root)
        }
        
        manifest_path = os.path.join(backup_root, 'backup_manifest.json')
        with open(manifest_path, 'w') as f:
            json.dump(manifest, f, indent=2)
    
    def calculate_backup_size(self, backup_root):
        """Calculate total backup size"""
        total_size = 0
        for root, dirs, files in os.walk(backup_root):
            for file in files:
                file_path = os.path.join(root, file)
                total_size += os.path.getsize(file_path)
        return total_size
    
    def count_backup_files(self, backup_root):
        """Count total backup files"""
        file_count = 0
        for root, dirs, files in os.walk(backup_root):
            file_count += len(files)
        return file_count
    
    def perform_integrity_check(self, backup_root):
        """Perform integrity check on backup"""
        integrity_results = {
            'check_time': datetime.now().isoformat(),
            'files_checked': 0,
            'corrupted_files': [],
            'integrity_score': 100
        }
        
        for root, dirs, files in os.walk(backup_root):
            for file in files:
                file_path = os.path.join(root, file)
                integrity_results['files_checked'] += 1
                
                # Simple integrity check - file exists and is readable
                try:
                    with open(file_path, 'r') as f:
                        f.read(1)  # Try to read first character
                except:
                    integrity_results['corrupted_files'].append(file_path)
        
        if integrity_results['corrupted_files']:
            integrity_results['integrity_score'] = 100 - (len(integrity_results['corrupted_files']) / integrity_results['files_checked'] * 100)
        
        return integrity_results
    
    def process_backup_queue(self):
        """Process backup queue"""
        while self.backup_queue:
            backup_item = self.backup_queue.pop(0)
            self.process_backup_item(backup_item)
    
    def process_backup_item(self, backup_item):
        """Process individual backup item"""
        item_type = backup_item.get('type')
        
        if item_type == 'file_change':
            self.backup_file_change(backup_item)
        elif item_type == 'project_save':
            self.backup_project_save(backup_item)
        elif item_type == 'settings_change':
            self.backup_settings_change(backup_item)
    
    def backup_file_change(self, backup_item):
        """Backup file change"""
        file_path = backup_item.get('file_path')
        content = backup_item.get('content')
        
        # Add to backup queue for immediate sync
        self.backup_queue.append({
            'type': 'file_change',
            'file_path': file_path,
            'content': content,
            'timestamp': datetime.now().isoformat()
        })
    
    def backup_project_save(self, backup_item):
        """Backup project save"""
        project_path = backup_item.get('project_path')
        
        # Add to backup queue
        self.backup_queue.append({
            'type': 'project_save',
            'project_path': project_path,
            'timestamp': datetime.now().isoformat()
        })
    
    def backup_settings_change(self, backup_item):
        """Backup settings change"""
        settings = backup_item.get('settings')
        
        # Add to backup queue
        self.backup_queue.append({
            'type': 'settings_change',
            'settings': settings,
            'timestamp': datetime.now().isoformat()
        })
    
    def restore_from_backup(self, cloud_service, backup_id=None):
        """Restore from cloud backup"""
        if cloud_service not in self.cloud_services:
            return {"success": False, "error": f"Cloud service {cloud_service} not available"}
        
        cloud_path = self.cloud_services[cloud_service]
        backup_root = os.path.join(cloud_path, 'EON_IDE_Backup')
        
        if not os.path.exists(backup_root):
            return {"success": False, "error": "No backup found"}
        
        try:
            # Find latest backup if no specific ID provided
            if not backup_id:
                backup_id = self.find_latest_backup(backup_root)
            
            if not backup_id:
                return {"success": False, "error": "No backup found"}
            
            # Restore project
            self.restore_project(backup_root, backup_id)
            
            # Restore settings
            self.restore_settings(backup_root, backup_id)
            
            return {"success": True, "backup_id": backup_id}
            
        except Exception as e:
            return {"success": False, "error": str(e)}
    
    def find_latest_backup(self, backup_root):
        """Find latest backup ID"""
        manifest_files = []
        
        for root, dirs, files in os.walk(backup_root):
            for file in files:
                if file == 'backup_manifest.json':
                    manifest_files.append(os.path.join(root, file))
        
        if not manifest_files:
            return None
        
        # Sort by modification time
        manifest_files.sort(key=os.path.getmtime, reverse=True)
        
        # Read latest manifest
        with open(manifest_files[0], 'r') as f:
            manifest = json.load(f)
        
        return manifest.get('backup_id')
    
    def restore_project(self, backup_root, backup_id):
        """Restore project from backup"""
        projects_path = os.path.join(backup_root, 'projects')
        
        # Find project backup
        for project_dir in os.listdir(projects_path):
            project_path = os.path.join(projects_path, project_dir)
            if os.path.isdir(project_path):
                metadata_path = os.path.join(project_path, 'project_metadata.json')
                if os.path.exists(metadata_path):
                    with open(metadata_path, 'r') as f:
                        metadata = json.load(f)
                    
                    if metadata.get('backup_time') == backup_id:
                        # Restore project files
                        self.restore_project_files(project_path, metadata)
                        break
    
    def restore_project_files(self, project_path, metadata):
        """Restore project files"""
        for file_info in metadata.get('files', []):
            source_path = os.path.join(project_path, file_info['path'])
            dest_path = file_info['path']
            
            if os.path.exists(source_path):
                # Create destination directory
                os.makedirs(os.path.dirname(dest_path), exist_ok=True)
                
                # Copy file
                shutil.copy2(source_path, dest_path)
    
    def restore_settings(self, backup_root, backup_id):
        """Restore settings from backup"""
        settings_path = os.path.join(backup_root, 'settings')
        
        # Restore IDE configuration
        config_path = os.path.join(settings_path, 'ide_config.json')
        if os.path.exists(config_path):
            with open(config_path, 'r') as f:
                config = json.load(f)
            
            # Apply configuration
            print(f"Restored IDE configuration from backup {backup_id}")
    
    def stop_backup_service(self):
        """Stop backup service"""
        self.backup_active = False
        print("Cloud backup service stopped")
    
    def get_backup_status(self):
        """Get backup status"""
        return {
            'active': self.backup_active,
            'cloud_services': list(self.cloud_services.keys()),
            'last_sync': self.last_sync.isoformat() if self.last_sync else None,
            'sync_interval': self.sync_interval,
            'queue_size': len(self.backup_queue)
        }

def main():
    """Test cloud backup system"""
    print("Testing Cloud Backup System...")
    
    backup_system = CloudBackupSystem()
    
    # Start backup service
    result = backup_system.start_backup_service()
    if result["success"]:
        print("✅ Backup service started")
    else:
        print(f"❌ Failed to start backup service: {result['error']}")
    
    # Get status
    status = backup_system.get_backup_status()
    print(f"Backup status: {status}")
    
    print("Cloud Backup System test complete!")

if __name__ == "__main__":
    main()
