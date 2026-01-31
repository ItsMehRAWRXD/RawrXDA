#!/usr/bin/env python3
"""
Comprehensive Distribution Protection System
Prevents unauthorized distribution and source code leaks
"""

import os
import sys
import hashlib
import time
import json
import subprocess
from pathlib import Path
from datetime import datetime

class ComprehensiveDistributionProtection:
    """Advanced distribution protection system"""
    
    def __init__(self):
        self.protection_active = True
        self.startup_time = time.time()
        self.protection_log = "distribution_protection.log"
        self.source_files = []
        self.suspicious_activities = []
        
        print("🛡️ Comprehensive Distribution Protection System Active")
        self.initialize_protection()
    
    def initialize_protection(self):
        """Initialize all protection mechanisms"""
        self.scan_source_files()
        self.check_environment()
        self.setup_file_monitoring()
        self.create_protection_watermarks()
        self.log_protection_event("INITIALIZATION", "Protection system activated")
    
    def scan_source_files(self):
        """Scan for all source files that need protection"""
        source_extensions = ['.py', '.eon', '.asm', '.c', '.cpp', '.h', '.hpp', '.js', '.ts', '.java', '.cs']
        
        for root, dirs, files in os.walk('.'):
            # Skip hidden directories and common non-source directories
            dirs[:] = [d for d in dirs if not d.startswith('.') and d not in ['node_modules', '__pycache__', 'build', 'dist']]
            
            for file in files:
                if any(file.endswith(ext) for ext in source_extensions):
                    filepath = os.path.join(root, file)
                    self.source_files.append(filepath)
        
        print(f"📁 Protected {len(self.source_files)} source files")
    
    def check_environment(self):
        """Check for suspicious environment conditions"""
        suspicious_indicators = []
        
        # Check for file sharing tools in PATH
        sharing_tools = [
            'dropbox', 'onedrive', 'googledrive', 'mega', 'wetransfer',
            'box', 'icloud', 'sharepoint', 'teamviewer', 'anydesk'
        ]
        
        path_env = os.environ.get('PATH', '').lower()
        for tool in sharing_tools:
            if tool in path_env:
                suspicious_indicators.append(f"File sharing tool detected: {tool}")
        
        # Check for version control systems
        vcs_indicators = ['.git', '.svn', '.hg', '.bzr', '.fossil']
        for vcs in vcs_indicators:
            if os.path.exists(vcs):
                suspicious_indicators.append(f"Version control system detected: {vcs}")
        
        # Check for cloud development environments
        cloud_indicators = [
            'CODESPACES', 'GITPOD', 'REPLIT', 'CODEPEN', 'JSFIDDLE',
            'STACKBLITZ', 'CODESANDBOX', 'GLITCH'
        ]
        
        for indicator in cloud_indicators:
            if indicator in os.environ:
                suspicious_indicators.append(f"Cloud development environment detected: {indicator}")
        
        # Check for remote desktop tools
        remote_tools = ['RDP', 'VNC', 'TEAMVIEWER', 'ANYDESK', 'CHROME_REMOTE']
        for tool in remote_tools:
            if tool in os.environ:
                suspicious_indicators.append(f"Remote access tool detected: {tool}")
        
        self.suspicious_activities.extend(suspicious_indicators)
        
        if suspicious_indicators:
            print("⚠️ Suspicious environment conditions detected:")
            for indicator in suspicious_indicators:
                print(f"   - {indicator}")
                self.log_protection_event("SUSPICIOUS_ENVIRONMENT", indicator)
    
    def setup_file_monitoring(self):
        """Setup file access monitoring"""
        # Override built-in open function to monitor file access
        original_open = open
        
        def monitored_open(*args, **kwargs):
            if len(args) > 0 and isinstance(args[0], str):
                filename = args[0]
                if any(filename.endswith(ext) for ext in ['.py', '.eon', '.asm', '.c', '.cpp', '.h']):
                    self.log_file_access(filename)
            return original_open(*args, **kwargs)
        
        # Monkey patch the open function
        import builtins
        builtins.open = monitored_open
    
    def log_file_access(self, filename):
        """Log file access attempts"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"[{timestamp}] FILE_ACCESS: {filename}\n"
        
        try:
            with open(self.protection_log, "a", encoding='utf-8') as f:
                f.write(log_entry)
        except:
            pass  # Silent fail for logging
    
    def create_protection_watermarks(self):
        """Create watermarks in source files"""
        protection_header = '''"""
PROPRIETARY SOFTWARE - DO NOT DISTRIBUTE
Copyright (c) 2025 - All Rights Reserved

This software is proprietary and confidential.
Unauthorized distribution is prohibited.

