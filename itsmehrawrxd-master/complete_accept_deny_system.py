#!/usr/bin/env python3
"""
Complete Accept/Deny System - User Permission and Confirmation Management
Integrates with all IDE systems for comprehensive user control
"""

import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import os
import sys
import threading
import time
import json
from pathlib import Path
from enum import Enum
from typing import Dict, List, Optional, Any
import hashlib
import random

class PermissionLevel(Enum):
    """Permission levels for operations"""
    ALLOW = "allow"
    DENY = "deny"
    ASK = "ask"
    AUTO = "auto"

class OperationType(Enum):
    """Types of operations requiring permissions"""
    FILE_ACCESS = "file_access"
    NETWORK_ACCESS = "network_access"
    SYSTEM_COMMAND = "system_command"
    AI_REQUEST = "ai_request"
    SANDBOX_CREATION = "sandbox_creation"
    CODE_EXECUTION = "code_execution"
    PLUGIN_INSTALL = "plugin_install"
    CLOUD_SYNC = "cloud_sync"
    MEMORY_ALLOCATION = "memory_allocation"
    PROCESS_SPAWN = "process_spawn"

class CompleteAcceptDenySystem:
    """
    Complete Accept/Deny system for user permissions and confirmations
    Provides granular control over all IDE operations
    """
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.permissions = {}
        self.pending_requests = {}
        self.request_history = []
        self.user_preferences = {}
        self.security_policies = {}
        
        # Initialize default permissions
        self._initialize_default_permissions()
        
        print("🔐 Complete Accept/Deny System initialized")
    
    def _initialize_default_permissions(self):
        """Initialize default permission settings"""
        
        default_permissions = {
            OperationType.FILE_ACCESS: PermissionLevel.ASK,
            OperationType.NETWORK_ACCESS: PermissionLevel.ASK,
            OperationType.SYSTEM_COMMAND: PermissionLevel.DENY,
            OperationType.AI_REQUEST: PermissionLevel.ASK,
            OperationType.SANDBOX_CREATION: PermissionLevel.ASK,
            OperationType.CODE_EXECUTION: PermissionLevel.ASK,
            OperationType.PLUGIN_INSTALL: PermissionLevel.DENY,
            OperationType.CLOUD_SYNC: PermissionLevel.ASK,
            OperationType.MEMORY_ALLOCATION: PermissionLevel.AUTO,
            OperationType.PROCESS_SPAWN: PermissionLevel.DENY
        }
        
        self.permissions = default_permissions
        
        # Initialize security policies
        self.security_policies = {
            'strict_mode': False,
            'auto_deny_suspicious': True,
            'require_explicit_consent': True,
            'log_all_requests': True,
            'max_requests_per_minute': 60
        }
        
        print("🔐 Default permissions and security policies initialized")
    
    def request_permission(self, operation_type: OperationType, details: Dict, 
                          context: Dict = None) -> bool:
        """Request permission for an operation"""
        
        permission_level = self.permissions.get(operation_type, PermissionLevel.ASK)
        
        # Check rate limiting
        if self._is_rate_limited():
            self._log_request(operation_type, details, "RATE_LIMITED", False)
            return False
        
        # Check if auto-allowed
        if permission_level == PermissionLevel.ALLOW:
            self._log_request(operation_type, details, "AUTO_ALLOWED", True)
            return True
        
        # Check if auto-denied
        if permission_level == PermissionLevel.DENY:
            self._log_request(operation_type, details, "AUTO_DENIED", False)
            return False
        
        # Check security policies
        if self._should_auto_deny(operation_type, details):
            self._log_request(operation_type, details, "AUTO_DENIED_SECURITY", False)
            return False
        
        # Check if auto mode
        if permission_level == PermissionLevel.AUTO:
            auto_decision = self._auto_decide_permission(operation_type, details, context)
            self._log_request(operation_type, details, "AUTO_DECIDED", auto_decision)
            return auto_decision
        
        # Ask user for permission
        return self._ask_user_permission(operation_type, details, context)
    
    def _is_rate_limited(self) -> bool:
        """Check if request is rate limited"""
        
        max_requests = self.security_policies.get('max_requests_per_minute', 60)
        
        # Count requests in last minute
        current_time = time.time()
        recent_requests = [
            req for req in self.request_history 
            if current_time - req['timestamp'] < 60
        ]
        
        return len(recent_requests) >= max_requests
    
    def _should_auto_deny(self, operation_type: OperationType, details: Dict) -> bool:
        """Check if operation should be auto-denied based on security policies"""
        
        if not self.security_policies.get('auto_deny_suspicious', True):
            return False
        
        # Check for suspicious patterns
        if operation_type == OperationType.SYSTEM_COMMAND:
            command = details.get('command', '')
            suspicious_commands = ['rm -rf', 'del /s', 'format', 'fdisk', 'mkfs']
            if any(suspicious in command.lower() for suspicious in suspicious_commands):
                return True
        
        elif operation_type == OperationType.NETWORK_ACCESS:
            url = details.get('url', '')
            if any(domain in url.lower() for domain in ['malicious.com', 'phishing.net']):
                return True
        
        elif operation_type == OperationType.FILE_ACCESS:
            file_path = details.get('file_path', '')
            sensitive_paths = ['/etc/', '/system32/', '/boot/', 'C:\\Windows\\']
            if any(sensitive in file_path for sensitive in sensitive_paths):
                return True
        
        return False
    
    def _auto_decide_permission(self, operation_type: OperationType, details: Dict, 
                               context: Dict = None) -> bool:
        """Automatically decide permission based on context and history"""
        
        # Get historical success rate for this operation type
        historical_success = self._get_historical_success_rate(operation_type)
        
        # Get user preference for this operation
        user_preference = self.user_preferences.get(operation_type, 0.5)
        
        # Get context trust level
        trust_level = self._calculate_trust_level(context)
        
        # Calculate decision score
        decision_score = (
            historical_success * 0.4 +
            user_preference * 0.3 +
            trust_level * 0.3
        )
        
        return decision_score > 0.6
    
    def _get_historical_success_rate(self, operation_type: OperationType) -> float:
        """Get historical success rate for operation type"""
        
        operation_requests = [
            req for req in self.request_history 
            if req['operation_type'] == operation_type
        ]
        
        if not operation_requests:
            return 0.5  # Default neutral
        
        successful = sum(1 for req in operation_requests if req['granted'])
        return successful / len(operation_requests)
    
    def _calculate_trust_level(self, context: Dict = None) -> float:
        """Calculate trust level based on context"""
        
        if not context:
            return 0.5
        
        trust_score = 0.5
        
        # Check if from trusted source
        source = context.get('source', '')
        trusted_sources = ['safe_hybrid_ide', 'internal_system', 'user_initiated']
        if source in trusted_sources:
            trust_score += 0.3
        
        # Check if in sandbox
        if context.get('in_sandbox', False):
            trust_score += 0.2
        
        # Check user interaction
        if context.get('user_initiated', False):
            trust_score += 0.2
        
        # Check time of day (more suspicious at night)
        current_hour = time.localtime().tm_hour
        if 22 <= current_hour or current_hour <= 6:
            trust_score -= 0.1
        
        return max(0.0, min(1.0, trust_score))
    
    def _ask_user_permission(self, operation_type: OperationType, details: Dict, 
                           context: Dict = None) -> bool:
        """Ask user for permission"""
        
        # Generate unique request ID
        request_id = self._generate_request_id()
        
        # Store pending request
        self.pending_requests[request_id] = {
            'operation_type': operation_type,
            'details': details,
            'context': context,
            'timestamp': time.time(),
            'status': 'pending'
        }
        
        # Show permission dialog
        return self._show_permission_dialog(request_id)
    
    def _generate_request_id(self) -> str:
        """Generate unique request ID"""
        
        timestamp = str(time.time())
        random_data = str(random.random())
        return hashlib.sha256((timestamp + random_data).encode()).hexdigest()[:12]
    
    def _show_permission_dialog(self, request_id: str) -> bool:
        """Show permission dialog to user"""
        
        if not hasattr(self.ide, 'root'):
            # Fallback for non-GUI environments
            return self._console_permission_dialog(request_id)
        
        request = self.pending_requests[request_id]
        operation_type = request['operation_type']
        details = request['details']
        
        # Create dialog
        dialog = tk.Toplevel(self.ide.root)
        dialog.title("🔐 Permission Request")
        dialog.geometry("600x500")
        dialog.transient(self.ide.root)
        dialog.grab_set()
        
        # Center dialog
        dialog.geometry("+%d+%d" % (
            self.ide.root.winfo_rootx() + 50,
            self.ide.root.winfo_rooty() + 50
        ))
        
        # Main frame
        main_frame = ttk.Frame(dialog, padding="20")
        main_frame.pack(fill='both', expand=True)
        
        # Title
        title_label = ttk.Label(main_frame, 
                               text="🔐 Permission Request", 
                               font=("Arial", 16, "bold"))
        title_label.pack(pady=(0, 20))
        
        # Operation info
        info_frame = ttk.LabelFrame(main_frame, text="Operation Details", padding="10")
        info_frame.pack(fill='x', pady=(0, 20))
        
        ttk.Label(info_frame, text=f"Operation: {operation_type.value}").pack(anchor=tk.W)
        ttk.Label(info_frame, text=f"Time: {time.ctime()}").pack(anchor=tk.W)
        
        # Details
        details_frame = ttk.LabelFrame(main_frame, text="Details", padding="10")
        details_frame.pack(fill='both', expand=True, pady=(0, 20))
        
        details_text = scrolledtext.ScrolledText(details_frame, height=10, width=60)
        details_text.pack(fill='both', expand=True)
        
        # Format details for display
        formatted_details = json.dumps(details, indent=2)
        details_text.insert(1.0, formatted_details)
        details_text.config(state='disabled')
        
        # Buttons
        button_frame = ttk.Frame(main_frame)
        button_frame.pack(pady=(0, 10))
        
        result = {'decision': None}
        
        def allow():
            result['decision'] = True
            dialog.destroy()
        
        def deny():
            result['decision'] = False
            dialog.destroy()
        
        def remember_allow():
            self._set_permission_level(operation_type, PermissionLevel.ALLOW)
            result['decision'] = True
            dialog.destroy()
        
        def remember_deny():
            self._set_permission_level(operation_type, PermissionLevel.DENY)
            result['decision'] = False
            dialog.destroy()
        
        ttk.Button(button_frame, text="✅ Allow", command=allow).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="❌ Deny", command=deny).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="✅ Always Allow", command=remember_allow).pack(side=tk.LEFT, padx=5)
        ttk.Button(button_frame, text="❌ Always Deny", command=remember_deny).pack(side=tk.LEFT, padx=5)
        
        # Wait for user decision
        dialog.wait_window()
        
        # Clean up
        decision = result['decision']
        if decision is None:
            decision = False  # Default to deny if dialog was closed
        
        # Update request status
        self.pending_requests[request_id]['status'] = 'completed'
        self.pending_requests[request_id]['decision'] = decision
        
        # Log request
        self._log_request(operation_type, details, "USER_DECIDED", decision)
        
        return decision
    
    def _console_permission_dialog(self, request_id: str) -> bool:
        """Console-based permission dialog for non-GUI environments"""
        
        request = self.pending_requests[request_id]
        operation_type = request['operation_type']
        details = request['details']
        
        print(f"\n🔐 Permission Request:")
        print(f"Operation: {operation_type.value}")
        print(f"Details: {details}")
        print(f"Time: {time.ctime()}")
        
        while True:
            choice = input("Allow? (y/n/always y/always n): ").lower().strip()
            
            if choice in ['y', 'yes']:
                decision = True
                break
            elif choice in ['n', 'no']:
                decision = False
                break
            elif choice in ['always y', 'always allow']:
                self._set_permission_level(operation_type, PermissionLevel.ALLOW)
                decision = True
                break
            elif choice in ['always n', 'always deny']:
                self._set_permission_level(operation_type, PermissionLevel.DENY)
                decision = False
                break
            else:
                print("Please enter y, n, 'always y', or 'always n'")
        
        # Update request status
        self.pending_requests[request_id]['status'] = 'completed'
        self.pending_requests[request_id]['decision'] = decision
        
        # Log request
        self._log_request(operation_type, details, "USER_DECIDED", decision)
        
        return decision
    
    def _set_permission_level(self, operation_type: OperationType, level: PermissionLevel):
        """Set permission level for operation type"""
        
        self.permissions[operation_type] = level
        print(f"🔐 Permission level set for {operation_type.value}: {level.value}")
    
    def _log_request(self, operation_type: OperationType, details: Dict, 
                    reason: str, granted: bool):
        """Log permission request"""
        
        log_entry = {
            'timestamp': time.time(),
            'operation_type': operation_type,
            'details': details,
            'reason': reason,
            'granted': granted
        }
        
        self.request_history.append(log_entry)
        
        # Keep only recent history
        if len(self.request_history) > 1000:
            self.request_history = self.request_history[-1000:]
        
        if self.security_policies.get('log_all_requests', True):
            print(f"🔐 Permission log: {operation_type.value} - {reason} - {'GRANTED' if granted else 'DENIED'}")
    
    def get_permission_status(self) -> Dict:
        """Get current permission status"""
        
        return {
            'permissions': {op.value: perm.value for op, perm in self.permissions.items()},
            'pending_requests': len(self.pending_requests),
            'total_requests': len(self.request_history),
            'security_policies': self.security_policies,
            'user_preferences': {op.value: pref for op, pref in self.user_preferences.items()}
        }
    
    def set_user_preference(self, operation_type: OperationType, preference: float):
        """Set user preference for operation type (0.0 to 1.0)"""
        
        self.user_preferences[operation_type] = max(0.0, min(1.0, preference))
        print(f"👤 User preference set for {operation_type.value}: {preference}")
    
    def update_security_policy(self, policy_name: str, value: Any):
        """Update security policy"""
        
        if policy_name in self.security_policies:
            self.security_policies[policy_name] = value
            print(f"🔐 Security policy updated: {policy_name} = {value}")
        else:
            print(f"⚠️ Unknown security policy: {policy_name}")
    
    def get_request_history(self, limit: int = 50) -> List[Dict]:
        """Get recent request history"""
        
        return self.request_history[-limit:] if limit else self.request_history