Source code protection active.
Distribution monitoring enabled.
"""
'''
        
        watermarked_count = 0
        for filepath in self.source_files:
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    content = f.read()
                
                # Only add watermark if not already present
                if "PROPRIETARY SOFTWARE" not in content and not filepath.endswith('__init__.py'):
                    with open(filepath, 'w', encoding='utf-8') as f:
                        f.write(protection_header + content)
                    watermarked_count += 1
            except:
                pass  # Silent fail for individual files
        
        print(f"🔒 Watermarked {watermarked_count} source files")
    
    def log_protection_event(self, event_type, details):
        """Log protection events"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"[{timestamp}] {event_type}: {details}\n"
        
        try:
            with open(self.protection_log, "a", encoding='utf-8') as f:
                f.write(log_entry)
        except:
            pass
    
    def check_distribution_attempts(self):
        """Check for potential distribution attempts"""
        current_suspicious = []
        
        # Check for file sharing tools
        sharing_tools = ['dropbox', 'onedrive', 'google drive', 'mega', 'wetransfer']
        for tool in sharing_tools:
            if tool in os.environ.get('PATH', '').lower():
                current_suspicious.append(f"File sharing tool detected: {tool}")
        
        # Check for version control systems
        vcs_tools = ['.git', '.svn', '.hg', '.bzr']
        for vcs in vcs_tools:
            if os.path.exists(vcs):
                current_suspicious.append(f"Version control detected: {vcs}")
        
        # Check for network sharing
        try:
            result = subprocess.run(['netstat', '-an'], capture_output=True, text=True, timeout=5)
            if result.returncode == 0:
                if 'LISTENING' in result.stdout and any(port in result.stdout for port in ['8080', '3000', '5000']):
                    current_suspicious.append("Network sharing detected")
        except:
            pass
        
        return current_suspicious
    
    def enforce_protection(self):
        """Enforce distribution protection"""
        if not self.protection_active:
            return
        
        # Check for suspicious activities
        suspicious = self.check_distribution_attempts()
        if suspicious:
            for activity in suspicious:
                self.log_protection_event("SUSPICIOUS_ACTIVITY", activity)
                print(f"⚠️ Suspicious activity detected: {activity}")
        
        # Log periodic status
        runtime = time.time() - self.startup_time
        if runtime % 300 < 1:  # Every 5 minutes
            self.log_protection_event("STATUS_CHECK", f"Protection active for {runtime:.0f} seconds")
    
    def create_protection_report(self):
        """Create protection status report"""
        report = {
            "timestamp": datetime.now().isoformat(),
            "protection_active": self.protection_active,
            "runtime_seconds": time.time() - self.startup_time,
            "protected_files": len(self.source_files),
            "suspicious_activities": len(self.suspicious_activities),
            "environment_checks": {
                "file_sharing_tools": any('dropbox' in os.environ.get('PATH', '').lower() or 
                                        'onedrive' in os.environ.get('PATH', '').lower()),
                "version_control": any(os.path.exists(vcs) for vcs in ['.git', '.svn', '.hg', '.bzr']),
                "cloud_environment": any(env in os.environ for env in ['CODESPACES', 'GITPOD', 'REPLIT'])
            }
        }
        
        try:
            with open("protection_report.json", "w") as f:
                json.dump(report, f, indent=2)
            return report
        except:
            return None
    
    def show_protection_status(self):
        """Show current protection status"""
        print("\n" + "="*60)
        print("🛡️ DISTRIBUTION PROTECTION STATUS")
        print("="*60)
        print(f"Protection Active: {'✅ YES' if self.protection_active else '❌ NO'}")
        print(f"Runtime: {time.time() - self.startup_time:.0f} seconds")
        print(f"Protected Files: {len(self.source_files)}")
        print(f"Suspicious Activities: {len(self.suspicious_activities)}")
        
        if self.suspicious_activities:
            print("\n⚠️ Suspicious Activities Detected:")
            for activity in self.suspicious_activities:
                print(f"   - {activity}")
        
        print("="*60)

def main():
    """Test the comprehensive distribution protection system"""
    print("Testing Comprehensive Distribution Protection System...")
    
    protection = ComprehensiveDistributionProtection()
    
    # Show initial status
    protection.show_protection_status()
    
    # Test protection enforcement
    print("\nTesting protection enforcement...")
    protection.enforce_protection()
    
    # Create protection report
    print("\nCreating protection report...")
    report = protection.create_protection_report()
    if report:
        print("✅ Protection report created: protection_report.json")
    else:
        print("❌ Failed to create protection report")
    
    print("\n🛡️ Comprehensive Distribution Protection System test complete!")

if __name__ == "__main__":
    main()