class AcceptDenySystemGUI:
    """GUI for the Accept/Deny system"""
    
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.accept_deny_system = CompleteAcceptDenySystem(ide_instance)
        self.setup_gui()
    
    def setup_gui(self):
        """Setup GUI for Accept/Deny system"""
        
        # Create new window
        self.perm_window = tk.Toplevel(self.ide.root)
        self.perm_window.title("🔐 Accept/Deny Permission System")
        self.perm_window.geometry("800x600")
        
        # Main frame
        main_frame = ttk.Frame(self.perm_window, padding="10")
        main_frame.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Title
        title_label = ttk.Label(main_frame, text="🔐 Accept/Deny Permission System", 
                               font=("Arial", 16, "bold"))
        title_label.grid(row=0, column=0, columnspan=3, pady=(0, 20))
        
        # Permission settings
        self.setup_permissions_section(main_frame)
        
        # Security policies
        self.setup_security_section(main_frame)
        
        # Request history
        self.setup_history_section(main_frame)
        
        # Configure grid weights
        self.perm_window.columnconfigure(0, weight=1)
        self.perm_window.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)
        main_frame.rowconfigure(3, weight=1)
    
    def setup_permissions_section(self, parent):
        """Setup permissions configuration"""
        
        perm_frame = ttk.LabelFrame(parent, text="🔐 Permission Settings", padding="10")
        perm_frame.grid(row=1, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Permission list
        self.permission_vars = {}
        
        for i, (op_type, perm_level) in enumerate(self.accept_deny_system.permissions.items()):
            ttk.Label(perm_frame, text=op_type.value.replace('_', ' ').title()).grid(
                row=i, column=0, sticky=tk.W, pady=2)
            
            perm_var = tk.StringVar(value=perm_level.value)
            self.permission_vars[op_type] = perm_var
            
            perm_combo = ttk.Combobox(perm_frame, textvariable=perm_var,
                                     values=['allow', 'deny', 'ask', 'auto'], width=10)
            perm_combo.grid(row=i, column=1, sticky=tk.W, padx=(10, 0), pady=2)
        
        # Save button
        ttk.Button(perm_frame, text="💾 Save Permissions", 
                  command=self.save_permissions).grid(row=len(OperationType), column=0, columnspan=2, pady=10)
    
    def setup_security_section(self, parent):
        """Setup security policies"""
        
        security_frame = ttk.LabelFrame(parent, text="🛡️ Security Policies", padding="10")
        security_frame.grid(row=2, column=0, columnspan=3, sticky=(tk.W, tk.E), pady=(0, 10))
        
        # Security policy checkboxes
        self.security_vars = {}
        
        policies = [
            ('strict_mode', 'Strict Mode'),
            ('auto_deny_suspicious', 'Auto-deny Suspicious Operations'),
            ('require_explicit_consent', 'Require Explicit Consent'),
            ('log_all_requests', 'Log All Requests')
        ]
        
        for i, (policy_key, policy_label) in enumerate(policies):
            var = tk.BooleanVar(value=self.accept_deny_system.security_policies.get(policy_key, False))
            self.security_vars[policy_key] = var
            
            checkbox = ttk.Checkbutton(security_frame, text=policy_label, variable=var)
            checkbox.grid(row=i, column=0, sticky=tk.W, pady=2)
        
        # Rate limit setting
        ttk.Label(security_frame, text="Max Requests per Minute:").grid(row=len(policies), column=0, sticky=tk.W, pady=2)
        self.rate_limit_var = tk.StringVar(value=str(self.accept_deny_system.security_policies.get('max_requests_per_minute', 60)))
        rate_limit_entry = ttk.Entry(security_frame, textvariable=self.rate_limit_var, width=10)
        rate_limit_entry.grid(row=len(policies), column=1, sticky=tk.W, padx=(10, 0), pady=2)
        
        # Save button
        ttk.Button(security_frame, text="💾 Save Security Policies", 
                  command=self.save_security_policies).grid(row=len(policies)+1, column=0, columnspan=2, pady=10)
    
    def setup_history_section(self, parent):
        """Setup request history display"""
        
        history_frame = ttk.LabelFrame(parent, text="📋 Request History", padding="10")
        history_frame.grid(row=3, column=0, columnspan=3, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # History text
        self.history_text = scrolledtext.ScrolledText(history_frame, height=15, width=80)
        self.history_text.grid(row=0, column=0, sticky=(tk.W, tk.E, tk.N, tk.S))
        
        # Refresh button
        ttk.Button(history_frame, text="🔄 Refresh History", 
                  command=self.refresh_history).grid(row=1, column=0, pady=5)
        
        history_frame.columnconfigure(0, weight=1)
        history_frame.rowconfigure(0, weight=1)
        
        # Initial history
        self.refresh_history()
    
    def save_permissions(self):
        """Save permission settings"""
        
        for op_type, perm_var in self.permission_vars.items():
            new_level = PermissionLevel(perm_var.get())
            self.accept_deny_system.permissions[op_type] = new_level
        
        messagebox.showinfo("Success", "Permissions saved successfully!")
        print("💾 Permissions saved")
    
    def save_security_policies(self):
        """Save security policies"""
        
        for policy_key, var in self.security_vars.items():
            self.accept_deny_system.security_policies[policy_key] = var.get()
        
        # Update rate limit
        try:
            rate_limit = int(self.rate_limit_var.get())
            self.accept_deny_system.security_policies['max_requests_per_minute'] = rate_limit
        except ValueError:
            messagebox.showerror("Error", "Invalid rate limit value")
            return
        
        messagebox.showinfo("Success", "Security policies saved successfully!")
        print("💾 Security policies saved")
    
    def refresh_history(self):
        """Refresh request history display"""
        
        self.history_text.delete(1.0, tk.END)
        
        history = self.accept_deny_system.get_request_history(50)
        
        self.history_text.insert(tk.END, "📋 Recent Permission Requests\n")
        self.history_text.insert(tk.END, "=" * 60 + "\n\n")
        
        for entry in reversed(history[-20:]):  # Show last 20 entries
            status = "✅ GRANTED" if entry['granted'] else "❌ DENIED"
            timestamp = time.ctime(entry['timestamp'])
            
            self.history_text.insert(tk.END, f"{status} - {entry['operation_type'].value}\n")
            self.history_text.insert(tk.END, f"  Time: {timestamp}\n")
            self.history_text.insert(tk.END, f"  Reason: {entry['reason']}\n")
            self.history_text.insert(tk.END, "\n")

# Integration function
def integrate_accept_deny_system(ide_instance):
    """Integrate Accept/Deny system with IDE"""
    
    if hasattr(ide_instance, 'add_menu_item'):
        ide_instance.add_menu_item("Tools", "Accept/Deny System", 
                                 lambda: AcceptDenySystemGUI(ide_instance))
    
    # Add the system to IDE instance
    ide_instance.accept_deny_system = CompleteAcceptDenySystem(ide_instance)
    
    print("🔐 Accept/Deny System integrated with IDE")

if __name__ == "__main__":
    print("🔐 Complete Accept/Deny System")
    print("=" * 50)
    
    class MockIDE:
        def __init__(self):
            self.root = tk.Tk()
    
    ide = MockIDE()
    accept_deny_system = CompleteAcceptDenySystem(ide)
    
    print("✅ Accept/Deny System ready!")
    print("🔐 Provides granular control over all IDE operations!")
